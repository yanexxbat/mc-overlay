// Echter OpenGL/PBO-Leseweg. JNI-Aufrufe erhalten feste Testobjekte; kein Spiel-Attach.
#include "agent/depth_capture.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
struct NativeContext final {
    HWND window{}; HDC dc{}; HGLRC gl{};
    ~NativeContext() { wglMakeCurrent(nullptr,nullptr); if(gl) wglDeleteContext(gl); if(dc) ReleaseDC(window,dc); if(window) DestroyWindow(window); }
    bool Open() {
        window=CreateWindowExW(0,L"STATIC",L"mcov depth test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window) return false; dc=GetDC(window); if(!dc) return false;
        PIXELFORMATDESCRIPTOR p{}; p.nSize=sizeof p; p.nVersion=1;
        p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER; p.iPixelType=PFD_TYPE_RGBA; p.cColorBits=24; p.cDepthBits=24;
        const auto format=ChoosePixelFormat(dc,&p);
        if(!format||!SetPixelFormat(dc,format,&p)) return false;
        gl=wglCreateContext(dc); return gl&&wglMakeCurrent(dc,gl);
    }
};
namespace {
GLuint testTexture{};
_jobject testObject;
jobject JNICALL Object(JNIEnv*,jobject,jmethodID,va_list) { return &testObject; }
jint JNICALL Integer(JNIEnv*,jobject,jmethodID,va_list) { return jint(testTexture); }
jfloat JNICALL Float(JNIEnv*,jobject,jmethodID,va_list) { return 1000; }
jint JNICALL Field(JNIEnv*,jobject,jfieldID field) { return field==reinterpret_cast<jfieldID>(1)?64:48; }
jboolean JNICALL Instance(JNIEnv*,jobject,jclass) { return JNI_TRUE; }
jboolean JNICALL Exception(JNIEnv*) { return JNI_FALSE; }
jint JNICALL Push(JNIEnv*,jint) { return JNI_OK; }
jobject JNICALL Pop(JNIEnv*,jobject value) { return value; }
}
namespace mcov::agent {
struct DepthCaptureCheck {
    static bool Arm(DepthCapture& capture,SharedBlock& world) {
        if(!capture.gl_.Load()) return false;
        Require(capture.channel_.Open(GetCurrentProcessId(),true),"Could not open depth mapping");
        capture.channel_.Block()->requested=1; capture.world_=&world;
        capture.textureClass_=reinterpret_cast<jclass>(&testObject);
        capture.client_=capture.mainTarget_=capture.depthTexture_=capture.glId_=capture.far_=reinterpret_cast<jmethodID>(1);
        capture.width_=reinterpret_cast<jfieldID>(1); capture.height_=reinterpret_cast<jfieldID>(2);
        return true;
    }
    static void Release(DepthCapture& capture) { capture.ReleaseGl(); }
};
}
int main() {
    try {
        NativeContext context;
        if(!context.Open()) { std::cout<<"SKIP no WGL context\n"; return 77; }
        auto capture=std::make_unique<mcov::agent::DepthCapture>(); auto world=std::make_unique<mcov::SharedBlock>();
        if(!mcov::agent::DepthCaptureCheck::Arm(*capture,*world)) { std::cout<<"SKIP OpenGL PBO/fence support missing\n"; return 77; }
        JNINativeInterface_ functions{}; functions.CallObjectMethodV=Object; functions.CallIntMethodV=Integer;
        functions.CallFloatMethodV=Float; functions.GetIntField=Field; functions.IsInstanceOf=Instance;
        functions.ExceptionCheck=Exception; functions.PushLocalFrame=Push; functions.PopLocalFrame=Pop;
        JNIEnv env{&functions};
        mcov::DepthChannel reader; Require(reader.Open(GetCurrentProcessId(),false),"Could not open depth reader");
        auto frame=std::make_unique<mcov::DepthFrame>();
        auto snapshot=std::make_unique<mcov::WorldSnapshot>();
        // Wie GlConst.toGlInternalId(DEPTH32) in Minecraft 1.21.11.
        for(GLint format:{0x81a7,0x8cac,0x81a6,0x81a5}) {
            std::vector<float> pixels(64*48);
            for(int y=0;y<48;++y) for(int x=0;x<64;++x) pixels[y*64+x]=(y<24?.2f:.6f)+(x<32?0:.15f);
            glGenTextures(1,&testTexture); glBindTexture(GL_TEXTURE_2D,testTexture);
            glTexImage2D(GL_TEXTURE_2D,0,format,64,48,0,GL_DEPTH_COMPONENT,GL_FLOAT,pixels.data());
            Require(glGetError()==GL_NO_ERROR,"Depth test texture allocation failed");
            snapshot->renderEpoch+=4; snapshot->frameIndex++; snapshot->camX=double(format);
            glPixelStorei(GL_PACK_ALIGNMENT,8); glPixelStorei(GL_PACK_ROW_LENGTH,128); glPixelStorei(GL_PACK_SKIP_ROWS,2); glPixelStorei(GL_PACK_SKIP_PIXELS,3);
            // GPU ist zuerst fertig; der Sampler muss spaeter dieselbe Epoche verbinden.
            capture->Capture(&env,&testObject,snapshot->renderEpoch); glFinish(); capture->ObserveSnapshot(*snapshot);
            capture->Capture(&env,&testObject,snapshot->renderEpoch+2);
            Require(reader.Read(*frame),"Native depth capture did not publish pixels");
            Require(frame->epoch==snapshot->renderEpoch&&frame->world.camX==snapshot->camX,"Depth joined the wrong camera");
            Require(std::abs(frame->pixels[10*mcov::kDepthWidth+10]-.6f)<.0001f&&
                std::abs(frame->pixels[(mcov::kDepthHeight-10)*mcov::kDepthWidth+mcov::kDepthWidth-10]-.35f)<.0001f,"Depth conversion / vertical flip incorrect");
            GLint restored{}; glGetIntegerv(GL_PACK_ROW_LENGTH,&restored); Require(restored==128,"Pixel pack state was changed");
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&restored); Require(GLuint(restored)==testTexture,"Texture binding was changed");
            Require(glGetError()==GL_NO_ERROR,"Capture generated an OpenGL error");
            mcov::agent::DepthCaptureCheck::Release(*capture); glDeleteTextures(1,&testTexture);
            std::cout<<"PASS native GL depth format 0x"<<std::hex<<format<<std::dec<<": PBO, late snapshot, row flip, state restore\n";
        }
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
