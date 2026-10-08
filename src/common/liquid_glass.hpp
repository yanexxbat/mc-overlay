#pragma once
#include "imgui.h"
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace mcov {
// GPU-Material fuer unabhaengige Fenster mit gemeinsamem Backdrop.
// Kein Zugriff auf JVM oder Spielzustand.
class LiquidGlass final {
public:
    LiquidGlass();
    ~LiquidGlass();
    LiquidGlass(const LiquidGlass&) = delete;
    LiquidGlass& operator=(const LiquidGlass&) = delete;
    bool Initialize(ID3D11Device*, ID3D11DeviceContext*);
    void Shutdown();
    // Optional: bereits vorhandene Client-Textur statt Desktop-Capture (z.B. Preview).
    // Die Textur wird waehrend dieses Aufrufs kopiert; keine eigene Lebensdauerannahme.
    void BeginFrame(void* hwnd, ImVec2 size, bool visible, bool allowCapture = true,
                    ID3D11Texture2D* sceneTexture = nullptr);
    void RenderTexture();
    bool ready() const;
    bool liveBackdrop() const;
    const char* captureStatus() const;
    static void Panel(ImDrawList&, ImVec2 min, ImVec2 max, float radius = 22.f);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mcov
