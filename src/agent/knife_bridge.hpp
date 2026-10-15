#pragma once
#include "jvm_interface.hpp"
#include "../common/shared_state.hpp"

namespace mcov::agent {
class KnifeBridge {
    SharedBlock* world_{};
    jclass bridge_{};
    jmethodID update_{},status_{},disable_{};
    std::uint64_t next_{};
    bool Error(JNIEnv* env) { if(!env->ExceptionCheck()) return false; env->ExceptionClear(); return true; }
public:
    template<class Bindings> bool Install(JNIEnv* env,const Bindings& b,SharedBlock* world) {
        world_=world;
        if(std::strcmp(b.profile->version,"1.21.11")) return false;
        jvm::LocalFrame frame(env,8);
        auto local=env->FindClass("mcov/KnifeBridge"); if(Error(env)||!local) return false;
        bridge_=static_cast<jclass>(env->NewGlobalRef(local));
        const auto configure=env->GetStaticMethodID(bridge_,"configure","(Ljava/lang/Class;Ljava/lang/String;)Z");
        update_=env->GetStaticMethodID(bridge_,"update","(ZIFFFZZZJJ)V");
        status_=env->GetStaticMethodID(bridge_,"status","()I"); disable_=env->GetStaticMethodID(bridge_,"disable","()V");
        if(Error(env)||!configure||!update_||!status_||!disable_) return false;
        const auto profile=env->NewStringUTF(b.set->id);
        const bool configured=env->CallStaticBooleanMethod(bridge_,configure,b.client.minecraftClass,profile);
        if(Error(env)||!configured) return false;
        return true;
    }
    void Refresh(JNIEnv* env) {
        if(!world_||!bridge_||!update_||!status_) return;
        const auto now=GetTickCount64(); if(now<next_) return; next_=now+25;
        knife::Request r;
        bool fresh=world_->knifeRequest.Read(r)&&knife::Fresh(r,now);
        const auto flags=world_->controlFlags.load(std::memory_order_acquire);
        const bool enabled=fresh&&r.config.enabled&&!(flags&(kCtrlPauseSampling|kCtrlRequestUnload));
        const auto& c=r.config;
        env->CallStaticVoidMethod(bridge_,update_,jboolean(enabled),jint(c.skin),jfloat(c.speed),jfloat(c.size),jfloat(c.trailSeconds),
            jboolean(c.deploy),jboolean(c.trails),jboolean(enabled&&r.inspectHeld),jlong(r.inspectSerial),jlong(fresh?knife::kLeaseMs-(now-r.sentAtMs):0));
        knife::Report report; report.sampledAtMs=now;
        if(Error(env)) report.status=knife::Status::Failed;
        else {
            int status=env->CallStaticIntMethod(bridge_,status_);
            report.status=!Error(env)&&status>=0&&status<=4?static_cast<knife::Status>(status):knife::Status::Failed;
        }
        world_->knifeReport.Publish(report);
    }
    void Close(JNIEnv* env) {
        if(bridge_) { if(disable_) env->CallStaticVoidMethod(bridge_,disable_); Error(env); env->DeleteGlobalRef(bridge_); }
        bridge_=nullptr; world_=nullptr;
    }
};
} // namespace mcov::agent
