#pragma once
#include "imgui.h"
struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace mcov {
// Einmaliges Dekodieren/Hochladen; der Renderer leiht nur die Textur-ID.
class ItemIconAtlasDX11 final {
    ID3D11ShaderResourceView* texture_{};
public:
    ItemIconAtlasDX11() = default;
    ~ItemIconAtlasDX11() { Shutdown(); }
    ItemIconAtlasDX11(const ItemIconAtlasDX11&) = delete;
    ItemIconAtlasDX11& operator=(const ItemIconAtlasDX11&) = delete;
    bool Initialize(ID3D11Device* device) noexcept;
    void Shutdown() noexcept;
    ImTextureID Texture() const noexcept { return reinterpret_cast<ImTextureID>(texture_); }
};
} // namespace mcov
