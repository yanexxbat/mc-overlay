// agent_main.cpp - the half of the tool that runs INSIDE javaw.exe.
//
// Loaded by the JVM through the standard attach mechanism (see AgentLoader.java),
// which calls Agent_OnAttach below. From there we:
//   1. adopt the JavaVM* the JVM handed us (no JNI_GetCreatedJavaVMs needed),
//   2. install a JVMTI-backed class resolver so Minecraft's classes are findable
//      from a natively attached thread,
//   3. start one daemon sampler thread that publishes world state into shared
//      memory after each coherent camera/entity capture,
//   4. tear all of that down deterministically on VM death.
//
// Everything the external overlay knows comes through the shared block; the
// overlay never touches JNI.
#include <jvmti.h>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include "../common/ipc.hpp"
#include "../common/log.hpp"
#include "../common/shared_state.hpp"
#include "jvm_interface.hpp"
#include "generated_profiles.hpp"
#include "geometry_bindings.hpp"
#include "frame_sync.hpp"
#include "entity_details_cache.hpp"
#include "entity_identity_cache.hpp"
#include "../common/equipment_icons.hpp"
#include "../common/chunk_hints.hpp"
#include "combat_live.hpp"
#include "item_assist_live.hpp"

using namespace mcov;

namespace {

// ---------------------------------------------------------------------------
// Agent-wide state
// ---------------------------------------------------------------------------
jvmtiEnv*         g_jvmti = nullptr;
SharedChannel     g_channel;
std::thread       g_sampler;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_started{false};
FrameSync        g_frameSync;
mcov::agent::MinecraftCombat g_combat;
void JNICALL OnCombatTick(JNIEnv* env,jclass,jobject client);

void SetStatus(AgentStatus status) {
    if (auto* block = g_channel.block()) {
        block->status.store(static_cast<std::uint32_t>(status), std::memory_order_release);
    }
}

std::wstring AgentLogPath() {
    // Log next to the DLL rather than the CWD - javaw.exe's working directory is
    // the game directory, which is not always writable and is easy to lose.
    wchar_t path[MAX_PATH] = {};
    HMODULE self = nullptr;
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&AgentLogPath), &self);
    if (self && ::GetModuleFileNameW(self, path, MAX_PATH)) {
        std::wstring result(path);
        const std::size_t slash = result.find_last_of(L"\\/");
        if (slash != std::wstring::npos) result.resize(slash + 1);
        return result + L"mcov_agent.log";
    }
    return L"mcov_agent.log";
}

// ---------------------------------------------------------------------------
// JVMTI class resolver
//
// This exists because env->FindClass() on a thread we attached ourselves
// searches only the system class loader. Minecraft's classes are loaded by the
// launcher's loader (or Fabric's Knot / NeoForge's module layer), so FindClass
// raises NoClassDefFoundError for every one of them.
//
// GetLoadedClasses ignores loader scoping entirely: it returns every class the
// VM currently has loaded, whoever loaded it. We match on the JVM type
// descriptor ("Lnet/minecraft/client/Minecraft;").
//
// It is a linear scan over ~30-60k classes with an allocation per signature, so
// it is only acceptable because Runtime::ResolveClass caches the result as a
// global ref and never asks twice.
// ---------------------------------------------------------------------------
jclass ResolveClassViaJvmti(JNIEnv* env, const char* slashedName) {
    if (!g_jvmti || !env || !slashedName) return nullptr;

    jint    count   = 0;
    jclass* classes = nullptr;
    if (g_jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE || !classes) {
        return nullptr;
    }

    const std::string wanted = std::string("L") + slashedName + ";";
    jclass            found  = nullptr;

    for (jint i = 0; i < count; ++i) {
        if (!found) {
            char* signature = nullptr;
            if (g_jvmti->GetClassSignature(classes[i], &signature, nullptr) == JVMTI_ERROR_NONE && signature) {
                if (wanted == signature) {
                    // Take an independent ref: the array's refs are all deleted below.
                    found = static_cast<jclass>(env->NewLocalRef(classes[i]));
                }
                g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
            }
        }
        // GetLoadedClasses hands back JNI *local* references; leaving 40k of them
        // alive would overflow the local reference table immediately.
        env->DeleteLocalRef(classes[i]);
    }

    g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(classes));
    return found;
}

// ---------------------------------------------------------------------------
// Resolved handles for one mapping set
// ---------------------------------------------------------------------------
struct Bindings {
    GeometryBindings geometry;
    const mappings::MappingSet* set = nullptr;
    const mappings::RuntimeProfile* profile = nullptr;
    bool                        ready = false;

    jvm::demo::MinecraftHandles client;

    jclass clsLevel   = nullptr;
    jclass clsEntity  = nullptr;
    jclass clsLiving  = nullptr;
    jclass clsPlayer  = nullptr;
    jclass clsEnemy   = nullptr;
    jclass clsVec3    = nullptr;
    jclass clsCamera  = nullptr;
    jclass clsRenderer= nullptr;
    jclass clsText    = nullptr;
    jclass clsOptions = nullptr;
    jclass clsOptVal  = nullptr;
    jclass clsServer  = nullptr;

    jclass clsItemStack = nullptr;
    jclass clsEquipSlot = nullptr;
    jclass clsEffectInst= nullptr;

    // Bootstrap classes - always reachable through plain FindClass.
    jclass clsIterable = nullptr;
    jclass clsIterator = nullptr;
    jclass clsObject   = nullptr;
    jclass clsNumber   = nullptr;
    jclass clsBoolean  = nullptr;
    jclass clsRuntime  = nullptr;

    jfieldID fPlayer = nullptr, fLevel = nullptr, fRenderer = nullptr, fOptions = nullptr;
    jfieldID fFps = nullptr;                      // static, optional
    jfieldID fVecX = nullptr, fVecY = nullptr, fVecZ = nullptr;
    jfieldID fServerAddress = nullptr;

    jmethodID mGetCamera = nullptr, mCamPos = nullptr, mRealFov = nullptr;
    jfieldID  fBobView = nullptr;
    jfieldID  fCamPos = nullptr;   // fallback if an accessor cannot be used
    jmethodID mCamYaw = nullptr, mCamPitch = nullptr;
    jmethodID mGetX = nullptr, mGetY = nullptr, mGetZ = nullptr;
    jmethodID mGetId = nullptr, mGetName = nullptr, mEntityYaw = nullptr;
    jmethodID mBbWidth = nullptr, mBbHeight = nullptr;
    jmethodID mHealth = nullptr, mMaxHealth = nullptr, mAbsorption = nullptr, mArmor = nullptr;
    jmethodID mEntities = nullptr, mDimension = nullptr;
    jmethodID mTextToString = nullptr;
    jmethodID mFov = nullptr, mOptGet = nullptr;
    jmethodID mCurrentServer = nullptr;
    jmethodID mIterator = nullptr, mHasNext = nullptr, mNext = nullptr;
    jmethodID mObjToString = nullptr, mNumberIntValue = nullptr;
    jmethodID mBooleanValue = nullptr;

    // Equipment. The slot constants are global refs to the enum singletons.
    jmethodID mGetEquipped = nullptr;
    jobject   slotConstants[kSlotCount] = {};
    jmethodID mItemIsEmpty = nullptr, mItemDamage = nullptr, mItemMaxDamage = nullptr;
    jmethodID mItemIsDamageable = nullptr, mItemCount = nullptr, mItemName = nullptr;
    jmethodID mConnection = nullptr, mEntityUuid = nullptr, mPlayerInfo = nullptr;
    jmethodID mLatency = nullptr, mEntityById = nullptr;
    jmethodID mUuidHigh = nullptr, mUuidLow = nullptr;

    // Status effects
    jmethodID mGetStatusEffects = nullptr;
    jmethodID mEffAmplifier = nullptr, mEffDuration = nullptr;
    jmethodID mEffTransKey = nullptr, mEffAmbient = nullptr;

    // Crosshair target
    jfieldID fTargetedEntity = nullptr;

    // java.lang.Runtime - bootstrap, so heap telemetry needs no mappings at all.
    jmethodID mGetRuntime = nullptr, mTotalMemory = nullptr, mFreeMemory = nullptr, mMaxMemory = nullptr;

    // Block scanning
    jclass    clsBlockState = nullptr, clsBlock = nullptr, clsPosMut = nullptr;
    jmethodID mGetBlockState = nullptr, mIsAir = nullptr, mGetBlock = nullptr;
    jmethodID mBlockTransKey = nullptr, mMutableSet = nullptr, mMutableCtor = nullptr;

    // Auto totem - the only path that writes to the client.
    jclass    clsInteractionMgr = nullptr, clsSlotAction = nullptr;
    jclass    clsPlayerInv = nullptr, clsItem = nullptr;
    jfieldID  fInteractionMgr = nullptr, fCurrentScreen = nullptr, fGui = nullptr;
    jfieldID  fScreenHandler = nullptr, fSyncId = nullptr, fInventory = nullptr;
    jmethodID mInvGetStack = nullptr, mInvSize = nullptr;
    jmethodID mStackGetItem = nullptr, mItemTransKey = nullptr, mClickSlot = nullptr;
    jobject   swapAction = nullptr;   // global ref to SlotActionType.SWAP
    jmethodID mStateValue=nullptr;
    jobject axisProperty=nullptr;
};

// Block identity -> kind, resolved once per distinct Block instance.
//
// Going through the registry (Registries.BLOCK -> Registry.get(Identifier))
// would need four more mappings; caching by object identity needs none. A scan
// volume only contains a few dozen distinct blocks, so the linear IsSameObject
// search stays trivial - and the expensive getTranslationKey call happens once
// per block type rather than once per position.
struct BlockKindCache {
    struct Entry {
        jobject     block;   // global ref
        std::int8_t kind;    // BlockKind, or -1 for "not interesting"
    };
    std::vector<Entry> entries;

    std::int8_t Lookup(JNIEnv* env, const Bindings& b, jobject block) {
        for (const Entry& e : entries) {
            if (env->IsSameObject(block, e.block)) return e.kind;
        }

        std::int8_t kind = -1;
        if (b.mBlockTransKey) {
            auto key = static_cast<jstring>(env->CallObjectMethod(block, b.mBlockTransKey));
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (key) {
                const std::string text = jvm::JStringToUtf8(env, key);
                env->DeleteLocalRef(key);
                for (std::uint8_t i = 0; i < kBlockKindCount; ++i) {
                    if(i==kBlockRotatedDeepslate && text!="block.minecraft.deepslate") continue;
                    if (text.find(BlockKindKey(i)) != std::string::npos) {
                        kind = static_cast<std::int8_t>(i);
                        break;
                    }
                }
            }
        }
        entries.push_back({env->NewGlobalRef(block), kind});
        return kind;
    }

