#pragma once
#include "../common/shared_state.hpp"
#include "jvm_interface.hpp"
#include <mutex>
#include <stdexcept>

namespace mcov::agent {
// Eigene Binding-Kopie: Client-Tick und Sampler schreiben niemals dieselben Handles.
template<class B> class InventoryAssist final {
    mutable std::mutex mutex_;
    B b_{}; SharedBlock* channel_=nullptr;
    bool ready_=false,totemReady_=false,macroReady_=false,anchorReady_=false;
    std::uint64_t retry_=0,lastError_=0;
    jclass signal_=nullptr,crystal_=nullptr,blockHit_=nullptr;
    jfieldID enabled_=nullptr,selected_=nullptr,ownMenu_=nullptr,hit_=nullptr;
    jmethodID carried_=nullptr,use_=nullptr,paused_=nullptr,type_=nullptr,blockPos_=nullptr,stateValue_=nullptr;
    jmethodID attack_=nullptr,swing_=nullptr,dim_=nullptr,anchorWorks_=nullptr,environment_=nullptr,environmentValue_=nullptr;
    jmethodID integer_=nullptr,boolean_=nullptr,enumName_=nullptr;
    jobject hand_=nullptr,charge_=nullptr,environmentAnchor_=nullptr;
    jweak world_=nullptr,player_=nullptr;
    items::Controller controller_;
    jclass Class(JNIEnv* env,const char* name) {
        if(!name||!*name) return nullptr;
        const std::string key=std::string(b_.set->id)+".inventory."+name;
        jvm::Error error; return jvm::Runtime::Instance().ResolveClass(env,key.c_str(),{name},&error);
    }
    jmethodID Method(JNIEnv* env,jclass c,mappings::Member m) {
        return m.name&&*m.name?jvm::TryMethod(env,c,m.name,m.signature,false):nullptr;
    }
    jfieldID Field(JNIEnv* env,jclass c,mappings::Member m) {
        return m.name&&*m.name?jvm::TryField(env,c,m.name,m.signature,false):nullptr;
    }
    void Constant(JNIEnv* env,jclass c,mappings::Member m,jobject& value) {
        if(value||!c||!m.name||!*m.name) return;
        const auto id=jvm::TryField(env,c,m.name,m.signature,true);
        if(id) { auto local=env->GetStaticObjectField(c,id); if(local) value=env->NewGlobalRef(local); }
        if(env->ExceptionCheck()) env->ExceptionClear();
    }
    void Bind(JNIEnv* env) {
        totemReady_=EnsureTotemBindings(env,b_);
        const auto& g=*b_.profile->geometry;
        selected_=Field(env,b_.clsPlayerInv,g.selectedSlot);
        ownMenu_=Field(env,b_.clsPlayer,g.inventoryMenu);
        const std::string menu=b_.set->fieldScreenHandler.signature;
        carried_=Method(env,Class(env,menu.substr(1,menu.size()-2).c_str()),g.carriedStack);
        use_=Method(env,b_.client.minecraftClass,g.useItem);
        paused_=Method(env,b_.client.minecraftClass,g.clientPaused);
        hit_=Field(env,b_.client.minecraftClass,g.hitResult);
        crystal_=Class(env,g.crystalClass); blockHit_=Class(env,g.blockHitClass);
        type_=Method(env,Class(env,g.hitClass),g.hitType);
        blockPos_=Method(env,blockHit_,g.blockHitPos);
        stateValue_=Method(env,b_.clsBlockState,g.stateValue);
        attack_=Method(env,b_.clsInteractionMgr,g.attackEntity); swing_=Method(env,b_.clsLiving,g.swingHand);
        Constant(env,Class(env,g.handClass),g.mainHand,hand_);
        Constant(env,Class(env,g.anchorClass),g.anchorCharge,charge_);
        dim_=Method(env,b_.clsLevel,g.dimensionType); anchorWorks_=Method(env,Class(env,g.dimensionTypeClass),g.anchorWorks);
        environment_=Method(env,b_.clsLevel,g.environment);
        environmentValue_=Method(env,Class(env,g.environmentClass),g.environmentValue);
        Constant(env,Class(env,g.environmentAttrsClass),g.environmentAnchor,environmentAnchor_);
        auto integer=env->FindClass("java/lang/Integer"),boolean=env->FindClass("java/lang/Boolean"),enumeration=env->FindClass("java/lang/Enum");
        integer_=jvm::TryMethod(env,integer,"intValue","()I",false);
        boolean_=jvm::TryMethod(env,boolean,"booleanValue","()Z",false);
        enumName_=jvm::TryMethod(env,enumeration,"name","()Ljava/lang/String;",false);
        totemReady_=totemReady_&&ownMenu_&&carried_&&b_.mItemIsEmpty&&b_.mGetEquipped&&b_.slotConstants[kSlotOffHand]&&paused_;
        macroReady_=totemReady_&&selected_&&use_&&hit_&&type_&&blockHit_&&crystal_&&blockPos_&&enumName_&&attack_&&swing_&&hand_;
        anchorReady_=macroReady_&&charge_&&integer_&&stateValue_&&((dim_&&anchorWorks_)||(environment_&&environmentValue_&&environmentAnchor_&&boolean_));
    }
    static void Check(JNIEnv* env) {
        if(env->ExceptionCheck()) { env->ExceptionClear(); throw std::runtime_error("JNI inventory operation"); }
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
public:
    bool Install(JNIEnv* env,const B& bindings,SharedBlock* channel,void* callback) {
        const std::scoped_lock lock(mutex_);
        if(ready_) return true;
        if(!channel||!bindings.profile->geometry) return false;
        b_=bindings; channel_=channel;
        b_.swapAction=bindings.swapAction?env->NewGlobalRef(bindings.swapAction):nullptr;
        jvm::LocalFrame frame(env,64); Bind(env);
        auto signal=env->FindClass("mcov/FrameSignal");
        if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if(!signal) return false;
        signal_=static_cast<jclass>(env->NewGlobalRef(signal));
        enabled_=jvm::TryField(env,signal_,"combatEnabled","Z",true);
        JNINativeMethod native{const_cast<char*>("nativeCombatTick"),const_cast<char*>("(Ljava/lang/Object;)V"),callback};
        if(!enabled_||env->RegisterNatives(signal_,&native,1)!=JNI_OK) { if(env->ExceptionCheck()) env->ExceptionClear(); return false; }
        ready_=true; env->SetStaticBooleanField(signal_,enabled_,JNI_TRUE);
        if(env->ExceptionCheck()) { env->ExceptionClear(); ready_=false; }
        return ready_;
    }
    void Close(JNIEnv* env) {
        const std::scoped_lock lock(mutex_); ready_=false;
        if(signal_&&enabled_) env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE);
        if(env->ExceptionCheck()) env->ExceptionClear();
        for(auto* ref:{&hand_,&charge_,&environmentAnchor_,&b_.swapAction}) if(*ref) { env->DeleteGlobalRef(*ref); *ref=nullptr; }
        if(world_) env->DeleteWeakGlobalRef(world_); if(player_) env->DeleteWeakGlobalRef(player_);
        world_=player_=nullptr;
        if(signal_) env->DeleteGlobalRef(signal_); signal_=nullptr; channel_=nullptr; controller_.Reset();
    }
    void Tick(JNIEnv* env,jobject client,bool checkForeground=true) noexcept {
        const std::scoped_lock lock(mutex_);
        if(!ready_||!channel_) return;
        items::Request request; if(!channel_->itemRequest.Read(request)) return;
        const auto now=GetTickCount64();
        if(checkForeground) { DWORD pid=0; GetWindowThreadProcessId(GetForegroundWindow(),&pid); request.inputAllowed=request.inputAllowed&&pid==GetCurrentProcessId(); }
        try {
            jvm::LocalFrame frame(env,128); if(!frame.ok()) throw std::runtime_error("local frame");
            if(now>=retry_&&(!totemReady_||!macroReady_||!anchorReady_)) { Bind(env); retry_=now+1000; }
            items::Frame f;
            auto level=env->GetObjectField(client,b_.fLevel),player=env->GetObjectField(client,b_.fPlayer); Check(env);
            f.world=level&&player;
            if(f.world) {
                if(!world_||!player_||!env->IsSameObject(world_,level)||!env->IsSameObject(player_,player)) {
                    controller_.Reset();
                    if(world_) env->DeleteWeakGlobalRef(world_); if(player_) env->DeleteWeakGlobalRef(player_);
                    world_=env->NewWeakGlobalRef(level); player_=env->NewWeakGlobalRef(player);
                }
                f.alive=env->CallFloatMethod(player,b_.mHealth)>0; Check(env);
                f.totemSupported=totemReady_; f.macroSupported=macroReady_; f.anchorSupported=anchorReady_;
                f.paused=paused_?env->CallBooleanMethod(client,paused_)==JNI_TRUE:true; Check(env);
                jobject screenOwner=b_.fGui?env->GetObjectField(client,b_.fGui):client; Check(env);
                f.screen=!screenOwner||!b_.fCurrentScreen||env->GetObjectField(screenOwner,b_.fCurrentScreen)!=nullptr; Check(env);
                if(totemReady_) {
                    auto menu=env->GetObjectField(player,b_.fScreenHandler),own=env->GetObjectField(player,ownMenu_); Check(env);
                    f.ownMenu=menu&&own&&env->IsSameObject(menu,own);
                    auto carried=menu?env->CallObjectMethod(menu,carried_):nullptr; Check(env);
                    f.cursorEmpty=Item(env,carried)==items::Item::Empty;
                    auto inventory=env->GetObjectField(player,b_.fInventory); Check(env);
                    if(inventory) {
                        const int count=std::clamp(env->CallIntMethod(inventory,b_.mInvSize),0,36); Check(env);
                        for(int i=0;i<count;++i) {
                            jvm::LocalFrame slot(env,8);
                            auto stack=env->CallObjectMethod(inventory,b_.mInvGetStack,i); Check(env); f.inventory[i]=Item(env,stack);
                        }
                        if(selected_) { f.selected=env->GetIntField(inventory,selected_); Check(env); }
                    }
                    auto offhand=env->CallObjectMethod(player,b_.mGetEquipped,b_.slotConstants[kSlotOffHand]); Check(env);
                    f.offhand=Item(env,offhand);
                }
                if(macroReady_) {
                    auto target=b_.fTargetedEntity?env->GetObjectField(client,b_.fTargetedEntity):nullptr; Check(env);
                    if(target&&env->IsInstanceOf(target,crystal_)) f.target=items::Target::Crystal;
                    else {
                        auto hit=env->GetObjectField(client,hit_); Check(env);
                        if(hit&&env->IsInstanceOf(hit,blockHit_)) {
                            auto type=env->CallObjectMethod(hit,type_); Check(env);
                            auto name=type?static_cast<jstring>(env->CallObjectMethod(type,enumName_)):nullptr; Check(env);
                            if(jvm::JStringToUtf8(env,name)=="BLOCK") {
                                auto position=env->CallObjectMethod(hit,blockPos_); Check(env);
                                auto state=position?env->CallObjectMethod(level,b_.mGetBlockState,position):nullptr; Check(env);
                                auto block=state?env->CallObjectMethod(state,b_.mGetBlock):nullptr; Check(env);
                                auto key=block?static_cast<jstring>(env->CallObjectMethod(block,b_.mBlockTransKey)):nullptr; Check(env);
                                const auto text=jvm::JStringToUtf8(env,key);
                                if(text=="block.minecraft.obsidian"||text=="block.minecraft.bedrock") f.target=items::Target::CrystalBase;
                                else if(text=="block.minecraft.respawn_anchor") {
                                    f.target=items::Target::Anchor;
                                    if(anchorReady_) { auto value=env->CallObjectMethod(state,stateValue_,charge_); Check(env); f.charges=value?env->CallIntMethod(value,integer_):-1; Check(env); }
                                } else if(block) f.target=items::Target::OtherBlock;
                                if(anchorReady_) {
                                    if(dim_) { auto d=env->CallObjectMethod(level,dim_); Check(env); f.anchorWorks=d&&env->CallBooleanMethod(d,anchorWorks_)==JNI_TRUE; Check(env); }
                                    else { auto e=env->CallObjectMethod(level,environment_); Check(env); auto v=e&&position?env->CallObjectMethod(e,environmentValue_,environmentAnchor_,position):nullptr; Check(env); f.anchorWorks=v&&env->CallBooleanMethod(v,boolean_)==JNI_TRUE; Check(env); }
                                }
                            }
                        }
                    }
                }
            }
            const auto action=controller_.Tick(request,f,now);
            bool success=true;
            if(action.operation!=items::Operation::None) {
                auto inventory=env->GetObjectField(player,b_.fInventory); Check(env);
                auto manager=env->GetObjectField(client,b_.fInteractionMgr); Check(env);
                if(action.operation==items::Operation::SwapTotem) {
                    auto menu=env->GetObjectField(player,b_.fScreenHandler); Check(env);
                    const auto id=env->GetIntField(menu,b_.fSyncId); Check(env);
                    env->CallVoidMethod(manager,b_.mClickSlot,id,items::ContainerSlot(action.slot),40,b_.swapAction,player); Check(env);
                } else if(action.operation==items::Operation::RestoreSlot) {
                    if(selected_&&inventory) env->SetIntField(inventory,selected_,action.slot); Check(env);
                } else {
                    env->SetIntField(inventory,selected_,action.slot); Check(env);
                    if(action.operation==items::Operation::Use) { env->CallVoidMethod(client,use_); Check(env); }
                    else {
                        auto target=env->GetObjectField(client,b_.fTargetedEntity); Check(env);
                        success=target&&env->IsInstanceOf(target,crystal_);
                        if(success) { env->CallVoidMethod(manager,attack_,player,target); Check(env); env->CallVoidMethod(player,swing_,hand_); Check(env); }
                    }
                }
                controller_.Complete(action,success);
            }
            channel_->itemReport.Publish(controller_.GetReport());
        } catch(const std::exception& error) {
            if(env->ExceptionCheck()) env->ExceptionClear();
            if(now-lastError_>1000) { MCOV_LOG_WARN("inventory client-tick action failed: %s",error.what()); lastError_=now; }
            auto report=controller_.GetReport(); report.sampledAtMs=now;
            if(request.totem.enabled) report.totem=items::Status::Error;
            if(request.crystal.enabled) report.crystal=items::Status::Error;
            if(request.anchor.enabled) report.anchor=items::Status::Error;
            channel_->itemReport.Publish(report);
        }
    }
};
} // namespace mcov::agent
