#include "flat_app.hpp"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM for the drag hit-test

#include <d3d11.h>
#include <dwmapi.h>

#include <cwchar>
#include <cstdio>
#include <stdexcept>

#include "flat_style.hpp"
#include "log.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace mcov {
namespace {

constexpr wchar_t kClassName[]    = L"mcov_flat_app";
constexpr UINT_PTR kMoveTimerId   = 1;

std::string RenderFailure(const char* action,HRESULT result) {
    char text[160]{};
    std::snprintf(text,sizeof(text),"%s (HRESULT 0x%08lX)",action,unsigned long(result));
    return text;
}

FlatApp* AppFromWindow(HWND hwnd) {
    return reinterpret_cast<FlatApp*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT WINAPI FlatAppWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 1;

    FlatApp* app = AppFromWindow(hwnd);

    switch (msg) {
        case WM_NCHITTEST: {
            // Borderless, so Windows gives us no caption to drag. Treat the top
            // strip as one - but never where ImGui wants the mouse, and never
            // over a registered no-drag rect (our close button is drawn on a
            // draw list, so ImGui does not claim it and it would otherwise be
            // swallowed as a caption drag).
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ::ScreenToClient(hwnd, &pt);
            if (pt.y >= 0 && pt.y < FlatApp::kTitleBarHeight &&
                (!ImGui::GetCurrentContext()||!ImGui::GetIO().WantCaptureMouse) &&
                (!app || app->PointIsDraggable(pt.x, pt.y))) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        // A caption drag enters a modal loop that never returns to our own
        // PeekMessage loop. A timer is the supported way back in: it fires
        // inside that loop, so we can keep rendering while the window moves.
        case WM_ENTERSIZEMOVE:
            ::SetTimer(hwnd, kMoveTimerId, 15, nullptr);
            return 0;
        case WM_EXITSIZEMOVE:
            ::KillTimer(hwnd, kMoveTimerId);
            return 0;
        case WM_TIMER:
            if (wParam == kMoveTimerId && app) app->RenderFrame();
            return 0;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

FlatApp::~FlatApp() { Shutdown(); }

bool FlatApp::CreateAppWindow(const wchar_t* title, std::string* error) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &FlatAppWndProc;
    wc.hInstance     = ::GetModuleHandleW(nullptr);
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    ::RegisterClassExW(&wc);

    // Innerhalb der Arbeitsflaeche bleiben, auch bei hoher Windows-Skalierung.
    RECT work{0,0,::GetSystemMetrics(SM_CXSCREEN),::GetSystemMetrics(SM_CYSCREEN)};
    ::SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    width_=std::min(width_,int(work.right-work.left)-24);
    height_=std::min(height_,int(work.bottom-work.top)-24);
    const int x=work.left+(work.right-work.left-width_)/2;
    const int y=work.top+(work.bottom-work.top-height_)/2;

    hwnd_ = ::CreateWindowExW(WS_EX_LAYERED | WS_EX_APPWINDOW,
                              kClassName, title, WS_POPUP,
                              x, y, width_, height_,
                              nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd_) {
        if (error) *error = "CreateWindowExW failed";
        return false;
    }

    auto hwnd = static_cast<HWND>(hwnd_);

    // So the window procedure can reach us for hit-testing and move-time frames.
    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Match the overlay: DWM composites the panel alpha, keeping text opaque.
    const MARGINS margins{-1, -1, -1, -1};
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);
    if(!::SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA)) {
        if(error) *error="SetLayeredWindowAttributes failed";
        return false;
    }
    UpdateWindowRegion();
    return true;
}

void FlatApp::UpdateWindowRegion() {
    // Windows schneidet die transparenten Aussenraender zuverlaessig aus.
    const auto region=::CreateRoundRectRgn(12,12,width_-11,height_-11,24,24);
    if(region&&!::SetWindowRgn(static_cast<HWND>(hwnd_),region,TRUE)) ::DeleteObject(region);
    // Bei Erfolg gehoert die Region Windows, deshalb dann kein DeleteObject.
}

bool FlatApp::CreateDevice(std::string* error) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = static_cast<HWND>(hwnd_);
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL wanted[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL       obtained{};

    HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, wanted, 2, D3D11_SDK_VERSION,
        &sd, &swapChain_, &device_, &obtained, &context_);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = ::D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, wanted, 2, D3D11_SDK_VERSION,
            &sd, &swapChain_, &device_, &obtained, &context_);
    }
    if (FAILED(hr)) {
        if (error) *error = RenderFailure("D3D11CreateDeviceAndSwapChain failed",hr);
        return false;
    }
    return CreateRenderTarget(error);
}

bool FlatApp::CreateRenderTarget(std::string* error) {
    ID3D11Texture2D* back = nullptr;
    HRESULT hr=swapChain_->GetBuffer(0, IID_PPV_ARGS(&back));
    if(SUCCEEDED(hr)&&back) {
        hr=device_->CreateRenderTargetView(back, nullptr, &backBuffer_);
    }
    if(back) back->Release();
    if(FAILED(hr)||!backBuffer_) {
        if(error) *error=RenderFailure("Injector render target failed",FAILED(hr)?hr:E_FAIL);
        return false;
    }
    return true;
}