    void Release(JNIEnv* env) {
        for (Entry& e : entries) {
            if (e.block) env->DeleteGlobalRef(e.block);
        }
        entries.clear();
    }
};

// Resolve a class through the cache, tolerating a miss.
jclass SoftClass(JNIEnv* env, const char* key, const char* name) {
    jvm::Error ignored;
    return jvm::Runtime::Instance().ResolveClass(env, key, {name}, &ignored);
}

bool BindBootstrap(JNIEnv* env, Bindings& b) {
    b.clsIterable = SoftClass(env, "java.lang.Iterable", "java/lang/Iterable");
    b.clsIterator = SoftClass(env, "java.util.Iterator", "java/util/Iterator");
    b.clsObject   = SoftClass(env, "java.lang.Object",   "java/lang/Object");
    b.clsNumber   = SoftClass(env, "java.lang.Number",   "java/lang/Number");
    b.clsBoolean  = SoftClass(env, "java.lang.Boolean",  "java/lang/Boolean");
    b.clsRuntime  = SoftClass(env, "java.lang.Runtime",  "java/lang/Runtime");
    if (!b.clsIterable || !b.clsIterator || !b.clsObject || !b.clsNumber) return false;

    b.mIterator       = jvm::TryMethod(env, b.clsIterable, "iterator", "()Ljava/util/Iterator;", false);
    b.mHasNext        = jvm::TryMethod(env, b.clsIterator, "hasNext",  "()Z", false);
    b.mNext           = jvm::TryMethod(env, b.clsIterator, "next",     "()Ljava/lang/Object;", false);
    b.mObjToString    = jvm::TryMethod(env, b.clsObject,   "toString", "()Ljava/lang/String;", false);
    b.mNumberIntValue = jvm::TryMethod(env, b.clsNumber,   "intValue", "()I", false);
    b.mBooleanValue = jvm::TryMethod(env, b.clsBoolean, "booleanValue", "()Z", false);

    if (b.clsRuntime) {
        b.mGetRuntime  = jvm::TryMethod(env, b.clsRuntime, "getRuntime", "()Ljava/lang/Runtime;", true);
        b.mTotalMemory = jvm::TryMethod(env, b.clsRuntime, "totalMemory", "()J", false);
        b.mFreeMemory  = jvm::TryMethod(env, b.clsRuntime, "freeMemory",  "()J", false);
        b.mMaxMemory   = jvm::TryMethod(env, b.clsRuntime, "maxMemory",   "()J", false);
    }

    return b.mIterator && b.mHasNext && b.mNext;
}

