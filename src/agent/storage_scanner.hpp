#pragma once
#include "storage_profiles.hpp"
#include "jvm_interface.hpp"
#include "../common/shared_state.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace mcov::agent {
// Walk the loaded chunk cache and block-entity keys, not the block volume.
// Only one chunk's key snapshot is retained at a time. Global references are
// explicitly released on completion, cancellation and world changes.
class StorageScanner {
    const StorageProfile* profile_=nullptr;
    jobject chunks_=nullptr;
    jobjectArray keys_=nullptr;
    int chunk_=0,chunksCount_=0,key_=0,keysCount_=0;
    jmethodID source_=nullptr,length_=nullptr,get_=nullptr,entities_=nullptr,keySet_=nullptr,toArray_=nullptr,x_=nullptr,y_=nullptr,z_=nullptr;
    jfieldID storage_=nullptr,array_=nullptr;
    double x0_=0,y0_=0,z0_=0;
    static bool Check(JNIEnv* env) { if(!env->ExceptionCheck()) return true; env->ExceptionClear(); return false; }
    static jmethodID Method(JNIEnv* env,jobject object,const char* name,const char* desc) {
        auto cls=env->GetObjectClass(object); if(!cls) { Check(env); return nullptr; }
        auto id=jvm::TryMethod(env,cls,name,desc,false); env->DeleteLocalRef(cls); return id;
    }
    static jfieldID Field(JNIEnv* env,jobject object,const char* name,const char* desc) {
        auto cls=env->GetObjectClass(object); if(!cls) { Check(env); return nullptr; }
        auto id=jvm::TryField(env,cls,name,desc,false); env->DeleteLocalRef(cls); return id;
    }
    static jmethodID BootstrapMethod(JNIEnv* env,const char* owner,const char* name,const char* desc) {
        auto cls=env->FindClass(owner); if(!cls) { Check(env); return nullptr; }
        auto id=jvm::TryMethod(env,cls,name,desc,false); env->DeleteLocalRef(cls); return id;
    }
    static jclass GameClass(JNIEnv* env,const StorageProfile& profile,const char* name) {
        jvm::Error ignored; const auto key=std::string(profile.id)+".storage."+name;
        return jvm::Runtime::Instance().ResolveClass(env,key.c_str(),{name},&ignored);
    }
    void ReleaseKeys(JNIEnv* env) { if(keys_) env->DeleteGlobalRef(keys_); keys_=nullptr; key_=keysCount_=0; }
public:
    bool running=false,successful=false,limited=false;
    int radius=0;
    std::uint32_t mask=0;
    std::uint64_t id=0,positionsRead=0;
    std::vector<BlockSnapshot> accum;
    StorageScanner() { accum.reserve(kMaxBlocks); }
    void Reset(JNIEnv* env) {
        ReleaseKeys(env); if(chunks_) env->DeleteGlobalRef(chunks_); chunks_=nullptr;
        running=false; chunk_=chunksCount_=0;
    }
    unsigned Progress() const { return chunksCount_?unsigned(100ull*std::min(chunk_,chunksCount_)/chunksCount_):100; }
    bool Begin(JNIEnv* env,std::string_view mapping,jobject level,const WorldSnapshot& s,int range,std::uint32_t types) {
        Reset(env); successful=true; limited=false; accum.clear(); positionsRead=0;
        radius=range; mask=types; x0_=s.posX; y0_=s.posY; z0_=s.posZ; ++id;
        const auto* profile=StorageMapping(mapping); if(!profile||!level) return false;
        if(profile!=profile_) {
            source_=length_=get_=entities_=keySet_=toArray_=x_=y_=z_=nullptr; storage_=array_=nullptr; profile_=profile;
        }
        jvm::LocalFrame frame(env,16); if(!frame.ok()) return false;
        if(!source_) source_=Method(env,level,profile->source.name,profile->source.signature);
        if(!source_) return false;
        auto source=env->CallObjectMethod(level,source_); if(!Check(env)||!source) return false;
        if(!storage_) storage_=Field(env,source,profile->storage.name,profile->storage.signature);
        if(!storage_) return false;
        auto storage=env->GetObjectField(source,storage_); if(!Check(env)||!storage) return false;
        if(!array_) array_=Field(env,storage,profile->chunks.name,profile->chunks.signature);
        if(!array_) return false;
        auto array=env->GetObjectField(storage,array_); if(!Check(env)||!array) return false;
        if(!length_) length_=Method(env,array,"length","()I");
        if(!get_) get_=Method(env,array,"get","(I)Ljava/lang/Object;");
        if(!length_||!get_) return false;
        auto posClass=GameClass(env,*profile,profile->posClass),chunkClass=GameClass(env,*profile,profile->chunkClass);
        if(!posClass||!chunkClass) return false;
        entities_=jvm::TryMethod(env,chunkClass,profile->entities.name,profile->entities.signature,false);
        x_=jvm::TryMethod(env,posClass,profile->x.name,profile->x.signature,false);
        y_=jvm::TryMethod(env,posClass,profile->y.name,profile->y.signature,false);
        z_=jvm::TryMethod(env,posClass,profile->z.name,profile->z.signature,false);
        keySet_=BootstrapMethod(env,"java/util/Map","keySet","()Ljava/util/Set;");
        toArray_=BootstrapMethod(env,"java/util/Collection","toArray","()[Ljava/lang/Object;");
        if(!entities_||!x_||!y_||!z_||!keySet_||!toArray_) return false;
        chunksCount_=env->CallIntMethod(array,length_); if(!Check(env)||chunksCount_<0||chunksCount_>65536) return false;
        chunks_=env->NewGlobalRef(array); if(!Check(env)||!chunks_) return false;
        running=true; return true;
    }
    template<class Read> bool Step(JNIEnv* env,Read read,int milliseconds=1) {
        if(!running) return true;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(std::max(1,milliseconds));
        unsigned work=0;
        while(accum.size()<kMaxBlocks) {
            if((work++&15)==0&&std::chrono::steady_clock::now()>=deadline) return false;
            jvm::LocalFrame frame(env,16); if(!frame.ok()) { successful=false; Reset(env); return true; }
            if(keys_&&key_<keysCount_) {
                auto pos=env->GetObjectArrayElement(keys_,key_++);
                if(!Check(env)||!pos) { successful=false; continue; }
                if(!x_) x_=Method(env,pos,profile_->x.name,profile_->x.signature);
                if(!y_) y_=Method(env,pos,profile_->y.name,profile_->y.signature);
                if(!z_) z_=Method(env,pos,profile_->z.name,profile_->z.signature);
                if(!x_||!y_||!z_) { successful=false; Reset(env); return true; }
                const int x=env->CallIntMethod(pos,x_); if(!Check(env)) { successful=false; continue; }
                const int y=env->CallIntMethod(pos,y_); if(!Check(env)) { successful=false; continue; }
                const int z=env->CallIntMethod(pos,z_); if(!Check(env)) { successful=false; continue; }
                if(std::hypot(x+.5-x0_,y+.5-y0_,z+.5-z0_)>radius) continue;
                ++positionsRead; const auto kind=read(x,y,z);
                if(kind<0||kind>=kBlockKindCount||!(mask&(1u<<unsigned(kind)))) continue;
                BlockSnapshot record{}; record.x=float(x); record.y=float(y); record.z=float(z); record.kind=std::uint8_t(kind); accum.push_back(record);
                continue;
            }
            ReleaseKeys(env);
            if(chunk_>=chunksCount_) { Reset(env); return true; }
            auto chunk=env->CallObjectMethod(chunks_,get_,chunk_++); if(!Check(env)) { successful=false; continue; }
            if(!chunk) continue;
            if(!entities_) entities_=Method(env,chunk,profile_->entities.name,profile_->entities.signature);
            if(!entities_) { successful=false; Reset(env); return true; }
            auto map=env->CallObjectMethod(chunk,entities_); if(!Check(env)) { successful=false; continue; }
            if(!map) continue;
            if(!keySet_) keySet_=Method(env,map,"keySet","()Ljava/util/Set;");
            if(!keySet_) { successful=false; Reset(env); return true; }
            auto set=env->CallObjectMethod(map,keySet_); if(!Check(env)||!set) { successful=false; continue; }
            if(!toArray_) toArray_=Method(env,set,"toArray","()[Ljava/lang/Object;");
            if(!toArray_) { successful=false; Reset(env); return true; }
            auto keys=static_cast<jobjectArray>(env->CallObjectMethod(set,toArray_)); if(!Check(env)||!keys) { successful=false; continue; }
            keysCount_=env->GetArrayLength(keys);
            if(keysCount_>16384) { limited=true; keysCount_=16384; }
            keys_=static_cast<jobjectArray>(env->NewGlobalRef(keys));
            if(!Check(env)||!keys_) { successful=false; Reset(env); return true; }
        }
        limited=true; Reset(env); return true;
    }
};
}
