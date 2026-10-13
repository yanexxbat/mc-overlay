#include "base_texture_gl.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <string>
#include "../common/log.hpp"

namespace mcov::agent {
namespace {
constexpr GLenum Fbo=0x8d40,ReadFbo=0x8ca8,DrawFbo=0x8ca9,ArrayBuffer=0x8892;
constexpr GLenum Texture0=0x84c0,ColourAttachment=0x8ce0,DepthAttachment=0x8d00;
constexpr int Stride=14;
template<class T> bool Load(T& pointer,const char* name) {
    pointer=reinterpret_cast<T>(wglGetProcAddress(name));
    const auto value=reinterpret_cast<std::intptr_t>(pointer);
    return value>3||value<-1;
}
void Enable(GLenum flag,bool enabled) { if(enabled) glEnable(flag); else glDisable(flag); }
std::array<float,16> Matrix(const BaseMesh& mesh,const BaseCamera& c) {
    constexpr double rad=3.14159265358979323846/180.;
    const double cy=std::cos(c.yaw*rad),sy=std::sin(c.yaw*rad),cp=std::cos(c.pitch*rad),sp=std::sin(c.pitch*rad);
    const double x=mesh.x-c.x,y=mesh.y-c.y,z=mesh.z-c.z;
    const double view[16]={-cy,-sy*sp,sy*cp,0, 0,cp,sp,0, -sy,cy*sp,-cy*cp,0,
        -cy*x-sy*z,-sy*sp*x+cp*y+cy*sp*z,sy*cp*x+sp*y-cy*cp*z,1};
    const double t=1/std::tan(c.fov*rad*.5),nearPlane=.05,farPlane=4096;
    const double projection[16]={t*c.height/c.width,0,0,0, 0,t,0,0, 0,0,-(farPlane+nearPlane)/(farPlane-nearPlane),-1, 0,0,-2*farPlane*nearPlane/(farPlane-nearPlane),0};
    std::array<float,16> result{};
    for(int col=0;col<4;++col) for(int row=0;row<4;++row) for(int k=0;k<4;++k)
        result[col*4+row]+=float(projection[k*4+row]*view[col*4+k]);
    return result;
}
}
struct BaseTextureGl::Api {
    void(APIENTRY *genFbo)(GLsizei,GLuint*){}; void(APIENTRY *deleteFbo)(GLsizei,const GLuint*){};
    void(APIENTRY *bindFbo)(GLenum,GLuint){}; void(APIENTRY *attach)(GLenum,GLenum,GLenum,GLuint,GLint){};
    GLenum(APIENTRY *check)(GLenum){};
    void(APIENTRY *genBuffer)(GLsizei,GLuint*){}; void(APIENTRY *deleteBuffer)(GLsizei,const GLuint*){};
    void(APIENTRY *bindBuffer)(GLenum,GLuint){}; void(APIENTRY *bufferData)(GLenum,std::ptrdiff_t,const void*,GLenum){};
    void(APIENTRY *genVao)(GLsizei,GLuint*){}; void(APIENTRY *deleteVao)(GLsizei,const GLuint*){}; void(APIENTRY *bindVao)(GLuint){};
    void(APIENTRY *attribute)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*){}; void(APIENTRY *enableAttribute)(GLuint){};
    GLuint(APIENTRY *createShader)(GLenum){}; void(APIENTRY *shaderSource)(GLuint,GLsizei,const char*const*,const GLint*){};
    void(APIENTRY *compileShader)(GLuint){}; void(APIENTRY *shaderStatus)(GLuint,GLenum,GLint*){}; void(APIENTRY *deleteShader)(GLuint){};
    GLuint(APIENTRY *createProgram)(){}; void(APIENTRY *attachShader)(GLuint,GLuint){}; void(APIENTRY *bindAttribute)(GLuint,GLuint,const char*){};
    void(APIENTRY *linkProgram)(GLuint){}; void(APIENTRY *programStatus)(GLuint,GLenum,GLint*){};
    void(APIENTRY *deleteProgram)(GLuint){}; void(APIENTRY *useProgram)(GLuint){};
    GLint(APIENTRY *uniformLocation)(GLuint,const char*){}; void(APIENTRY *uniformInt)(GLint,GLint){};
    void(APIENTRY *uniformFloat)(GLint,GLfloat){}; void(APIENTRY *uniformMatrix)(GLint,GLsizei,GLboolean,const GLfloat*){};
    void(APIENTRY *activeTexture)(GLenum){};
    void(APIENTRY *genSampler)(GLsizei,GLuint*){}; void(APIENTRY *deleteSampler)(GLsizei,const GLuint*){};
    void(APIENTRY *bindSampler)(GLuint,GLuint){}; void(APIENTRY *samplerParam)(GLuint,GLenum,GLint){};
    void(APIENTRY *blendSeparate)(GLenum,GLenum,GLenum,GLenum){}; void(APIENTRY *blendEquation)(GLenum,GLenum){};
    bool LoadAll() {
#define MCOV_BASE_GL_LOAD(member,name) if(!Load(member,name)) return false;
        MCOV_BASE_GL_LOAD(genFbo,"glGenFramebuffers") MCOV_BASE_GL_LOAD(deleteFbo,"glDeleteFramebuffers") MCOV_BASE_GL_LOAD(bindFbo,"glBindFramebuffer")
        MCOV_BASE_GL_LOAD(attach,"glFramebufferTexture2D") MCOV_BASE_GL_LOAD(check,"glCheckFramebufferStatus")
        MCOV_BASE_GL_LOAD(genBuffer,"glGenBuffers") MCOV_BASE_GL_LOAD(deleteBuffer,"glDeleteBuffers") MCOV_BASE_GL_LOAD(bindBuffer,"glBindBuffer") MCOV_BASE_GL_LOAD(bufferData,"glBufferData")
        MCOV_BASE_GL_LOAD(genVao,"glGenVertexArrays") MCOV_BASE_GL_LOAD(deleteVao,"glDeleteVertexArrays") MCOV_BASE_GL_LOAD(bindVao,"glBindVertexArray")
        MCOV_BASE_GL_LOAD(attribute,"glVertexAttribPointer") MCOV_BASE_GL_LOAD(enableAttribute,"glEnableVertexAttribArray")
        MCOV_BASE_GL_LOAD(createShader,"glCreateShader") MCOV_BASE_GL_LOAD(shaderSource,"glShaderSource") MCOV_BASE_GL_LOAD(compileShader,"glCompileShader")
        MCOV_BASE_GL_LOAD(shaderStatus,"glGetShaderiv") MCOV_BASE_GL_LOAD(deleteShader,"glDeleteShader") MCOV_BASE_GL_LOAD(createProgram,"glCreateProgram")
        MCOV_BASE_GL_LOAD(attachShader,"glAttachShader") MCOV_BASE_GL_LOAD(bindAttribute,"glBindAttribLocation") MCOV_BASE_GL_LOAD(linkProgram,"glLinkProgram")
        MCOV_BASE_GL_LOAD(programStatus,"glGetProgramiv") MCOV_BASE_GL_LOAD(deleteProgram,"glDeleteProgram") MCOV_BASE_GL_LOAD(useProgram,"glUseProgram")
        MCOV_BASE_GL_LOAD(uniformLocation,"glGetUniformLocation") MCOV_BASE_GL_LOAD(uniformInt,"glUniform1i") MCOV_BASE_GL_LOAD(uniformFloat,"glUniform1f") MCOV_BASE_GL_LOAD(uniformMatrix,"glUniformMatrix4fv")
        MCOV_BASE_GL_LOAD(activeTexture,"glActiveTexture") MCOV_BASE_GL_LOAD(genSampler,"glGenSamplers") MCOV_BASE_GL_LOAD(deleteSampler,"glDeleteSamplers")
        MCOV_BASE_GL_LOAD(bindSampler,"glBindSampler") MCOV_BASE_GL_LOAD(samplerParam,"glSamplerParameteri")
        MCOV_BASE_GL_LOAD(blendSeparate,"glBlendFuncSeparate") MCOV_BASE_GL_LOAD(blendEquation,"glBlendEquationSeparate")
#undef MCOV_BASE_GL_LOAD
        return true;
    }
    struct State {
        Api& a; GLint read{},draw{},program{},vao{},buffer{},unpack{},active{},textures[2]{},samplers[2]{},viewport[4]{};
        GLint depthFunc{},srcRGB{},dstRGB{},srcAlpha{},dstAlpha{},eqRGB{},eqAlpha{};
        GLboolean depth{},blend{},cull{},scissor{},srgb{},depthMask{},mask[4]{}; GLfloat clearColour[4]{}; GLdouble clearDepth{};
        State(Api& api):a(api) {
            glGetIntegerv(0x8caa,&read); glGetIntegerv(0x8ca6,&draw); glGetIntegerv(0x8b8d,&program); glGetIntegerv(0x85b5,&vao);
            glGetIntegerv(0x8894,&buffer); glGetIntegerv(0x88ef,&unpack); glGetIntegerv(0x84e0,&active); glGetIntegerv(GL_VIEWPORT,viewport);
            for(int i=0;i<2;++i) { a.activeTexture(Texture0+i); glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]); glGetIntegerv(0x8919,&samplers[i]); }
            depth=glIsEnabled(GL_DEPTH_TEST); blend=glIsEnabled(GL_BLEND); cull=glIsEnabled(GL_CULL_FACE); scissor=glIsEnabled(GL_SCISSOR_TEST); srgb=glIsEnabled(0x8db9);
            glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask); glGetBooleanv(GL_COLOR_WRITEMASK,mask); glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
            glGetIntegerv(0x80c9,&srcRGB); glGetIntegerv(0x80c8,&dstRGB); glGetIntegerv(0x80cb,&srcAlpha); glGetIntegerv(0x80ca,&dstAlpha);
            glGetIntegerv(0x8009,&eqRGB); glGetIntegerv(0x883d,&eqAlpha); glGetFloatv(GL_COLOR_CLEAR_VALUE,clearColour); glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clearDepth);
        }
        ~State() {
            a.bindFbo(ReadFbo,read); a.bindFbo(DrawFbo,draw); a.useProgram(program); a.bindVao(vao); a.bindBuffer(ArrayBuffer,buffer); a.bindBuffer(0x88ec,unpack);
            for(int i=0;i<2;++i) { a.activeTexture(Texture0+i); glBindTexture(GL_TEXTURE_2D,textures[i]); a.bindSampler(i,samplers[i]); } a.activeTexture(active);
            glViewport(viewport[0],viewport[1],viewport[2],viewport[3]); glDepthFunc(depthFunc); glDepthMask(depthMask); glColorMask(mask[0],mask[1],mask[2],mask[3]);
            a.blendSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha); a.blendEquation(eqRGB,eqAlpha);
            Enable(GL_DEPTH_TEST,depth); Enable(GL_BLEND,blend); Enable(GL_CULL_FACE,cull); Enable(GL_SCISSOR_TEST,scissor); Enable(0x8db9,srgb);
            glClearColor(clearColour[0],clearColour[1],clearColour[2],clearColour[3]); glClearDepth(clearDepth);
        }
    };
    GLuint Program(const char* vertex,const char* fragment) {
        GLuint shaders[2]={createShader(0x8b31),createShader(0x8b30)}; const char* sources[2]={vertex,fragment}; GLint okay{};
        for(int i=0;i<2;++i) { shaderSource(shaders[i],1,&sources[i],nullptr); compileShader(shaders[i]); shaderStatus(shaders[i],0x8b81,&okay);
            if(!okay) { for(auto shader:shaders) deleteShader(shader); return 0; } }
        GLuint result=createProgram(); for(auto shader:shaders) attachShader(result,shader);
        bindAttribute(result,0,"position"); bindAttribute(result,1,"uv"); bindAttribute(result,2,"tint"); bindAttribute(result,3,"lightUV");
        linkProgram(result); programStatus(result,0x8b82,&okay); for(auto shader:shaders) deleteShader(shader);
        if(!okay) { deleteProgram(result); return 0; } return result;
    }
};
bool BaseTextureGl::Init() {
    if(gl_) return context_==wglGetCurrentContext()&&program_&&composite_;
    if(!wglGetCurrentContext()) return false;
    gl_=new Api; if(!gl_->LoadAll()) { delete gl_; gl_=nullptr; return false; } context_=wglGetCurrentContext();
    Api::State state(*gl_);
    program_=gl_->Program(R"(#version 150
in vec3 position; in vec2 uv; in vec4 tint; in vec2 lightUV;
uniform mat4 matrix; out vec2 vUV; out vec4 vTint; out vec2 vLight;
void main(){gl_Position=matrix*vec4(position,1);vUV=uv;vTint=tint;vLight=lightUV;}
)",R"(#version 150
uniform sampler2D atlas; uniform sampler2D lightmap; uniform int pass;
in vec2 vUV; in vec4 vTint; in vec2 vLight; out vec4 colour;
void main(){vec4 c=texture(atlas,vUV)*vTint;if(c.a<0.01)discard;
if(pass==0&&c.a<0.99)discard;if(pass==1&&c.a>=0.99)discard;
colour=vec4(c.rgb*texture(lightmap,vLight).rgb,c.a);}
)");
    composite_=gl_->Program(R"(#version 150
out vec2 vUV; void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);vUV=p;gl_Position=vec4(p*2-1,0,1);}
)",R"(#version 150
in vec2 vUV; uniform sampler2D base; uniform float opacity; out vec4 colour;
void main(){colour=texture(base,vUV)*opacity;}
)");
    if(!program_||!composite_) { MCOV_LOG_WARN("textured base shader compile/link failed"); return false; }
    gl_->useProgram(program_); gl_->uniformInt(gl_->uniformLocation(program_,"atlas"),0); gl_->uniformInt(gl_->uniformLocation(program_,"lightmap"),1);
    matrix_=gl_->uniformLocation(program_,"matrix"); pass_=gl_->uniformLocation(program_,"pass");
    gl_->useProgram(composite_); gl_->uniformInt(gl_->uniformLocation(composite_,"base"),0); opacity_=gl_->uniformLocation(composite_,"opacity");
    gl_->genVao(1,&vao_); gl_->genBuffer(1,&vbo_); gl_->bindVao(vao_); gl_->bindBuffer(ArrayBuffer,vbo_);
    const int sizes[4]={3,2,4,2},offsets[4]={0,3,5,9};
    for(int i=0;i<4;++i) { gl_->enableAttribute(i); gl_->attribute(i,sizes[i],GL_FLOAT,GL_FALSE,Stride*sizeof(float),reinterpret_cast<void*>(std::uintptr_t(offsets[i]*sizeof(float)))); }
    gl_->genFbo(1,&fbo_); gl_->genFbo(1,&destination_); glGenTextures(1,&colour_); glGenTextures(1,&depth_);
    gl_->genSampler(1,&sampler_); gl_->genSampler(1,&lightSampler_);
    for(auto sampler:{sampler_,lightSampler_}) {
        gl_->samplerParam(sampler,GL_TEXTURE_MIN_FILTER,sampler==sampler_?GL_NEAREST:GL_LINEAR);
        gl_->samplerParam(sampler,GL_TEXTURE_MAG_FILTER,sampler==sampler_?GL_NEAREST:GL_LINEAR);
        gl_->samplerParam(sampler,GL_TEXTURE_WRAP_S,0x812f); gl_->samplerParam(sampler,GL_TEXTURE_WRAP_T,0x812f);
    }
    return true;
}
bool BaseTextureGl::Resize(int w,int h) {
    if(width_==w&&height_==h) return true;
    gl_->bindBuffer(0x88ec,0); gl_->activeTexture(Texture0);
    glBindTexture(GL_TEXTURE_2D,colour_); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glBindTexture(GL_TEXTURE_2D,depth_); glTexImage2D(GL_TEXTURE_2D,0,0x81a6,w,h,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    gl_->bindFbo(Fbo,fbo_); gl_->attach(Fbo,ColourAttachment,GL_TEXTURE_2D,colour_,0); gl_->attach(Fbo,DepthAttachment,GL_TEXTURE_2D,depth_,0);
    glReadBuffer(ColourAttachment); glDrawBuffer(ColourAttachment);
    if(gl_->check(Fbo)!=0x8cd5) return false; width_=w; height_=h; return true;
}
bool BaseTextureGl::Draw(const BaseMesh& mesh,const BaseCamera& camera,float opacity) {
    if(!camera.colour||!camera.light||camera.width<1||camera.height<1||camera.width>8192||camera.height>8192||
       !std::isfinite(camera.fov)||camera.fov<1||camera.fov>179||!std::isfinite(camera.x)||!std::isfinite(camera.y)||!std::isfinite(camera.z)||
       !std::isfinite(camera.pitch)||!std::isfinite(camera.yaw)||!std::isfinite(mesh.x)||!std::isfinite(mesh.y)||!std::isfinite(mesh.z)||!std::isfinite(opacity)||!Init()) return false;
    Api::State state(*gl_);
    gl_->bindVao(vao_); gl_->bindBuffer(ArrayBuffer,vbo_);
    if(uploaded_!=mesh.serial) {
        std::vector<float> data; std::vector<Batch> batches;
        std::size_t capacity=0; for(const auto& group:mesh.groups) capacity+=group.vertices.size();
        if(capacity>std::size_t(base::kMaxVertices)*Stride) return false;
        data.reserve(capacity);
        for(const auto& group:mesh.groups) {
            if(group.vertices.size()%(Stride*3)||data.size()+group.vertices.size()>std::size_t(base::kMaxVertices)*Stride||!glIsTexture(group.texture)) return false;
            if(!std::all_of(group.vertices.begin(),group.vertices.end(),[](float v){return std::isfinite(v);})) return false;
            // Back to front for translucent texels. Opaque pass still uses its own depth.
            std::vector<std::size_t> triangles(group.vertices.size()/(Stride*3)); std::iota(triangles.begin(),triangles.end(),0);
            auto distance=[&](std::size_t t) { double x=0,y=0,z=0; for(int i=0;i<3;++i) { const float* v=group.vertices.data()+(t*3+i)*Stride; x+=v[0]; y+=v[1]; z+=v[2]; }
                x=x/3+mesh.x-camera.x; y=y/3+mesh.y-camera.y; z=z/3+mesh.z-camera.z; return x*x+y*y+z*z; };
            std::vector<double> distances(triangles.size()); for(auto triangle:triangles) distances[triangle]=distance(triangle);
            std::stable_sort(triangles.begin(),triangles.end(),[&](auto a,auto b){return distances[a]>distances[b];});
            batches.push_back({group.texture,GLint(data.size()/Stride),GLsizei(group.vertices.size()/Stride)});
            for(auto triangle:triangles) data.insert(data.end(),group.vertices.begin()+triangle*Stride*3,group.vertices.begin()+(triangle+1)*Stride*3);
        }
        gl_->bufferData(ArrayBuffer,data.size()*sizeof(float),data.data(),0x88e4); batches_=std::move(batches); uploaded_=mesh.serial;
    }
    if(batches_.empty()) return true;
    if(!Resize(camera.width,camera.height)) return false;
    glViewport(0,0,width_,height_); glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE); glDisable(0x8db9);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    gl_->bindFbo(DrawFbo,fbo_); glClearColor(0,0,0,0); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    gl_->useProgram(program_); const auto matrix=Matrix(mesh,camera); gl_->uniformMatrix(matrix_,1,GL_FALSE,matrix.data());
    gl_->activeTexture(Texture0+1); glBindTexture(GL_TEXTURE_2D,camera.light); gl_->bindSampler(1,lightSampler_);
    gl_->activeTexture(Texture0); gl_->bindSampler(0,sampler_); gl_->blendEquation(0x8006,0x8006);
    for(int pass=0;pass<2;++pass) {
        gl_->uniformInt(pass_,pass); Enable(GL_BLEND,pass==1); glDepthMask(pass==0);
        gl_->blendSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        for(auto batch:batches_) { glBindTexture(GL_TEXTURE_2D,batch.texture); glDrawArrays(GL_TRIANGLES,batch.first,batch.count); }
    }
    gl_->bindFbo(DrawFbo,destination_); gl_->attach(DrawFbo,ColourAttachment,GL_TEXTURE_2D,camera.colour,0); glDrawBuffer(ColourAttachment);
    if(gl_->check(DrawFbo)!=0x8cd5) return false;
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glEnable(GL_BLEND);
    gl_->blendSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    gl_->useProgram(composite_); gl_->uniformFloat(opacity_,std::clamp(opacity,0.f,1.f));
    glBindTexture(GL_TEXTURE_2D,colour_); glDrawArrays(GL_TRIANGLES,0,3); return true;
}
void BaseTextureGl::Clear() { uploaded_=0; batches_.clear(); }
void BaseTextureGl::Release() {
    if(!gl_) return;
    if(context_==wglGetCurrentContext()) {
        if(program_) gl_->deleteProgram(program_); if(composite_) gl_->deleteProgram(composite_);
        if(vao_) gl_->deleteVao(1,&vao_); if(vbo_) gl_->deleteBuffer(1,&vbo_);
        if(fbo_) gl_->deleteFbo(1,&fbo_); if(destination_) gl_->deleteFbo(1,&destination_);
        if(colour_) glDeleteTextures(1,&colour_); if(depth_) glDeleteTextures(1,&depth_);
        if(sampler_) gl_->deleteSampler(1,&sampler_); if(lightSampler_) gl_->deleteSampler(1,&lightSampler_);
    }
    delete gl_; gl_=nullptr; context_=nullptr; program_=composite_=vao_=vbo_=fbo_=destination_=colour_=depth_=sampler_=lightSampler_=0;
    width_=height_=0; Clear();
}
BaseTextureGl::~BaseTextureGl() { Release(); }
bool ValidBaseRequest(const base::Request& r,std::uint64_t now,const char* dimension,const char* server) {
    if(!r.enabled||r.count==0||r.count>base::kMaxRegions||r.sentAtMs>now||now-r.sentAtMs>base::kLeaseMs||
       !r.revision||!std::isfinite(r.opacity)||r.opacity<=0||r.opacity>1||!r.dimension[0]||
       !std::memchr(r.dimension,0,sizeof(r.dimension))||!std::memchr(r.server,0,sizeof(r.server))||
       std::strcmp(r.dimension,dimension)||std::strcmp(r.server,server)) return false;
    for(unsigned i=0;i<r.count;++i) { const auto& b=r.regions[i];
        if(!std::isfinite(b.minX)||!std::isfinite(b.minY)||!std::isfinite(b.minZ)||!std::isfinite(b.maxX)||!std::isfinite(b.maxY)||!std::isfinite(b.maxZ)||
           b.minX>=b.maxX||b.minY>=b.maxY||b.minZ>=b.maxZ||std::abs(b.minX)>3e7||std::abs(b.maxX)>3e7||
           std::abs(b.minZ)>3e7||std::abs(b.maxZ)>3e7||b.minY<-4096||b.maxY>4096||
           b.maxX-b.minX>128||b.maxY-b.minY>128||b.maxZ-b.minZ>128) return false;
    }
    return true;
}
} // namespace mcov::agent