// Attempts to bind everything for one mapping set. Returns true only if the
// essentials (version, client, player/level, camera and entities) are present -
// individual optional members are allowed to stay null.
bool BindMappingSet(JNIEnv* env, const mappings::RuntimeProfile& profile, Bindings& out) {
    const auto& set = *profile.set;
    Bindings b;
    b.set = &set;
    b.profile = &profile;

    jvm::Error error;
    const std::string prefix = std::string(set.id) + ".";

    b.client.minecraftClass = jvm::Runtime::Instance().ResolveClass(
        env, (prefix + "client").c_str(), {set.classes.client}, &error);
    if (!b.client.minecraftClass) {
        MCOV_LOG_TRACE("[%s] client class miss: %s", set.id, error.what());
        return false;
    }

    // The class name alone cannot distinguish Mojmap releases. Read the
    // game's own version before trying version-specific member descriptors.
    if (!jvm::Runtime::Instance().SetGameClassLoader(env, b.client.minecraftClass)) return false;
    {
        jvm::LocalFrame frame(env, 8);
        if (!frame.ok()) return false;
        jclass constants = SoftClass(env, (prefix + "version").c_str(), profile.versionClass);
        jclass worldVersion = SoftClass(env, (prefix + "worldversion").c_str(), profile.worldVersionClass);
        jmethodID current = jvm::TryMethod(env, constants, profile.currentVersion.name,
                                           profile.currentVersion.signature, true);
        jmethodID id = jvm::TryMethod(env, worldVersion, profile.versionId.name,
                                      profile.versionId.signature, false);
        if (!current || !id) return false;
        jobject version = env->CallStaticObjectMethod(constants, current);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if (!version) return false;
        auto text = static_cast<jstring>(env->CallObjectMethod(version, id));
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if (!text || jvm::JStringToUtf8(env, text) != profile.version) return false;
    }
    if (!BindBootstrap(env, b)) return false;

    // getInstance is itself a mapped name - it is method_1551 under intermediary,
    // so it cannot be hardcoded.
    b.client.getInstance = jvm::TryMethod(env, b.client.minecraftClass,
                                          set.methodGetInstance.name,
                                          set.methodGetInstance.signature,
                                          /*isStatic=*/true);
    if (!b.client.getInstance) {
        MCOV_LOG_WARN("[%s] found %s but no static %s%s",
                      set.id, set.classes.client,
                      set.methodGetInstance.name, set.methodGetInstance.signature);
        return false;
    }
    b.client.resolved  = true;
    b.client.mappingId = set.classes.client;

    // Required instance fields on the client singleton.
    b.fPlayer = jvm::TryField(env, b.client.minecraftClass, set.fieldPlayer.name, set.fieldPlayer.signature, false);
    b.fLevel  = jvm::TryField(env, b.client.minecraftClass, set.fieldLevel.name,  set.fieldLevel.signature,  false);
    if (!b.fPlayer || !b.fLevel) {
        MCOV_LOG_WARN("[%s] client class matched but player/level fields did not - "
                      "mapping set is probably for a different MC version", set.id);
        return false;
    }

    // Optional-but-nice members from here on.
    b.fRenderer = jvm::TryField(env, b.client.minecraftClass, set.fieldGameRenderer.name, set.fieldGameRenderer.signature, false);
    b.fOptions  = jvm::TryField(env, b.client.minecraftClass, set.fieldOptions.name, set.fieldOptions.signature, false);
    b.fFps      = jvm::TryField(env, b.client.minecraftClass, set.fieldFps.name, set.fieldFps.signature, /*isStatic=*/true);

    b.clsLevel    = SoftClass(env, (prefix + "level").c_str(),    set.classes.clientLevel);
    b.clsEntity   = SoftClass(env, (prefix + "entity").c_str(),   set.classes.entity);
    b.clsLiving   = SoftClass(env, (prefix + "living").c_str(),   set.classes.livingEntity);
    b.clsPlayer   = SoftClass(env, (prefix + "player").c_str(),   set.classes.player);
    b.clsEnemy    = SoftClass(env, (prefix + "enemy").c_str(),    set.classes.enemyMarker);
    b.clsVec3     = SoftClass(env, (prefix + "vec3").c_str(),     set.classes.vec3);
    b.clsCamera   = SoftClass(env, (prefix + "camera").c_str(),   set.classes.camera);
    b.clsRenderer = SoftClass(env, (prefix + "renderer").c_str(), set.classes.gameRenderer);
    b.clsText     = SoftClass(env, (prefix + "text").c_str(),     set.classes.component);
    b.clsOptions  = SoftClass(env, (prefix + "options").c_str(),  set.classes.options);
    b.clsOptVal   = SoftClass(env, (prefix + "optval").c_str(),   set.classes.optionValue);
    b.clsServer   = SoftClass(env, (prefix + "server").c_str(),   set.classes.serverData);

    if (b.clsEntity) {
        b.mGetX      = jvm::TryMethod(env, b.clsEntity, set.methodGetX.name, set.methodGetX.signature, false);
        b.mGetY      = jvm::TryMethod(env, b.clsEntity, set.methodGetY.name, set.methodGetY.signature, false);
        b.mGetZ      = jvm::TryMethod(env, b.clsEntity, set.methodGetZ.name, set.methodGetZ.signature, false);
        b.mGetId     = jvm::TryMethod(env, b.clsEntity, set.methodGetId.name, set.methodGetId.signature, false);
        b.mGetName   = jvm::TryMethod(env, b.clsEntity, set.methodGetName.name, set.methodGetName.signature, false);
        b.mBbWidth   = jvm::TryMethod(env, b.clsEntity, set.methodGetBbWidth.name, set.methodGetBbWidth.signature, false);
        b.mBbHeight  = jvm::TryMethod(env, b.clsEntity, set.methodGetBbHeight.name, set.methodGetBbHeight.signature, false);
        b.mEntityYaw = jvm::TryMethod(env, b.clsEntity, set.methodEntityYaw.name, set.methodEntityYaw.signature, false);
    }
    if (b.clsLiving) {
        b.mHealth     = jvm::TryMethod(env, b.clsLiving, set.methodGetHealth.name, set.methodGetHealth.signature, false);
        b.mMaxHealth  = jvm::TryMethod(env, b.clsLiving, set.methodGetMaxHealth.name, set.methodGetMaxHealth.signature, false);
        b.mAbsorption = jvm::TryMethod(env, b.clsLiving, set.methodGetAbsorption.name, set.methodGetAbsorption.signature, false);
        b.mArmor      = jvm::TryMethod(env, b.clsLiving, set.methodGetArmor.name, set.methodGetArmor.signature, false);
    }
    if (b.clsLevel) {
        b.mEntities  = jvm::TryMethod(env, b.clsLevel, set.methodEntitiesForRendering.name, set.methodEntitiesForRendering.signature, false);
        b.mDimension = jvm::TryMethod(env, b.clsLevel, set.methodDimension.name, set.methodDimension.signature, false);
    }
    if (b.clsRenderer) {
        b.mGetCamera = jvm::TryMethod(env, b.clsRenderer, set.methodGetCamera.name, set.methodGetCamera.signature, false);
    }
    b.mRealFov = jvm::TryMethod(env,
        profile.fovSource == mappings::FovSource::CameraFloat ? b.clsCamera : b.clsRenderer,
        set.methodRealFov.name, set.methodRealFov.signature, false);
    if (b.clsOptions) {
        b.fBobView = jvm::TryField(env, b.clsOptions, set.fieldBobView.name, set.fieldBobView.signature, false);
    }
    if (b.clsCamera) {
        b.mCamPos   = jvm::TryMethod(env, b.clsCamera, set.methodCameraPosition.name, set.methodCameraPosition.signature, false);
        b.fCamPos   = jvm::TryField (env, b.clsCamera, set.fieldCameraPosition.name, set.fieldCameraPosition.signature, false);
        b.mCamYaw   = jvm::TryMethod(env, b.clsCamera, set.methodGetYaw.name, set.methodGetYaw.signature, false);
        b.mCamPitch = jvm::TryMethod(env, b.clsCamera, set.methodGetPitch.name, set.methodGetPitch.signature, false);
    }
    if (b.clsVec3) {
        // Vec3/Vec3d expose the components as public final doubles. The names are
        // mapped like anything else - they are field_1352/1351/1350 under
        // intermediary - so they come from the table, not from literals.
        b.fVecX = jvm::TryField(env, b.clsVec3, set.fieldVecX.name, set.fieldVecX.signature, false);
        b.fVecY = jvm::TryField(env, b.clsVec3, set.fieldVecY.name, set.fieldVecY.signature, false);
        b.fVecZ = jvm::TryField(env, b.clsVec3, set.fieldVecZ.name, set.fieldVecZ.signature, false);
    }
    if (!b.fRenderer || !b.mGetCamera || (!b.mCamPos && !b.fCamPos) ||
        !b.mCamYaw || !b.mCamPitch || !b.fVecX || !b.fVecY || !b.fVecZ ||
        !b.mRealFov || !b.mGetX || !b.mGetY || !b.mGetZ || !b.mEntities) {
        MCOV_LOG_WARN("[%s] camera/entity bindings incomplete; refusing this profile", set.id);
        return false;
    }
    if (b.clsText) {
        b.mTextToString = jvm::TryMethod(env, b.clsText, set.methodComponentToString.name, set.methodComponentToString.signature, false);
    }
    if (b.clsOptions) b.mFov    = jvm::TryMethod(env, b.clsOptions, set.methodFov.name, set.methodFov.signature, false);
    if (b.clsOptVal)  b.mOptGet = jvm::TryMethod(env, b.clsOptVal,  set.methodOptionGet.name, set.methodOptionGet.signature, false);
    b.mCurrentServer = jvm::TryMethod(env, b.client.minecraftClass, set.methodCurrentServer.name, set.methodCurrentServer.signature, false);
    if (b.clsServer)  b.fServerAddress = jvm::TryField(env, b.clsServer, set.fieldServerAddress.name, set.fieldServerAddress.signature, false);

    // --- equipment / effects / target ------------------------------------
    b.clsItemStack  = SoftClass(env, (prefix + "itemstack").c_str(), set.classes.itemStack);
    b.clsEquipSlot  = SoftClass(env, (prefix + "equipslot").c_str(), set.classes.equipSlot);
    b.clsEffectInst = SoftClass(env, (prefix + "effectinst").c_str(), set.classes.effectInst);

    if (b.clsLiving) {
        b.mGetEquipped      = jvm::TryMethod(env, b.clsLiving, set.methodGetEquipped.name, set.methodGetEquipped.signature, false);
        b.mGetStatusEffects = jvm::TryMethod(env, b.clsLiving, set.methodGetStatusEffects.name, set.methodGetStatusEffects.signature, false);
    }
    if (b.clsItemStack) {
        b.mItemIsEmpty      = jvm::TryMethod(env, b.clsItemStack, set.methodItemIsEmpty.name, set.methodItemIsEmpty.signature, false);
        b.mItemDamage       = jvm::TryMethod(env, b.clsItemStack, set.methodItemGetDamage.name, set.methodItemGetDamage.signature, false);
        b.mItemMaxDamage    = jvm::TryMethod(env, b.clsItemStack, set.methodItemGetMaxDamage.name, set.methodItemGetMaxDamage.signature, false);
        b.mItemIsDamageable = jvm::TryMethod(env, b.clsItemStack, set.methodItemIsDamageable.name, set.methodItemIsDamageable.signature, false);
        b.mItemCount        = jvm::TryMethod(env, b.clsItemStack, set.methodItemGetCount.name, set.methodItemGetCount.signature, false);
        b.mItemName         = jvm::TryMethod(env, b.clsItemStack, set.methodItemGetName.name, set.methodItemGetName.signature, false);
    }
    if (b.clsEffectInst) {
        b.mEffAmplifier = jvm::TryMethod(env, b.clsEffectInst, set.methodEffectAmplifier.name, set.methodEffectAmplifier.signature, false);
        b.mEffDuration  = jvm::TryMethod(env, b.clsEffectInst, set.methodEffectDuration.name, set.methodEffectDuration.signature, false);
        b.mEffTransKey  = jvm::TryMethod(env, b.clsEffectInst, set.methodEffectTranslationKey.name, set.methodEffectTranslationKey.signature, false);
        b.mEffAmbient   = jvm::TryMethod(env, b.clsEffectInst, set.methodEffectIsAmbient.name, set.methodEffectIsAmbient.signature, false);
    }
    b.fTargetedEntity = jvm::TryField(env, b.client.minecraftClass, set.fieldTargetedEntity.name, set.fieldTargetedEntity.signature, false);
    auto connection=SoftClass(env,(prefix+"connection").c_str(),set.classes.connection);
    auto playerInfo=SoftClass(env,(prefix+"playerinfo").c_str(),set.classes.playerInfo);
    b.mConnection=jvm::TryMethod(env,b.client.minecraftClass,set.methodConnection.name,set.methodConnection.signature,false);
    b.mEntityUuid=jvm::TryMethod(env,b.clsEntity,set.methodEntityUuid.name,set.methodEntityUuid.signature,false);
    auto uuidClass=env->FindClass("java/util/UUID");
    if(env->ExceptionCheck()) env->ExceptionClear();
    if(uuidClass) {
        b.mUuidHigh=jvm::TryMethod(env,uuidClass,"getMostSignificantBits","()J",false);
        b.mUuidLow=jvm::TryMethod(env,uuidClass,"getLeastSignificantBits","()J",false);
        env->DeleteLocalRef(uuidClass);
    }
    b.mPlayerInfo=jvm::TryMethod(env,connection,set.methodPlayerInfo.name,set.methodPlayerInfo.signature,false);
    b.mLatency=jvm::TryMethod(env,playerInfo,set.methodLatency.name,set.methodLatency.signature,false);
    b.mEntityById=jvm::TryMethod(env,b.clsLevel,set.methodEntityById.name,set.methodEntityById.signature,false);

    // --- block scanning ---------------------------------------------------
    b.clsBlockState = SoftClass(env, (prefix + "blockstate").c_str(), set.classes.blockState);
    b.clsBlock      = SoftClass(env, (prefix + "block").c_str(),      set.classes.block);
    b.clsPosMut     = SoftClass(env, (prefix + "posmut").c_str(),     set.classes.blockPosMut);

    if (b.clsLevel) {
        b.mGetBlockState = jvm::TryMethod(env, b.clsLevel, set.methodGetBlockState.name, set.methodGetBlockState.signature, false);
    }
    if (b.clsBlockState) {
        b.mIsAir    = jvm::TryMethod(env, b.clsBlockState, set.methodStateIsAir.name, set.methodStateIsAir.signature, false);
        b.mGetBlock = jvm::TryMethod(env, b.clsBlockState, set.methodStateGetBlock.name, set.methodStateGetBlock.signature, false);
    }
    if (b.clsBlock) {
        b.mBlockTransKey = jvm::TryMethod(env, b.clsBlock, set.methodBlockTransKey.name, set.methodBlockTransKey.signature, false);
    }
    if (b.clsPosMut) {
        b.mMutableSet  = jvm::TryMethod(env, b.clsPosMut, set.methodMutableSet.name, set.methodMutableSet.signature, false);
        b.mMutableCtor = jvm::TryMethod(env, b.clsPosMut, "<init>", "()V", false);
    }

    // --- auto totem --------------------------------------------------------
    b.clsInteractionMgr = SoftClass(env, (prefix + "interact").c_str(), set.classes.interactionMgr);
    b.clsSlotAction     = SoftClass(env, (prefix + "slotaction").c_str(), set.classes.slotAction);
    b.clsPlayerInv      = SoftClass(env, (prefix + "playerinv").c_str(), set.classes.playerInv);
    b.clsItem           = SoftClass(env, (prefix + "item").c_str(), set.classes.item);

    b.fInteractionMgr = jvm::TryField(env, b.client.minecraftClass, set.fieldInteractionMgr.name, set.fieldInteractionMgr.signature, false);
    jclass screenOwner = b.client.minecraftClass;
    if (*profile.guiClass) {
        screenOwner = SoftClass(env, (prefix + "gui").c_str(), profile.guiClass);
        b.fGui = jvm::TryField(env, b.client.minecraftClass, profile.clientGui.name,
                               profile.clientGui.signature, false);
    }
    b.fCurrentScreen = jvm::TryField(env, screenOwner, set.fieldCurrentScreen.name,
                                    set.fieldCurrentScreen.signature, false);

    if (b.clsPlayer) {
        b.fScreenHandler = jvm::TryField(env, b.clsPlayer, set.fieldScreenHandler.name, set.fieldScreenHandler.signature, false);
        b.fInventory     = jvm::TryField(env, b.clsPlayer, set.fieldInventory.name, set.fieldInventory.signature, false);
    }
    // Bind on the declared base menu class. A modded handler may hide fields;
    // caching an ID obtained from its concrete subclass would be unsafe when
    // another handler becomes active. The descriptor already names the owner.
    const std::string menuDescriptor = set.fieldScreenHandler.signature;
    if (menuDescriptor.size() > 2 && menuDescriptor.front() == 'L' && menuDescriptor.back() == ';') {
        const std::string menuName = menuDescriptor.substr(1, menuDescriptor.size() - 2);
        jclass menuClass = SoftClass(env, (prefix + "menu").c_str(), menuName.c_str());
        b.fSyncId = jvm::TryField(env, menuClass, set.fieldSyncId.name, set.fieldSyncId.signature, false);
    }
    if (b.clsPlayerInv) {
        b.mInvGetStack = jvm::TryMethod(env, b.clsPlayerInv, set.methodInvGetStack.name, set.methodInvGetStack.signature, false);
        b.mInvSize     = jvm::TryMethod(env, b.clsPlayerInv, set.methodInvSize.name, set.methodInvSize.signature, false);
    }
    if (b.clsItemStack) {
        b.mStackGetItem = jvm::TryMethod(env, b.clsItemStack, set.methodStackGetItem.name, set.methodStackGetItem.signature, false);
    }
    if (b.clsItem) {
        b.mItemTransKey = jvm::TryMethod(env, b.clsItem, set.methodItemTransKey.name, set.methodItemTransKey.signature, false);
    }
    if (b.clsInteractionMgr) {
        b.mClickSlot = jvm::TryMethod(env, b.clsInteractionMgr, set.methodClickSlot.name, set.methodClickSlot.signature, false);
    }
    if (b.clsSlotAction) {
        if (jfieldID swap = jvm::TryField(env, b.clsSlotAction, set.fieldSwapAction.name,
                                          set.fieldSwapAction.signature, /*isStatic=*/true)) {
            jobject local = env->GetStaticObjectField(b.clsSlotAction, swap);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (local) { b.swapAction = env->NewGlobalRef(local); env->DeleteLocalRef(local); }
        }
    }
    // Auto totem needs every one of these; report exactly which is missing
    // rather than silently doing nothing at runtime.
    {
        const struct { const char* what; bool ok; } required[] = {
            {"interactionManager field", b.fInteractionMgr != nullptr},
            {"currentScreen field",      b.fCurrentScreen && (!*profile.guiClass || b.fGui)},
            {"screenHandler field",      b.fScreenHandler  != nullptr},
            {"container ID field",       b.fSyncId         != nullptr},
            {"inventory field",          b.fInventory      != nullptr},
            {"Inventory.getStack",       b.mInvGetStack    != nullptr},
            {"Inventory.size",           b.mInvSize        != nullptr},
            {"ItemStack.getItem",        b.mStackGetItem   != nullptr},
            {"Item.getTranslationKey",   b.mItemTransKey   != nullptr},
            {"clickSlot",                b.mClickSlot      != nullptr},
            {"SlotActionType.SWAP",      b.swapAction      != nullptr},
        };
        std::string missing;
        for (const auto& r : required) {
            if (!r.ok) { if (!missing.empty()) missing += ", "; missing += r.what; }
        }
        if (missing.empty()) {
            MCOV_LOG_INFO("[%s] auto totem: all members bound", set.id);
        } else {
            MCOV_LOG_WARN("[%s] auto totem disabled, unbound: %s", set.id, missing.c_str());
        }
    }

    // Pin the EquipmentSlot enum singletons as global refs. They live for the
    // life of the VM, so this is a one-off cost rather than per-frame work.
    if (b.clsEquipSlot) {
        const mappings::Member slots[kSlotCount] = {
            set.slotHead, set.slotChest, set.slotLegs,
            set.slotFeet, set.slotMainHand, set.slotOffHand,
        };
        for (int i = 0; i < static_cast<int>(kSlotCount); ++i) {
            jfieldID fid = jvm::TryField(env, b.clsEquipSlot, slots[i].name, slots[i].signature, /*isStatic=*/true);
            if (!fid) continue;
            jobject local = env->GetStaticObjectField(b.clsEquipSlot, fid);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (local) {
                b.slotConstants[i] = env->NewGlobalRef(local);
                env->DeleteLocalRef(local);
            }
        }
    }

    b.geometry = BindGeometry(env, profile, b.client.minecraftClass, b.clsEntity, b.clsLiving, b.clsPlayer, b.clsCamera);
    b.ready = true;
    out     = b;
    MCOV_LOG_INFO("bound mapping set '%s' (%s)", set.id, set.classes.client);
    return true;
}

