// Reusable borderless Win32/D3D11 shell. All UI is drawn by Dear ImGui.
#pragma once

#include <functional>
#include <string>
#include "liquid_glass.hpp"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

namespace mcov {
class FlatApp {
public:
    FlatApp() = default;
    ~FlatApp();
    FlatApp(const FlatApp&) = delete;
    FlatApp& operator=(const FlatApp&) = delete;

    bool Initialize(const wchar_t* title, int width, int height, std::string* error);
    void Shutdown();
    // Also renders from WM_TIMER during the native modal window-drag loop.
    void Run(std::function<void()> draw);
    void RequestClose() { closing_ = true; }
    void RenderFrame();

    void SetNoDragRect(float x0, float y0, float x1, float y1);
    void ClearNoDragRect() { hasNoDrag_ = false; }
    bool PointIsDraggable(int x, int y) const;
    void Panel(float x0, float y0, float x1, float y1, float rounding = 10.0f);
    int width() const { return width_; }
    int height() const { return height_; }
    void* hwnd() const { return hwnd_; }
    static constexpr int kTitleBarHeight = 58;

private:
    friend struct FlatAppRenderingCheck;
    bool CreateAppWindow(const wchar_t* title, std::string* error);
    bool CreateDevice(std::string* error);
    bool CreateRenderTarget(std::string* error);
    void UpdateWindowRegion();
    void CleanupRenderTarget();
    void ResizeIfNeeded();
    void DrawFrame();
    void EndFrameInternal();

    std::function<void()> draw_;
    LiquidGlass glass_;
    float noDrag_[4] = {};
    bool hasNoDrag_ = false;
    void* hwnd_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* backBuffer_ = nullptr;
    bool closing_ = false;
    std::string renderingFailure_;
    int width_ = 0;
    int height_ = 0;
};
} // namespace mcov
