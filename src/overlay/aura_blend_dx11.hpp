#pragma once
#include "aura_renderer.hpp"
struct ID3D11Device;
struct ID3D11BlendState;

namespace mcov {
// Besitzt genau einen GPU-Zustand. Kein Query/Allocate im Draw-Callback.
class AuraBlendDX11 final {
public:
    AuraBlendDX11() = default;
    ~AuraBlendDX11();
    AuraBlendDX11(const AuraBlendDX11&) = delete;
    AuraBlendDX11& operator=(const AuraBlendDX11&) = delete;
    bool Initialize(ID3D11Device* device) noexcept;
    void Shutdown() noexcept;
    AuraBlend Callbacks() const noexcept;
private:
    static void Apply(const ImDrawList*, const ImDrawCmd* command);
    ID3D11BlendState* state_{};
};
} // namespace mcov