// ---------------------------------------------------------------------------
// Sampling helpers
// ---------------------------------------------------------------------------
float CallFloat(JNIEnv* env, jobject obj, jmethodID method, float fallback = 0.0f) {
    if (!obj || !method) return fallback;
    const jfloat value = env->CallFloatMethod(obj, method);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return fallback; }
    return static_cast<float>(value);
}

double CallDouble(JNIEnv* env, jobject obj, jmethodID method, double fallback = 0.0) {
    if (!obj || !method) return fallback;
    const jdouble value = env->CallDoubleMethod(obj, method);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return fallback; }
    return static_cast<double>(value);
}

int CallInt(JNIEnv* env, jobject obj, jmethodID method, int fallback = 0) {
    if (!obj || !method) return fallback;
    const jint value = env->CallIntMethod(obj, method);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return fallback; }
    return static_cast<int>(value);
}

#include "geometry_sampling.hpp"

// Entity display name -> UTF-8. Component/Text -> getString() -> java.lang.String.
std::string ReadEntityName(JNIEnv* env, const Bindings& b, jobject entity) {
    if (!entity || !b.mGetName) return {};

    jvm::LocalFrame frame(env, 8);

    jobject component = env->CallObjectMethod(entity, b.mGetName);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    if (!component) return {};

    // Prefer Component/Text.getString(). That name is absent from the Fabric
    // mapping file, so if the "unmapped names survive verbatim" assumption is
    // wrong for this build we fall back to Object.toString(), which always
    // exists - uglier output, but a name rather than a blank.
    jmethodID toText = b.mTextToString ? b.mTextToString : b.mObjToString;
    if (!toText) return {};

    auto text = static_cast<jstring>(env->CallObjectMethod(component, toText));
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    if (!text) return {};

    return jvm::JStringToUtf8(env, text);
}

// Reads a Vec3/Vec3d's three public double fields.
bool ReadVec3(JNIEnv* env, const Bindings& b, jobject vec, double& x, double& y, double& z) {
    if (!vec || !b.fVecX || !b.fVecY || !b.fVecZ) return false;
    x = env->GetDoubleField(vec, b.fVecX);
    y = env->GetDoubleField(vec, b.fVecY);
    z = env->GetDoubleField(vec, b.fVecZ);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    return true;
}

void SampleCamera(JNIEnv* env, const Bindings& b, jobject client, WorldSnapshot& snap) {
    // Defaults matter: if the camera chain is unavailable the overlay still gets
    // a usable (if slightly wrong) projection instead of NaNs.
    snap.fovDeg     = 70.0f;
    snap.nearPlane  = 0.05f;
    snap.farPlane   = 1000.0f;
    snap.partialTick = 1.0f;

    if (!b.fRenderer || !b.mGetCamera) return;

    jvm::LocalFrame frame(env, 12);

    jobject renderer = env->GetObjectField(client, b.fRenderer);
    if (!renderer) return;

    jobject camera = env->CallObjectMethod(renderer, b.mGetCamera);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!camera) return;

    // Camera position: accessor if this mapping set has one, otherwise the field.
    // The selected profile supplies the release-specific accessor name.
    jobject position = nullptr;
    if (b.mCamPos) {
        position = env->CallObjectMethod(camera, b.mCamPos);
        if (env->ExceptionCheck()) { env->ExceptionClear(); position = nullptr; }
    }
    if (!position && b.fCamPos) {
        position = env->GetObjectField(camera, b.fCamPos);
        if (env->ExceptionCheck()) { env->ExceptionClear(); position = nullptr; }
    }
    if (position) ReadVec3(env, b, position, snap.camX, snap.camY, snap.camZ);

    // The camera's rotation, not the player's: they differ in third person and
    // whenever the player is riding an entity.
    snap.yawDeg   = CallFloat(env, camera, b.mCamYaw,   snap.yawDeg);
    snap.pitchDeg = CallFloat(env, camera, b.mCamPitch, snap.pitchDeg);
    SampleCameraGeometry(env, b, client, camera, snap);

    // Preferred: the renderer's actual FOV for this frame. Options.fov() is only
    // the base setting and excludes the sprint/speed/bow modifiers, which is why
    // ESP boxes drift the moment you start sprinting.
    if (b.mRealFov) {
        double fov = 0.0;
        switch (b.profile->fovSource) {
            case mappings::FovSource::CameraFloat:
                fov = env->CallFloatMethod(camera, b.mRealFov);
                break;
            case mappings::FovSource::RendererDouble:
                fov = env->CallDoubleMethod(renderer, b.mRealFov, camera, snap.partialTick, JNI_TRUE);
                break;
            case mappings::FovSource::RendererFloat:
                fov = env->CallFloatMethod(renderer, b.mRealFov, camera, snap.partialTick, JNI_TRUE);
                break;
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (fov > 1.0f && fov < 179.0f) {
            snap.fovDeg = static_cast<float>(fov);

            return;   // real FOV obtained; skip the options fallback below
        }
    }

    // Fallback chain: Minecraft.options -> Options.fov() -> OptionInstance.get()
    if (b.fOptions && b.mFov && b.mOptGet && b.mNumberIntValue) {
        jobject options = env->GetObjectField(client, b.fOptions);
        if (options) {
            jobject option = env->CallObjectMethod(options, b.mFov);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (option) {
                jobject boxed = env->CallObjectMethod(option, b.mOptGet);
                if (env->ExceptionCheck()) env->ExceptionClear();
                else if (boxed) {
                    const int fov = CallInt(env, boxed, b.mNumberIntValue, 70);
                    if (fov > 1 && fov < 180) snap.fovDeg = static_cast<float>(fov);
                }
            }
        }
    }
}

