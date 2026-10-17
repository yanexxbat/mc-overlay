#pragma once
#include <windows.h>
#include <GL/gl.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include "../common/base_render_state.hpp"

namespace mcov::agent {
struct BaseMeshGroup { GLuint texture{}; std::vector<float> vertices; };
struct BaseMesh { std::vector<BaseMeshGroup> groups; double x{},y{},z{}; std::uint64_t serial{}; };
struct BaseCamera {
    double x{},y{},z{},yaw{},pitch{},fov{};
    GLuint light{},colour{}; int width{},height{};
};
// Own depth attachment ignores terrain, but keeps base geometry self-occluded.
// All methods are called with Minecraft's current OpenGL context on its render thread.
class BaseTextureGl {
    struct Api;
    Api* gl_{};
    HGLRC context_{};
    GLuint program_{},composite_{},vao_{},vbo_{},fbo_{},destination_{},colour_{},depth_{},sampler_{},lightSampler_{};
    GLint matrix_{},pass_{},opacity_{};
    int width_{},height_{};
    std::uint64_t uploaded_{};
    struct Batch { GLuint texture{}; GLint first{}; GLsizei count{}; };
    std::vector<Batch> batches_;
    bool Init();
    bool Resize(int width,int height);
public:
    ~BaseTextureGl();
    bool Draw(const BaseMesh& mesh,const BaseCamera& camera,float opacity);
    void Clear();
    void Release();
};
template<std::size_t Capacity> bool ValidBaseRequest(const base::TextureRequest<Capacity>& r,std::uint64_t now,const char* dimension,const char* server) {
    if(!r.enabled||r.count==0||r.count>Capacity||r.sentAtMs>now||now-r.sentAtMs>base::kLeaseMs||
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
