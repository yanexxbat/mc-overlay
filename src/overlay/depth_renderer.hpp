#pragma once
#include "../common/depth_channel.hpp"
#include "depth_tags.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace mcov {
// Zusatz-Tiefenattribute behalten das originale ImDrawVert-/ImGui-Layout bei.
class DepthRenderer final {
    static constexpr UINT kVertexCapacity=1000000;
    template<class T> using Com=Microsoft::WRL::ComPtr<T>;
    Com<ID3D11VertexShader> vertex_;
    Com<ID3D11PixelShader> pixel_;
    Com<ID3D11InputLayout> layout_;
    Com<ID3D11Texture2D> texture_;
    Com<ID3D11ShaderResourceView> depthView_;
    Com<ID3D11Buffer> tagBuffer_,constants_;
    std::vector<float> tags_;
    ImDrawList* draw_{};
    int vertexOffset_{},indexOffset_{};
    struct Params { float width{},height{},nearPlane{},farPlane{}; std::uint32_t vertexOffset{},valid{}; float bias=.015f,relativeBias=.001f; } params_;
    static void Render(const ImDrawList*,const ImDrawCmd*);
public:
    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void Begin(ImDrawList& draw) { draw_=&draw; tags_.clear(); }
    std::vector<float>& Tags() { return tags_; }
    bool Finish(ID3D11DeviceContext* context,ImDrawData& data,const DepthFrame* frame);
};
}