void SampleLocalPlayer(JNIEnv* env, const Bindings& b, jobject player, WorldSnapshot& snap) {
    snap.posX = static_cast<float>(CallDouble(env, player, b.mGetX));
    snap.posY = static_cast<float>(CallDouble(env, player, b.mGetY));
    snap.posZ = static_cast<float>(CallDouble(env, player, b.mGetZ));

    snap.health     = CallFloat(env, player, b.mHealth,     -1.0f);
    snap.maxHealth  = CallFloat(env, player, b.mMaxHealth,  20.0f);
    snap.absorption = CallFloat(env, player, b.mAbsorption,  0.0f);
    snap.armorValue = CallInt  (env, player, b.mArmor,       0);

    const std::string name = ReadEntityName(env, b, player);
    CopyField(snap.username, name.c_str());

    // Food/saturation live on Player.getFoodData() in MojMap and on
    // PlayerEntity.getHungerManager() in Yarn - two more class lookups for two
    // numbers. Left unbound here; wire it the same way the FOV chain is done
    // above if you need it.
    snap.foodLevel  = -1;
    snap.saturation = -1.0f;

    // Der echte Ping wird im langsamen Entity-Details-Pass aktualisiert.
}
bool EnsureChunkBindings(JNIEnv* env,Bindings& b) {
    if(b.mStateValue&&b.axisProperty) return true;
    const auto& g=*b.profile->geometry;
    if(!b.mStateValue) b.mStateValue=jvm::TryMethod(env,b.clsBlockState,g.stateValue.name,g.stateValue.signature,false);
    if(!b.axisProperty) {
        const std::string key=std::string(b.set->id)+".axis";
        auto cls=SoftClass(env,key.c_str(),g.pillarClass);
        const auto id=jvm::TryField(env,cls,g.axisProperty.name,g.axisProperty.signature,true);
        if(id) {
            auto local=env->GetStaticObjectField(cls,id);
            if(!env->ExceptionCheck()&&local) { b.axisProperty=env->NewGlobalRef(local); env->DeleteLocalRef(local); }
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
    }
    return b.mStateValue&&b.axisProperty;
}
bool IsRotatedDeepslate(JNIEnv* env,const Bindings& b,jobject state) {
    if(!b.mStateValue||!b.axisProperty) return false;
    jvm::LocalFrame frame(env,8);
    auto value=env->CallObjectMethod(state,b.mStateValue,b.axisProperty);
    if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    auto text=value?static_cast<jstring>(env->CallObjectMethod(value,b.mObjToString)):nullptr;
    if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    return RotatedDeepslate("block.minecraft.deepslate",jvm::JStringToUtf8(env,text));
}

struct EntityDetailsState {
    EntityDetailsCache cache;
    EntityIdentityCache identities;
    jobject world{};
    std::uint64_t nextTick{};
    std::uint32_t cursor{};
    bool active{};
    void Clear(JNIEnv* env) {
        cache.Clear(); nextTick=0; cursor=0; active=false;
        identities.Clear(env);
        if (world) { env->DeleteGlobalRef(world); world=nullptr; }
    }
    void ObserveWorld(JNIEnv* env,jobject level) {
        if (world && env->IsSameObject(world,level)) return;
        Clear(env);
        if (level) world=env->NewGlobalRef(level);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
};

void SampleEntities(JNIEnv* env, const Bindings& b, jobject client, jobject localPlayer,
                    WorldSnapshot& snap,EntityDetailsState& details) {
    snap.entityCount = 0;
    if (!b.mEntities) return;

    jvm::LocalFrame outer(env, 16);

    jobject level = env->GetObjectField(client, b.fLevel);
    if (!level) return;
    details.ObserveWorld(env,level);
    const auto detailsNow=::GetTickCount64();

    const auto sample=[&](jobject entity,bool self) {
        jvm::LocalFrame sampleFrame(env,32);
        if (!entity || !sampleFrame.ok()) return;
        EntitySnapshot record{};
        if (self) record.flags |= kEntityIsSelf;
        record.x = CallDouble(env, entity, b.mGetX);
        record.y = CallDouble(env, entity, b.mGetY);
        record.z = CallDouble(env, entity, b.mGetZ);

        record.width    = CallFloat(env, entity, b.mBbWidth,  0.6f);
        record.height   = CallFloat(env, entity, b.mBbHeight, 1.8f);
        record.yaw      = CallFloat(env, entity, b.mEntityYaw, 0.0f);
        record.entityId = static_cast<std::uint32_t>(CallInt(env, entity, b.mGetId, 0));

        if (b.clsLiving && env->IsInstanceOf(entity, b.clsLiving)) {
            record.flags |= kEntityIsLiving;
            record.health    = CallFloat(env, entity, b.mHealth,    0.0f);
            record.maxHealth = CallFloat(env, entity, b.mMaxHealth, 20.0f);
        }
        if (b.clsPlayer && env->IsInstanceOf(entity, b.clsPlayer)) {
            record.flags |= kEntityIsPlayer;
            record.uuid=details.identities.Read(env,entity,record.entityId,b.mEntityUuid,b.mUuidHigh,b.mUuidLow);
        }
        if (b.clsEnemy  && env->IsInstanceOf(entity, b.clsEnemy))  record.flags |= kEntityIsHostile;
        SampleEntityGeometry(env, b, entity, snap.partialTick, record);
        const double dx = record.x - snap.camX, dy = record.y - snap.camY, dz = record.z - snap.camZ;
        record.distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));

        const std::string name = self ? std::string(snap.username) : ReadEntityName(env, b, entity);
        CopyField(record.name, name.c_str());
        if (const auto* cached=details.cache.Find(record.entityId,detailsNow)) record.details=*cached;

        snap.entities[snap.entityCount++] = record;
    };
    // Der eigene Körper erhält einen garantierten Slot; gleiche Interpolation wie die Kamera.
    sample(localPlayer,true);

    jobject iterable = env->CallObjectMethod(level, b.mEntities);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!iterable) return;

    jobject iterator = env->CallObjectMethod(iterable, b.mIterator);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!iterator) return;

    while (snap.entityCount < kMaxEntities) {
        const jboolean more = env->CallBooleanMethod(iterator, b.mHasNext);
        if (env->ExceptionCheck()) {
            // Almost always ConcurrentModificationException: we are walking a
            // live view of the entity list from a thread that is not the client
            // thread, and the client mutated it mid-iteration. Dropping the rest
            // of this capture is safe; the frame epoch check also rejects the
            // entire capture when a game update overlapped this iteration.
            env->ExceptionClear();
            break;
        }
        if (more != JNI_TRUE) break;

        // One frame per entity: getName() alone allocates several refs, and 256
        // iterations would otherwise blow past HotSpot's local ref table.
        jvm::LocalFrame frame(env, 24);

        jobject entity = env->CallObjectMethod(iterator, b.mNext);
        if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
        if (!entity) continue;

        // Bereits separat erfasst; kein zweiter Self-Record aus dem World-Iterator.
        if (localPlayer && env->IsSameObject(entity, localPlayer)) continue;

        sample(entity,false);
    }
}

// Reads a Text/Component off any object that returns one, then stringifies it.
std::string ReadTextFrom(JNIEnv* env, const Bindings& b, jobject owner, jmethodID getter) {
    if (!owner || !getter) return {};

    jvm::LocalFrame frame(env, 8);

    jobject component = env->CallObjectMethod(owner, getter);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    if (!component) return {};

    jmethodID toText = b.mTextToString ? b.mTextToString : b.mObjToString;
    if (!toText) return {};

    auto text = static_cast<jstring>(env->CallObjectMethod(component, toText));
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    if (!text) return {};

    return jvm::JStringToUtf8(env, text);
}

// "effect.minecraft.speed" -> "Speed"; "item.minecraft.oak_log" -> "Oak Log".
std::string PrettyTranslationKey(const std::string& key) {
    const std::size_t dot = key.find_last_of('.');
    std::string       out = (dot == std::string::npos) ? key : key.substr(dot + 1);

    bool capitalise = true;
    for (char& c : out) {
        if (c == '_') { c = ' '; capitalise = true; }
        else if (capitalise) { c = static_cast<char>(::toupper(static_cast<unsigned char>(c))); capitalise = false; }
    }
    return out;
}

void SampleEquipmentInto(JNIEnv* env, const Bindings& b, jobject player, ItemSnapshot (&equipment)[kSlotCount]) {
    for (auto& item:equipment) item={};
    if (!b.mGetEquipped) return;

    for (int slot = 0; slot < static_cast<int>(kSlotCount); ++slot) {
        jobject slotConstant = b.slotConstants[slot];
        if (!slotConstant) continue;

        jvm::LocalFrame frame(env, 16);

        jobject stack = env->CallObjectMethod(player, b.mGetEquipped, slotConstant);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (!stack) continue;

        // An empty slot returns the ItemStack.EMPTY singleton rather than null.
        if (b.mItemIsEmpty) {
            const jboolean empty = env->CallBooleanMethod(stack, b.mItemIsEmpty);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (empty == JNI_TRUE) continue;
        }

        ItemSnapshot& record = equipment[slot];
        record.present = 1;
        record.count   = CallInt(env, stack, b.mItemCount, 1);
        record.damage  = CallInt(env, stack, b.mItemDamage, 0);

        // Only trust maxDamage when the item is actually damageable; blocks and
        // food report a max of 0 and must not render a durability bar.
        bool damageable = true;
        if (b.mItemIsDamageable) {
            const jboolean d = env->CallBooleanMethod(stack, b.mItemIsDamageable);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else damageable = (d == JNI_TRUE);
        }
        record.maxDamage = damageable ? CallInt(env, stack, b.mItemMaxDamage, 0) : 0;

        CopyField(record.name, ReadTextFrom(env, b, stack, b.mItemName).c_str());
        if (b.mStackGetItem && b.mItemTransKey) {
            jobject item=env->CallObjectMethod(stack,b.mStackGetItem);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (item) {
                auto key=static_cast<jstring>(env->CallObjectMethod(item,b.mItemTransKey));
                if (env->ExceptionCheck()) env->ExceptionClear();
                else if (key) record.iconIndex=equipment_icons::Find(jvm::JStringToUtf8(env,key));
            }
        }
    }
}
void SampleEquipment(JNIEnv* env,const Bindings& b,jobject player,WorldSnapshot& snap) {
    SampleEquipmentInto(env,b,player,snap.equipment);
}
int ReadPlayerPing(JNIEnv* env,const Bindings& b,jobject player,jobject connection) {
    if (!player || !connection || !b.mEntityUuid || !b.mPlayerInfo || !b.mLatency) return -1;
    jvm::LocalFrame frame(env,8);
    if (!frame.ok()) return -1;
    jobject uuid=env->CallObjectMethod(player,b.mEntityUuid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return -1; }
    if (!uuid) return -1;
    jobject info=env->CallObjectMethod(connection,b.mPlayerInfo,uuid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return -1; }
    return info ? std::max(-1,CallInt(env,info,b.mLatency,-1)) : -1;
}
EntityDetailsSnapshot SampleDetails(JNIEnv* env,const Bindings& b,jobject entity,jobject connection) {
    EntityDetailsSnapshot data;
    if (b.clsLiving && env->IsInstanceOf(entity,b.clsLiving)) {
        data.armorValue=std::clamp(CallInt(env,entity,b.mArmor,0),0,100);
        SampleEquipmentInto(env,b,entity,data.equipment);
    }
    if (b.clsPlayer && env->IsInstanceOf(entity,b.clsPlayer)) data.ping=ReadPlayerPing(env,b,entity,connection);
    return data;
}
void SampleEntityDetails(JNIEnv* env,const Bindings& b,jobject client,jobject localPlayer,
                         WorldSnapshot& snapshot,EntityDetailsState& state,bool enabled) {
    if (!enabled) {
        if (state.active) { state.cache.Clear(); state.active=false; }
        snapshot.ping=-1;
        return;
    }
    state.active=true;
    const auto now=::GetTickCount64();
    if (now<state.nextTick || !state.world || !b.mEntityById) return;
    state.nextTick=now+50;
    jvm::LocalFrame frame(env,16);
    if (!frame.ok()) return;
    jobject connection=b.mConnection ? env->CallObjectMethod(client,b.mConnection) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); connection=nullptr; }
    snapshot.ping=ReadPlayerPing(env,b,localPlayer,connection);
    int sampled=0;
    const auto count=std::min(snapshot.entityCount,kMaxEntities);
    const auto sample=[&](EntitySnapshot& e) {
        if ((e.flags&kEntityIsSelf) || !(e.flags&kEntityIsLiving) || !std::isfinite(e.distance) || e.distance>512 ||
            state.cache.Find(e.entityId,now,300)) return;
        jvm::LocalFrame entityFrame(env,16);
        if (!entityFrame.ok()) return;
        jobject entity=env->CallObjectMethod(state.world,b.mEntityById,jint(e.entityId));
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
        if (!entity) return;
        e.details=SampleDetails(env,b,entity,connection);
        state.cache.Put(e.entityId,::GetTickCount64(),e.details);
        ++sampled;
    };
    // Ziel zuerst, danach faire Rotation durch die sichtbaren Entity-IDs.
    for (std::uint32_t i=0;snapshot.hasTarget && i<count;++i)
        if (snapshot.entities[i].entityId==snapshot.targetEntityId) { sample(snapshot.entities[i]); break; }
    for (std::uint32_t checked=0;checked<count && sampled<4 && ::GetTickCount64()-now<3;++checked) {
        sample(snapshot.entities[state.cursor%count]); ++state.cursor;
    }
}

