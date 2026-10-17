#pragma once
#include "base_texture_gl.hpp"
#include "jvm_interface.hpp"
#include "../common/shared_state.hpp"
#include "../common/log.hpp"
#include <mutex>
#include <cstring>

namespace mcov::agent {
template<bool Storage=false> class BaseTextureCapture {
    using Request=std::conditional_t<Storage,base::StorageRequest,base::Request>;
    std::mutex mutex_;
    Request request_{}; // Large storage selections stay off the JVM render-thread stack.
    SharedBlock* world_{};
    jclass signal_{},bridge_{},meshClass_{};
    jfieldID enabled_{},textures_{},vertices_{},x_{},y_{},z_{},serial_{};
    jmethodID step_{},camera_{},clear_{},stats_{};
    bool supported_{},armed_{},failed_{};
    std::uint64_t observedAt_{};
    char dimension_[kNameLen]{},server_[kAddrLen]{};
    BaseTextureGl gl_;
    BaseMesh mesh_;
    auto& Requests() {
        if constexpr(Storage) return world_->storageTextureRequest;
        else return world_->baseRequest;
    }
    auto& Reports() {
        if constexpr(Storage) return world_->storageTextureReport;
        else return world_->baseReport;
    }
    void Report(base::Status status,const Request& request) {
        if(!world_) return;
        base::Report report; report.sampledAtMs=GetTickCount64(); report.revision=request.revision; report.status=status;
        Reports().Publish(report);
    }
    bool Exception(JNIEnv* env) {
        if(!env->ExceptionCheck()) return false;
        jobject error=env->ExceptionOccurred(); env->ExceptionClear(); failed_=true;
        MCOV_LOG_WARN("textured ESP: game model bridge raised an exception");
        // Preserve the underlying game exception instead of losing the cause
        // behind reflection's InvocationTargetException.
        auto type=env->FindClass("java/lang/Throwable");
        auto describe=type?env->GetMethodID(type,"toString","()Ljava/lang/String;"):nullptr;
        auto cause=type?env->GetMethodID(type,"getCause","()Ljava/lang/Throwable;"):nullptr;
        for(int depth=0;error&&describe&&cause&&!env->ExceptionCheck()&&depth<6;++depth) {
            auto message=static_cast<jstring>(env->CallObjectMethod(error,describe));
            if(env->ExceptionCheck()) break;
            if(message) {
                const char* text=env->GetStringUTFChars(message,nullptr);
                if(text) { MCOV_LOG_WARN("  cause: %.1500s",text); env->ReleaseStringUTFChars(message,text); }
                env->DeleteLocalRef(message);
            }
            auto next=env->CallObjectMethod(error,cause); env->DeleteLocalRef(error); error=next;
        }
        if(error) env->DeleteLocalRef(error); if(type) env->DeleteLocalRef(type);
        if(env->ExceptionCheck()) env->ExceptionClear(); return true;
    }
    void Reset(JNIEnv* env) {
        mesh_={}; gl_.Release();
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
        auto configure=env->GetStaticMethodID(bridge_,Storage?"storageConfigure":"configure","(Ljava/lang/Class;Ljava/lang/String;)Z");
        step_=env->GetStaticMethodID(bridge_,Storage?"storageStep":"step","(Ljava/lang/Object;J[D)Lmcov/BaseTextureBridge$Mesh;");
        camera_=env->GetStaticMethodID(bridge_,Storage?"storageCamera":"camera","(Ljava/lang/Object;)[D");
        clear_=env->GetStaticMethodID(bridge_,Storage?"storageClear":"clear","()V"); stats_=env->GetStaticMethodID(bridge_,Storage?"storageStats":"stats","()[I");
        enabled_=env->GetStaticFieldID(signal_,Storage?"texturedStorageEnabled":"texturedBaseEnabled","Z");
        textures_=env->GetFieldID(meshClass_,"textures","[I"); vertices_=env->GetFieldID(meshClass_,"vertices","[[F");
        x_=env->GetFieldID(meshClass_,"x","D"); y_=env->GetFieldID(meshClass_,"y","D"); z_=env->GetFieldID(meshClass_,"z","D"); serial_=env->GetFieldID(meshClass_,"serial","J");
        if(Exception(env)||!configure||!step_||!camera_||!clear_||!stats_||!enabled_||!textures_||!vertices_||!serial_) return false;
        const auto profile=env->NewStringUTF(b.set->id);
        const bool configured=env->CallStaticBooleanMethod(bridge_,configure,b.client.minecraftClass,profile);
        if(Exception(env)||!configured) return false;
        JNINativeMethod native{const_cast<char*>(Storage?"nativeTexturedStorage":"nativeTexturedBase"),const_cast<char*>("(Ljava/lang/Object;J)V"),callback};
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
        auto& request=request_; if(!Requests().Read(request)) return;
        if(request.enabled&&!armed_) {
            env->SetStaticBooleanField(signal_,enabled_,JNI_TRUE); armed_=!Exception(env);
        }
    }
    void Capture(JNIEnv* env,jobject renderer) {
        std::unique_lock lock(mutex_,std::try_to_lock); if(!lock||!world_||!supported_) return;
        auto& request=request_; if(!Requests().Read(request)) return;
        const auto now=GetTickCount64();
        if(!ValidBaseRequest(request,now,dimension_,server_)||now-observedAt_>1500||
           (world_->controlFlags.load(std::memory_order_acquire)&(kCtrlPauseSampling|kCtrlRequestUnload))) {
            Reset(env); failed_=false; Report(request.enabled?base::Status::Waiting:base::Status::Disabled,request);
            env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE); armed_=false; if(env->ExceptionCheck()) env->ExceptionClear(); return;
        }
        if(failed_) { Report(base::Status::Failed,request); return; }
        jvm::LocalFrame frame(env,32);
        auto bounds=env->NewDoubleArray(request.count*6);
        if(Exception(env)||!bounds) { Report(base::Status::Failed,request); return; }
        std::vector<double> values(request.count*6);
        for(unsigned i=0;i<request.count;++i) { const auto& r=request.regions[i]; const double v[6]={r.minX,r.minY,r.minZ,r.maxX,r.maxY,r.maxZ}; std::memcpy(values.data()+i*6,v,sizeof(v)); }
        env->SetDoubleArrayRegion(bounds,0,request.count*6,values.data());
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
                const bool valid=std::all_of(camera,camera+11,[](double v){return std::isfinite(v);})&&
                    camera[6]>0&&camera[6]<=2147483647.&&camera[7]>0&&camera[7]<=2147483647.&&
                    camera[8]>0&&camera[8]<=8192&&camera[9]>0&&camera[9]<=8192;
                if(!valid||!gl_.Draw(mesh_,{camera[0],camera[1],camera[2],camera[3],camera[4],camera[5],GLuint(camera[6]),GLuint(camera[7]),int(camera[8]),int(camera[9])},request.opacity)) {
                    report.status=base::Status::Failed; failed_=true; MCOV_LOG_WARN("textured ESP: OpenGL pass unavailable");
                }
            }
        }
        Reports().Publish(report);
    }
    void Close(JNIEnv* env) {
        const std::scoped_lock lock(mutex_); world_=nullptr; supported_=false;
        if(signal_&&enabled_) env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE);
        Reset(env); for(auto type:{signal_,bridge_,meshClass_}) if(type) env->DeleteGlobalRef(type);
        signal_=bridge_=meshClass_=nullptr;
    }
};
} // namespace mcov::agent
