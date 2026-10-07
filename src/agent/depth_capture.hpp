#pragma once
#include "../common/depth_channel.hpp"
#include "../common/depth_snapshots.hpp"
#include "../common/log.hpp"
#include "jvm_interface.hpp"
#include <GL/gl.h>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace mcov::agent {
// Nur Minecrafts OpenGL-Thread besitzt den Kontext. Der Sampler bindet lediglich JNI-IDs.
class DepthCapture final {
    struct Gl {
        void (APIENTRY *genFbo)(GLsizei,GLuint*){};
        void (APIENTRY *deleteFbo)(GLsizei,const GLuint*){};
        void (APIENTRY *bindFbo)(GLenum,GLuint){};
        void (APIENTRY *attach)(GLenum,GLenum,GLenum,GLuint,GLint){};
        GLenum (APIENTRY *check)(GLenum){};
        void (APIENTRY *blit)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum){};
        void (APIENTRY *genBuffer)(GLsizei,GLuint*){};
        void (APIENTRY *deleteBuffer)(GLsizei,const GLuint*){};
        void (APIENTRY *bindBuffer)(GLenum,GLuint){};
        void (APIENTRY *bufferData)(GLenum,std::ptrdiff_t,const void*,GLenum){};
        void* (APIENTRY *map)(GLenum,std::ptrdiff_t,std::ptrdiff_t,GLbitfield){};
        GLboolean (APIENTRY *unmap)(GLenum){};
        void* (APIENTRY *fence)(GLenum,GLbitfield){};
        GLenum (APIENTRY *wait)(void*,GLbitfield,std::uint64_t){};
        void (APIENTRY *deleteSync)(void*){};
        bool Load() {
#define MCOV_GL_LOAD(member,name) member=reinterpret_cast<decltype(member)>(wglGetProcAddress(name)); if(!member||reinterpret_cast<std::uintptr_t>(member)<=3||reinterpret_cast<std::intptr_t>(member)==-1) return false;
            MCOV_GL_LOAD(genFbo,"glGenFramebuffers") MCOV_GL_LOAD(deleteFbo,"glDeleteFramebuffers")
            MCOV_GL_LOAD(bindFbo,"glBindFramebuffer") MCOV_GL_LOAD(attach,"glFramebufferTexture2D")
            MCOV_GL_LOAD(check,"glCheckFramebufferStatus") MCOV_GL_LOAD(blit,"glBlitFramebuffer")
            MCOV_GL_LOAD(genBuffer,"glGenBuffers") MCOV_GL_LOAD(deleteBuffer,"glDeleteBuffers")
            MCOV_GL_LOAD(bindBuffer,"glBindBuffer") MCOV_GL_LOAD(bufferData,"glBufferData")
            MCOV_GL_LOAD(map,"glMapBufferRange") MCOV_GL_LOAD(unmap,"glUnmapBuffer")
            MCOV_GL_LOAD(fence,"glFenceSync") MCOV_GL_LOAD(wait,"glClientWaitSync") MCOV_GL_LOAD(deleteSync,"glDeleteSync")
#undef MCOV_GL_LOAD
            return true;
        }
    } gl_;
    static constexpr GLenum ReadFbo=0x8ca8,DrawFbo=0x8ca9,Fbo=0x8d40,DepthAttachment=0x8d00,Pack=0x88eb;
    static constexpr std::size_t Bytes=kDepthWidth*kDepthHeight*sizeof(float);
    struct Slot { GLuint pbo{}; void* fence{}; std::uint64_t epoch{},time{}; int width{},height{}; float farPlane{}; WorldSnapshot world{}; };
    std::array<Slot,3> slots_{};
    GLuint source_{},target_{},texture_{}; GLint format_{}; HGLRC context_{};
    std::mutex mutex_; DepthChannel channel_; SharedBlock* world_{};
    DepthSnapshots snapshots_;
    std::uint64_t callbacks_{},queued_{},published_{},unmatched_{},reportAt_{};
    jclass signal_{},textureClass_{}; jfieldID enabled_{},width_{},height_{};
    jmethodID client_{},mainTarget_{},depthTexture_{},glId_{},far_{};
    bool enabledState_{};
    void Status(DepthStatus status) { if(auto* b=channel_.Block()) b->status.store(std::uint32_t(status),std::memory_order_release); }
    void ReleaseGl() {
        if(!context_||wglGetCurrentContext()!=context_) return;
        for(auto& s:slots_) { if(s.fence) gl_.deleteSync(s.fence); if(s.pbo) gl_.deleteBuffer(1,&s.pbo); s.fence=nullptr; s.pbo=0; }
        if(source_) gl_.deleteFbo(1,&source_); if(target_) gl_.deleteFbo(1,&target_); if(texture_) glDeleteTextures(1,&texture_);
        source_=target_=texture_=0; format_=0;
    }
    struct State {
        Gl& gl; GLint read{},draw{},pack{},unpack{},texture{},alignment{},row{},skipRows{},skipPixels{},swap{}; GLboolean scissor{};
        State(Gl& api):gl(api) {
            glGetIntegerv(0x8caa,&read); glGetIntegerv(0x8ca6,&draw); glGetIntegerv(0x88ed,&pack); glGetIntegerv(0x88ef,&unpack);
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture); glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
            glGetIntegerv(GL_PACK_ROW_LENGTH,&row); glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows); glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
            glGetIntegerv(GL_PACK_SWAP_BYTES,&swap); scissor=glIsEnabled(GL_SCISSOR_TEST);
        }
        ~State() {
            gl.bindFbo(ReadFbo,read); gl.bindFbo(DrawFbo,draw); gl.bindBuffer(Pack,pack); gl.bindBuffer(0x88ec,unpack);
            glBindTexture(GL_TEXTURE_2D,texture); glPixelStorei(GL_PACK_ALIGNMENT,alignment); glPixelStorei(GL_PACK_ROW_LENGTH,row);
            glPixelStorei(GL_PACK_SKIP_ROWS,skipRows); glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels); glPixelStorei(GL_PACK_SWAP_BYTES,swap);
            if(scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        }
    };
