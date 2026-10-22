#pragma once
#include "../common/shared_state.hpp"
#include "jvm_interface.hpp"
#include "trident_profiles.hpp"
#include <mutex>
#include <stdexcept>
#include <cmath>

namespace mcov::agent {
// JNI nur im Client-Thread; Install/Close verwenden denselben Mutex.
template<class B> class InventoryAssist final {
    mutable std::mutex mutex_;
    B b_{}; SharedBlock* channel_=nullptr;
    bool ready_=false,totemReady_=false,macroReady_=false,anchorReady_=false;
    std::uint64_t retry_=0,lastError_=0;
    jclass signal_=nullptr,crystal_=nullptr,blockHit_=nullptr,posClass_=nullptr;
    jfieldID enabled_=nullptr,macroEnabled_=nullptr,selected_=nullptr,ownMenu_=nullptr,hit_=nullptr,useKey_=nullptr;
    jmethodID carried_=nullptr,paused_=nullptr,type_=nullptr,blockPos_=nullptr,stateValue_=nullptr;
    jmethodID attack_=nullptr,swing_=nullptr,dim_=nullptr,anchorWorks_=nullptr,environment_=nullptr,environmentValue_=nullptr;
    jmethodID integer_=nullptr,boolean_=nullptr,enumName_=nullptr,keyDown_=nullptr,eye_=nullptr,location_=nullptr;
    jmethodID posCtor_=nullptr,hitCtor_=nullptr,vecCtor_=nullptr,direction_=nullptr,useOn_=nullptr,consumes_=nullptr,replaceable_=nullptr;
    jmethodID blockReach_=nullptr,oldBlockReach_=nullptr,attackReach_=nullptr,farReach_=nullptr,alive_=nullptr;
    jmethodID coords_[3]{},steps_[3]{};
    jobject hand_=nullptr,up_=nullptr,charge_=nullptr,environmentAnchor_=nullptr;
    jweak world_=nullptr,player_=nullptr;
    items::Controller controller_;
    items::TridentController tridentController_{static_cast<std::uint32_t>(GetTickCount64())^GetCurrentProcessId()};
    bool tridentReady_=false,legacyRiptide_=false;
    jclass enchantments_=nullptr;
    jmethodID tridentUsing_=nullptr,tridentTicks_=nullptr,tridentHand_=nullptr,tridentStack_=nullptr,tridentWet_=nullptr,tridentAutoSpin_=nullptr,tridentRelease_=nullptr,tridentSpin_=nullptr;
    std::uint64_t tridentActions_=0;
    // Referenzen gehören einem Haltevorgang, niemals einem Render-Frame.
    struct Site {
        items::Item kind=items::Item::Empty;
        int clicked[3]{},cell[3]{},selected=-1;
        double hit[3]{};
        bool replacing=false;
        jobject placement=nullptr,position=nullptr,interaction=nullptr;
    } site_;
    jclass Class(JNIEnv* env,const char* name) {
        if(!name||!*name) return nullptr;
        const std::string key=std::string(b_.set->id)+".inventory."+name;
        jvm::Error error; return jvm::Runtime::Instance().ResolveClass(env,key.c_str(),{name},&error);
    }
    jmethodID Method(JNIEnv* env,jclass c,mappings::Member m) { return c&&m.name&&*m.name?jvm::TryMethod(env,c,m.name,m.signature,false):nullptr; }
    jfieldID Field(JNIEnv* env,jclass c,mappings::Member m) { return c&&m.name&&*m.name?jvm::TryField(env,c,m.name,m.signature,false):nullptr; }
    static void Check(JNIEnv* env) {
        if(env->ExceptionCheck()) { env->ExceptionClear(); throw std::runtime_error("JNI inventory operation"); }
    }
    void Constant(JNIEnv* env,jclass c,mappings::Member m,jobject& value) {
        if(value||!c||!m.name||!*m.name) return;
        const auto id=jvm::TryField(env,c,m.name,m.signature,true);
        if(id) { auto local=env->GetStaticObjectField(c,id); Check(env); if(local) value=env->NewGlobalRef(local); Check(env); }
    }
    void ClearSite(JNIEnv* env) {
        for(auto* ref:{&site_.placement,&site_.position,&site_.interaction}) if(*ref) env->DeleteGlobalRef(*ref);
        site_={};
    }
    void Bind(JNIEnv* env) {
        totemReady_=EnsureTotemBindings(env,b_);
        const auto& g=*b_.profile->geometry;
        selected_=Field(env,b_.clsPlayerInv,g.selectedSlot); ownMenu_=Field(env,b_.clsPlayer,g.inventoryMenu);
        const std::string menu=b_.set->fieldScreenHandler.signature;
        carried_=Method(env,Class(env,menu.substr(1,menu.size()-2).c_str()),g.carriedStack);
        paused_=Method(env,b_.client.minecraftClass,g.clientPaused); hit_=Field(env,b_.client.minecraftClass,g.hitResult);
        crystal_=Class(env,g.crystalClass); blockHit_=Class(env,g.blockHitClass); posClass_=Class(env,g.blockPosClass);
        type_=Method(env,Class(env,g.hitClass),g.hitType); location_=Method(env,Class(env,g.hitClass),g.hitLocation);
        blockPos_=Method(env,blockHit_,g.blockHitPos); direction_=Method(env,blockHit_,g.hitDirection);
        stateValue_=Method(env,b_.clsBlockState,g.stateValue);
        posCtor_=Method(env,posClass_,g.blockPosCtor); hitCtor_=Method(env,blockHit_,g.blockHitCtor); vecCtor_=Method(env,b_.clsVec3,g.vecCtor);
        const mappings::Member positions[]={g.posX,g.posY,g.posZ},directions[]={g.stepX,g.stepY,g.stepZ};
        auto directionClass=Class(env,g.directionClass);
        for(int i=0;i<3;++i) { coords_[i]=Method(env,posClass_,positions[i]); steps_[i]=Method(env,directionClass,directions[i]); }
        useKey_=Field(env,b_.clsOptions,g.useKey); keyDown_=Method(env,Class(env,g.keyMappingClass),g.keyDown);
        useOn_=Method(env,b_.clsInteractionMgr,g.useOn); consumes_=Method(env,Class(env,g.resultClass),g.consumesAction);
        replaceable_=Method(env,b_.clsBlockState,g.replaceable); eye_=Method(env,b_.clsEntity,g.eyePosition);
        blockReach_=Method(env,b_.clsPlayer,g.blockReach); oldBlockReach_=Method(env,b_.clsInteractionMgr,g.oldBlockReach);
        attackReach_=Method(env,b_.clsPlayer,g.attackReach); farReach_=Method(env,b_.clsInteractionMgr,g.farReach);
        alive_=Method(env,b_.clsEntity,g.entityAlive);
        attack_=Method(env,b_.clsInteractionMgr,g.attackEntity); swing_=Method(env,b_.clsLiving,g.swingHand);
        Constant(env,Class(env,g.handClass),g.mainHand,hand_); Constant(env,directionClass,g.upDirection,up_);
        Constant(env,Class(env,g.anchorClass),g.anchorCharge,charge_);
        dim_=Method(env,b_.clsLevel,g.dimensionType); anchorWorks_=Method(env,Class(env,g.dimensionTypeClass),g.anchorWorks);
        environment_=Method(env,b_.clsLevel,g.environment); environmentValue_=Method(env,Class(env,g.environmentClass),g.environmentValue);
        Constant(env,Class(env,g.environmentAttrsClass),g.environmentAnchor,environmentAnchor_);
        integer_=jvm::TryMethod(env,env->FindClass("java/lang/Integer"),"intValue","()I",false);
        boolean_=jvm::TryMethod(env,env->FindClass("java/lang/Boolean"),"booleanValue","()Z",false);
        enumName_=jvm::TryMethod(env,env->FindClass("java/lang/Enum"),"name","()Ljava/lang/String;",false);
        totemReady_=totemReady_&&ownMenu_&&carried_&&b_.mItemIsEmpty&&b_.mGetEquipped&&b_.slotConstants[kSlotOffHand]&&paused_;
        macroReady_=totemReady_&&selected_&&hit_&&type_&&location_&&blockHit_&&crystal_&&blockPos_&&enumName_&&attack_&&swing_&&hand_&&
            b_.mGetBlockState&&b_.mGetBlock&&b_.mBlockTransKey&&useKey_&&keyDown_&&posCtor_&&hitCtor_&&vecCtor_&&direction_&&
            useOn_&&consumes_&&replaceable_&&eye_&&(blockReach_||oldBlockReach_)&&(attackReach_||farReach_)&&alive_&&
            coords_[0]&&coords_[1]&&coords_[2]&&steps_[0]&&steps_[1]&&steps_[2]&&up_&&b_.mEntities&&b_.mIterator&&
            b_.mHasNext&&b_.mNext&&b_.geometry.getBounds&&b_.fVecX&&b_.fVecY&&b_.fVecZ;
        anchorReady_=macroReady_&&charge_&&integer_&&stateValue_&&((dim_&&anchorWorks_)||(environment_&&environmentValue_&&environmentAnchor_&&boolean_));
        if(const auto* p=TridentMapping(b_.set->id)) {
            tridentUsing_=Method(env,b_.clsLiving,p->usingItem); tridentTicks_=Method(env,b_.clsLiving,p->ticks);
            tridentHand_=Method(env,b_.clsLiving,p->hand); tridentStack_=Method(env,b_.clsLiving,p->stack);
            tridentWet_=Method(env,b_.clsEntity,p->wet); tridentRelease_=Method(env,b_.clsInteractionMgr,p->release);
            tridentAutoSpin_=Method(env,b_.clsLiving,p->autoSpin);
            enchantments_=Class(env,p->helperClass);
            tridentSpin_=enchantments_?jvm::TryMethod(env,enchantments_,p->spin.name,p->spin.signature,true):nullptr;
            legacyRiptide_=std::string_view(p->spin.signature).ends_with(")I");
            tridentReady_=totemReady_&&b_.slotConstants[kSlotMainHand]&&hand_&&useKey_&&keyDown_&&tridentUsing_&&tridentTicks_&&tridentHand_&&tridentStack_&&tridentWet_&&tridentAutoSpin_&&tridentRelease_&&tridentSpin_;
        }
    }
    items::Item Item(JNIEnv* env,jobject stack) {
        if(!stack) return items::Item::Empty;
        jvm::LocalFrame frame(env,8);
        const bool empty=env->CallBooleanMethod(stack,b_.mItemIsEmpty)==JNI_TRUE; Check(env);
        if(empty) return items::Item::Empty;
        auto item=env->CallObjectMethod(stack,b_.mStackGetItem); Check(env);
        if(!item) return items::Item::Other;
        auto key=static_cast<jstring>(env->CallObjectMethod(item,b_.mItemTransKey)); Check(env);
        const auto text=jvm::JStringToUtf8(env,key);
        if(text=="item.minecraft.totem_of_undying") return items::Item::Totem;
        if(text=="item.minecraft.end_crystal") return items::Item::Crystal;
        if(text=="block.minecraft.respawn_anchor") return items::Item::Anchor;
        if(text=="block.minecraft.glowstone") return items::Item::Glowstone;
        return items::Item::Other;
    }
    std::string BlockKey(JNIEnv* env,jobject state) {
        if(!state) return {};
        jvm::LocalFrame frame(env,8);
        auto block=env->CallObjectMethod(state,b_.mGetBlock); Check(env);
        auto key=block?static_cast<jstring>(env->CallObjectMethod(block,b_.mBlockTransKey)):nullptr; Check(env);
        return jvm::JStringToUtf8(env,key);
    }
    void Coordinates(JNIEnv* env,jobject pos,int* out) { for(int i=0;i<3;++i) { out[i]=env->CallIntMethod(pos,coords_[i]); Check(env); } }
    bool BlockHit(JNIEnv* env,jobject hit) {
        if(!hit||!env->IsInstanceOf(hit,blockHit_)) return false;
        auto type=env->CallObjectMethod(hit,type_); Check(env);
        auto name=type?static_cast<jstring>(env->CallObjectMethod(type,enumName_)):nullptr; Check(env);
        return jvm::JStringToUtf8(env,name)=="BLOCK";
    }
    bool UseDown(JNIEnv* env,jobject client) {
        if(!useKey_||!keyDown_) return false;
        auto options=env->GetObjectField(client,b_.fOptions); Check(env);
        auto key=options?env->GetObjectField(options,useKey_):nullptr; Check(env);
        const bool down=key&&env->CallBooleanMethod(key,keyDown_)==JNI_TRUE; Check(env); return down;
    }
    void Gate(JNIEnv* env,jobject client,items::Frame& f,jobject& level,jobject& player) {
        level=env->GetObjectField(client,b_.fLevel); Check(env); player=env->GetObjectField(client,b_.fPlayer); Check(env);
        f.world=level&&player;
        if(!f.world) return;
        if(!world_||!player_||!env->IsSameObject(world_,level)||!env->IsSameObject(player_,player)) {
            controller_.Reset(); tridentController_.Reset(); ClearSite(env);
            if(world_) env->DeleteWeakGlobalRef(world_); if(player_) env->DeleteWeakGlobalRef(player_);
            world_=env->NewWeakGlobalRef(level); player_=env->NewWeakGlobalRef(player); Check(env);
        }
        f.health=env->CallFloatMethod(player,b_.mHealth); Check(env);
        f.alive=std::isfinite(f.health)&&f.health>0;
        f.totemSupported=totemReady_; f.macroSupported=macroReady_; f.anchorSupported=anchorReady_;
        f.paused=paused_?env->CallBooleanMethod(client,paused_)==JNI_TRUE:true; Check(env);
        auto screenOwner=b_.fGui?env->GetObjectField(client,b_.fGui):client; Check(env);
        f.screen=!screenOwner||!b_.fCurrentScreen||env->GetObjectField(screenOwner,b_.fCurrentScreen)!=nullptr; Check(env);
        f.useDown=UseDown(env,client);
    }
    void Menu(JNIEnv* env,jobject player,items::Frame& f) {
        auto menu=env->GetObjectField(player,b_.fScreenHandler); Check(env);
        auto own=env->GetObjectField(player,ownMenu_); Check(env);
        f.ownMenu=menu&&own&&env->IsSameObject(menu,own);
        auto carried=menu?env->CallObjectMethod(menu,carried_):nullptr; Check(env);
        f.cursorEmpty=Item(env,carried)==items::Item::Empty;
    }
    static void Foreground(items::Request& request,bool check) {
        if(check) { DWORD pid=0; GetWindowThreadProcessId(GetForegroundWindow(),&pid); request.inputAllowed=request.inputAllowed&&pid==GetCurrentProcessId(); }
    }
    void Trident(JNIEnv* env,jobject client,jobject player,const items::Request& r,const items::Frame& f,bool busy,std::uint64_t now,items::Report& report) {
        items::TridentFrame t; t.supported=tridentReady_; t.busy=busy;
        // Read only the held/active stacks. Vanilla handles the next held-Use attempt,
        // cooldown, durability and the actual Riptide movement.
        if(r.trident.enabled&&f.world&&f.alive&&t.supported&&items::Lease(r,now)&&!f.paused&&!f.screen&&!r.combatActive&&f.ownMenu&&f.cursorEmpty&&!busy) {
            auto stack=env->CallObjectMethod(player,b_.mGetEquipped,b_.slotConstants[kSlotMainHand]); Check(env);
            if(stack) {
                auto item=env->CallObjectMethod(stack,b_.mStackGetItem); Check(env);
                auto key=item?static_cast<jstring>(env->CallObjectMethod(item,b_.mItemTransKey)):nullptr; Check(env);
                t.holding=jvm::JStringToUtf8(env,key)=="item.minecraft.trident";
                if(t.holding) {
                    const double spin=legacyRiptide_?env->CallStaticIntMethod(enchantments_,tridentSpin_,stack):env->CallStaticFloatMethod(enchantments_,tridentSpin_,stack,player); Check(env);
                    t.riptide=std::isfinite(spin)&&spin>0;
                    t.wet=env->CallBooleanMethod(player,tridentWet_)==JNI_TRUE; Check(env);
                    t.spinning=env->CallBooleanMethod(player,tridentAutoSpin_)==JNI_TRUE; Check(env);
                    const bool usingItem=env->CallBooleanMethod(player,tridentUsing_)==JNI_TRUE; Check(env);
                    auto hand=env->CallObjectMethod(player,tridentHand_); Check(env);
                    auto active=env->CallObjectMethod(player,tridentStack_); Check(env);
                    t.usingMain=usingItem&&hand&&env->IsSameObject(hand,hand_)&&active&&env->IsSameObject(active,stack);
                    t.ticks=env->CallIntMethod(player,tridentTicks_); Check(env);
                }
            }
        }
        report.trident=tridentController_.Tick(r,f,t,now);
        if(report.trident==items::Status::ReleaseTrident) {
            auto manager=env->GetObjectField(client,b_.fInteractionMgr); Check(env);
            if(!manager) { report.trident=items::Status::Unsupported; tridentController_.Reset(); }
            else { env->CallVoidMethod(manager,tridentRelease_,player); Check(env); ++tridentActions_; }
        }
        report.tridentActions=tridentActions_;
    }
    bool AnchorWorks(JNIEnv* env,jobject level,jobject pos) {
        if(!anchorReady_) return true;
        if(dim_&&anchorWorks_) {
            auto d=env->CallObjectMethod(level,dim_); Check(env);
            const bool works=!d||env->CallBooleanMethod(d,anchorWorks_)==JNI_TRUE; Check(env); return works;
        }
        auto e=env->CallObjectMethod(level,environment_); Check(env);
        auto v=e?env->CallObjectMethod(e,environmentValue_,environmentAnchor_,pos):nullptr; Check(env);
        const bool works=!v||env->CallBooleanMethod(v,boolean_)==JNI_TRUE; Check(env); return works;
    }
    bool AtSite(JNIEnv* env,jobject entity) {
        if(!entity||!env->IsInstanceOf(entity,crystal_)) return false;
        const double x=env->CallDoubleMethod(entity,b_.mGetX); Check(env);
        const double y=env->CallDoubleMethod(entity,b_.mGetY); Check(env);
        const double z=env->CallDoubleMethod(entity,b_.mGetZ); Check(env);
        return std::abs(x-(site_.cell[0]+.5))<.2&&std::abs(y-site_.cell[1])<.2&&std::abs(z-(site_.cell[2]+.5))<.2;
    }
    bool AimMatches(JNIEnv* env,jobject client,items::Frame& f) {
        auto entity=b_.fTargetedEntity?env->GetObjectField(client,b_.fTargetedEntity):nullptr; Check(env);
        if(site_.kind==items::Item::Crystal&&AtSite(env,entity)) { f.crosshairCrystal=true; return true; }
        auto hit=env->GetObjectField(client,hit_); Check(env);
        if(!BlockHit(env,hit)) return false;
        auto pos=env->CallObjectMethod(hit,blockPos_); Check(env); if(!pos) return false;
        int xyz[3]; Coordinates(env,pos,xyz);
        f.crosshairSupport=std::equal(xyz,xyz+3,site_.clicked);
        f.crosshairCell=std::equal(xyz,xyz+3,site_.cell);
        return f.crosshairSupport||f.crosshairCell;
    }
    jobject Observe(JNIEnv* env,jobject client,jobject level,jobject player,jobject manager,items::Frame& f,const items::MacroConfig& config) {
        f.cycle=site_.kind;
        if(site_.kind==items::Item::Empty) return nullptr;
        f.siteValid=AimMatches(env,client,f); if(!f.siteValid) return nullptr;
        auto eye=env->CallObjectMethod(player,eye_); Check(env); if(!eye) { f.siteValid=false; return nullptr; }
        const double e[]={env->GetDoubleField(eye,b_.fVecX),env->GetDoubleField(eye,b_.fVecY),env->GetDoubleField(eye,b_.fVecZ)}; Check(env);
        const double reach=blockReach_?env->CallDoubleMethod(player,blockReach_):env->CallFloatMethod(manager,oldBlockReach_); Check(env);
        const double dx=e[0]-site_.hit[0],dy=e[1]-site_.hit[1],dz=e[2]-site_.hit[2];
        f.distance=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(!std::isfinite(dx+dy+dz+reach)||reach<=0||dx*dx+dy*dy+dz*dz>reach*reach) { f.siteValid=false; return nullptr; }
        // Eigene Reichweite verkürzt nur Vanilla. Kein Scan außerhalb des Limits.
        if(f.distance>config.maxRange) return nullptr;
        if(site_.kind==items::Item::Anchor) {
            auto state=env->CallObjectMethod(level,b_.mGetBlockState,site_.position); Check(env);
            const auto key=BlockKey(env,state);
            if(key=="block.minecraft.respawn_anchor") {
                f.target=items::Target::Anchor;
                auto value=env->CallObjectMethod(state,stateValue_,charge_); Check(env);
                f.charges=value?env->CallIntMethod(value,integer_):-1; Check(env);
            } else {
                const bool replace=state&&env->CallBooleanMethod(state,replaceable_)==JNI_TRUE; Check(env);
                f.siteValid=replace; f.target=items::Target::OtherBlock;
                if(!site_.replacing) {
                    auto clicked=env->CallObjectMethod(site_.placement,blockPos_); Check(env);
                    auto support=env->CallObjectMethod(level,b_.mGetBlockState,clicked); Check(env);
                    const bool gone=!support||env->CallBooleanMethod(support,replaceable_)==JNI_TRUE; Check(env);
                    if(gone) f.siteValid=false;
                }
            }
            f.anchorWorks=AnchorWorks(env,level,site_.position); return nullptr;
        }
        auto clicked=env->CallObjectMethod(site_.placement,blockPos_); Check(env);
        auto state=env->CallObjectMethod(level,b_.mGetBlockState,clicked); Check(env);
        const auto key=BlockKey(env,state);
        if(key!="block.minecraft.obsidian"&&key!="block.minecraft.bedrock") { f.siteValid=false; return nullptr; }
        f.target=items::Target::CrystalBase;
        // Begrenzter Scan vorhandener Client-Entitäten, keine Arbeit im Render-Thread.
        auto iterable=env->CallObjectMethod(level,b_.mEntities); Check(env);
        auto iterator=iterable?env->CallObjectMethod(iterable,b_.mIterator):nullptr; Check(env);
        if(!iterator) { f.siteValid=false; return nullptr; }
        for(int i=0;i<4096;++i) {
            const bool more=env->CallBooleanMethod(iterator,b_.mHasNext)==JNI_TRUE; Check(env);
            if(!more) return nullptr;
            auto entity=env->CallObjectMethod(iterator,b_.mNext); Check(env);
            if(AtSite(env,entity)) {
                const bool live=env->CallBooleanMethod(entity,alive_)==JNI_TRUE; Check(env);
                if(live) {
                    auto bounds=env->CallObjectMethod(entity,b_.geometry.getBounds); Check(env);
                    const double attackReach=std::min(double(config.maxRange),attackReach_?env->CallDoubleMethod(player,attackReach_):env->CallBooleanMethod(manager,farReach_)==JNI_TRUE?6.:3.); Check(env);
                    double squared=0;
                    if(!bounds||!std::isfinite(attackReach)||attackReach<=0) { f.siteValid=false; return nullptr; }
                    for(int axis=0;axis<3;++axis) {
                        const double low=env->GetDoubleField(bounds,b_.geometry.min[axis]),high=env->GetDoubleField(bounds,b_.geometry.max[axis]); Check(env);
                        if(!std::isfinite(low+high)||low>high) { f.siteValid=false; return nullptr; }
                        const double gap=e[axis]-std::clamp(e[axis],low,high); squared+=gap*gap;
                    }
                    if(squared>attackReach*attackReach) { f.siteValid=false; return nullptr; }
                    f.target=items::Target::Crystal; f.crystalId=env->CallIntMethod(entity,b_.mGetId); Check(env); return entity;
                }
            }
            if(entity) env->DeleteLocalRef(entity);
        }
        // Ein unvollständiger Scan darf keinen zweiten Crystal platzieren.
        f.siteValid=false; return nullptr;
    }
public:
    bool Install(JNIEnv* env,const B& bindings,SharedBlock* channel,void* callback,void* captureCallback) {
        const std::scoped_lock lock(mutex_);
        if(ready_) return true;
        if(!channel||!bindings.profile->geometry) return false;
        b_=bindings; channel_=channel;
        b_.swapAction=bindings.swapAction?env->NewGlobalRef(bindings.swapAction):nullptr;
        for(int i=0;i<kSlotCount;++i) b_.slotConstants[i]=bindings.slotConstants[i]?env->NewGlobalRef(bindings.slotConstants[i]):nullptr;
        jvm::LocalFrame frame(env,96); Bind(env);
        auto signal=env->FindClass("mcov/FrameSignal");
        if(env->ExceptionCheck()) { env->ExceptionClear(); return false; } if(!signal) return false;
        signal_=static_cast<jclass>(env->NewGlobalRef(signal));
        enabled_=jvm::TryField(env,signal_,"combatEnabled","Z",true); macroEnabled_=jvm::TryField(env,signal_,"macroEnabled","Z",true);
        JNINativeMethod natives[]={
            {const_cast<char*>("nativeCombatTick"),const_cast<char*>("(Ljava/lang/Object;)V"),callback},
            {const_cast<char*>("nativeMacroUse"),const_cast<char*>("(Ljava/lang/Object;)Z"),captureCallback}};
        if(!enabled_||!macroEnabled_||env->RegisterNatives(signal_,natives,2)!=JNI_OK) { if(env->ExceptionCheck()) env->ExceptionClear(); return false; }
        ready_=true; env->SetStaticBooleanField(signal_,enabled_,JNI_TRUE);
        if(env->ExceptionCheck()) { env->ExceptionClear(); ready_=false; } return ready_;
    }
    // Nur echte Use-Versuche mit dem passenden ausgewählten Item übernehmen.
    bool CaptureUse(JNIEnv* env,jobject client,bool checkForeground=true) noexcept {
        // Mod-Callbacks dürfen aus useItemOn erneut Use aufrufen. Kein rekursives
        // Warten auf unseren eigenen Tick-Mutex; diese Interaktion bleibt Vanilla.
        const std::unique_lock lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock()) return false;
        if(!ready_||!channel_||!macroReady_) return false;
        items::Request r; if(!channel_->itemRequest.Read(r)) return false;
        Foreground(r,checkForeground);
        if(!items::Lease(r,GetTickCount64())||r.combatActive) return false;
        try {
            jvm::LocalFrame frame(env,96); if(!frame.ok()) return false;
            items::Frame f; jobject level=nullptr,player=nullptr; Gate(env,client,f,level,player);
            if(!f.world||!f.alive||f.paused||f.screen||!f.useDown) return false;
            Menu(env,player,f); if(!f.ownMenu||!f.cursorEmpty) return false;
            auto inventory=env->GetObjectField(player,b_.fInventory); Check(env); if(!inventory) return false;
            f.selected=env->GetIntField(inventory,selected_); Check(env); if(f.selected<0||f.selected>=9) return false;
            if(controller_.HasRestore()&&!controller_.Active()) return f.selected==controller_.ExpectedSlot();
            if(site_.kind!=items::Item::Empty) {
                const bool armed=site_.kind==items::Item::Crystal?r.crystal.enabled:r.anchor.enabled;
                // Wegsehen beendet den Ablauf im Tick. Bis zum Slot-Restore darf
                // Vanilla das automatisch ausgewählte Glowstone nicht verwenden.
                return armed&&f.selected==site_.selected;
            }
            auto stack=env->CallObjectMethod(inventory,b_.mInvGetStack,f.selected); Check(env);
            const auto kind=Item(env,stack);
            if(!(kind==items::Item::Crystal&&r.crystal.enabled)&&!(kind==items::Item::Anchor&&r.anchor.enabled&&anchorReady_)) return false;
            auto hit=env->GetObjectField(client,hit_); Check(env); if(!BlockHit(env,hit)) return false;
            auto pos=env->CallObjectMethod(hit,blockPos_); Check(env); if(!pos) return false;
            auto state=env->CallObjectMethod(level,b_.mGetBlockState,pos); Check(env);
            const auto key=BlockKey(env,state);
            if(kind==items::Item::Crystal&&key!="block.minecraft.obsidian"&&key!="block.minecraft.bedrock") return false;
            if(kind==items::Item::Anchor&&AnchorWorks(env,level,pos)) return false;
            auto direction=env->CallObjectMethod(hit,direction_); Check(env); if(!direction||!state) return false;
            ClearSite(env); site_.kind=kind; site_.selected=f.selected; Coordinates(env,pos,site_.clicked);
            std::copy(site_.clicked,site_.clicked+3,site_.cell);
            if(kind==items::Item::Crystal) ++site_.cell[1];
            else if(key!="block.minecraft.respawn_anchor") {
                site_.replacing=env->CallBooleanMethod(state,replaceable_)==JNI_TRUE; Check(env);
                if(!site_.replacing) for(int i=0;i<3;++i) { site_.cell[i]+=env->CallIntMethod(direction,steps_[i]); Check(env); }
            }
            // Start am vorhandenen Anchor: später auf seinem bisherigen Untergrund neu platzieren.
            if(kind==items::Item::Anchor&&key=="block.minecraft.respawn_anchor") {
                site_.clicked[1]=site_.cell[1]-1;
                auto support=env->NewObject(posClass_,posCtor_,site_.clicked[0],site_.clicked[1],site_.clicked[2]); Check(env);
                auto point=env->NewObject(b_.clsVec3,vecCtor_,site_.cell[0]+.5,double(site_.cell[1]),site_.cell[2]+.5); Check(env);
                hit=env->NewObject(blockHit_,hitCtor_,point,up_,support,JNI_FALSE); Check(env);
            }
            auto location=env->CallObjectMethod(hit,location_); Check(env);
            if(!location) { ClearSite(env); return false; }
            site_.hit[0]=env->GetDoubleField(location,b_.fVecX); site_.hit[1]=env->GetDoubleField(location,b_.fVecY); site_.hit[2]=env->GetDoubleField(location,b_.fVecZ); Check(env);
            auto cell=env->NewObject(posClass_,posCtor_,site_.cell[0],site_.cell[1],site_.cell[2]); Check(env);
            auto point=env->NewObject(b_.clsVec3,vecCtor_,site_.cell[0]+.5,site_.cell[1]+1.,site_.cell[2]+.5); Check(env);
            auto interaction=env->NewObject(blockHit_,hitCtor_,point,up_,cell,JNI_FALSE); Check(env);
            site_.placement=env->NewGlobalRef(hit); site_.position=env->NewGlobalRef(cell); site_.interaction=env->NewGlobalRef(interaction); Check(env);
            if(!site_.placement||!site_.position||!site_.interaction) { ClearSite(env); return false; }
            return true;
        } catch(...) { if(env->ExceptionCheck()) env->ExceptionClear(); ClearSite(env); controller_.Reset(); return false; }
    }
    void Close(JNIEnv* env) {
        const std::scoped_lock lock(mutex_); ready_=false;
        if(signal_&&enabled_) env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE);
        if(signal_&&macroEnabled_) env->SetStaticBooleanField(signal_,macroEnabled_,JNI_FALSE);
        if(env->ExceptionCheck()) env->ExceptionClear(); ClearSite(env);
        for(auto* ref:{&hand_,&up_,&charge_,&environmentAnchor_,&b_.swapAction}) if(*ref) { env->DeleteGlobalRef(*ref); *ref=nullptr; }
        for(auto& ref:b_.slotConstants) if(ref) { env->DeleteGlobalRef(ref); ref=nullptr; }
        if(world_) env->DeleteWeakGlobalRef(world_); if(player_) env->DeleteWeakGlobalRef(player_); world_=player_=nullptr;
        if(signal_) env->DeleteGlobalRef(signal_); signal_=nullptr; channel_=nullptr; controller_.Reset(); tridentController_.Reset();
    }
    void Tick(JNIEnv* env,jobject client,bool checkForeground=true) noexcept {
        const std::scoped_lock lock(mutex_); if(!ready_||!channel_) return;
        items::Request request; if(!channel_->itemRequest.Read(request)) return;
        request.crystal.Clamp(); request.anchor.Clamp();
        const auto now=GetTickCount64(); Foreground(request,checkForeground);
        try {
            jvm::LocalFrame frame(env,192); if(!frame.ok()) throw std::runtime_error("local frame");
            if(now>=retry_&&(!totemReady_||!macroReady_||!anchorReady_||!tridentReady_)) { Bind(env); retry_=now+1000; }
            const bool armed=(request.crystal.enabled||request.anchor.enabled)&&items::Lease(request,now)&&!request.combatActive&&macroReady_;
            env->SetStaticBooleanField(signal_,macroEnabled_,armed?JNI_TRUE:JNI_FALSE); Check(env);
            if(!request.totem.enabled&&!request.crystal.enabled&&!request.anchor.enabled&&!request.trident.enabled&&!controller_.HasRestore()) {
                controller_.Reset(); tridentController_.Reset(); ClearSite(env);
                auto report=controller_.GetReport(); report.sampledAtMs=now; report.totem=report.crystal=report.anchor=items::Status::Off;
                report.trident=items::Status::Off; report.tridentActions=tridentActions_;
                channel_->itemReport.Publish(report); return;
            }
            items::Frame f; jobject level=nullptr,player=nullptr,inventory=nullptr,manager=nullptr,target=nullptr;
            Gate(env,client,f,level,player);
            if(f.world&&totemReady_) {
                Menu(env,player,f);
                auto offhand=env->CallObjectMethod(player,b_.mGetEquipped,b_.slotConstants[kSlotOffHand]); Check(env); f.offhand=Item(env,offhand);
                const bool macroActive=site_.kind!=items::Item::Empty;
                const bool needTotem=request.totem.ShouldEquip(f.health)&&f.offhand!=items::Item::Totem&&items::Lease(request,now)&&!f.paused&&f.ownMenu&&f.cursorEmpty;
                if(needTotem||macroActive||controller_.HasRestore()) {
                    inventory=env->GetObjectField(player,b_.fInventory); Check(env);
                    if(!inventory) f.ownMenu=false;
                    else {
                        if(needTotem||macroActive) {
                            const int size=env->CallIntMethod(inventory,b_.mInvSize); Check(env);
                            const int count=std::clamp(size,0,needTotem?36:9);
                            for(int i=0;i<count;++i) { jvm::LocalFrame slot(env,8); auto stack=env->CallObjectMethod(inventory,b_.mInvGetStack,i); Check(env); f.inventory[i]=Item(env,stack); }
                        }
                        if(selected_) { f.selected=env->GetIntField(inventory,selected_); Check(env); }
                    }
                }
                manager=env->GetObjectField(client,b_.fInteractionMgr); Check(env);
                if(macroActive&&macroReady_&&manager&&f.selected==site_.selected&&f.useDown&&!f.screen&&!f.paused&&items::Lease(request,now)) target=Observe(env,client,level,player,manager,f,site_.kind==items::Item::Anchor?request.anchor:request.crystal);
                else f.cycle=site_.kind;
            }
            const auto action=controller_.Tick(request,f,now);
            bool success=true;
            if(action.operation!=items::Operation::None) {
                if(!inventory||!manager) throw std::runtime_error("inventory / interaction manager unavailable");
                if(action.operation==items::Operation::SwapTotem) {
                    auto menu=env->GetObjectField(player,b_.fScreenHandler); Check(env); if(!menu) throw std::runtime_error("inventory menu unavailable");
                    const auto id=env->GetIntField(menu,b_.fSyncId); Check(env);
                    env->CallVoidMethod(manager,b_.mClickSlot,id,items::ContainerSlot(action.slot),40,b_.swapAction,player); Check(env);
                } else {
                    jobject currentHit=nullptr;
                    const auto& config=action.anchor?request.anchor:request.crystal;
                    if(config.requireCrosshairHit&&action.operation!=items::Operation::RestoreSlot) {
                        // Nur reale Minecraft-Treffer verwenden: exakte Blockseite
                        // und Trefferposition statt der synthetischen Oberseite.
                        if(action.operation==items::Operation::AttackCrystal) {
                            auto aimed=b_.fTargetedEntity?env->GetObjectField(client,b_.fTargetedEntity):nullptr; Check(env);
                            if(!aimed||!target||!env->IsSameObject(aimed,target)) throw std::runtime_error("crosshair crystal changed before action");
                        } else {
                            currentHit=env->GetObjectField(client,hit_); Check(env);
                            if(!BlockHit(env,currentHit)) throw std::runtime_error("crosshair block changed before action");
                            auto position=env->CallObjectMethod(currentHit,blockPos_); Check(env);
                            int xyz[3]; Coordinates(env,position,xyz);
                            const bool placing=action.operation==items::Operation::PlaceCrystal||action.operation==items::Operation::PlaceAnchor;
                            if(!std::equal(xyz,xyz+3,placing?site_.clicked:site_.cell)) throw std::runtime_error("crosshair position changed before action");
                        }
                    }
                    env->SetIntField(inventory,selected_,action.slot); Check(env); site_.selected=action.slot;
                    if(action.operation==items::Operation::AttackCrystal) {
                        success=target!=nullptr;
                        if(success) { env->CallVoidMethod(manager,attack_,player,target); Check(env); env->CallVoidMethod(player,swing_,hand_); Check(env); }
                    } else if(action.operation!=items::Operation::RestoreSlot) {
                        const bool placing=action.operation==items::Operation::PlaceCrystal||action.operation==items::Operation::PlaceAnchor;
                        auto result=env->CallObjectMethod(manager,useOn_,player,hand_,currentHit?currentHit:placing?site_.placement:site_.interaction); Check(env);
                        success=result&&env->CallBooleanMethod(result,consumes_)==JNI_TRUE; Check(env);
                        if(success) { env->CallVoidMethod(player,swing_,hand_); Check(env); }
                    }
                }
                controller_.Complete(action,success);
            }
            if(!controller_.Active()&&action.operation!=items::Operation::SwapTotem) ClearSite(env);
            auto report=controller_.GetReport();
            Trident(env,client,player,request,f,action.operation!=items::Operation::None||controller_.Active()||controller_.HasRestore(),now,report);
            channel_->itemReport.Publish(report);
        } catch(const std::exception& error) {
            if(env->ExceptionCheck()) env->ExceptionClear(); ClearSite(env); controller_.Reset(); tridentController_.Reset();
            if(now-lastError_>1000) { MCOV_LOG_WARN("inventory client-tick action failed: %s",error.what()); lastError_=now; }
            auto report=controller_.GetReport(); report.sampledAtMs=now;
            if(request.totem.enabled) report.totem=items::Status::Error; if(request.crystal.enabled) report.crystal=items::Status::Error; if(request.anchor.enabled) report.anchor=items::Status::Error;
            report.trident=request.trident.enabled?items::Status::Error:items::Status::Off; report.tridentActions=tridentActions_;
            channel_->itemReport.Publish(report);
        }
    }
};
} // namespace mcov::agent
