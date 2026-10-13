#include "agent/base_texture_gl.hpp"
#include <iostream>
#include <stdexcept>
#include <memory>
#include <cstring>
using namespace mcov;
void Require(bool yes,const char* message) { if(!yes) throw std::runtime_error(message); }
struct Context {
    HWND window{}; HDC dc{}; HGLRC gl{};
    bool Open() {
        window=CreateWindowExW(0,L"STATIC",L"mcov texture check",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        dc=GetDC(window); PIXELFORMATDESCRIPTOR p{}; p.nSize=sizeof p; p.nVersion=1;
        p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER; p.iPixelType=PFD_TYPE_RGBA; p.cColorBits=32; p.cDepthBits=24;
        const int format=ChoosePixelFormat(dc,&p); if(!format||!SetPixelFormat(dc,format,&p)) return false;
        gl=wglCreateContext(dc); return gl&&wglMakeCurrent(dc,gl);
    }
    ~Context() { wglMakeCurrent(nullptr,nullptr); if(gl) wglDeleteContext(gl); if(dc) ReleaseDC(window,dc); if(window) DestroyWindow(window); }
};
GLuint Texture(int width,int height,const std::vector<unsigned char>& rgba) {
    GLuint t{}; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data()); return t;
}
void Quad(agent::BaseMeshGroup& group,float depth) {
    const float xy[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}},uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    for(int index:{0,1,2,0,2,3}) {
        const float v[14]={xy[index][0],xy[index][1],depth,uv[index][0],uv[index][1],1,1,1,1,.5f,.5f,0,0,1};
        group.vertices.insert(group.vertices.end(),v,v+14);
    }
}
void Requests() {
    base::Request r; r.enabled=true; r.count=1; r.revision=1; r.sentAtMs=1000; r.regions[0]={0,0,0,16,16,16};
    strcpy_s(r.dimension,"overworld"); strcpy_s(r.server,"private");
    Require(agent::ValidBaseRequest(r,1500,"overworld","private"),"Fresh scoped request rejected");
    Require(!agent::ValidBaseRequest(r,1800,"overworld","private")&&!agent::ValidBaseRequest(r,999,"overworld","private"),"Lease or clock reversal ignored");
    Require(!agent::ValidBaseRequest(r,1500,"nether","private")&&!agent::ValidBaseRequest(r,1500,"overworld","other"),"World isolation ignored");
    r.regions[0].maxX=1e8; Require(!agent::ValidBaseRequest(r,1500,"overworld","private"),"Unbounded region accepted");
}
int main() {
    try {
        Requests(); Context context; if(!context.Open()) { std::cout<<"SKIP no WGL context\n"; return 77; }
        using Bind=void(APIENTRY*)(GLenum,GLuint); using Active=void(APIENTRY*)(GLenum);
        auto bind=reinterpret_cast<Bind>(wglGetProcAddress("glBindFramebuffer")); auto active=reinterpret_cast<Active>(wglGetProcAddress("glActiveTexture"));
        if(!bind||!active||reinterpret_cast<std::uintptr_t>(bind)<=3) { std::cout<<"SKIP no modern OpenGL\n"; return 77; }
        std::vector<unsigned char> blue(64*64*4); for(int i=0;i<64*64;++i) { blue[i*4+2]=255; blue[i*4+3]=255; }
        const GLuint target=Texture(64,64,blue),light=Texture(1,1,{255,255,255,255});
        const GLuint red=Texture(1,1,{255,0,0,255}),pattern=Texture(2,1,{0,255,0,255,255,255,255,255});
        agent::BaseMesh mesh; mesh.serial=1; mesh.groups.resize(2); mesh.groups[0].texture=pattern; Quad(mesh.groups[0],4);
        mesh.groups[1].texture=red; Quad(mesh.groups[1],6); // Draw farther surface last, depth must keep near texture.
        agent::BaseTextureGl render;
        glViewport(3,4,50,51); glEnable(GL_SCISSOR_TEST); glScissor(1,1,2,2); glEnable(GL_BLEND); glBlendFunc(GL_DST_COLOR,GL_ZERO);
        glEnable(GL_CULL_FACE); glDepthMask(GL_FALSE); glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE); glClearColor(.1f,.2f,.3f,.4f);
        active(0x84c0+3); glBindTexture(GL_TEXTURE_2D,red);
        Require(render.Draw(mesh,{0,0,0,0,0,90,light,target,64,64},1),"Native texture pass failed");
        GLint viewport[4]{},texture{},unit{}; GLboolean mask[4]{},depth{};
        glGetIntegerv(GL_VIEWPORT,viewport); glGetIntegerv(0x84e0,&unit); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture); glGetBooleanv(GL_COLOR_WRITEMASK,mask); glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);
        Require(viewport[0]==3&&viewport[1]==4&&viewport[2]==50&&viewport[3]==51&&unit==0x84c0+3&&GLuint(texture)==red,"Viewport or texture bindings leaked");
        Require(mask[0]&&!mask[1]&&mask[2]&&!mask[3]&&!depth&&glIsEnabled(GL_SCISSOR_TEST)&&glIsEnabled(GL_BLEND)&&glIsEnabled(GL_CULL_FACE),"Game state leaked");
        active(0x84c0); glBindTexture(GL_TEXTURE_2D,target); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,blue.data());
        auto pixel=[&](int x,int channel){return blue[(32*64+x)*4+channel];};
        Require(pixel(28,0)>250&&pixel(28,1)>250&&pixel(28,2)>250&&pixel(36,0)<3&&pixel(36,1)>250&&pixel(36,2)<3,"Atlas UVs, camera projection or base self-occlusion failed");
        Require(pixel(2,2)>250&&pixel(2,0)<3,"Composite replaced pixels outside the base");
        Require(glGetError()==GL_NO_ERROR,"OpenGL error in native texture pass");
        // Live atlas changes must show without rebuilding geometry.
        glBindTexture(GL_TEXTURE_2D,pattern); const unsigned char yellow[8]={255,255,0,255,255,255,0,255}; glTexSubImage2D(GL_TEXTURE_2D,0,0,0,2,1,GL_RGBA,GL_UNSIGNED_BYTE,yellow);
        Require(render.Draw(mesh,{0,0,0,0,0,90,light,target,64,64},1),"Animated atlas redraw failed");
        glBindTexture(GL_TEXTURE_2D,target); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,blue.data());
        Require(pixel(28,0)>250&&pixel(28,1)>250&&pixel(28,2)<3,"Atlas was copied instead of sampled live");
        for(int i=0;i<64*64;++i) { blue[i*4]=blue[i*4+1]=0; blue[i*4+2]=blue[i*4+3]=255; }
        glBindTexture(GL_TEXTURE_2D,target); glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,blue.data());
        Require(render.Draw(mesh,{0,0,0,0,0,90,light,target,64,64},.5f),"Texture opacity pass failed");
        glBindTexture(GL_TEXTURE_2D,target); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,blue.data());
        Require(std::abs(int(pixel(28,0))-128)<=1&&std::abs(int(pixel(28,1))-128)<=1&&std::abs(int(pixel(28,2))-128)<=1,"Texture opacity applied twice or lost world pixels");
        const unsigned char translucent[8]={255,255,0,128,255,255,0,128};
        glBindTexture(GL_TEXTURE_2D,pattern); glTexSubImage2D(GL_TEXTURE_2D,0,0,0,2,1,GL_RGBA,GL_UNSIGNED_BYTE,translucent);
        Require(render.Draw(mesh,{0,0,0,0,0,90,light,target,64,64},1),"Translucent texture pass failed");
        glBindTexture(GL_TEXTURE_2D,target); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,blue.data());
        Require(pixel(28,0)>250&&std::abs(int(pixel(28,1))-128)<=1&&pixel(28,2)<3,"Translucent texels did not blend with the farther base surface");
        render.Release();
        Require(render.Draw(mesh,{0,0,0,0,0,90,light,target,64,64},1),"Disable/re-enable did not recreate the texture renderer");
        Require(glGetError()==GL_NO_ERROR,"OpenGL error after renderer reset");
        render.Release(); std::cout<<"PASS native Base ESP: live textures, UVs, self-depth, through-world composite, state restore, reset and leased world scope\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
