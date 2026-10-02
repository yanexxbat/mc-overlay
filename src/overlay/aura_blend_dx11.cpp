#include "aura_blend_dx11.hpp"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

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
    return SUCCEEDED(device->CreateBlendState(&desc, &state_));
}
void AuraBlendDX11::Shutdown() noexcept {
    if (state_) { state_->Release(); state_ = nullptr; }
}
AuraBlend AuraBlendDX11::Callbacks() const noexcept { return {state_ ? Apply : nullptr, state_}; }
void AuraBlendDX11::Apply(const ImDrawList*, const ImDrawCmd* command) {
    auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if (state && command->UserCallbackData)
        state->DeviceContext->OMSetBlendState(static_cast<ID3D11BlendState*>(command->UserCallbackData),
                                             nullptr, 0xffffffff);
}
} // namespace mcov
