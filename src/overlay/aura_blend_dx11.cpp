#include "aura_blend_dx11.hpp"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace mcov {
AuraBlendDX11::~AuraBlendDX11() { Shutdown(); }
bool AuraBlendDX11::Initialize(ID3D11Device* device) noexcept {
    Shutdown();
    if (!device) return false;
    D3D11_BLEND_DESC desc{};
    auto& target = desc.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    target.DestBlend = D3D11_BLEND_ONE;       // RGB = Quelle * Alpha + Ziel.
    target.BlendOp = D3D11_BLEND_OP_ADD;
    // Das externe Fenster benötigt eine echte Alpha-Abdeckung. Alpha null
    // würde den Glow beim Compositing mit Minecraft unsichtbar machen.
    target.SrcBlendAlpha = D3D11_BLEND_ONE;
    target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device->CreateBlendState(&desc, &state_))) return false;
    // Eine einzige, mathematisch erzeugte Gauß-Textur: kein Shader-Blur und
    // keine Textur-/RenderTarget-Allokation während des Render-Loops.
    constexpr unsigned size=128;
    std::array<unsigned char,size*size*4> pixels{};
    const float edge=std::exp(-4.5f);
    for (unsigned y=0;y<size;++y) for (unsigned x=0;x<size;++x) {
        const float nx=(float(x)+.5f)/size*2-1, ny=(float(y)+.5f)/size*2-1;
        const float alpha=std::clamp((std::exp(-4.5f*(nx*nx+ny*ny))-edge)/(1-edge),0.f,1.f);
        const auto offset=(y*size+x)*4;
        pixels[offset]=pixels[offset+1]=pixels[offset+2]=255;
        pixels[offset+3]=static_cast<unsigned char>(alpha*255+.5f);
    }
    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width=textureDesc.Height=size;
    textureDesc.MipLevels=textureDesc.ArraySize=1;
    textureDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count=1;
    textureDesc.Usage=D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA data{pixels.data(),size*4,0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&textureDesc,&data,&texture)) ||
        FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&glowTexture_))) {
        Shutdown(); return false;
    }
    return true;
}
void AuraBlendDX11::Shutdown() noexcept {
    if (glowTexture_) { glowTexture_->Release(); glowTexture_=nullptr; }
    if (state_) { state_->Release(); state_ = nullptr; }
}
AuraBlend AuraBlendDX11::Callbacks() const noexcept { return {state_ ? Apply : nullptr, state_}; }
ImTextureID AuraBlendDX11::GlowTexture() const noexcept { return reinterpret_cast<ImTextureID>(glowTexture_); }
void AuraBlendDX11::Apply(const ImDrawList*, const ImDrawCmd* command) {
    auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if (state && command->UserCallbackData)
        state->DeviceContext->OMSetBlendState(static_cast<ID3D11BlendState*>(command->UserCallbackData),
                                             nullptr, 0xffffffff);
}
} // namespace mcov
