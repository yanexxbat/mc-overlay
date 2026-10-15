#pragma once
#include "jvm_interface.hpp"
#include "../common/shared_state.hpp"
namespace mcov::agent {
class AntiVanishBridge {
    SharedBlock* block_{};jclass bridge_{};jmethodID update_{},disable_{};jfieldID view_{},status_{},names_{},ids_{},evidence_{},limited_{};std::uint64_t next_{};
    bool Error(JNIEnv* env){if(!env->ExceptionCheck())return false;env->ExceptionClear();return true;}
public:
    template<class Bindings>bool Install(JNIEnv* env,const Bindings& b,SharedBlock* block){
        block_=block;if(std::strcmp(b.profile->version,"1.21.11"))return false;jvm::LocalFrame scope(env,12);
        auto cls=env->FindClass("mcov/AntiVanishBridge");if(Error(env)||!cls)return false;bridge_=static_cast<jclass>(env->NewGlobalRef(cls));
        auto configure=env->GetStaticMethodID(cls,"configure","(Ljava/lang/Class;Ljava/lang/String;)Z");
        update_=env->GetStaticMethodID(cls,"update","(ZZFJ)V");disable_=env->GetStaticMethodID(cls,"disable","()V");view_=env->GetStaticFieldID(cls,"view","Lmcov/AntiVanishBridge$View;");
        auto view=env->FindClass("mcov/AntiVanishBridge$View");if(Error(env)||!configure||!update_||!disable_||!view_||!view)return false;
        status_=env->GetFieldID(view,"status","I");names_=env->GetFieldID(view,"names","[Ljava/lang/String;");ids_=env->GetFieldID(view,"ids","[J");evidence_=env->GetFieldID(view,"evidence","[I");limited_=env->GetFieldID(view,"limited","Z");
        if(Error(env)||!status_||!names_||!ids_||!evidence_||!limited_)return false;
        auto profile=env->NewStringUTF(b.set->id);bool ok=env->CallStaticBooleanMethod(cls,configure,b.client.minecraftClass,profile);return !Error(env)&&ok;
    }
    void Refresh(JNIEnv* env){
        if(!block_)return;auto now=GetTickCount64();if(now<next_)return;next_=now+100;jvm::LocalFrame scope(env,48);
        antivanish::Report report;report.sampledAtMs=now;
        if(!bridge_||!update_||!view_||!status_||!names_||!ids_||!evidence_||!limited_){block_->antiVanishReport.Publish(report);return;}
        antivanish::Request r;bool fresh=block_->antiVanishRequest.Read(r)&&r.sentAtMs&&now>=r.sentAtMs&&now-r.sentAtMs<=500&&antivanish::Valid(r.config);
        auto flags=block_->controlFlags.load(std::memory_order_acquire);bool on=fresh&&r.config.enabled&&!(flags&(kCtrlPauseSampling|kCtrlRequestUnload));
        env->CallStaticVoidMethod(bridge_,update_,jboolean(on),jboolean(r.config.invisible),jfloat(r.config.range),jlong(fresh?500-(now-r.sentAtMs):0));
        if(Error(env)){report.status=antivanish::Status::Failed;block_->antiVanishReport.Publish(report);return;}
        auto view=env->GetStaticObjectField(bridge_,view_);if(!view||Error(env)){report.status=antivanish::Status::Failed;block_->antiVanishReport.Publish(report);return;}
        int status=env->GetIntField(view,status_);report.status=status>=0&&status<=4?static_cast<antivanish::Status>(status):antivanish::Status::Failed;
        report.limited=env->GetBooleanField(view,limited_);auto names=static_cast<jobjectArray>(env->GetObjectField(view,names_));auto ids=static_cast<jlongArray>(env->GetObjectField(view,ids_));auto evidence=static_cast<jintArray>(env->GetObjectField(view,evidence_));
        if(names&&ids&&evidence&&!Error(env)){
            int count=std::min({int(env->GetArrayLength(names)),int(env->GetArrayLength(ids)/2),int(env->GetArrayLength(evidence)),antivanish::kRows});
            jlong values[antivanish::kRows*2]{};jint signals[antivanish::kRows]{};env->GetLongArrayRegion(ids,0,count*2,values);env->GetIntArrayRegion(evidence,0,count,signals);
            for(int i=0;i<count&&!Error(env);++i){auto name=static_cast<jstring>(env->GetObjectArrayElement(names,i));auto text=jvm::JStringToUtf8(env,name);auto& e=report.rows[report.count++];std::snprintf(e.name,sizeof(e.name),"%s",text.c_str());e.id={std::uint64_t(values[i*2]),std::uint64_t(values[i*2+1])};e.evidence=signals[i];if(name)env->DeleteLocalRef(name);}
        }
        if(Error(env)){report.status=antivanish::Status::Failed;report.count=0;}block_->antiVanishReport.Publish(report);
    }
    void Close(JNIEnv* env){if(bridge_){if(disable_)env->CallStaticVoidMethod(bridge_,disable_);Error(env);env->DeleteGlobalRef(bridge_);}bridge_=nullptr;block_=nullptr;}
};
}