void SampleEffects(JNIEnv* env, const Bindings& b, jobject player, WorldSnapshot& snap) {
    snap.effectCount = 0;
    if (!b.mGetStatusEffects || !b.mIterator) return;

    jvm::LocalFrame outer(env, 16);

    // Collection extends Iterable, so the Iterable.iterator() id works here.
    jobject collection = env->CallObjectMethod(player, b.mGetStatusEffects);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!collection) return;

    jobject iterator = env->CallObjectMethod(collection, b.mIterator);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!iterator) return;

    while (snap.effectCount < kMaxEffects) {
        const jboolean more = env->CallBooleanMethod(iterator, b.mHasNext);
        if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
        if (more != JNI_TRUE) break;

        jvm::LocalFrame frame(env, 16);

        jobject instance = env->CallObjectMethod(iterator, b.mNext);
        if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
        if (!instance) continue;

        EffectSnapshot record{};
        record.amplifier     = CallInt(env, instance, b.mEffAmplifier, 0);
        record.durationTicks = CallInt(env, instance, b.mEffDuration, 0);

        if (b.mEffAmbient) {
            const jboolean ambient = env->CallBooleanMethod(instance, b.mEffAmbient);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else record.ambient = (ambient == JNI_TRUE) ? 1u : 0u;
        }

        // getTranslationKey avoids the RegistryEntry -> StatusEffect -> Text
        // chain entirely: one call, one string, no extra classes to resolve.
        if (b.mEffTransKey) {
            auto key = static_cast<jstring>(env->CallObjectMethod(instance, b.mEffTransKey));
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (key) {
                CopyField(record.name, PrettyTranslationKey(jvm::JStringToUtf8(env, key)).c_str());
            }
        }

        snap.effects[snap.effectCount++] = record;
    }
}

void SampleTarget(JNIEnv* env, const Bindings& b, jobject client, WorldSnapshot& snap) {
    snap.hasTarget      = 0;
    snap.targetEntityId = 0;
    if (!b.fTargetedEntity) return;

    jvm::LocalFrame frame(env, 16);

    // Minecraft populates this itself from the crosshair pick, already clamped
    // to the player's interaction range - so non-null IS "in reach", by the
    // game's own definition rather than ours.
    jobject target = env->GetObjectField(client, b.fTargetedEntity);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!target) return;

    snap.hasTarget      = 1;
    snap.targetEntityId = static_cast<std::uint32_t>(CallInt(env, target, b.mGetId, 0));

    const double dx = CallDouble(env, target, b.mGetX) - snap.camX;
    const double dy = CallDouble(env, target, b.mGetY) - snap.camY;
    const double dz = CallDouble(env, target, b.mGetZ) - snap.camZ;
    snap.targetDistance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));

    if (b.clsLiving && env->IsInstanceOf(target, b.clsLiving)) {
        snap.targetHealth    = CallFloat(env, target, b.mHealth, 0.0f);
        snap.targetMaxHealth = CallFloat(env, target, b.mMaxHealth, 0.0f);
    }

    CopyField(snap.targetName, ReadEntityName(env, b, target).c_str());
}

void SampleHeap(JNIEnv* env, const Bindings& b, WorldSnapshot& snap) {
    if (!b.mGetRuntime || !b.mTotalMemory || !b.mFreeMemory) return;

    jvm::LocalFrame frame(env, 4);

    jobject runtime = env->CallStaticObjectMethod(b.clsRuntime, b.mGetRuntime);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!runtime) return;

    const jlong total = env->CallLongMethod(runtime, b.mTotalMemory);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    const jlong freeBytes = env->CallLongMethod(runtime, b.mFreeMemory);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

    snap.heapUsedBytes = static_cast<std::int64_t>(total - freeBytes);

    if (b.mMaxMemory) {
        const jlong max = env->CallLongMethod(runtime, b.mMaxMemory);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else snap.heapMaxBytes = static_cast<std::int64_t>(max);
    }
}

// Sweeps a cube around the player for the requested block kinds.
//
// This is by far the most expensive thing the agent does - a radius of 16 is
// 35937 getBlockState calls - so it is throttled by scanIntervalMs and only
// runs at all when the overlay has asked for at least one kind. Positions are
// walked with a single reusable BlockPos.Mutable, and every local ref is freed
// immediately rather than relying on a frame, because 36k live refs would
// overflow the local reference table long before the frame popped.
// Ein fortsetzbarer Zellen-Cursor begrenzt jeden Durchlauf zeitlich. Die Deadline
// wird alle 32 Zellen geprüft; ein einzelner Spielaufruf kann das Budget überziehen.
struct BlockScanner {
    int           radius  = 0;
    int           baseX   = 0, baseY = 0, baseZ = 0;
    int           nextDy  = 0;      // Legacy-Feld für bestehende Diagnosen.
    int           nextCell = 0;
    std::uint32_t mask    = 0;
    bool          running = false;

    std::vector<BlockSnapshot> accum;
    BlockScanner() { accum.reserve(kMaxBlocks); }

    void Begin(int r, std::uint32_t m, const WorldSnapshot& snap) {
        radius  = r;
        mask    = m;
        baseX   = static_cast<int>(std::floor(snap.posX));
        baseY   = static_cast<int>(std::floor(snap.posY));
        baseZ   = static_cast<int>(std::floor(snap.posZ));
        nextDy  = 0; nextCell=0;
        running = true;
        accum.clear();
    }

    // Returns true once the full cube has been walked.
    bool Step(JNIEnv* env, const Bindings& b, jobject client, BlockKindCache& cache, int sliceBudget) {
        if (!running) return true;
        if (!b.mGetBlockState || !b.mGetBlock || !b.mMutableSet || !b.mMutableCtor) {
            running = false;
            return true;
        }

        jvm::LocalFrame frame(env, 16);
        if(!frame.ok()) { running=false; return true; }

        jobject level = env->GetObjectField(client, b.fLevel);
        if (!level) { running = false; return true; }

        jobject cursor = env->NewObject(b.clsPosMut, b.mMutableCtor);
        if (env->ExceptionCheck()) { env->ExceptionClear(); running = false; return true; }
        if (!cursor) { running = false; return true; }

        const int span = 2 * radius + 1;

        const auto deadline=::GetTickCount64()+std::max(1,sliceBudget);
        const int cells=span*span*span;
        while(nextCell<cells&&accum.size()<kMaxBlocks) {
                    const int cell=nextCell++;
                    const int x=baseX-radius+cell%span;
                    const int z=baseZ-radius+(cell/span)%span;
                    const int y=baseY-radius+cell/(span*span);
                    nextDy=cell/(span*span);
                    if((cell&31)==0&&::GetTickCount64()>=deadline) { --nextCell; break; }
                    if(y < -64 || y >= 320) continue;
                    auto moved=env->CallObjectMethod(cursor, b.mMutableSet, x, y, z);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if(moved) env->DeleteLocalRef(moved);

                    jobject state = env->CallObjectMethod(level, b.mGetBlockState, cursor);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (!state) continue;

                    // isAir() is skipped deliberately: it is a third JNI call on
                    // every position, and underground almost nothing is air. The
                    // kind cache already answers "not interesting" for stone in
                    // one pointer comparison, so going straight to getBlock is
                    // a third fewer calls across solid terrain.
                    jobject block = env->CallObjectMethod(state, b.mGetBlock);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(state); continue; }
                    if (!block) { env->DeleteLocalRef(state); continue; }
                    const std::int8_t kind=cache.Lookup(env,b,block);
                    env->DeleteLocalRef(block);
                    bool selected=kind>=0&&(mask&(1u<<static_cast<std::uint32_t>(kind)));
                    if(selected&&kind==kBlockRotatedDeepslate) selected=IsRotatedDeepslate(env,b,state);
                    env->DeleteLocalRef(state);
                    if(!selected) continue;

                    BlockSnapshot record{};
                    record.x    = static_cast<float>(x);
                    record.y    = static_cast<float>(y);
                    record.z    = static_cast<float>(z);
                    record.kind = static_cast<std::uint8_t>(kind);
                    accum.push_back(record);
        }

        env->DeleteLocalRef(cursor);

