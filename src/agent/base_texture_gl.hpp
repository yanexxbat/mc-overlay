#pragma once
#include <windows.h>
#include <GL/gl.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
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
bool ValidBaseRequest(const base::Request& request,std::uint64_t now,const char* dimension,const char* server);
} // namespace mcov::agent