public:
    template<class Bindings> bool Install(JNIEnv* env,const Bindings& b,SharedBlock* block,void* callback) {
        const std::scoped_lock lock(mutex_);
        if(!channel_.Open(GetCurrentProcessId(),true)) return false;
        Status(DepthStatus::Unsupported);
        if(std::strcmp(b.profile->version,"1.21.11")) return false;
        const bool intermediary=std::strstr(b.set->id,"intermediary")!=nullptr,vanilla=std::strstr(b.set->id,"vanilla")!=nullptr;
        auto resolve=[&](const char* key,const char* name) { jvm::Error error; return jvm::Runtime::Instance().ResolveClass(env,key,{name},&error); };
        auto target=resolve("depth.target",intermediary?"net/minecraft/class_276":vanilla?"fxt":"com/mojang/blaze3d/pipeline/RenderTarget");
        textureClass_=resolve("depth.glTexture",intermediary?"net/minecraft/class_10868":vanilla?"fxl":"com/mojang/blaze3d/opengl/GlTexture");
        if(!target||!textureClass_) return false;
        const std::string clientDesc=std::string("()L")+b.set->classes.client+";";
        const std::string targetDesc=std::string("()L")+(intermediary?"net/minecraft/class_276":vanilla?"fxt":"com/mojang/blaze3d/pipeline/RenderTarget")+";";
        client_=jvm::TryMethod(env,b.clsRenderer,intermediary?"method_35772":vanilla?"n":"getMinecraft",clientDesc.c_str(),false);
        far_=jvm::TryMethod(env,b.clsRenderer,intermediary?"method_32796":vanilla?"l":"getDepthFar","()F",false);
        mainTarget_=jvm::TryMethod(env,b.client.minecraftClass,intermediary?"method_1522":vanilla?"l":"getMainRenderTarget",targetDesc.c_str(),false);
        depthTexture_=jvm::TryMethod(env,target,intermediary?"method_30278":vanilla?"e":"getDepthTexture","()Lcom/mojang/blaze3d/textures/GpuTexture;",false);
        glId_=jvm::TryMethod(env,textureClass_,intermediary?"method_68427":vanilla?"a":"glId","()I",false);
        width_=jvm::TryField(env,target,intermediary?"field_1482":vanilla?"c":"width","I",false);
        height_=jvm::TryField(env,target,intermediary?"field_1481":vanilla?"d":"height","I",false);
        if(!client_||!far_||!mainTarget_||!depthTexture_||!glId_||!width_||!height_) return false;
        auto local=env->FindClass("mcov/FrameSignal"); if(env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if(!local) return false; signal_=static_cast<jclass>(env->NewGlobalRef(local)); env->DeleteLocalRef(local);
        enabled_=jvm::TryField(env,signal_,"depthEnabled","Z",true); if(!enabled_) return false;
        JNINativeMethod native{const_cast<char*>("nativeDepthReady"),const_cast<char*>("(Ljava/lang/Object;J)V"),callback};
        if(env->RegisterNatives(signal_,&native,1)!=JNI_OK) { if(env->ExceptionCheck()) env->ExceptionClear(); return false; }
        world_=block; Status(DepthStatus::Waiting); env->SetStaticBooleanField(signal_,enabled_,JNI_TRUE); enabledState_=true; return !env->ExceptionCheck();
    }
    void RefreshRequest(JNIEnv* env) {
        const std::scoped_lock lock(mutex_);
        if(world_&&channel_.Block()->requested.load(std::memory_order_acquire)&&GetTickCount64()>=reportAt_) {
            reportAt_=GetTickCount64()+5000;
            MCOV_LOG_INFO("world depth: callbacks=%llu queued=%llu published=%llu unmatched=%llu status=%u",
                static_cast<unsigned long long>(callbacks_),static_cast<unsigned long long>(queued_),
                static_cast<unsigned long long>(published_),static_cast<unsigned long long>(unmatched_),channel_.Block()->status.load());
        }
        if(world_&&signal_&&enabled_&&!enabledState_&&channel_.Block()->requested.load(std::memory_order_acquire)) {
            env->SetStaticBooleanField(signal_,enabled_,JNI_TRUE); enabledState_=true; Status(DepthStatus::Waiting);
            if(env->ExceptionCheck()) { env->ExceptionClear(); enabledState_=false; }
        }
    }
    void ObserveSnapshot(const WorldSnapshot& snapshot) {
        const std::scoped_lock lock(mutex_);
        if(!world_||!channel_.Block()->requested.load(std::memory_order_acquire)) return;
        snapshots_.Put(snapshot);
        for(auto& slot:slots_) if(slot.fence&&slot.epoch==snapshot.renderEpoch) slot.world=snapshot;
    }
    void Capture(JNIEnv* env,jobject renderer,std::uint64_t epoch) {
        std::unique_lock lock(mutex_,std::try_to_lock); if(!lock) return;
        auto* block=channel_.Block(); if(!block||!world_) { ReleaseGl(); return; }
        ++callbacks_;
        if(!block->requested.load(std::memory_order_acquire)) {
            ReleaseGl(); Status(DepthStatus::Disabled); env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE); enabledState_=false;
            if(env->ExceptionCheck()) env->ExceptionClear(); return;
        }
        if(!wglGetCurrentContext()) { Status(DepthStatus::Unsupported); return; }
        if(!context_) { if(!gl_.Load()) { Status(DepthStatus::Unsupported); return; } context_=wglGetCurrentContext(); }
        if(context_!=wglGetCurrentContext()) { Status(DepthStatus::Unsupported); return; }
        State state(gl_);
        // Kein Warten auf die GPU: drei PBOs und Fence-Poll mit Timeout 0.
        for(auto& s:slots_) if(s.fence) {
            // Der Sampler darf erst NACH dem GL-Capture fertig werden.
            if(s.world.renderEpoch!=s.epoch) {
                if(snapshots_.Latest()<=s.epoch&&GetTickCount64()-s.time<100) continue;
                ++unmatched_; gl_.deleteSync(s.fence); s.fence=nullptr; continue;
            }
            const auto done=gl_.wait(s.fence,0,0);
            if(done==0x911d) { gl_.deleteSync(s.fence); s.fence=nullptr; Status(DepthStatus::Unsupported); continue; }
            if(done!=0x911a&&done!=0x911c) continue;
            gl_.bindBuffer(Pack,s.pbo);
            const auto* data=static_cast<const float*>(gl_.map(Pack,0,Bytes,1));
            if(data) {
                if(GetTickCount64()-s.time<100 && s.epoch>=block->frame.epoch) {
                    block->sequence.fetch_add(1,std::memory_order_acq_rel);
                    auto& f=block->frame; f.epoch=s.epoch; f.timeMs=s.time; f.nearPlane=.05f; f.farPlane=s.farPlane;
                    f.sourceWidth=s.width; f.sourceHeight=s.height; f.world=s.world;
                    f.world.nearPlane=f.nearPlane; f.world.farPlane=f.farPlane;
                    for(int y=0;y<kDepthHeight;++y) std::memcpy(f.pixels.data()+y*kDepthWidth,data+(kDepthHeight-1-y)*kDepthWidth,kDepthWidth*sizeof(float));
                    block->sequence.fetch_add(1,std::memory_order_release); ++published_; Status(DepthStatus::Ready);
                }
                gl_.unmap(Pack);
            }
            gl_.deleteSync(s.fence); s.fence=nullptr;
        }
        auto found=std::find_if(slots_.begin(),slots_.end(),[](const Slot& s){return !s.fence;}); if(found==slots_.end()) return;
        auto& slot=*found;
        if(!epoch||(epoch&1)) return;
        slot.world.renderEpoch=0;
        snapshots_.Get(epoch,slot.world);
        jvm::LocalFrame frame(env,8);
        auto client=env->CallObjectMethod(renderer,client_); if(env->ExceptionCheck()) { env->ExceptionClear(); return; }
        auto target=client?env->CallObjectMethod(client,mainTarget_):nullptr; if(env->ExceptionCheck()) { env->ExceptionClear(); return; }
        auto texture=target?env->CallObjectMethod(target,depthTexture_):nullptr; if(env->ExceptionCheck()) { env->ExceptionClear(); return; }
        if(!texture||!env->IsInstanceOf(texture,textureClass_)) { Status(DepthStatus::Unsupported); return; }
        const auto id=env->CallIntMethod(texture,glId_);
        slot.width=env->GetIntField(target,width_); slot.height=env->GetIntField(target,height_); slot.farPlane=env->CallFloatMethod(renderer,far_);
        if(env->ExceptionCheck()) { env->ExceptionClear(); return; }
        if(id<=0||slot.width<1||slot.height<1||!std::isfinite(slot.farPlane)||slot.farPlane<=.05f) return;
        glBindTexture(GL_TEXTURE_2D,id); GLint internal{}; glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&internal);
        if(internal!=0x8cac && internal!=0x81a6 && internal!=0x81a5) { Status(DepthStatus::Unsupported); return; }
        if(format_!=internal) {
            ReleaseGl(); format_=internal;
            gl_.genFbo(1,&source_); gl_.genFbo(1,&target_); glGenTextures(1,&texture_);
            glBindTexture(GL_TEXTURE_2D,texture_); gl_.bindBuffer(0x88ec,0);
            glTexImage2D(GL_TEXTURE_2D,0,internal,kDepthWidth,kDepthHeight,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
            gl_.bindFbo(Fbo,target_); gl_.attach(Fbo,DepthAttachment,GL_TEXTURE_2D,texture_,0); glReadBuffer(GL_NONE); glDrawBuffer(GL_NONE);
            for(auto& s:slots_) { gl_.genBuffer(1,&s.pbo); gl_.bindBuffer(Pack,s.pbo); gl_.bufferData(Pack,Bytes,nullptr,0x88e1); }
        }
        gl_.bindFbo(Fbo,source_); gl_.attach(Fbo,DepthAttachment,GL_TEXTURE_2D,id,0); glReadBuffer(GL_NONE); glDrawBuffer(GL_NONE);
        if(gl_.check(Fbo)!=0x8cd5) { Status(DepthStatus::Unsupported); return; }
        gl_.bindFbo(DrawFbo,target_); if(gl_.check(DrawFbo)!=0x8cd5) { Status(DepthStatus::Unsupported); return; }
        glDisable(GL_SCISSOR_TEST);
        gl_.blit(0,0,slot.width,slot.height,0,0,kDepthWidth,kDepthHeight,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
        gl_.bindFbo(ReadFbo,target_); gl_.bindBuffer(Pack,slot.pbo);
        glPixelStorei(GL_PACK_ALIGNMENT,4); glPixelStorei(GL_PACK_ROW_LENGTH,0); glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0); glPixelStorei(GL_PACK_SWAP_BYTES,0);
        glReadPixels(0,0,kDepthWidth,kDepthHeight,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        slot.epoch=epoch; slot.time=GetTickCount64(); slot.fence=gl_.fence(0x9117,0); ++queued_;
    }
    void Close(JNIEnv* env) {
        const std::scoped_lock lock(mutex_); world_=nullptr;
        if(signal_&&enabled_) env->SetStaticBooleanField(signal_,enabled_,JNI_FALSE);
        if(env->ExceptionCheck()) env->ExceptionClear();
        if(signal_) env->DeleteGlobalRef(signal_); signal_=nullptr; channel_.Close();
        // GL-Ressourcen gehoeren dem Spielkontext und werden bei dessen Ende freigegeben.
    }
};
}