        if (nextCell >= cells || accum.size() >= kMaxBlocks) {
            running = false;
            return true;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
// Inventaraktionen werden ausschließlich im instrumentierten Client-Tick ausgeführt.
// Fehlende optionale Handles werden nach dem Weltbeitritt erneut gebunden.
// Resolves the two pieces that are usually not loaded yet at bind time.
//
// ClientPlayerInteractionManager and SlotActionType only get class-loaded once
// you actually join a world, while BindMappingSet typically runs at the main
// menu. Binding them once at startup therefore fails permanently. Retrying here
// is cheap because the caller is throttled to a few times per second.
bool EnsureTotemBindings(JNIEnv* env, Bindings& b) {
    if (!b.set) return false;

    const mappings::MappingSet& set = *b.set;
    const std::string           prefix = std::string(set.id) + ".";

    if(!b.fInteractionMgr) b.fInteractionMgr=jvm::TryField(env,b.client.minecraftClass,set.fieldInteractionMgr.name,set.fieldInteractionMgr.signature,false);
    if(!b.fCurrentScreen||(*b.profile->guiClass&&!b.fGui)) {
        auto owner=b.client.minecraftClass;
        if(*b.profile->guiClass) {
            owner=SoftClass(env,(prefix+"gui").c_str(),b.profile->guiClass);
            b.fGui=jvm::TryField(env,b.client.minecraftClass,b.profile->clientGui.name,b.profile->clientGui.signature,false);
        }
        b.fCurrentScreen=jvm::TryField(env,owner,set.fieldCurrentScreen.name,set.fieldCurrentScreen.signature,false);
    }
    if(!b.clsItemStack) b.clsItemStack=SoftClass(env,(prefix+"itemstack").c_str(),set.classes.itemStack);
    if(!b.mItemIsEmpty) b.mItemIsEmpty=jvm::TryMethod(env,b.clsItemStack,set.methodItemIsEmpty.name,set.methodItemIsEmpty.signature,false);

    if (!b.clsInteractionMgr) {
        b.clsInteractionMgr = SoftClass(env, (prefix + "interact").c_str(), set.classes.interactionMgr);
    }
    if (!b.clsSlotAction) {
        b.clsSlotAction = SoftClass(env, (prefix + "slotaction").c_str(), set.classes.slotAction);
    }

    if (b.clsInteractionMgr && !b.mClickSlot) {
        b.mClickSlot = jvm::TryMethod(env, b.clsInteractionMgr,
                                      set.methodClickSlot.name, set.methodClickSlot.signature, false);
    }
    if (b.clsSlotAction && !b.swapAction) {
        if (jfieldID swap = jvm::TryField(env, b.clsSlotAction, set.fieldSwapAction.name,
                                          set.fieldSwapAction.signature, /*isStatic=*/true)) {
            jobject local = env->GetStaticObjectField(b.clsSlotAction, swap);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (local) { b.swapAction = env->NewGlobalRef(local); env->DeleteLocalRef(local); }
        }
    }

    if (!b.mGetEquipped) b.mGetEquipped=jvm::TryMethod(env,b.clsLiving,set.methodGetEquipped.name,set.methodGetEquipped.signature,false);
    if (!b.slotConstants[kSlotOffHand]) {
        if(!b.clsEquipSlot) b.clsEquipSlot=SoftClass(env,(prefix+"equip").c_str(),set.classes.equipSlot);
        const auto id=jvm::TryField(env,b.clsEquipSlot,set.slotOffHand.name,set.slotOffHand.signature,true);
        if(id) { auto value=env->GetStaticObjectField(b.clsEquipSlot,id); if(value) { b.slotConstants[kSlotOffHand]=env->NewGlobalRef(value); env->DeleteLocalRef(value); } }
        if(env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!b.clsPlayerInv) b.clsPlayerInv=SoftClass(env,(prefix+"playerinv").c_str(),set.classes.playerInv);
    if (!b.clsItem) b.clsItem=SoftClass(env,(prefix+"item").c_str(),set.classes.item);
    if (!b.fInventory) b.fInventory=jvm::TryField(env,b.clsPlayer,set.fieldInventory.name,set.fieldInventory.signature,false);
    if (!b.fScreenHandler) b.fScreenHandler=jvm::TryField(env,b.clsPlayer,set.fieldScreenHandler.name,set.fieldScreenHandler.signature,false);
    if (!b.mInvSize) b.mInvSize=jvm::TryMethod(env,b.clsPlayerInv,set.methodInvSize.name,set.methodInvSize.signature,false);
    if (!b.mInvGetStack) b.mInvGetStack=jvm::TryMethod(env,b.clsPlayerInv,set.methodInvGetStack.name,set.methodInvGetStack.signature,false);
    if (!b.mStackGetItem) b.mStackGetItem=jvm::TryMethod(env,b.clsItemStack,set.methodStackGetItem.name,set.methodStackGetItem.signature,false);
    if (!b.mItemTransKey) b.mItemTransKey=jvm::TryMethod(env,b.clsItem,set.methodItemTransKey.name,set.methodItemTransKey.signature,false);
    if (!b.fSyncId) {
        const std::string desc=set.fieldScreenHandler.signature;
        auto cls=SoftClass(env,(prefix+"menu").c_str(),desc.substr(1,desc.size()-2).c_str());
        b.fSyncId=jvm::TryField(env,cls,set.fieldSyncId.name,set.fieldSyncId.signature,false);
    }
    if (b.mClickSlot && b.swapAction && b.fInteractionMgr && b.fCurrentScreen && (!*b.profile->guiClass||b.fGui) &&
        b.fInventory && b.fScreenHandler && b.fSyncId && b.mInvSize && b.mInvGetStack && b.mStackGetItem && b.mItemTransKey && b.mItemIsEmpty) {
        return true;
    }
    return false;
}

mcov::agent::InventoryAssist<Bindings> g_inventory;
void JNICALL OnCombatTick(JNIEnv* env,jclass,jobject client) {
    g_combat.Tick(env,client);
    g_inventory.Tick(env,client);
}

void SampleWorldMeta(JNIEnv* env, const Bindings& b, jobject client, WorldSnapshot& snap) {
    jvm::LocalFrame frame(env, 12);

    // Dimension: level.dimension() gives a ResourceKey whose toString() already
    // reads like "ResourceKey[minecraft:dimension / minecraft:overworld]".
    // Going through java.lang.Object.toString() keeps this mapping-independent.
    if (b.mDimension && b.mObjToString) {
        jobject level = env->GetObjectField(client, b.fLevel);
        if (level) {
            jobject key = env->CallObjectMethod(level, b.mDimension);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (key) {
                auto text = static_cast<jstring>(env->CallObjectMethod(key, b.mObjToString));
                if (env->ExceptionCheck()) env->ExceptionClear();
                else if (text) CopyField(snap.dimension, jvm::JStringToUtf8(env, text).c_str());
            }
        }
    }

    // Current server is null in singleplayer - not an error.
    if (b.mCurrentServer && b.fServerAddress) {
        jobject server = env->CallObjectMethod(client, b.mCurrentServer);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (server) {
            auto address = static_cast<jstring>(env->GetObjectField(server, b.fServerAddress));
            if (address) CopyField(snap.serverAddress, jvm::JStringToUtf8(env, address).c_str());
        } else {
            CopyField(snap.serverAddress, "singleplayer");
        }
    }

    if (b.fFps) {
        const jint fps = env->GetStaticIntField(b.client.minecraftClass, b.fFps);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else snap.fps = static_cast<int>(fps);
    } else {
        snap.fps = -1;
    }
}

// ---------------------------------------------------------------------------
// Sampler thread
// ---------------------------------------------------------------------------
void SamplerThread() {
    // Attach ONCE for the thread's whole lifetime. Attaching and detaching every
    // frame would be far more expensive than the sampling itself and would churn
    // the VM's thread-local JNI state on every sample.
    jvm::ScopedEnv scope("mcov-sampler");
    if (!scope.ok()) {
        MCOV_LOG_ERROR("sampler could not attach to the JVM; giving up");
        SetStatus(AgentStatus::MappingsFailed);
        return;
    }
    JNIEnv* env = scope.get();
    MCOV_LOG_INFO("sampler attached (owns attachment: %s)", scope.ownsAttachment() ? "yes" : "no");

    Bindings       bindings;
    BlockKindCache blockCache;
    BlockScanner   scanner;
    WorldSnapshot  snapshot{};
    std::uint64_t  frame           = 0;
    int            resolveAttempts = 0;

    // Block results persist between scans so the ESP does not blink at the
    // game frame cadence while the scanner runs at ~1 Hz.
    static BlockSnapshot cachedBlocks[kMaxBlocks];
    std::uint32_t        lastBlocks    = 0;
    std::uint64_t        nextScanTick  = 0;
    jweak scanWorld=nullptr;
    bool                 syncAttempted = false;
    std::uint64_t        acceptedEpoch = 0;
    std::uint64_t        nextHudTick = 0;
    EntityDetailsState  entityDetails;
    std::uint64_t        nextHeartbeatTick = 0;
    std::uint64_t        lastCaptureTick = 0;
    std::uint64_t        waitingSinceTick = 0;
    bool                 waitAnnounced = false;

    LARGE_INTEGER counter{};
    auto awaitingCapture = [&](const FrameSync::Stamp& stamp) {
        const auto now = ::GetTickCount64();
        if (!lastCaptureTick || now - lastCaptureTick > 250) SetStatus(AgentStatus::WaitingForFrame);
        if (!waitingSinceTick) waitingSinceTick = now;
        if (!waitAnnounced && now - waitingSinceTick >= 2000) {
            MCOV_LOG_WARN("waiting for a coherent camera/entity frame (epoch %llu); worker heartbeat remains active",
                          static_cast<unsigned long long>(stamp.rawSequence));
            waitAnnounced = true;
        }
    };

    while (!g_stop.load(std::memory_order_acquire) &&
           !jvm::Runtime::Instance().shuttingDown()) {

        // A discarded geometry capture must not look like a dead connection.
        // Keep liveness separate while loading, paused or awaiting the camera.
        const auto loopNow = ::GetTickCount64();
        if (loopNow >= nextHeartbeatTick) {
            if (auto* block = g_channel.block()) block->heartbeat.fetch_add(1, std::memory_order_release);
            nextHeartbeatTick = loopNow + 100;
        }

        // Honour overlay -> agent control bits.
        if (auto* block = g_channel.block()) {
            const std::uint32_t flags = block->controlFlags.load(std::memory_order_acquire);
            if (flags & kCtrlRequestUnload) {
                MCOV_LOG_INFO("overlay requested unload");
                break;
            }
            if (flags & kCtrlPauseSampling) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        // Lazily bind. Minecraft's classes are not loaded yet if the agent went
        // in at JVM start, so this keeps retrying until one mapping set matches.
        if (!bindings.ready) {
            SetStatus(resolveAttempts >= 20 ? AgentStatus::MappingsFailed : AgentStatus::ResolvingMappings);
            for (const auto& profile : mappings::kRuntimeProfiles) {
                if (BindMappingSet(env, profile, bindings)) break;
            }
            if (!bindings.ready) {
                if (++resolveAttempts == 20) {
                    MCOV_LOG_ERROR("no runtime profile matched after %d attempts. Supported: "
                                   "26.2, 1.21.11, 1.21.1, 1.20.1; see docs/compatibility.md",
                                   resolveAttempts);
                    SetStatus(AgentStatus::MappingsFailed);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
        }
        if (!syncAttempted) {
            syncAttempted = true;
            if (g_frameSync.Install(env)) {
                MCOV_LOG_INFO("frame synchronization active: %s; mixed render/tick snapshots are rejected", bindings.set->id);
                if(g_inventory.Install(env,bindings,g_channel.block(),reinterpret_cast<void*>(&OnCombatTick)))
                    MCOV_LOG_INFO("inventory client-tick bridge active: %s",bindings.set->id);
                else MCOV_LOG_WARN("inventory client-tick bridge unavailable");
                if(g_combat.Install(env,bindings,g_channel.block(),reinterpret_cast<void*>(&OnCombatTick)))
                    MCOV_LOG_INFO("combat client-tick bridge active: %s",bindings.set->id);
                else MCOV_LOG_WARN("combat bridge unavailable; ESP and HUD remain active");
            } else MCOV_LOG_WARN("frame synchronization unavailable; using verified camera double-reads");
        }

        jvm::Error error;
        jobject    client = jvm::demo::GetClientInstance(env, bindings.client, &error);
        if (!client) {
            SetStatus(AgentStatus::NoClientInstance);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        {
            jvm::LocalFrame frame_guard(env, 16);

            WorldSnapshot candidate = snapshot; // retain slow HUD data between geometry frames
            candidate.entityCount = 0;
            const auto stamp = g_frameSync.Read(env);
            jobject player = env->GetObjectField(client, bindings.fPlayer);
            if (g_frameSync.enabled() && player && (!stamp.sequence || stamp.sequence == acceptedEpoch)) {
                awaitingCapture(stamp);
                env->DeleteLocalRef(client);
                g_frameSync.Wait();
                continue;
            }
            ::QueryPerformanceCounter(&counter);
            candidate.captureQpc = stamp.sequence ? stamp.cameraQpc : counter.QuadPart;
            CopyField(candidate.gameVersion, bindings.profile->version);
            CopyField(candidate.mappingProfile, bindings.set->id);

            SampleCamera(env, bindings, client, candidate);

            if (player) {
                SampleEntities(env, bindings, client, player, candidate,entityDetails);
                bool coherent = g_frameSync.enabled() ? g_frameSync.StillCurrent(env, stamp) : true;
                if (!g_frameSync.enabled()) {
                    WorldSnapshot verify{};
                    SampleCamera(env, bindings, client, verify);
                    coherent = candidate.camX == verify.camX && candidate.camY == verify.camY && candidate.camZ == verify.camZ &&
                               candidate.yawDeg == verify.yawDeg && candidate.pitchDeg == verify.pitchDeg &&
                               candidate.partialTick == verify.partialTick && candidate.fovDeg == verify.fovDeg;
                }
                if (!coherent) {
                    awaitingCapture(stamp);
                    env->DeleteLocalRef(client);
                    g_frameSync.Wait(1);
                    continue;
                }
                acceptedEpoch = stamp.sequence;
                lastCaptureTick = ::GetTickCount64();
                waitingSinceTick = 0;
                waitAnnounced = false;
                candidate.frameIndex = ++frame;
                snapshot = candidate;
                SetStatus(AgentStatus::Running);
                // Publish geometry BEFORE equipment, telemetry and block scanning.
                // These slower modules must not add latency to the entity anchor.
                if (auto* block = g_channel.block()) PublishSnapshot(block, snapshot);
                const auto hudNow = ::GetTickCount64();
                if (hudNow >= nextHudTick) {
                    SampleLocalPlayer(env, bindings, player, snapshot);
                    SampleWorldMeta(env, bindings, client, snapshot);
                    SampleEquipment(env, bindings, player, snapshot);
                    SampleEffects(env, bindings, player, snapshot);
                    SampleHeap(env, bindings, snapshot);
                    nextHudTick = hudNow + 100;
                }
                SampleTarget(env, bindings, client, snapshot);
                const auto* detailsBlock=g_channel.block();
                const bool detailsEnabled=detailsBlock &&
                    (detailsBlock->controlFlags.load(std::memory_order_acquire)&kCtrlEntityDetails);
                SampleEntityDetails(env,bindings,client,player,snapshot,entityDetails,detailsEnabled);

                // Resumierbarer Scan mit höchstens 2 ms Budget pro Sampler-Durchlauf.
                // Welt-/Filterwechsel löschen alte Treffer; neue Treffer erscheinen sofort.
                if(auto* block=g_channel.block()) {
                    const auto mask=block->scanMask.load(std::memory_order_acquire);
                    const int radius=static_cast<int>(std::clamp(block->scanRadius.load(std::memory_order_acquire),4u,48u));
                    const auto interval=std::clamp(block->scanIntervalMs.load(std::memory_order_acquire),200u,3000u);
                    const auto now=::GetTickCount64();
                    auto level=env->GetObjectField(client,bindings.fLevel);
                    const bool newWorld=level&&(!scanWorld||!env->IsSameObject(scanWorld,level));
                    if(newWorld) {
                        if(scanWorld) env->DeleteWeakGlobalRef(scanWorld);
                        scanWorld=env->NewWeakGlobalRef(level);
                    }
                    if(newWorld||mask!=scanner.mask||radius!=scanner.radius||!mask) {
                        lastBlocks=0; scanner.running=false; scanner.accum.clear(); nextScanTick=0;
                        scanner.mask=mask; scanner.radius=radius;
                    }
                    ScanState state=ScanState::Idle;
                    if(mask&&level) {
                        const bool bindingsReady=bindings.mGetBlockState&&bindings.mGetBlock&&bindings.mMutableSet&&bindings.mMutableCtor&&
                            (!(mask&(1u<<kBlockRotatedDeepslate))||EnsureChunkBindings(env,bindings));
                        if(!bindingsReady) state=ScanState::MissingBindings;
                        else {
                            if(!scanner.running&&now>=nextScanTick) scanner.Begin(radius,mask,snapshot);
                            if(scanner.running) {
                                const bool complete=scanner.Step(env,bindings,client,blockCache,2);
                                lastBlocks=static_cast<std::uint32_t>(scanner.accum.size());
                                if(lastBlocks) std::memcpy(cachedBlocks,scanner.accum.data(),sizeof(BlockSnapshot)*lastBlocks);
                                if(complete) nextScanTick=now+interval;
                            }
                            state=scanner.running?ScanState::Scanning:ScanState::Ready;
                        }
                    }
                    block->scanState.store(static_cast<std::uint32_t>(state),std::memory_order_release);
                    const int span=radius*2+1;
                    block->scanProgress.store(state==ScanState::Ready?100u:state==ScanState::Scanning?
                        static_cast<unsigned>(100ull*scanner.nextCell/(span*span*span)):0u,std::memory_order_release);
                    snapshot.blockCount=lastBlocks;
                    if(lastBlocks) std::memcpy(snapshot.blocks,cachedBlocks,sizeof(BlockSnapshot)*lastBlocks);
                }

                SetStatus(AgentStatus::Running);
            } else {
                // In a menu / not in a world yet.
                scanner.running=false; scanner.accum.clear(); lastBlocks=0; nextScanTick=0;
                if(scanWorld) { env->DeleteWeakGlobalRef(scanWorld); scanWorld=nullptr; }
                if(auto* block=g_channel.block()) { block->scanState.store(0); block->scanProgress.store(0); }
                lastCaptureTick = 0;
                waitingSinceTick = 0;
                waitAnnounced = false;
                snapshot = WorldSnapshot{};
                entityDetails.Clear(env);
                snapshot.frameIndex = ++frame;
                snapshot.captureQpc = counter.QuadPart;
                CopyField(snapshot.gameVersion, bindings.profile->version);
                CopyField(snapshot.mappingProfile, bindings.set->id);
                SetStatus(AgentStatus::NoClientInstance);
            }

            if (auto* block = g_channel.block()) PublishSnapshot(block, snapshot);
        }

        env->DeleteLocalRef(client);

        // Wake as soon as the next game camera is ready, rather than drifting
        // against its frame cadence with a fixed 16 ms sleep.
        g_frameSync.Wait(1);
    }

    SetStatus(AgentStatus::Detaching);
    if(scanWorld) env->DeleteWeakGlobalRef(scanWorld);
    g_combat.Close(env);
    g_inventory.Close(env);
    g_frameSync.Close(env);

    // Release global refs while we still hold a valid env and are still attached.
    // Doing this after DetachCurrentThread would corrupt the VM's ref table.
    for (jobject& slot : bindings.slotConstants) {
        if (slot) { env->DeleteGlobalRef(slot); slot = nullptr; }
    }
    blockCache.Release(env);
    entityDetails.Clear(env);
    if (bindings.axisProperty) { env->DeleteGlobalRef(bindings.axisProperty); bindings.axisProperty=nullptr; }
    if (bindings.swapAction) { env->DeleteGlobalRef(bindings.swapAction); bindings.swapAction = nullptr; }
    bindings.geometry.Release(env);
    jvm::Runtime::Instance().ReleaseCachedRefs(env);

    MCOV_LOG_INFO("sampler loop exited after %llu frames; detaching",
                  static_cast<unsigned long long>(frame));
    // ~ScopedEnv runs DetachCurrentThread here.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void StopAgent(const char* reason) {
    if (!g_started.exchange(false)) return;

    MCOV_LOG_INFO("stopping agent (%s)", reason);

    g_stop.store(true, std::memory_order_release);
    jvm::Runtime::Instance().BeginShutdown();

    if (g_sampler.joinable()) g_sampler.join();

    // Belt and braces: the sampler detaches itself in ~ScopedEnv, but if anyone
    // added another worker this is where we would find out.
    if (!jvm::Runtime::Instance().WaitForThreadsDetached(3000)) {
        MCOV_LOG_ERROR("proceeding with threads still attached - the VM may abort");
    }

    g_channel.Close();
    MCOV_LOG_INFO("agent stopped cleanly");
    mcov::log::Shutdown();
}

void JNICALL OnVmDeath(jvmtiEnv*, JNIEnv*) {
    // Agent_OnUnload is not reliably invoked for dynamically attached agents, so
    // VMDeath is the dependable shutdown signal. It fires while the VM is still
    // alive enough for us to detach properly.
    StopAgent("VMDeath");
}

jint StartAgent(JavaVM* vm, const char* /*options*/) {
    if (g_started.exchange(true)) return JNI_OK;

    mcov::log::Init("agent", AgentLogPath(), /*alsoConsole=*/false);
    MCOV_LOG_INFO("=== mcov agent starting (pid %lu) ===", ::GetCurrentProcessId());

    // 1. Adopt the VM we were handed. DiscoverVm()/JNI_GetCreatedJavaVMs would
    //    also work from in here, but there is no reason to go looking for a
    //    pointer the JVM just gave us.
    jvm::Runtime::Instance().AdoptVm(vm);

    // 2. JVMTI env. JVMTI_VERSION_1_2 is the baseline every modern HotSpot
    //    accepts; GetLoadedClasses needs no extra capabilities, which keeps this
    //    agent's footprint minimal.
    void* rawJvmti = nullptr;
    if (vm->GetEnv(&rawJvmti, JVMTI_VERSION_1_2) != JNI_OK || !rawJvmti) {
        MCOV_LOG_ERROR("GetEnv(JVMTI_VERSION_1_2) failed - cannot install the class resolver");
        g_started.store(false);
        return JNI_ERR;
    }
    g_jvmti = static_cast<jvmtiEnv*>(rawJvmti);

    jvmtiEventCallbacks callbacks{};
    callbacks.VMDeath = &OnVmDeath;
    g_jvmti->SetEventCallbacks(&callbacks, static_cast<jint>(sizeof(callbacks)));
    g_jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);

    jvm::Runtime::Instance().SetFallbackClassResolver(&ResolveClassViaJvmti);

    // 3. Shared memory, keyed on our own PID so the overlay can find us.
    std::string ipcError;
    if (!g_channel.CreateWriter(::GetCurrentProcessId(), &ipcError)) {
        MCOV_LOG_ERROR("shared memory setup failed: %s", ipcError.c_str());
        g_started.store(false);
        return JNI_ERR;
    }

    // 4. Sampler.
    g_stop.store(false, std::memory_order_release);
    g_sampler = std::thread(&SamplerThread);

    MCOV_LOG_INFO("agent ready");
    return JNI_OK;
}

} // namespace

// ---------------------------------------------------------------------------
// JVMTI entry points
// ---------------------------------------------------------------------------
extern "C" {

// Loaded at JVM startup via -agentpath:mcov_agent.dll
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* /*reserved*/) {
    return StartAgent(vm, options);
}

// Loaded into a running JVM via VirtualMachine.loadAgentPath() - our normal path.
JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* /*reserved*/) {
    return StartAgent(vm, options);
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* /*vm*/) {
    StopAgent("Agent_OnUnload");
}

} // extern "C"

// DllMain deliberately does nothing on detach: joining a thread or touching JNI
// under the Windows loader lock is a classic self-inflicted deadlock. All real
// teardown happens in OnVmDeath / Agent_OnUnload.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) ::DisableThreadLibraryCalls(module);
    return TRUE;
}