void FlatApp::CleanupRenderTarget() {
    if (backBuffer_) { backBuffer_->Release(); backBuffer_ = nullptr; }
}

bool FlatApp::Initialize(const wchar_t* title, int width, int height, std::string* error) {
    ImGui_ImplWin32_EnableDpiAwareness();
    closing_=false; renderingFailure_.clear();
    width_  = width;
    height_ = height;

    if (!CreateAppWindow(title, error)) { Shutdown(); return false; }
    if (!CreateDevice(error)) { Shutdown(); return false; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    style::ApplyFlatTheme(ImGui::GetStyle());

    if(!ImGui_ImplWin32_Init(hwnd_)||!ImGui_ImplDX11_Init(device_, context_)) {
        if(error) *error="Injector ImGui backend initialization failed";
        Shutdown(); return false;
    }
    style::LoadFonts();

    return true;
}

void FlatApp::Shutdown() {
    if (ImGui::GetCurrentContext()) {
        if(ImGui::GetIO().BackendRendererUserData) ImGui_ImplDX11_Shutdown();
        if(ImGui::GetIO().BackendPlatformUserData) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    CleanupRenderTarget();
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (context_)   { context_->Release();   context_   = nullptr; }
    if (device_)    { device_->Release();    device_    = nullptr; }

    if (hwnd_) {
        ::DestroyWindow(static_cast<HWND>(hwnd_));
        ::UnregisterClassW(kClassName, ::GetModuleHandleW(nullptr));
        hwnd_ = nullptr;
    }
}

void FlatApp::SetNoDragRect(float x0, float y0, float x1, float y1) {
    noDrag_[0] = x0; noDrag_[1] = y0; noDrag_[2] = x1; noDrag_[3] = y1;
    hasNoDrag_ = true;
}

bool FlatApp::PointIsDraggable(int x, int y) const {
    if (!hasNoDrag_) return true;
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return !(fx >= noDrag_[0] && fx <= noDrag_[2] && fy >= noDrag_[1] && fy <= noDrag_[3]);
}

void FlatApp::ResizeIfNeeded() {
    RECT rc{};
    if (!::GetClientRect(static_cast<HWND>(hwnd_), &rc)) return;
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if ((w == width_ && h == height_) || w <= 0 || h <= 0) return;

    width_ = w;
    height_ = h;
    context_->OMSetRenderTargets(0,nullptr,nullptr);
    CleanupRenderTarget();
    const auto resized=swapChain_->ResizeBuffers(0, static_cast<UINT>(w), static_cast<UINT>(h), DXGI_FORMAT_UNKNOWN, 0);
    if(FAILED(resized)) throw std::runtime_error(RenderFailure("Injector resize failed",resized));
    std::string failure;
    if(!CreateRenderTarget(&failure)) throw std::runtime_error(failure);
    UpdateWindowRegion();
}

void FlatApp::Run(std::function<void()> draw) {
    draw_ = std::move(draw);
    // Erst einen vollstaendigen Frame praesentieren, dann das Fenster zeigen.
    RenderFrame();
    if(!renderingFailure_.empty()) throw std::runtime_error(renderingFailure_);
    ::ShowWindow(static_cast<HWND>(hwnd_), SW_SHOW);
    ::UpdateWindow(static_cast<HWND>(hwnd_));

    MSG msg;
    while (!closing_) {
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) closing_ = true;
        }
        if (closing_) break;
        if(::IsIconic(static_cast<HWND>(hwnd_))) {
            ::MsgWaitForMultipleObjects(0,nullptr,FALSE,100,QS_ALLINPUT); continue;
        }
        RenderFrame();
    }
    if(!renderingFailure_.empty()) throw std::runtime_error(renderingFailure_);
}

void FlatApp::RenderFrame() {
    if (!device_ || !draw_ || closing_) return;
    try {
    ResizeIfNeeded();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    draw_();
    EndFrameInternal();
    } catch(const std::exception& error) {
        // Auch WM_TIMER waehrend eines Fenster-Drags ruft uns auf. Exceptions
        // duerfen die Windows-Callback-Grenze nicht verlassen.
        renderingFailure_=error.what(); closing_=true;
        MCOV_LOG_ERROR("%s",renderingFailure_.c_str());
    }
}

void FlatApp::Panel(float x0, float y0, float x1, float y1, float rounding) {
    style::DrawFlatPanel(*ImGui::GetBackgroundDrawList(),
                         ImVec2(x0, y0), ImVec2(x1, y1), rounding, style::kBackground);
}

void FlatApp::DrawFrame() {
    ImGui::Render();

    if (backBuffer_) {
        context_->OMSetRenderTargets(1, &backBuffer_, nullptr);
        const float clear[4] = {0, 0, 0, 0};
        context_->ClearRenderTargetView(backBuffer_, clear);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}

void FlatApp::EndFrameInternal() {
    DrawFrame();
    const auto presented=swapChain_->Present(1, 0);
    if(FAILED(presented)) throw std::runtime_error(RenderFailure("Injector Present failed",presented));
}

} // namespace mcov
