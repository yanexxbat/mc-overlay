#pragma once
#include "base_texture_gl.hpp"
#include "jvm_interface.hpp"
#include "../common/shared_state.hpp"
#include "../common/log.hpp"
#include <mutex>
#include <cstring>

namespace mcov::agent {
class BaseTextureCapture {
    std::mutex mutex_;
    SharedBlock* world_{};
    jclass signal_{},bridge_{},meshClass_{};
    jfieldID enabled_{},textures_{},vertices_{},x_{},y_{},z_{},serial_{};
    jmethodID step_{},camera_{},clear_{},stats_{};
    bool supported_{},armed_{},failed_{};
    std::uint64_t observedAt_{};
    char dimension_[kNameLen]{},server_[kAddrLen]{};
    BaseTextureGl gl_;
    BaseMesh mesh_;
    void Report(base::Status status,const base::Request& request) {
        if(!world_) return;
        base::Report report; report.sampledAtMs=GetTickCount64(); report.revision=request.revision; report.status=status;
        world_->baseReport.Publish(report);
    }
    bool Exception(JNIEnv* env) {
        if(!env->ExceptionCheck()) return false;
        env->ExceptionClear(); failed_=true; MCOV_LOG_WARN("textured Base ESP: game model bridge raised an exception"); return true;
    }
    void Reset(JNIEnv* env) {
        mesh_={}; gl_.Clear();
        if(bridge_&&clear_) env->CallStaticVoidMethod(bridge_,clear_);
        if(env->ExceptionCheck()) env->ExceptionClear();
    }
public:
    template<class Bindings> bool Install(JNIEnv* env,const Bindings& b,SharedBlock* block,void* callback) {
        const std::scoped_lock lock(mutex_); world_=block;
        Report(base::Status::Unsupported,{});
        if(std::strcmp(b.profile->version,"1.21.11")) return false;
        jvm::LocalFrame frame(env,12);
        auto local=env->FindClass("mcov/FrameSignal"); if(Exception(env)||!local) return false;
        signal_=static_cast<jclass>(env->NewGlobalRef(local));
        local=env->FindClass("mcov/BaseTextureBridge"); if(Exception(env)||!local) return false;
        bridge_=static_cast<jclass>(env->NewGlobalRef(local));
        local=env->FindClass("mcov/BaseTextureBridge$Mesh"); if(Exception(env)||!local) return false;
        meshClass_=static_cast<jclass>(env->NewGlobalRef(local));
        auto configure=env->GetStaticMethodID(bridge_,"configure","(Ljava/lang/Class;Ljava/lang/String;)Z");
        step_=env->GetStaticMethodID(bridge_,"step","(Ljava/lang/Object;J[D)Lmcov/BaseTextureBridge$Mesh;");
        camera_=env->GetStaticMethodID(bridge_,"camera","(Ljava/lang/Object;)[D");
        clear_=env->GetStaticMethodID(bridge_,"clear","()V"); stats_=env->GetStaticMethodID(bridge_,"stats","()[I");
        enabled_=env->GetStaticFieldID(signal_,"texturedBaseEnabled","Z");
        textures_=env->GetFieldID(meshClass_,"textures","[I"); vertices_=env->GetFieldID(meshClass_,"vertices","[[F");
        x_=env->GetFieldID(meshClass_,"x","D"); y_=env->GetFieldID(meshClass_,"y","D"); z_=env->GetFieldID(meshClass_,"z","D"); serial_=env->GetFieldID(meshClass_,"serial","J");
        if(Exception(env)||!configure||!step_||!camera_||!clear_||!stats_||!enabled_||!textures_||!vertices_||!serial_) return false;
        const auto profile=env->NewStringUTF(b.set->id);
        const bool configured=env->CallStaticBooleanMethod(bridge_,configure,b.client.minecraftClass,profile);
        if(Exception(env)||!configured) return false;
        JNINativeMethod native{const_cast<char*>("nativeTexturedBase"),const_cast<char*>("(Ljava/lang/Object;J)V"),callback};
        if(env->RegisterNatives(signal_,&native,1)!=JNI_OK||Exception(env)) return false;
        supported_=true; failed_=false; Report(base::Status::Disabled,{}); return true;
    }
    void Observe(const WorldSnapshot& snapshot) {
        const std::scoped_lock lock(mutex_);
        std::memcpy(dimension_,snapshot.dimension,sizeof(dimension_)); dimension_[sizeof(dimension_)-1]=0;
        std::memcpy(server_,snapshot.serverAddress,sizeof(server_)); server_[sizeof(server_)-1]=0; observedAt_=GetTickCount64();
    }
    void RefreshRequest(JNIEnv* env) {
        const std::scoped_lock lock(mutex_); if(!world_||!supported_) return;
        base::Request request; if(!world_->baseRequest.Read(request)) return;
        if(request.enabled&&!armed_) {
            env->SetStaticBooleanField(signal_,enabled_,JNI_TRUE); armed_=!Exception(env);
        }
    }
    void Capture(JNIEnv* env,jobject renderer) {
        std::unique_lock lock(mutex_,std::try_to_lock); if(!lock||!world_||!supported_) return;
        base::Request request; if(!world_->baseRequest.Read(request)) return;
        const auto now=GetTickCount64();
        if(!ValidBaseRequest(request,now,dimension_,server_)||now-observedAt_>1500||
           (world_->controlFlags.load(std::memory_order_acquire)&(kCtrlPauseSampling|kCtrlRequestUnload))) {
            Reset(env); failed_=false; Report(request.enabled?base::Status::Waiting:base::Status::Disabled,request);
            env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE); armed_=false; if(env->ExceptionCheck()) env->ExceptionClear(); return;
        }
        if(failed_) { Report(base::Status::Failed,request); return; }
        jvm::LocalFrame frame(env,32);
        auto bounds=env->NewDoubleArray(request.count*6); double values[base::kMaxRegions*6]{};
        for(unsigned i=0;i<request.count;++i) { const auto& r=request.regions[i]; const double v[6]={r.minX,r.minY,r.minZ,r.maxX,r.maxY,r.maxZ}; std::memcpy(values+i*6,v,sizeof(v)); }
        env->SetDoubleArrayRegion(bounds,0,request.count*6,values);
        auto object=env->CallStaticObjectMethod(bridge_,step_,renderer,jlong(request.revision),bounds);
        if(Exception(env)) { Report(base::Status::Failed,request); return; }
        if(!object) { mesh_={}; gl_.Clear(); }
        else {
            const auto serial=std::uint64_t(env->GetLongField(object,serial_));
            if(serial!=mesh_.serial) {
                BaseMesh next; next.serial=serial;
                next.x=env->GetDoubleField(object,x_); next.y=env->GetDoubleField(object,y_); next.z=env->GetDoubleField(object,z_);
                auto ids=static_cast<jintArray>(env->GetObjectField(object,textures_)); auto groups=static_cast<jobjectArray>(env->GetObjectField(object,vertices_));
                if(Exception(env)||!ids||!groups) { Report(base::Status::Failed,request); failed_=true; return; }
                const int count=env->GetArrayLength(ids);
                if(count>256||env->GetArrayLength(groups)!=count) { failed_=true; Report(base::Status::Failed,request); return; }
                std::vector<jint> textures(count); env->GetIntArrayRegion(ids,0,count,textures.data()); std::size_t total=0;
                for(int i=0;i<count;++i) {
                    auto vertices=static_cast<jfloatArray>(env->GetObjectArrayElement(groups,i)); if(!vertices||Exception(env)) { failed_=true; break; }
                    const int length=env->GetArrayLength(vertices); total+=length;
                    if(length%84||total>std::size_t(base::kMaxVertices)*14) { failed_=true; break; }
                    if(length) { BaseMeshGroup group; group.texture=textures[i]; group.vertices.resize(length);
                        env->GetFloatArrayRegion(vertices,0,length,group.vertices.data()); next.groups.push_back(std::move(group)); }
                    env->DeleteLocalRef(vertices);
                }
                if(Exception(env)||failed_) { Report(base::Status::Failed,request); return; }
                mesh_=std::move(next);
            }
        }
        base::Report report; report.sampledAtMs=now; report.revision=request.revision;
        auto stats=static_cast<jintArray>(env->CallStaticObjectMethod(bridge_,stats_)); int counts[5]{};
        if(!Exception(env)&&stats&&env->GetArrayLength(stats)==5) env->GetIntArrayRegion(stats,0,5,counts);
        report.status=counts[0]?base::Status::Building:mesh_.groups.empty()?base::Status::Empty:base::Status::Ready;
        report.scanned=counts[1]; report.blocks=counts[2]; report.skipped=counts[3]; report.limited=counts[4]!=0;
        for(const auto& group:mesh_.groups) report.vertices+=std::uint32_t(group.vertices.size()/14);
        if(!mesh_.groups.empty()) {
            auto coords=static_cast<jdoubleArray>(env->CallStaticObjectMethod(bridge_,camera_,renderer)); double camera[11]{};
            if(Exception(env)||!coords||env->GetArrayLength(coords)!=11) { report.status=base::Status::Failed; failed_=true; }
            else {
                env->GetDoubleArrayRegion(coords,0,11,camera);
                if(!gl_.Draw(mesh_,{camera[0],camera[1],camera[2],camera[3],camera[4],camera[5],GLuint(camera[6]),GLuint(camera[7]),int(camera[8]),int(camera[9])},request.opacity)) {
                    report.status=base::Status::Failed; failed_=true; MCOV_LOG_WARN("textured Base ESP: OpenGL pass unavailable");
                }
            }
        }
        world_->baseReport.Publish(report);
    }
    void Close(JNIEnv* env) {
        const std::scoped_lock lock(mutex_); world_=nullptr; supported_=false;
        if(signal_&&enabled_) env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE);
        Reset(env); for(auto type:{signal_,bridge_,meshClass_}) if(type) env->DeleteGlobalRef(type);
        signal_=bridge_=meshClass_=nullptr;
    }
};
} // namespace mcov::agent
