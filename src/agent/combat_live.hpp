#pragma once
#include "../combat/combat_assist.hpp"
#include "../common/shared_state.hpp"
#include "jvm_interface.hpp"
#include "generated_profiles.hpp"
#include <windows.h>
#include <mutex>
#include <stdexcept>

namespace mcov::agent {
inline std::optional<double> RayBox(combat::Vec3 eye,combat::Angles rotation,const combat::Entity& e) {
    const auto d=combat::Direction(rotation);
    const double origins[]={eye.x,eye.y,eye.z},dirs[]={d.x,d.y,d.z};
    const double lows[]={e.boundsMin.x,e.boundsMin.y,e.boundsMin.z},highs[]={e.boundsMax.x,e.boundsMax.y,e.boundsMax.z};
    double entryDistance=0,exitDistance=std::numeric_limits<double>::infinity();
    for(int axis=0;axis<3;++axis) {
        if(std::abs(dirs[axis])<1e-10) { if(origins[axis]<lows[axis] || origins[axis]>highs[axis]) return {}; }
        else {
            const double a=(lows[axis]-origins[axis])/dirs[axis],b=(highs[axis]-origins[axis])/dirs[axis];
            entryDistance=std::max(entryDistance,std::min(a,b)); exitDistance=std::min(exitDistance,std::max(a,b));
            if(exitDistance<entryDistance) return {};
        }
    }
    return entryDistance;
}

// JNI ausschließlich im instrumentierten Client-Tick. Install/Close besitzen
// denselben Mutex und halten globale Referenzen bis zum letzten Callback gültig.
class MinecraftCombat final : public combat::Transport {
public:
    MinecraftCombat() : assist_(config_,relations_,*this,combat::ExecutionMode::Continuous) {}
    template<class B> bool Install(JNIEnv* env,const B& b,SharedBlock* channel,void* callback) {
        const std::scoped_lock lock(mutex_);
        if(ready_) return true;
        channel_=channel;
        if(!channel || !b.profile->geometry) return false;
        const auto& g=*b.profile->geometry;
        auto method=[&](jclass c,mappings::Member m) { return m.name && *m.name ? jvm::TryMethod(env,c,m.name,m.signature,false) : nullptr; };
        auto cls=[&](const char* name) {
            jvm::Error error;
            const std::string key=std::string(b.set->id)+".combat."+name;
            return jvm::Runtime::Instance().ResolveClass(env,key.c_str(),{name},&error);
        };
        entityClass_=b.clsEntity; livingClass_=b.clsLiving; playerClass_=b.clsPlayer;
        playerField_=b.fPlayer; levelField_=b.fLevel; screenField_=b.fCurrentScreen;
        guiField_=b.fGui; modeField_=b.fInteractionMgr;
        x_=b.mGetX; y_=b.mGetY; z_=b.mGetZ; id_=b.mGetId; name_=b.mGetName;
        health_=b.mHealth; uuid_=b.mEntityUuid; yaw_=b.mEntityYaw; pitch_=b.geometry.headPitch;
        entities_=b.mEntities; iterator_=b.mIterator; hasNext_=b.mHasNext; next_=b.mNext; text_=b.mTextToString;
        bounds_=b.geometry.getBounds;
        for(int i=0;i<3;++i) { minimum_[i]=b.geometry.min[i]; maximum_[i]=b.geometry.max[i]; }
        vecFields_[0]=b.fVecX; vecFields_[1]=b.fVecY; vecFields_[2]=b.fVecZ;
        crosshair_=b.fTargetedEntity;
        yawOld_=jvm::TryField(env,entityClass_,g.yawOld.name,g.yawOld.signature,false);
        pitchOld_=b.geometry.headPitchOld;
        bodyYaw_=b.geometry.bodyYaw; bodyYawOld_=b.geometry.bodyYawOld;
        headYaw_=b.geometry.headYaw; headYawOld_=b.geometry.headYawOld;
        paused_=method(b.client.minecraftClass,g.clientPaused);
        setYaw_=method(entityClass_,g.setYaw); setPitch_=method(entityClass_,g.setPitch);
        eye_=method(entityClass_,g.eyePosition); see_=method(livingClass_,g.lineOfSight);
        cooldown_=method(playerClass_,g.attackCooldown); reach_=method(playerClass_,g.attackReach);
        farReach_=method(b.clsInteractionMgr,g.farReach); attack_=method(b.clsInteractionMgr,g.attackEntity);
        swing_=method(livingClass_,g.swingHand); pick_=method(entityClass_,g.pickBlocks);
        alive_=method(entityClass_,g.entityAlive); location_=method(cls(g.hitClass),g.hitLocation);
        auto hand=cls(g.handClass);
        auto main=jvm::TryField(env,hand,g.mainHand.name,g.mainHand.signature,true);
        if(main) {
            auto value=env->GetStaticObjectField(hand,main);
            if(!env->ExceptionCheck() && value) hand_=env->NewGlobalRef(value);
            if(value) env->DeleteLocalRef(value);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        auto uuidClass=env->FindClass("java/util/UUID");
        if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        uuidHigh_=jvm::TryMethod(env,uuidClass,"getMostSignificantBits","()J",false);
        uuidLow_=jvm::TryMethod(env,uuidClass,"getLeastSignificantBits","()J",false);
        env->DeleteLocalRef(uuidClass);
        const auto tick=method(b.client.minecraftClass,g.clientTick);
        if(!tick || !paused_ || !screenField_ || !modeField_ || !setYaw_ || !setPitch_ || !eye_ || !see_ || !cooldown_ ||
           (!reach_ && !farReach_) || !attack_ || !swing_ || !pick_ || !location_ || !alive_ || !hand_ || !uuid_ || !uuidHigh_ || !uuidLow_ ||
           !bounds_ || !bodyYaw_ || !bodyYawOld_ || !headYaw_ || !headYawOld_ ||
           !minimum_[0] || !minimum_[1] || !minimum_[2] || !maximum_[0] || !maximum_[1] || !maximum_[2]) return false;
        auto signal=env->FindClass("mcov/FrameSignal");
        if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if(!signal) return false;
        signal_=static_cast<jclass>(env->NewGlobalRef(signal)); env->DeleteLocalRef(signal);
        if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        enabled_=jvm::TryField(env,signal_,"combatEnabled","Z",true);
        if(!enabled_) return false;
        silentPlayer_=jvm::TryField(env,signal_,"silentPlayer","Ljava/lang/Object;",true);
        viewYaw_=jvm::TryField(env,signal_,"viewYaw","F",true); viewPitch_=jvm::TryField(env,signal_,"viewPitch","F",true);
        const auto cameraBits=jvm::TryField(env,signal_,"cameraProfiles","J",true),turnBits=jvm::TryField(env,signal_,"turnProfiles","J",true);
        if(silentPlayer_ && viewYaw_ && viewPitch_ && yawOld_ && pitchOld_ && cameraBits && turnBits) {
            const auto cameras=env->GetStaticLongField(signal_,cameraBits);
            if(env->ExceptionCheck()) env->ExceptionClear();
            else {
                const auto turns=env->GetStaticLongField(signal_,turnBits);
                if(env->ExceptionCheck()) env->ExceptionClear();
                else for(std::size_t i=0;i<std::size(mappings::kRuntimeProfiles);++i) if(std::strcmp(b.set->id,mappings::kRuntimeProfiles[i].set->id)==0)
                    silentSupported_=(static_cast<std::uint64_t>(cameras&turns)&(std::uint64_t{1}<<i))!=0;
            }
        }
        JNINativeMethod native{const_cast<char*>("nativeCombatTick"),const_cast<char*>("(Ljava/lang/Object;)V"),callback};
        const auto registered=env->RegisterNatives(signal_,&native,1);
        if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if(registered!=JNI_OK) return false;
        ready_=true;
        env->SetStaticBooleanField(signal_,enabled_,JNI_TRUE);
        if(env->ExceptionCheck()) { env->ExceptionClear(); ready_=false; return false; }
        Report(CombatBridgeStatus::Ready);
        return true;
    }
    void Close(JNIEnv* env) {
        const std::scoped_lock lock(mutex_);
        ready_=false;
        if(signal_ && enabled_) env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE);
        if(env->ExceptionCheck()) env->ExceptionClear();
        // Close kann auf dem Sampler/VM-Shutdown laufen: hier keine Entity schreiben.
        if(signal_ && silentPlayer_) env->SetStaticObjectField(signal_,silentPlayer_,nullptr);
        if(env->ExceptionCheck()) env->ExceptionClear();
        cameraDetached_=false;
        // Der native Stub bleibt registriert und läuft nach Close als No-op.
        // So gibt es kein Rennen mit einem bereits begonnenen Java-Aufruf.
        if(world_) { env->DeleteWeakGlobalRef(world_); world_=nullptr; }
        if(playerIdentity_) { env->DeleteWeakGlobalRef(playerIdentity_); playerIdentity_=nullptr; }
        if(hand_) { env->DeleteGlobalRef(hand_); hand_=nullptr; }
        if(signal_) env->DeleteGlobalRef(signal_);
        signal_=nullptr; channel_=nullptr;
    }
    bool Ready() const { const std::scoped_lock lock(mutex_); return ready_; }
    void Tick(JNIEnv* env,jobject client,bool checkForeground=true) noexcept {
        const std::scoped_lock lock(mutex_);
        if(!ready_ || !channel_) return;
        env_=env;
        struct ContextExit { MinecraftCombat& self; ~ContextExit() { self.env_=nullptr; self.player_=self.mode_=nullptr; } } contextExit{*this};
        try {
            jvm::LocalFrame locals(env,1024);
            if(!locals.ok()) throw std::runtime_error("local frame");
            const auto now=GetTickCount64(); ++tick_;
            CombatRequest request;
            const bool read=channel_->combatRequest.Read(request);
            auto spin=read?request.spin:combat::SpinConfig{};
            if(!combat::Valid(spin)) spin={};
            spinRequested_=spin.enabled;
            spinStatus_=spin.enabled?combat::SpinStatus::Paused:combat::SpinStatus::Disabled;
            if(read && request.sentAtMs && request.revision!=revision_) {
                const auto next=combat::MakeKillAuraConfig(request.config);
                if(!config_.Publish(next)) throw std::runtime_error("combat configuration");
                revision_=request.revision; failed_=false;
            }
            if(read) {
                for(const auto& entry:relations_.Read()) if(entry.used) relations_.Set(entry.uuid,combat::Relation::Neutral);
                for(const auto& entry:request.relations) if(entry.used) relations_.Set(entry.uuid,entry.relation);
            }
            frame_={}; frame_.now=frame_.sampledAt=static_cast<double>(now)/1000;
            frame_.dt=previousTime_ ? std::clamp(frame_.now-previousTime_,0.0,0.2) : 0.05;
            previousTime_=frame_.now; frame_.tick=tick_; frame_.world=worldSerial_;
            const auto cfg=config_.Read().values;
            if(failed_) { EndSilent(); Report(CombatBridgeStatus::Error); return; }
            const auto flags=channel_->controlFlags.load(std::memory_order_acquire);
            if(!read || (!cfg.enabled&&!spin.enabled) || !CombatLeaseValid(request,now) || (flags&(kCtrlRequestUnload|kCtrlPauseSampling))) {
                EndSilent();
                report_.debug=assist_.Step(frame_);
                Report(cfg.enabled ? CombatBridgeStatus::Paused : CombatBridgeStatus::Disabled); return;
            }
            DWORD pid=0; GetWindowThreadProcessId(GetForegroundWindow(),&pid);
            if((checkForeground && pid!=GetCurrentProcessId()) || Boolean(client,paused_)) { Pause(); return; }
            auto gui=guiField_ ? Object(client,guiField_) : client;
            if(!gui || Object(gui,screenField_)) { Pause(); return; }
            player_=Object(client,playerField_); auto level=Object(client,levelField_);
            mode_=Object(client,modeField_);
            if(!player_ || !level || !mode_ || !Boolean(player_,alive_)) { Pause(CombatBridgeStatus::NoWorld); return; }
            if(!world_ || !env->IsSameObject(world_,level) || !playerIdentity_ || !env->IsSameObject(playerIdentity_,player_)) {
                EndSilent(false);
                if(world_) env->DeleteWeakGlobalRef(world_);
                if(playerIdentity_) env->DeleteWeakGlobalRef(playerIdentity_);
                world_=env->NewWeakGlobalRef(level); Check(); ++worldSerial_;
                playerIdentity_=env->NewWeakGlobalRef(player_); Check();
            }
            frame_.world=worldSerial_; frame_.playerPosition=Position(player_); frame_.eye=Vector(CallObject(player_,eye_));
            frame_.cameraRotation=frame_.observedRotation=Rotation(player_);
            frame_.silentSupported=silentSupported_;
            if(silentSupported_) {
                if(!cameraDetached_) BeginSilent(frame_.cameraRotation);
                frame_.cameraRotation=ViewRotation();
            }
            frame_.verticalFovDeg=request.verticalFov; frame_.viewportHeight=request.viewportHeight;
            frame_.attackCooldown=Float(player_,cooldown_,0.0f);
            frame_.gameAttackReach=reach_ ? Double(player_,reach_) : (Boolean(mode_,farReach_) ? 6.0 : 3.0);
            frame_.inputAllowed=true;
            if(crosshair_) if(auto target=Object(client,crosshair_)) frame_.crosshairId=Id(target);
            count_=0;
            // Der Spinbot braucht keine Entity-Suche und erzeugt keine Angriffe.
            if(cfg.enabled) {
            auto iterable=CallObject(level,entities_);
            if(!iterable) { Pause(CombatBridgeStatus::NoWorld); return; }
            auto iter=CallObject(iterable,iterator_);
            if(!iter) { Pause(CombatBridgeStatus::NoWorld); return; }
            // Begrenzte JNI-Arbeit nur für die nahe Umgebung. Keine Render-Thread-Abfragen.
            std::size_t visited=0;
            while(count_<entitiesData_.size() && visited++<4096 && Boolean(iter,hasNext_)) {
                auto object=CallObject(iter,next_);
                if(!object) continue;
                if(env->IsSameObject(object,player_) || !Boolean(object,alive_)) { env->DeleteLocalRef(object); continue; }
                const auto position=Position(object);
                if(combat::Length(combat::Difference(position,frame_.playerPosition))>cfg.target.range+4) { env->DeleteLocalRef(object); continue; }
                auto box=CallObject(object,bounds_);
                if(!box) { env->DeleteLocalRef(object); continue; }
                combat::Entity e; e.id=Id(object); e.uuid=Identity(object);
                e.boundsMin=Bounds(box,minimum_); e.boundsMax=Bounds(box,maximum_); env->DeleteLocalRef(box);
                e.player=env->IsInstanceOf(object,playerClass_)==JNI_TRUE;
                e.living=env->IsInstanceOf(object,livingClass_)==JNI_TRUE;
                if(e.living) e.health=Float(object,health_);
                e.visibility=combat::Visibility{Boolean(player_,see_,object),frame_.now};
                auto component=CallObject(object,name_);
                if(component) {
                    auto name=static_cast<jstring>(CallObject(component,text_));
                    if(name) {
                        const char* chars=env->GetStringUTFChars(name,nullptr); Check();
                        if(chars) {
                            std::size_t length=0; while(length+1<e.name.size() && chars[length]) { e.name[length]=chars[length]; ++length; }
                            env->ReleaseStringUTFChars(name,chars);
                        }
                        env->DeleteLocalRef(name);
                    }
                    env->DeleteLocalRef(component);
                }
                refs_[count_]=object; entitiesData_[count_++]=e;
            }
            }
            frame_.entities={entitiesData_.data(),count_};
            if(cfg.enabled) frame_.attackHitId=RayTarget(frame_.observedRotation,std::min(cfg.attack.range,*frame_.gameAttackReach));
            report_.debug=assist_.Step(frame_);
            bool spinning=false;
            if(spin.enabled) {
                if(!silentSupported_) { spinbot_.Reset(); spinStatus_=combat::SpinStatus::SilentUnavailable; }
                else if(report_.debug.target) { spinbot_.Reset(); spinStatus_=combat::SpinStatus::CombatPriority; }
                else if(auto pose=spinbot_.Step(spin,Rotation(player_),frame_.cameraRotation,frame_.dt)) {
                    ApplyRotation(*pose); spinning=true; spinStatus_=combat::SpinStatus::Active;
                    report_.debug.current=Rotation(player_);
                }
            } else spinbot_.Reset();
            if(!report_.debug.target&&!spinning) EndSilent();
            else if(report_.debug.target&&cameraDetached_) {
                const auto applied=Rotation(player_);
                SyncModelRotation(player_,applied,frame_.observedRotation);
                report_.debug.current=applied;
                report_.debug.error=combat::AngularDistance(applied,report_.debug.target->desired);
            }
            Report(failed_ ? CombatBridgeStatus::Error : report_.debug.silentUnavailable ? CombatBridgeStatus::SilentUnavailable : report_.debug.target ? CombatBridgeStatus::Active : cfg.enabled?CombatBridgeStatus::NoTarget:CombatBridgeStatus::Disabled);
        } catch(...) {
            if(env->ExceptionCheck()) env->ExceptionClear();
            failed_=true; frame_.inputAllowed=false;
            try { EndSilent(); } catch(...) {}
            try { report_.debug=assist_.Step(frame_); } catch(...) {}
            Report(CombatBridgeStatus::Error);
        }
    }
    void RefreshAfterPose(combat::Frame& actual) override {
        // Auf demselben Client-Tick; ImGui liest weiterhin nur die IPC-Diagnose.
        if(!env_ || !player_ || actual.world!=frame_.world) return;
        const auto cfg=config_.Read().values;
        actual.observedRotation=Rotation(player_);
        actual.attackCooldown=Float(player_,cooldown_,0.0f);
        actual.attackHitId=RayTarget(actual.observedRotation,std::min(cfg.attack.range,*actual.gameAttackReach));
    }
    bool Send(const combat::Command& command) override {
        // Synchroner Abschluss auf dem Client-Thread, kein voreiliges Queue-Ack.
        if(!env_ || !player_ || command.world!=frame_.world || !silentSupported_ || !cameraDetached_) return false;
        std::size_t index=0;
        while(index<count_ && (entitiesData_[index].id!=command.target || entitiesData_[index].uuid!=command.identity)) ++index;
        if(index==count_) return false;
        auto entity=refs_[index];
        if(!Boolean(entity,alive_) || Identity(entity)!=command.identity) return false;
        if(command.kind==combat::CommandKind::Pose) {
            ApplyRotation(command.pose.rotation);
            return true;
        }
        const auto cfg=config_.Read().values;
        auto actual=frame_; actual.observedRotation=Rotation(player_);
        actual.attackCooldown=Float(player_,cooldown_,0.0f);
        actual.attackHitId=RayTarget(actual.observedRotation,std::min(cfg.attack.range,*actual.gameAttackReach));
        auto target=combat::Evaluate(entitiesData_[index],actual,cfg.target,relations_.Read());
        if(!target || !combat::CanAttack(actual,*target,cfg) || !Boolean(player_,see_,entity)) return false;
        env_->CallVoidMethod(mode_,attack_,player_,entity); Check();
        ++report_.attacks;
        env_->CallVoidMethod(player_,swing_,hand_); Check();
        return true;
    }
private:
    void ApplyRotation(combat::Angles pose) {
        const auto previous=Rotation(player_);
        const float yaw=Float(player_,yaw_);
        env_->CallVoidMethod(player_,setYaw_,static_cast<jfloat>(yaw+combat::Wrap(pose.yaw-yaw))); Check();
        env_->CallVoidMethod(player_,setPitch_,static_cast<jfloat>(pose.pitch)); Check();
        SyncModelRotation(player_,Rotation(player_),previous);
    }
    void SyncModelRotation(jobject player,combat::Angles applied,combat::Angles previous) {
        const auto yaw=static_cast<jfloat>(applied.yaw);
        const auto oldYaw=static_cast<jfloat>(applied.yaw+combat::Wrap(previous.yaw-applied.yaw));
        // Alle Modellwinkel gehören zur tatsächlich angewendeten Spielerrotation.
        // Die unabhängigen Kamera-Werte in FrameSignal werden hier nie verändert.
        env_->SetFloatField(player,bodyYaw_,yaw); Check();
        env_->SetFloatField(player,headYaw_,yaw); Check();
        env_->SetFloatField(player,bodyYawOld_,oldYaw); Check();
        env_->SetFloatField(player,headYawOld_,oldYaw); Check();
        env_->SetFloatField(player,yawOld_,oldYaw); Check();
        env_->SetFloatField(player,pitchOld_,static_cast<jfloat>(previous.pitch)); Check();
    }
    void BeginSilent(combat::Angles camera) {
        env_->SetStaticFloatField(signal_,viewYaw_,static_cast<jfloat>(camera.yaw)); Check();
        env_->SetStaticFloatField(signal_,viewPitch_,static_cast<jfloat>(camera.pitch)); Check();
        // Erst nach den Winkeln publizieren. Nur die Camera-Getter nutzen diesen Wert;
        // Entity-Getter, Modell und Vanilla-Movement sehen weiter die Combat-Rotation.
        env_->SetStaticObjectField(signal_,silentPlayer_,player_); Check();
        cameraDetached_=true;
    }
    combat::Angles ViewRotation() {
        const auto yaw=env_->GetStaticFloatField(signal_,viewYaw_); Check();
        const auto pitch=env_->GetStaticFloatField(signal_,viewPitch_); Check();
        return {yaw,pitch};
    }
    void EndSilent(bool restore=true) {
        spinbot_.Reset();
        if(!cameraDetached_) return;
        auto player=env_->GetStaticObjectField(signal_,silentPlayer_); Check();
        if(player && restore) {
            const auto view=ViewRotation();
            const float current=Float(player,yaw_);
            const auto restored=static_cast<jfloat>(current+combat::Wrap(view.yaw-current));
            env_->CallVoidMethod(player,setYaw_,restored); Check();
            env_->CallVoidMethod(player,setPitch_,static_cast<jfloat>(view.pitch)); Check();
            // Keine Interpolation von der letzten Angriffsrichtung zur Kamera nach Pause/Abschalten.
            env_->SetFloatField(player,yawOld_,restored); Check();
            env_->SetFloatField(player,pitchOld_,static_cast<jfloat>(view.pitch)); Check();
            SyncModelRotation(player,{restored,view.pitch},{restored,view.pitch});
        }
        env_->SetStaticObjectField(signal_,silentPlayer_,nullptr); Check();
        cameraDetached_=false;
        if(player) env_->DeleteLocalRef(player);
    }
    void Check() {
        if(env_->ExceptionCheck()) {
            env_->ExceptionClear(); failed_=true; frame_.inputAllowed=false;
            throw std::runtime_error("Minecraft JNI call");
        }
    }
    jobject Object(jobject o,jfieldID f) { auto v=env_->GetObjectField(o,f); Check(); return v; }
    template<class... A> jobject CallObject(jobject o,jmethodID m,A... a) { auto v=env_->CallObjectMethod(o,m,a...); Check(); return v; }
    template<class... A> bool Boolean(jobject o,jmethodID m,A... a) { const auto v=env_->CallBooleanMethod(o,m,a...); Check(); return v==JNI_TRUE; }
    template<class... A> float Float(jobject o,jmethodID m,A... a) { const auto v=env_->CallFloatMethod(o,m,a...); Check(); return v; }
    double Double(jobject o,jmethodID m) { const auto v=env_->CallDoubleMethod(o,m); Check(); return v; }
    std::uint32_t Id(jobject o) { const auto v=env_->CallIntMethod(o,id_); Check(); return static_cast<std::uint32_t>(v); }
    combat::Vec3 Position(jobject o) { return {Double(o,x_),Double(o,y_),Double(o,z_)}; }
    combat::Vec3 Vector(jobject o) { if(!o) throw std::runtime_error("missing Vec3"); return Bounds(o,vecFields_); }
    combat::Vec3 Bounds(jobject o,const jfieldID* fields) {
        double values[3]; for(int i=0;i<3;++i) { values[i]=env_->GetDoubleField(o,fields[i]); Check(); }
        return {values[0],values[1],values[2]};
    }
    combat::Angles Rotation(jobject player) { return {Float(player,yaw_),Float(player,pitch_)}; }
    combat::Identity Identity(jobject entity) {
        auto uuid=CallObject(entity,uuid_); if(!uuid) throw std::runtime_error("missing UUID");
        const auto high=env_->CallLongMethod(uuid,uuidHigh_); Check();
        const auto low=env_->CallLongMethod(uuid,uuidLow_); Check(); env_->DeleteLocalRef(uuid);
        return {static_cast<std::uint64_t>(high),static_cast<std::uint64_t>(low)};
    }
    std::uint32_t RayTarget(combat::Angles rotation,double reach) {
        // Entity.pick testet Blöcke entlang der tatsächlichen Spielerrotation.
        // Aufruf nach Pose-Ausgabe prüft auch die zuletzt geänderte Richtung.
        auto hit=CallObject(player_,pick_,static_cast<jdouble>(reach),static_cast<jfloat>(1),JNI_FALSE);
        if(!hit) return 0;
        auto point=CallObject(hit,location_);
        if(!point) { env_->DeleteLocalRef(hit); return 0; }
        double nearest=std::min(reach,combat::Length(combat::Difference(Vector(point),frame_.eye)));
        env_->DeleteLocalRef(point); env_->DeleteLocalRef(hit);
        if(!std::isfinite(nearest)) return 0;
        std::uint32_t id=0;
        for(std::size_t i=0;i<count_;++i) if(auto distance=RayBox(frame_.eye,rotation,entitiesData_[i]); distance && *distance<nearest) {
            nearest=*distance; id=entitiesData_[i].id;
        }
        return id;
    }
    void Pause(CombatBridgeStatus s=CombatBridgeStatus::Paused) {
        EndSilent();
        frame_.inputAllowed=false; report_.debug=assist_.Step(frame_); Report(s);
    }
    void Report(CombatBridgeStatus status) {
        if(spinRequested_) {
            if(status==CombatBridgeStatus::Error) spinStatus_=combat::SpinStatus::Error;
            else if(status==CombatBridgeStatus::NoWorld) spinStatus_=combat::SpinStatus::NoWorld;
        }
        report_.spinStatus=spinStatus_;
        report_.sampledAtMs=GetTickCount64(); report_.tick=tick_; report_.status=status;
        if(channel_) channel_->combatReport.Publish(report_);
    }
    mutable std::mutex mutex_;
    combat::ConfigStore config_;
    combat::FriendEnemyBook relations_;
    combat::CombatAssist assist_;
    combat::Spinbot spinbot_;
    combat::SpinStatus spinStatus_{};
    bool spinRequested_{};
    SharedBlock* channel_{};
    CombatReport report_{};
    std::array<combat::Entity,kMaxEntities> entitiesData_{};
    std::array<jobject,kMaxEntities> refs_{};
    std::size_t count_{};
    combat::Frame frame_{};
    JNIEnv* env_{};
    jobject player_{},mode_{},world_{},hand_{};
    jweak playerIdentity_{};
    jclass signal_{},entityClass_{},livingClass_{},playerClass_{};
    jfieldID enabled_{},playerField_{},levelField_{},screenField_{},guiField_{},modeField_{},crosshair_{};
    jfieldID silentPlayer_{},viewYaw_{},viewPitch_{},yawOld_{},pitchOld_{};
    jfieldID bodyYaw_{},bodyYawOld_{},headYaw_{},headYawOld_{};
    jfieldID minimum_[3]{},maximum_[3]{},vecFields_[3]{};
    jmethodID x_{},y_{},z_{},id_{},name_{},health_{},uuid_{},uuidHigh_{},uuidLow_{},yaw_{},pitch_{};
    jmethodID entities_{},iterator_{},hasNext_{},next_{},text_{},bounds_{},paused_{},alive_{};
    jmethodID setYaw_{},setPitch_{},eye_{},see_{},cooldown_{},reach_{},farReach_{},attack_{},swing_{},pick_{},location_{};
    std::uint64_t revision_{~std::uint64_t{}},worldSerial_{},tick_{};
    double previousTime_{};
    bool ready_{},failed_{};
    bool silentSupported_{},cameraDetached_{};
};
} // namespace mcov::agent
