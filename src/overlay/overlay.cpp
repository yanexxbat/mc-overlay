#include "overlay.hpp"
#include "../combat/debug_hud.hpp"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <windows.h>

#include <d3d11.h>
#include <dwmapi.h>

#include <cwchar>

#include "../common/flat_style.hpp"
#include "../common/log.hpp"
#include "render_math.hpp"
#include "aura_projection.hpp"

namespace style = mcov::style;

// Provided by imgui_impl_win32.cpp.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
constexpr wchar_t kWindowClass[] = L"mcov_overlay_window";

// WDA_EXCLUDEFROMCAPTURE. Declared by name because older SDK headers gate it
// behind a newer NTDDI level than we target.
constexpr DWORD kExcludeFromCapture = 0x11;
constexpr DWORD kCaptureNormal      = 0x00;
} // namespace

namespace mcov {
namespace {

ImU32 PackColor(const float rgba[4], float alphaScale = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(rgba[0], rgba[1], rgba[2], rgba[3] * alphaScale));
}

std::string NarrowUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                          out.data(), size, nullptr, nullptr);
    return out;
}

using style::SectionHeader;
// Rows retain the caller's real settings; no separate demo module state.
namespace panel = control_panel;
template<class Draw> void MenuColumns(const char* id, Draw draw) {
    const int columns = ImGui::GetContentRegionAvail().x >= 540 ? 2 : 1;
    style::StyleScope spacing;
    spacing.Var(ImGuiStyleVar_CellPadding,ImVec2(4 * panel::Scale(),0));
    if (!ImGui::BeginTable(id, columns, ImGuiTableFlags_SizingStretchSame)) return;
    for (int group=0; group<2; ++group) {
        ImGui::TableNextColumn();
        draw(group);
    }
    ImGui::EndTable();
}
void MenuHeading(const char* text) {
    panel::Section(text);
}

LRESULT WINAPI OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 1;

    switch (msg) {
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            // We repaint the whole client area every frame; letting Windows
            // erase it first produces a visible flicker on resize.
            return 1;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

const char* StatusText(AgentStatus status) {
    switch (status) {
        case AgentStatus::Booting:           return "booting";
        case AgentStatus::ResolvingMappings: return "resolving mappings";
        case AgentStatus::Running:           return "running";
        case AgentStatus::NoClientInstance:  return "no world loaded";
        case AgentStatus::MappingsFailed:    return "mapping lookup failed";
        case AgentStatus::Detaching:         return "detaching";
        case AgentStatus::WaitingForFrame:   return "waiting for camera frame";
    }
    return "unknown";
}

} // namespace

Overlay::~Overlay() { Shutdown(); }

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
bool Overlay::CreateOverlayWindow(std::string* error) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &OverlayWndProc;
    wc.hInstance     = ::GetModuleHandleW(nullptr);
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    ::RegisterClassExW(&wc);   // duplicate registration is harmless

    // WS_EX_LAYERED is what lets DWM composite our per-pixel alpha;
    // WS_EX_TOOLWINDOW keeps us out of alt-tab and the taskbar.
    hwnd_ = ::CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              kWindowClass, L"mcov overlay",
                              WS_POPUP,
                              windowX_, windowY_, windowW_, windowH_,
                              nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd_) {
        if (error) *error = "CreateWindowExW failed for the overlay window";
        return false;
    }

    auto hwnd = static_cast<HWND>(hwnd_);

    // Negative margins extend the DWM "glass frame" over the entire client
    // area, which is what makes alpha=0 pixels genuinely see-through rather
    // than composited against black. Without this the overlay is an opaque
    // rectangle over the game.
    const MARGINS margins{-1, -1, -1, -1};
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    ApplyExtendedStyles(/*clickThrough=*/true);
    ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    ::UpdateWindow(hwnd);
    return true;
}

bool Overlay::CreateDeviceD3D(std::string* error) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;   // track the window
    sd.BufferDesc.Height                  = 0;
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
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, wanted, 2, D3D11_SDK_VERSION,
        &sd, &swapChain_, &device_, &obtained, &context_);

    if (hr == DXGI_ERROR_UNSUPPORTED) {
        // WARP keeps the overlay usable on machines with no D3D11 hardware path,
        // at a real performance cost.
        MCOV_LOG_WARN("no hardware D3D11 device; falling back to WARP");
        hr = ::D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, wanted, 2, D3D11_SDK_VERSION,
            &sd, &swapChain_, &device_, &obtained, &context_);
    }

    if (FAILED(hr)) {
        if (error) *error = "D3D11CreateDeviceAndSwapChain failed";
        return false;
    }

    // Do not queue several old ESP frames behind Minecraft's current frame.
    IDXGIDevice1* dxgiDevice = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
        dxgiDevice->SetMaximumFrameLatency(1);
        dxgiDevice->Release();
    }

    if (!auraBlend_.Initialize(device_)) {
        if (error) *error = "could not create the Aura blend state";
        CleanupDeviceD3D();
        return false;
    }
    if (!itemIcons_.Initialize(device_)) {
        if (error) *error="could not load the embedded equipment icon atlas";
        CleanupDeviceD3D();
        return false;
    }
    CreateRenderTarget();
    return true;
}

void Overlay::CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&back))) && back) {
        device_->CreateRenderTargetView(back, nullptr, &backBuffer_);
        back->Release();
    }
}

void Overlay::CleanupRenderTarget() {
    if (backBuffer_) { backBuffer_->Release(); backBuffer_ = nullptr; }
}

void Overlay::CleanupDeviceD3D() {
    itemIcons_.Shutdown();
    auraBlend_.Shutdown();
    auraRenderer_.Reset();
    targetCube_.Reset();
    auraPrepared_ = false;
    CleanupRenderTarget();
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (context_)   { context_->Release();   context_   = nullptr; }
    if (device_)    { device_->Release();    device_    = nullptr; }
}

void Overlay::ApplyCaptureAffinity() {
    if (!hwnd_) return;
    // Independent recording preference; no desktop capture is needed by the UI.
    ::SetWindowDisplayAffinity(static_cast<HWND>(hwnd_),
                               settings_.excludeFromCapture ? kExcludeFromCapture : kCaptureNormal);
}

bool Overlay::Initialize(const TargetProcess& target, std::string* error) {
    ImGui_ImplWin32_EnableDpiAwareness();
    target_ = target;

    if (!GetClientRectOnScreen(target_.window, windowX_, windowY_, windowW_, windowH_)) {
        if (error) *error = "could not read the game window's client rectangle";
        return false;
    }

    if (!CreateOverlayWindow(error)) return false;
    if (!CreateDeviceD3D(error)) { Shutdown(); return false; }

    ApplyCaptureAffinity();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // do not litter imgui.ini next to the exe
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    style::ApplyFlatTheme(ImGui::GetStyle());

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, context_);

    style::LoadFonts();

    // Connect to the agent. A failure here is not fatal: the overlay still runs
    // and the Settings tab offers to load the agent.
    std::string ipcError;
    agentConnected_ = channel_.OpenReader(target_.pid, &ipcError);
    if (!agentConnected_) {
        MCOV_LOG_WARN("agent not connected: %s", ipcError.c_str());
        statusLine_ = "agent not loaded";
    }

    MCOV_LOG_INFO("overlay window %dx%d at (%d,%d)", windowW_, windowH_, windowX_, windowY_);
    return true;
}

void Overlay::Shutdown() {
    if(auto* block=channel_.block()) {
        CombatRequest stop;
        stop.sentAtMs=::GetTickCount64(); stop.revision=combatMenu_.Store().Read().revision+1;
        block->combatRequest.Publish(stop);
    }
    if (device_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    CleanupDeviceD3D();

    if (hwnd_) {
        ::DestroyWindow(static_cast<HWND>(hwnd_));
        ::UnregisterClassW(kWindowClass, ::GetModuleHandleW(nullptr));
        hwnd_ = nullptr;
    }
    channel_.Close();
}

void Overlay::PumpMessages() {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) quitRequested_ = true;
    }
}

// ---------------------------------------------------------------------------
// Win32 window styling
// ---------------------------------------------------------------------------
void Overlay::ApplyExtendedStyles(bool clickThrough) {
    auto window = static_cast<HWND>(hwnd_);
    if (!window) return;

    LONG_PTR style = ::GetWindowLongPtrW(window, GWL_EXSTYLE);

    // WS_EX_LAYERED is mandatory for per-pixel alpha; WS_EX_TOOLWINDOW hides us
    // from alt-tab and the taskbar so the overlay never becomes a target itself.
    style |= WS_EX_LAYERED | WS_EX_TOOLWINDOW;

    if (clickThrough) {
        // WS_EX_TRANSPARENT makes WM_NCHITTEST return HTTRANSPARENT, so every
        // click falls through to Minecraft. WS_EX_NOACTIVATE stops us grabbing
        // focus when Windows repositions or re-shows the window.
        style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    } else {
        style &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }

    ::SetWindowLongPtrW(window, GWL_EXSTYLE, style);

    // LWA_ALPHA, not LWA_COLORKEY: colour-keying would discard the framebuffer's
    // own alpha channel and give hard-edged, aliased widgets. 255 here means
    // "use the per-pixel alpha as-is" for the whole-window multiplier.
    ::SetLayeredWindowAttributes(window, 0, 255, LWA_ALPHA);

    // SWP_NOACTIVATE keeps the topmost re-assert from stealing focus.
    ::SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    clickThrough_ = clickThrough;
}

void Overlay::TrackTargetWindow() {
    int x = 0, y = 0, width = 0, height = 0;
    if (!GetClientRectOnScreen(target_.window, x, y, width, height)) return;

    const bool moved   = (x != windowX_ || y != windowY_);
    const bool resized = (width != windowW_ || height != windowH_);

    if (moved || resized) {
        ::SetWindowPos(static_cast<HWND>(hwnd_), nullptr, x, y, width, height,
                       SWP_NOACTIVATE | SWP_NOZORDER);
        windowX_ = x;
        windowY_ = y;

        if (resized) {
            windowW_ = width;
            windowH_ = height;
            // The swap chain must be resized explicitly; there is no GLFW-style
            // automatic framebuffer tracking here.
            if (swapChain_) {
                CleanupRenderTarget();
                swapChain_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                          DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
        }
    }

    // Minecraft going fullscreen re-asserts its own topmost position, so we have
    // to claim it back every frame rather than only on change.
    ::SetWindowPos(static_cast<HWND>(hwnd_), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void Overlay::PollHotkeys() {
    // GetAsyncKeyState rather than GLFW key callbacks: while the menu is closed
    // the overlay is click-through and unfocused, so it receives no keyboard
    // events at all. GetAsyncKeyState reads global key state regardless of who
    // owns focus, which is the only way a toggle key can work here.
    const bool toggleDown = (::GetAsyncKeyState(settings_.toggleKey) & 0x8000) != 0;
    if (toggleDown && !toggleKeyDown_) SetMenuVisible(!menuVisible_);
    toggleKeyDown_ = toggleDown;

    const bool panicDown = (::GetAsyncKeyState(settings_.panicKey) & 0x8000) != 0;
    if (panicDown && !panicKeyDown_) {
        MCOV_LOG_INFO("panic key pressed; shutting down");
        running_ = false;
    }
    panicKeyDown_ = panicDown;
}

void Overlay::SetMenuVisible(bool visible) {
    if (menuVisible_ == visible) return;
    menuVisible_ = visible;

    if (visible) {
        menu_.pageAlpha=menu_.pageStartAlpha=1.f;
        menu_.pageElapsed=.16f;
        menu_.animateNextPage=false;
        // Drop click-through, then take focus. Minecraft releases its captured
        // cursor the moment it loses focus, so the mouse becomes usable in the
        // menu without any extra work.
        ApplyExtendedStyles(/*clickThrough=*/false);
        ForceForeground(hwnd_, settings_.focusMode);
    } else {
        menu_.dragging = false;
        ApplyExtendedStyles(settings_.clickThroughWhenIdle);
        // Handing focus back is a plain activation of a window that is already
        // allowed to have it, so it never needs the riskier modes.
        ForceForeground(target_.window,
                        settings_.focusMode == FocusMode::None ? FocusMode::None
                                                               : FocusMode::Simple);
    }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
bool Overlay::RunFrame() {
    if (!running_ || !hwnd_ || quitRequested_) return false;

    if (!IsWindowAlive(target_.window)) {
        MCOV_LOG_INFO("target window disappeared; exiting");
        return false;
    }

    PumpMessages();
    if (quitRequested_) return false;

    PollHotkeys();
    TrackTargetWindow();

    // Pull the latest snapshot. A failed read just means the writer was mid-
    // update on all attempts; we keep drawing the previous frame's data.
    if (agentConnected_ && channel_.valid()) {
        const SharedBlock* block = channel_.block();
        WorldSnapshot      fresh{};
        if (TryReadSnapshot(block, fresh)) {
            lastSnapshot_ = fresh;
            haveSnapshot_ = true;
        }

        const std::uint32_t heartbeat = block->heartbeat.load(std::memory_order_acquire);
        const auto heartbeatNow = ::GetTickCount64();
        heartbeatClock_.Observe(heartbeat, heartbeatNow);

        const auto status = static_cast<AgentStatus>(block->status.load(std::memory_order_acquire));
        statusLine_       = StatusText(status);
        if (!heartbeatClock_.Alive(heartbeatNow)) statusLine_ = "agent stalled";
    } else if (!agentConnected_ && ++reconnectCountdown_ > 60) {
        // Retry roughly once a second rather than issuing an OpenFileMapping
        // syscall on every frame, so loading the agent mid-session still works
        // without spinning.
        reconnectCountdown_ = 0;
        if (channel_.OpenReader(target_.pid, nullptr)) {
            agentConnected_ = true;
            MCOV_LOG_INFO("agent connected");
        }
    }

    const bool targetFocused = IsForegroundWindowOwnedBy(target_.pid);
    const bool selfFocused   = ::GetForegroundWindow() == static_cast<HWND>(hwnd_);
    const bool shouldDraw    = !settings_.hideWhenUnfocused || targetFocused || selfFocused;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (!auraPrepared_) {
        AuraRenderer::Prepare(*ImGui::GetBackgroundDrawList());
        FriendHatRenderer::Prepare(*ImGui::GetBackgroundDrawList());
        TargetCubeRenderer::Prepare(*ImGui::GetBackgroundDrawList());
        auraPrepared_ = true;
    }

    if (haveSnapshot_) {
        SampleTelemetry(lastSnapshot_);
        UpdateTriggerbot(lastSnapshot_);
        UpdateAimLock(lastSnapshot_);
    }

    // Re-assert every frame so the agent always reflects the UI, even after it
    // was re-injected and zeroed the block.
    if (agentConnected_) PushScanRequest();
    if (agentConnected_) PushCombatRequest();

    if (shouldDraw) {
        if(combatMenu_.Store().Read().values.enabled && combatReport_.sampledAtMs &&
           ::GetTickCount64()-combatReport_.sampledAtMs<=kCombatLeaseMs)
            combat::CombatHud::Draw(*ImGui::GetBackgroundDrawList(),combatReport_.debug,combatMenu_.Store().Read().values.hud,ImGui::GetIO().DisplaySize);
        if (haveSnapshot_) {
            DrawEsp(lastSnapshot_);
            DrawHud(lastSnapshot_);
            if (settings_.showWatermark) DrawWatermark(lastSnapshot_);
        }
        if (menuVisible_) DrawMenu();
    }

    ImGui::Render();

    if (backBuffer_) {
        context_->OMSetRenderTargets(1, &backBuffer_, nullptr);

        // Fully transparent clear. DwmExtendFrameIntoClientArea makes alpha=0
        // genuinely see-through, so every pixel we do not draw shows the game.
        const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context_->ClearRenderTargetView(backBuffer_, clear);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    swapChain_->Present(settings_.vsync ? 1 : 0, 0);

    return running_;
}

// ---------------------------------------------------------------------------
// ESP
// ---------------------------------------------------------------------------
void Overlay::DrawAura(const WorldSnapshot& snapshot, const CameraProjection& projection, float width, float height) {
    if (!agentConnected_ || !heartbeatClock_.Alive(::GetTickCount64()) || snapshot.entityCount == 0) {
        auraRenderer_.Reset();
        return;
    }
    AuraAnchor selected{};
    const AuraAnchor* target = nullptr;
    const auto count = std::min(snapshot.entityCount, kMaxEntities);
    const auto targetId=combatMenu_.Store().Read().values.enabled && combatReport_.debug.enabled && combatReport_.debug.target &&
        combatReport_.sampledAtMs && ::GetTickCount64()-combatReport_.sampledAtMs<=kCombatLeaseMs ?
        combatReport_.debug.target->id : snapshot.hasTarget ? snapshot.targetEntityId : 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto& entity = snapshot.entities[i];
        if (entity.flags & kEntityIsSelf) continue;
        AuraAnchor anchor;
        if (!MakeAuraAnchor(entity, anchor)) continue;
        auraRenderer_.RefreshAnchor(anchor);
        if (targetId && entity.entityId == targetId &&
            std::isfinite(entity.distance) && entity.distance <= visuals_.maxDistance) {
            selected = anchor;
            target = &selected;
        }
    }
    // Der eigene Aura-Schalter steuert die Umlaufbahn. Die Einstellungen für
    // reduzierte Menü-/Windows-Animationen dürfen diesen Spieleffekt nicht stoppen.
    const auto& config = auraConfig_;
    auraRenderer_.UpdateTarget(target, ImGui::GetIO().DeltaTime, config);
    auto& draw = *ImGui::GetBackgroundDrawList();
    for (const auto& item : auraRenderer_.Targets()) {
        if (!item.valid || item.alpha <= 0) continue;
        AuraWorldFog world;
        AuraScreenFog screen;
        auraRenderer_.BuildWorldFog(item.anchor,config,world);
        if (!ProjectAuraFog(projection,world,width,height,screen,ImGui::GetMainViewport()->Pos)) continue;
        auraRenderer_.Draw(draw,screen,item.alpha,config,auraBlend_.Callbacks(),auraBlend_.GlowTexture());
    }
}

void Overlay::DrawTargetCube(const WorldSnapshot& snapshot,const CameraProjection& projection,ImVec2 viewport) {
    if(!targetCubeConfig_.enabled) { targetCube_.Reset(); return; }
    const auto now=::GetTickCount64();
    const auto& report=combatReport_;
    const EntitySnapshot* selected=nullptr;
    if(agentConnected_ && heartbeatClock_.Alive(now) && combatMenu_.Store().Read().values.enabled &&
       report.status==CombatBridgeStatus::Active && report.debug.enabled && report.debug.target &&
       report.sampledAtMs && report.sampledAtMs<=now && now-report.sampledAtMs<=kCombatLeaseMs) {
        const auto& target=*report.debug.target;
        for(std::uint32_t i=0;i<std::min(snapshot.entityCount,kMaxEntities);++i) {
            const auto& e=snapshot.entities[i];
            if(e.entityId!=target.id || ((e.uuid.high || e.uuid.low) && e.uuid!=target.uuid)) continue;
            selected=&e; break;
        }
    }
    // Windows-Menüanimationen steuern diesen Spieleffekt nicht. Der explizite
    // Reduced-Motion-Schalter lässt den Marker sichtbar und friert seine Drehung ein.
    targetCube_.Update(selected,ImGui::GetIO().DeltaTime,!menu_.reduceMotion,targetCubeConfig_);
    if(!selected) return;
    TargetCubeWorld world; TargetCubeScreen screen;
    if(targetCube_.BuildWorld(*selected,snapshot,targetCubeConfig_,world) &&
       TargetCubeRenderer::Project(projection,world,viewport,screen,ImGui::GetMainViewport()->Pos))
        targetCube_.Draw(*ImGui::GetBackgroundDrawList(),screen,targetCubeConfig_,auraBlend_.Callbacks(),auraBlend_.GlowTexture());
}

void Overlay::DrawEsp(const WorldSnapshot& snapshot) {
    const bool entitiesEnabled = visuals_.boxEsp || visuals_.skeleton || visuals_.tracers ||
                                 visuals_.nameTags || visuals_.healthBars || visuals_.distanceText || visuals_.targetInfo;
    const auto relations=combatMenu_.Relations().Read();
    const bool haveFriends=std::any_of(relations.begin(),relations.end(),[](const auto& entry) {
        return entry.used && entry.relation==combat::Relation::Friend;
    });
    if (!entitiesEnabled && !blocks_.enabled && !auraConfig_.enabled &&
        !haveFriends && !combatMenu_.Store().Read().values.enabled &&
        !auraRenderer_.Targets()[0].valid && !auraRenderer_.Targets()[1].valid) { targetCube_.Reset(); return; }

    const float width  = static_cast<float>(windowW_);
    const float height = static_cast<float>(windowH_);
    if (width < 1.0f || height < 1.0f) return;

    const CameraProjection projection(snapshot, width, height, settings_.fovTrim);

    // Die Aura liegt unter ESP und HUD. Sie teilt deren kohärenten Snapshot.
    DrawAura(snapshot, projection, width, height);
    DrawTargetCube(snapshot,projection,{width,height});
    if(haveFriends && agentConnected_ && heartbeatClock_.Alive(::GetTickCount64()))
        friendHats_.DrawFriends(*ImGui::GetBackgroundDrawList(),snapshot,projection,relations,visuals_.maxDistance,
            friendHatConfig_,auraConfig_.startColor,auraConfig_.endColor,auraBlend_.Callbacks(),auraBlend_.GlowTexture(),
            {width,height},ImGui::GetMainViewport()->Pos);
    DrawBlockEsp(snapshot, projection, width, height);
    if (!entitiesEnabled) return;

    EspRenderer::Draw(*ImGui::GetBackgroundDrawList(),snapshot,projection,visuals_,
                      {width,height},ImGui::GetMainViewport()->Pos,itemIcons_.Texture());
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
namespace {

constexpr float kPanelWidth = 232.0f;
constexpr float kPanelGap   = 16.0f;

void DrawHudText(ImDrawList* draw, float x, float y, ImU32 colour, const char* text) {
    draw->AddText(ImVec2(x, y), colour, text);
}

// Minecraft yaw -> compass bearing. Yaw 0 faces +Z (south), and the mapping to
// a 0..360 bearing where 0 = north is what the F3 screen shows.
float YawToBearing(float yawDeg) {
    float bearing = std::fmod(yawDeg + 180.0f, 360.0f);
    if (bearing < 0.0f) bearing += 360.0f;
    return bearing;
}

const char* FacingName(float yawDeg) {
    // Minecraft's own convention: yaw 0 = south, +90 = west.
    float y = std::fmod(yawDeg, 360.0f);
    if (y < 0.0f) y += 360.0f;
    if (y >= 315.0f || y < 45.0f)  return "South (+Z)";
    if (y < 135.0f)                return "West (-X)";
    if (y < 225.0f)                return "North (-Z)";
    return "East (+X)";
}

const char* kSlotNames[kSlotCount] = {"Helmet", "Chest", "Legs", "Boots", "Main", "Off"};

} // namespace

void Overlay::SampleTelemetry(const WorldSnapshot& snapshot) {
    const ImGuiIO& io = ImGui::GetIO();

    // Overlay-side frame time; the agent's fps figure is Minecraft's own.
    frameMsHistory_[historyCursor_] = io.DeltaTime * 1000.0f;
    fpsHistory_[historyCursor_]     = snapshot.fps >= 0 ? static_cast<float>(snapshot.fps)
                                                        : io.Framerate;
    heapHistory_[historyCursor_]    = snapshot.heapUsedBytes > 0
                                        ? static_cast<float>(snapshot.heapUsedBytes) / (1024.0f * 1024.0f)
                                        : 0.0f;
    historyCursor_ = (historyCursor_ + 1) % kHistory;
}

float Overlay::DrawEquipmentPanel(const WorldSnapshot& snapshot, float x, float y) {
    int visible = 0;
    for (const ItemSnapshot& item : snapshot.equipment) if (item.present) ++visible;
    if (visible == 0) return 0.0f;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float lineHeight = ImGui::GetTextLineHeight() + 9.0f;
    const float height     = 24.0f + visible * lineHeight;

    style::DrawFlatPanel(*draw, ImVec2(x, y), ImVec2(x + kPanelWidth, y + height));
    DrawHudText(draw, x + 10.0f, y + 6.0f, style::kMuted, "EQUIPMENT");

    float row = y + 24.0f;
    for (int slot = 0; slot < static_cast<int>(kSlotCount); ++slot) {
        const ItemSnapshot& item = snapshot.equipment[slot];
        if (!item.present) continue;

        DrawHudText(draw, x + 10.0f, row, style::kMuted, kSlotNames[slot]);

        char label[64];
        if (item.count > 1) std::snprintf(label, sizeof(label), "%s x%d", item.name, item.count);
        else                std::snprintf(label, sizeof(label), "%s", item.name);

        const ImVec2 size = ImGui::CalcTextSize(label);
        DrawHudText(draw, x + kPanelWidth - 10.0f - size.x, row, style::kText, label);

        // Durability bar. maxDamage == 0 means "no durability", which is not the
        // same as full - those items get no bar at all.
        if (item.maxDamage > 0) {
            const float remaining = 1.0f - static_cast<float>(item.damage) / static_cast<float>(item.maxDamage);
            const float clamped   = std::clamp(remaining, 0.0f, 1.0f);

            const float barY = row + ImGui::GetTextLineHeight() + 2.0f;
            const float barW = kPanelWidth - 20.0f;
            draw->AddRectFilled(ImVec2(x + 10.0f, barY), ImVec2(x + 10.0f + barW, barY + 3.0f),
                                style::kBackground, 1.5f);

            ImU32 colour = IM_COL32(90, 200, 110, 255);
            if (clamped < hud_.durabilityWarn) {
                // Pulse so a nearly-broken tool is hard to miss.
                const float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);
                colour = IM_COL32(230, static_cast<int>(60 * pulse), 60, 255);
            } else if (clamped < 0.5f) {
                colour = IM_COL32(225, 190, 70, 255);
            }
            draw->AddRectFilled(ImVec2(x + 10.0f, barY),
                                ImVec2(x + 10.0f + barW * clamped, barY + 3.0f), colour, 1.5f);
        }
        row += lineHeight;
    }
    return height + kPanelGap;
}

float Overlay::DrawEffectsPanel(const WorldSnapshot& snapshot, float x, float y) {
    if (snapshot.effectCount == 0) return 0.0f;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float lineHeight = ImGui::GetTextLineHeight() + 4.0f;
    const float height     = 24.0f + snapshot.effectCount * lineHeight + 4.0f;

    style::DrawFlatPanel(*draw, ImVec2(x, y), ImVec2(x + kPanelWidth, y + height));
    DrawHudText(draw, x + 10.0f, y + 6.0f, style::kMuted, "EFFECTS");

    float row = y + 24.0f;
    for (std::uint32_t i = 0; i < snapshot.effectCount && i < kMaxEffects; ++i) {
        const EffectSnapshot& effect = snapshot.effects[i];

        char name[64];
        // Amplifier is zero-based: 0 is level I. Roman numerals past IV are rare
        // enough that a plain number is clearer.
        if (effect.amplifier == 0)      std::snprintf(name, sizeof(name), "%s", effect.name);
        else if (effect.amplifier == 1) std::snprintf(name, sizeof(name), "%s II", effect.name);
        else if (effect.amplifier == 2) std::snprintf(name, sizeof(name), "%s III", effect.name);
        else                            std::snprintf(name, sizeof(name), "%s %d", effect.name, effect.amplifier + 1);

        char timing[24];
        if (effect.durationTicks < 0) {
            std::snprintf(timing, sizeof(timing), "inf");
        } else {
            const int seconds = effect.durationTicks / 20;
            std::snprintf(timing, sizeof(timing), "%d:%02d", seconds / 60, seconds % 60);
        }

        // Warn when an effect is about to lapse.
        const bool expiring = effect.durationTicks >= 0 && effect.durationTicks < 200; // <10s
        const ImU32 colour  = expiring ? IM_COL32(240, 150, 70, 255) : IM_COL32(150, 210, 255, 255);

        DrawHudText(draw, x + 10.0f, row, colour, name);
        const ImVec2 size = ImGui::CalcTextSize(timing);
        DrawHudText(draw, x + kPanelWidth - 10.0f - size.x, row,
                   expiring ? colour : style::kText, timing);
        row += lineHeight;
    }
    return height + kPanelGap;
}

float Overlay::DrawCoordinatesPanel(const WorldSnapshot& snapshot, float x, float y) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float lineHeight = ImGui::GetTextLineHeight() + 3.0f;
    const float height     = 24.0f + 4 * lineHeight + 4.0f;

    style::DrawFlatPanel(*draw, ImVec2(x, y), ImVec2(x + kPanelWidth, y + height));
    DrawHudText(draw, x + 10.0f, y + 6.0f, style::kMuted, "POSITION");

    char line[96];
    float row = y + 24.0f;

    std::snprintf(line, sizeof(line), "XYZ   %.1f  %.1f  %.1f", snapshot.posX, snapshot.posY, snapshot.posZ);
    DrawHudText(draw, x + 10.0f, row, style::kText, line);
    row += lineHeight;

    // Chunk-local coordinates, the same numbers F3 shows.
    const int blockX = static_cast<int>(std::floor(snapshot.posX));
    const int blockZ = static_cast<int>(std::floor(snapshot.posZ));
    std::snprintf(line, sizeof(line), "Chunk %d %d  (%d %d)",
                  blockX >> 4, blockZ >> 4, blockX & 15, blockZ & 15);
    DrawHudText(draw, x + 10.0f, row, style::kMuted, line);
    row += lineHeight;

    // The 1:8 overworld/nether ratio, in whichever direction is useful.
    const bool inNether = std::strstr(snapshot.dimension, "nether") != nullptr;
    if (inNether) {
        std::snprintf(line, sizeof(line), "Over  %.0f  ~  %.0f", snapshot.posX * 8.0f, snapshot.posZ * 8.0f);
    } else {
        std::snprintf(line, sizeof(line), "Nether %.0f  ~  %.0f", snapshot.posX / 8.0f, snapshot.posZ / 8.0f);
    }
    DrawHudText(draw, x + 10.0f, row, IM_COL32(200, 160, 120, 255), line);
    row += lineHeight;

    std::snprintf(line, sizeof(line), "Facing %s  %.0f deg", FacingName(snapshot.yawDeg), YawToBearing(snapshot.yawDeg));
    DrawHudText(draw, x + 10.0f, row, style::kMuted, line);

    return height + kPanelGap;
}

float Overlay::DrawPerformancePanel(const WorldSnapshot& snapshot, float x, float y) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float height = 132.0f;

    style::DrawFlatPanel(*draw, ImVec2(x, y), ImVec2(x + kPanelWidth, y + height));
    DrawHudText(draw, x + 10.0f, y + 6.0f, style::kMuted, "TELEMETRY");

    char line[96];
    const float latest = frameMsHistory_[(historyCursor_ + kHistory - 1) % kHistory];
    std::snprintf(line, sizeof(line), "%d fps   %.1f ms", snapshot.fps >= 0 ? snapshot.fps
                                                                            : static_cast<int>(ImGui::GetIO().Framerate),
                  latest);
    DrawHudText(draw, x + 10.0f, y + 22.0f, style::kText, line);

    // ImGui's plot widgets need a window; on the background draw list we render
    // the series by hand, which also lets the two graphs share one panel.
    auto plot = [&](const float* values, float px, float py, float pw, float ph, ImU32 colour, float fallbackMax) {
        float maxValue = fallbackMax;
        for (int i = 0; i < kHistory; ++i) maxValue = std::max(maxValue, values[i]);
        if (maxValue <= 0.0f) return;

        draw->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), style::kBackground, 3.0f);
        ImVec2 previous;
        for (int i = 0; i < kHistory; ++i) {
            const int   index = (historyCursor_ + i) % kHistory;
            const float t     = static_cast<float>(i) / static_cast<float>(kHistory - 1);
            const ImVec2 point(px + t * pw, py + ph - (values[index] / maxValue) * ph);
            if (i > 0) draw->AddLine(previous, point, colour, 1.2f);
            previous = point;
        }
        char scale[32];
        std::snprintf(scale, sizeof(scale), "%.0f", maxValue);
        draw->AddText(ImVec2(px + 3.0f, py + 1.0f), style::kMuted, scale);
    };

    plot(frameMsHistory_, x + 10.0f, y + 40.0f, kPanelWidth - 20.0f, 34.0f, IM_COL32(120, 200, 255, 255), 20.0f);

    if (snapshot.heapMaxBytes > 0) {
        const float usedMb = static_cast<float>(snapshot.heapUsedBytes) / (1024.0f * 1024.0f);
        const float maxMb  = static_cast<float>(snapshot.heapMaxBytes) / (1024.0f * 1024.0f);
        std::snprintf(line, sizeof(line), "heap %.0f / %.0f MB", usedMb, maxMb);
        DrawHudText(draw, x + 10.0f, y + 78.0f, style::kText, line);
        plot(heapHistory_, x + 10.0f, y + 94.0f, kPanelWidth - 20.0f, 30.0f, IM_COL32(180, 140, 255, 255), maxMb * 0.25f);
    } else {
        DrawHudText(draw, x + 10.0f, y + 78.0f, IM_COL32(120, 130, 150, 255), "heap unavailable");
    }

    return height + kPanelGap;
}

void Overlay::DrawCompassStrip(const WorldSnapshot& snapshot) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    const float width      = static_cast<float>(windowW_);
    const float stripWidth = std::min(420.0f, width * 0.42f);
    const float left       = (width - stripWidth) * 0.5f;
    const float top        = 14.0f;
    const float height     = 22.0f;

    style::DrawFlatPanel(*draw, ImVec2(left, top), ImVec2(left + stripWidth, top + height), 6.0f);

    const float bearing = YawToBearing(snapshot.yawDeg);
    // 90 degrees of arc across the strip.
    const float degreesPerPixel = 90.0f / stripWidth;

    struct Mark { float bearing; const char* label; };
    static const Mark marks[] = {
        {0.0f, "N"}, {45.0f, "NE"}, {90.0f, "E"}, {135.0f, "SE"},
        {180.0f, "S"}, {225.0f, "SW"}, {270.0f, "W"}, {315.0f, "NW"},
    };

    for (const Mark& mark : marks) {
        // Shortest signed angular distance, so marks wrap smoothly past 360.
        float delta = std::fmod(mark.bearing - bearing + 540.0f, 360.0f) - 180.0f;
        const float px = left + stripWidth * 0.5f + delta / degreesPerPixel;
        if (px < left + 6.0f || px > left + stripWidth - 6.0f) continue;

        const bool cardinal = (mark.label[1] == '\0');
        const ImU32 colour  = cardinal ? style::kText : style::kMuted;

        const ImVec2 size = ImGui::CalcTextSize(mark.label);
        DrawHudText(draw, px - size.x * 0.5f, top + 4.0f, colour, mark.label);
        draw->AddLine(ImVec2(px, top + height - 4.0f), ImVec2(px, top + height - 1.0f), colour, 1.0f);
    }

    // Centre pointer.
    const float centre = left + stripWidth * 0.5f;
    draw->AddTriangleFilled(ImVec2(centre - 4.0f, top - 1.0f), ImVec2(centre + 4.0f, top - 1.0f),
                            ImVec2(centre, top + 5.0f), style::kAccent);
}

void Overlay::DrawReachMarker(const WorldSnapshot& snapshot) {
    if (!snapshot.hasTarget) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float cx = static_cast<float>(windowW_) * 0.5f;
    const float cy = static_cast<float>(windowH_) * 0.5f;

    // Minecraft sets targetedEntity only for entities inside the interaction
    // range, so its presence alone means "you can hit this right now".
    const float pulse  = 0.6f + 0.4f * std::sin(static_cast<float>(ImGui::GetTime()) * 7.0f);
    const ImU32 colour = IM_COL32(90, 240, 130, static_cast<int>(210 * pulse));

    // Four corner brackets around the crosshair.
    const float radius = 13.0f;
    const float arm    = 5.0f;
    const ImVec2 corners[4] = {
        {cx - radius, cy - radius}, {cx + radius, cy - radius},
        {cx - radius, cy + radius}, {cx + radius, cy + radius},
    };
    const float dx[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    const float dy[4] = {1.0f, 1.0f, -1.0f, -1.0f};
    for (int i = 0; i < 4; ++i) {
        draw->AddLine(corners[i], ImVec2(corners[i].x + arm * dx[i], corners[i].y), colour, 2.0f);
        draw->AddLine(corners[i], ImVec2(corners[i].x, corners[i].y + arm * dy[i]), colour, 2.0f);
    }

    char label[96];
    if (snapshot.targetMaxHealth > 0.0f) {
        std::snprintf(label, sizeof(label), "%s   %.0f/%.0f   %.1fm",
                      snapshot.targetName, snapshot.targetHealth,
                      snapshot.targetMaxHealth, snapshot.targetDistance);
    } else {
        std::snprintf(label, sizeof(label), "%s   %.1fm", snapshot.targetName, snapshot.targetDistance);
    }
    const ImVec2 size = ImGui::CalcTextSize(label);
    DrawHudText(draw, cx - size.x * 0.5f, cy + radius + 8.0f, IM_COL32(200, 250, 215, 255), label);
}

void Overlay::DrawHud(const WorldSnapshot& snapshot) {
    // Heights are derived from the snapshot before anything is drawn, so a
    // bottom-anchored stack can be positioned in one pass and the panel order
    // stays visually identical in all four corners.
    const float lineHeight = ImGui::GetTextLineHeight();

    int visibleEquipment = 0;
    for (const ItemSnapshot& item : snapshot.equipment) if (item.present) ++visibleEquipment;

    enum Kind { kEquipment, kEffects, kCoordinates, kPerformance };
    struct Panel { Kind kind; float height; };

    Panel panels[4];
    int   count = 0;

    if (hud_.equipment && visibleEquipment > 0) {
        panels[count++] = {kEquipment, 24.0f + visibleEquipment * (lineHeight + 9.0f)};
    }
    if (hud_.effects && snapshot.effectCount > 0) {
        panels[count++] = {kEffects, 24.0f + snapshot.effectCount * (lineHeight + 4.0f) + 4.0f};
    }
    if (hud_.coordinates) {
        panels[count++] = {kCoordinates, 24.0f + 4 * (lineHeight + 3.0f) + 4.0f};
    }
    if (hud_.performance) {
        panels[count++] = {kPerformance, 132.0f};
    }

    const bool  right  = (hud_.corner == 1 || hud_.corner == 3);
    const bool  bottom = (hud_.corner == 2 || hud_.corner == 3);
    const float x      = right ? static_cast<float>(windowW_) - kPanelWidth - 16.0f : 16.0f;

    float total = 0.0f;
    for (int i = 0; i < count; ++i) total += panels[i].height + kPanelGap;

    float y = bottom ? static_cast<float>(windowH_) - 16.0f - total + kPanelGap
                     : 44.0f;

    for (int i = 0; i < count; ++i) {
        switch (panels[i].kind) {
            case kEquipment:   DrawEquipmentPanel(snapshot, x, y);   break;
            case kEffects:     DrawEffectsPanel(snapshot, x, y);     break;
            case kCoordinates: DrawCoordinatesPanel(snapshot, x, y); break;
            case kPerformance: DrawPerformancePanel(snapshot, x, y); break;
        }
        y += panels[i].height + kPanelGap;
    }

    if (hud_.compass)     DrawCompassStrip(snapshot);
    if (hud_.reachMarker) DrawReachMarker(snapshot);
}

// ---------------------------------------------------------------------------
// Triggerbot
// ---------------------------------------------------------------------------
void Overlay::UpdateTriggerbot(const WorldSnapshot& snapshot) {
    if(combatMenu_.Store().Read().values.enabled) return;
    if (!combat_.triggerbot) return;

    // Never while our own menu is up - the injected click would land on the UI.
    if (menuVisible_) return;

    // Only when Minecraft actually owns the foreground, so a click can never be
    // delivered to some other application.
    if (!IsForegroundWindowOwnedBy(target_.pid)) return;

    if (combat_.requireHoldKey && !(::GetAsyncKeyState(combat_.holdKey) & 0x8000)) return;

    // hasTarget comes from Minecraft's own crosshair pick, already clamped to
    // the interaction range, so this is the game's notion of "in reach".
    if (!snapshot.hasTarget) return;
    if (snapshot.targetDistance > combat_.maxDistance) return;
    if (combat_.livingOnly && snapshot.targetMaxHealth <= 0.0f) return;

    const unsigned long long now = ::GetTickCount64();
    if (now < nextClickTick_) return;

    INPUT input[2] = {};
    input[0].type           = INPUT_MOUSE;
    input[0].mi.dwFlags     = MOUSEEVENTF_LEFTDOWN;
    input[1].type           = INPUT_MOUSE;
    input[1].mi.dwFlags     = MOUSEEVENTF_LEFTUP;
    ::SendInput(2, input, sizeof(INPUT));
    ++clickCount_;

    const float cps      = std::clamp(combat_.cps, 1.0f, 20.0f);
    const float baseMs   = 1000.0f / cps;
    // Jitter the interval so the click train is not perfectly periodic.
    const float spread   = baseMs * std::clamp(combat_.jitter, 0.0f, 0.9f);
    const float offsetMs = spread * ((static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f);
    nextClickTick_       = now + static_cast<unsigned long long>(std::max(20.0f, baseMs + offsetMs));
}

// ---------------------------------------------------------------------------
// Block ESP
// ---------------------------------------------------------------------------
void Overlay::PushScanRequest() {
    SharedBlock* block = channel_.block();
    if (!block) return;

    const std::uint32_t mask = blocks_.enabled ? blocks_.mask : 0u;
    block->scanMask.store(mask, std::memory_order_release);
    block->scanRadius.store(static_cast<std::uint32_t>(blocks_.radius), std::memory_order_release);
    block->scanIntervalMs.store(static_cast<std::uint32_t>(blocks_.interval), std::memory_order_release);

    // Auto totem lives in the same push. Both are re-asserted EVERY frame, not
    // just when a control changes: the agent zeroes the shared block when it
    // attaches, so anything set before an inject - or before a re-inject - would
    // otherwise be silently lost while the UI still showed it as enabled.
    std::uint32_t flags = block->controlFlags.load(std::memory_order_acquire);
    if (combat_.autoTotem) flags |= kCtrlAutoTotem;
    else                   flags &= ~kCtrlAutoTotem;
    const bool details=visuals_.targetInfo || (visuals_.nameTags && (visuals_.nameTagPing || visuals_.nameTagEquipment));
    if (details) flags|=kCtrlEntityDetails;
    else flags&=~kCtrlEntityDetails;
    block->controlFlags.store(flags, std::memory_order_release);
}

void Overlay::PushCombatRequest() {
    auto* block=channel_.block(); if(!block) return;
    const auto version=combatMenu_.Store().Read();
    CombatRequest request;
    request.sentAtMs=::GetTickCount64(); request.revision=version.revision;
    request.config=version.values; request.relations=combatMenu_.Relations().Read();
    request.verticalFov=haveSnapshot_ && lastSnapshot_.fovDeg>1 ? lastSnapshot_.fovDeg : 70;
    request.viewportHeight=std::max(1.f,ImGui::GetIO().DisplaySize.y);
    request.inputAllowed=!menuVisible_ && IsForegroundWindowOwnedBy(target_.pid);
    block->combatRequest.Publish(request);
    CombatReport report;
    if(block->combatReport.Read(report)) {
        combatReport_=report;
        if(report.sampledAtMs && request.sentAtMs>=report.sampledAtMs && request.sentAtMs-report.sampledAtMs>1000)
            combatReport_.status=CombatBridgeStatus::Unavailable;
        combatMenu_.Report(combatReport_);
    }
}

void Overlay::DrawBlockEsp(const WorldSnapshot& snapshot, const CameraProjection& projection,
                           float width, float height) {
    if (!blocks_.enabled || snapshot.blockCount == 0) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 centre(width * 0.5f, height);

    // One colour per kind, so a wall of ore is still readable.
    static const ImU32 kColours[kBlockKindCount] = {
        IM_COL32( 90, 230, 240, 255),  // diamond
        IM_COL32(150,  90, 200, 255),  // ancient debris
        IM_COL32(250, 210,  70, 255),  // gold
        IM_COL32(220, 190, 170, 255),  // iron
        IM_COL32( 80, 230, 120, 255),  // emerald
        IM_COL32(240,  70,  70, 255),  // redstone
        IM_COL32( 70, 120, 240, 255),  // lapis
        IM_COL32(230, 140,  90, 255),  // copper
        IM_COL32(110, 110, 120, 255),  // coal
        IM_COL32(240, 235, 225, 255),  // quartz
        IM_COL32(200, 160,  90, 255),  // chest
        IM_COL32(120, 200, 190, 255),  // spawner
    };

    for (std::uint32_t i = 0; i < snapshot.blockCount && i < kMaxBlocks; ++i) {
        const BlockSnapshot& b = snapshot.blocks[i];

        const double dx = (b.x + 0.5) - snapshot.camX;
        const double dy = (b.y + 0.5) - snapshot.camY;
        const double dz = (b.z + 0.5) - snapshot.camZ;
        const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance > blocks_.maxDraw) continue;

        ScreenRect rect;
        if (!projection.Bounds({{b.x, b.y, b.z}, {b.x + 1.0, b.y + 1.0, b.z + 1.0}}, rect)) continue;
        const float minX = rect.left, minY = rect.top, maxX = rect.right, maxY = rect.bottom;

        const ImU32 colour = kColours[b.kind < kBlockKindCount ? b.kind : 0];
        draw->AddRect(ImVec2(minX - 1.0f, minY - 1.0f), ImVec2(maxX + 1.0f, maxY + 1.0f),
                      IM_COL32(0, 0, 0, 150), 0.0f, 0, 3.0f);
        draw->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), colour, 0.0f, 0, 1.4f);

        if (blocks_.tracers) {
            draw->AddLine(centre, ImVec2((minX + maxX) * 0.5f, maxY), colour, 1.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// Aim lock
// ---------------------------------------------------------------------------
void Overlay::UpdateAimLock(const WorldSnapshot& snapshot) {
    if(combatMenu_.Store().Read().values.enabled) return;
    if (!combat_.aimLock || menuVisible_) return;
    if (!IsForegroundWindowOwnedBy(target_.pid)) return;
    if (!(::GetAsyncKeyState(combat_.aimKey) & 0x8000)) return;

    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

    // Where the camera is pointing right now, in Minecraft's convention.
    const float bestYaw   = snapshot.yawDeg;
    const float bestPitch = snapshot.pitchDeg;

    float bestError = combat_.aimFov;
    float wantYaw = 0.0f, wantPitch = 0.0f;
    bool  found = false;

    for (std::uint32_t i = 0; i < snapshot.entityCount && i < kMaxEntities; ++i) {
        const EntitySnapshot& e = snapshot.entities[i];
        if (e.distance > combat_.aimRange) continue;
        if (combat_.aimPlayersOnly && !(e.flags & kEntityIsPlayer)) continue;
        if (!(e.flags & kEntityIsLiving)) continue;

        // Aim at upper-body height rather than the feet.
        const float tx = static_cast<float>(e.x - snapshot.camX);
        const float ty = static_cast<float>((e.y + e.height * 0.75f) - snapshot.camY);
        const float tz = static_cast<float>(e.z - snapshot.camZ);

        const float horizontal = std::sqrt(tx * tx + tz * tz);
        if (horizontal < 0.01f) continue;

        // Inverse of the direction formula in render_math.hpp.
        const float targetYaw   = std::atan2(-tx, tz) * kRadToDeg;
        const float targetPitch = -std::atan2(ty, horizontal) * kRadToDeg;

        // Shortest signed angular distance, so wrapping past 180 does not send
        // the aim the long way round.
        float dYaw = std::fmod(targetYaw - bestYaw + 540.0f, 360.0f) - 180.0f;
        const float dPitch = targetPitch - bestPitch;

        const float error = std::sqrt(dYaw * dYaw + dPitch * dPitch);
        if (error < bestError) {
            bestError = error;
            wantYaw   = dYaw;
            wantPitch = dPitch;
            found     = true;
        }
    }

    if (!found) return;

    // Proportional controller rather than an exact sensitivity conversion.
    // Minecraft's mouse-to-yaw factor depends on the sensitivity setting and a
    // cubic curve; feeding the error back each frame converges without needing
    // to know it, and the gain slider is what the user tunes.
    float stepX = wantYaw * combat_.aimGain / 0.15f;
    float stepY = wantPitch * combat_.aimGain / 0.15f;
    stepX = std::clamp(stepX, -combat_.aimMaxStep, combat_.aimMaxStep);
    stepY = std::clamp(stepY, -combat_.aimMaxStep, combat_.aimMaxStep);

    if (std::abs(stepX) < 0.5f && std::abs(stepY) < 0.5f) return;

    INPUT input{};
    input.type       = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;   // relative, which is what the game reads
    input.mi.dx      = static_cast<LONG>(stepX);
    input.mi.dy      = static_cast<LONG>(stepY);
    ::SendInput(1, &input, sizeof(INPUT));

    (void)kDegToRad;
}

void Overlay::DrawWatermark(const WorldSnapshot& snapshot) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    char text[192];
    std::snprintf(text, sizeof(text), "mcov  |  %s  |  %u entities  |  %s",
                  statusLine_.c_str(), snapshot.entityCount,
                  snapshot.serverAddress[0] ? snapshot.serverAddress : "-");

    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 pos(22.0f, 14.0f);
    style::DrawFlatPanel(*draw, ImVec2(pos.x - 6.0f, pos.y - 4.0f),
                         ImVec2(pos.x + size.x + 6.0f, pos.y + size.y + 4.0f), 6.0f);
    draw->AddText(pos, style::kText, text);
}

// ---------------------------------------------------------------------------
// Theme + menu
// ---------------------------------------------------------------------------
void Overlay::DrawMenu() {
    const panel::Status status{
        agentConnected_ && heartbeatClock_.Alive(::GetTickCount64()), statusLine_.c_str(),
        lastSnapshot_.gameVersion[0] ? lastSnapshot_.gameVersion : "detecting", target_.pid};
    const auto actions=panel::Render(menu_,status,this,[](void* owner,panel::Page page) {
        auto& self=*static_cast<Overlay*>(owner);
        switch (page) {
            case panel::Page::Combat: self.DrawCombatTab(); break;
            case panel::Page::Visuals: self.DrawVisualsTab(); break;
            case panel::Page::Blocks: self.DrawBlocksTab(); break;
            case panel::Page::Hud: self.DrawHudTab(); break;
            case panel::Page::Stats: self.DrawStatsTab(self.lastSnapshot_); break;
            case panel::Page::Settings: self.DrawSettingsTab(); break;
        }
    });
    if (actions.reset) ResetMenuPage(menu_.page);
    if (actions.close) SetMenuVisible(false);
}

void Overlay::ResetMenuPage(panel::Page page) {
    switch (page) {
        case panel::Page::Combat: combat_=CombatSettings{}; combatMenu_.Reset(); nextClickTick_=0; clickCount_=0; break;
        case panel::Page::Visuals:
            visuals_=VisualSettings{}; auraConfig_=AuraConfig{}; targetCubeConfig_=TargetCubeConfig{};
            auraRenderer_.Reset(); targetCube_.Reset(); break;
        case panel::Page::Blocks: blocks_=BlockEspSettings{}; break;
        case panel::Page::Hud: hud_=HudSettings{}; break;
        case panel::Page::Stats: break;
        case panel::Page::Settings:
            settings_=OverlaySettings{};
            ApplyCaptureAffinity();
            ApplyExtendedStyles(!menuVisible_ && settings_.clickThroughWhenIdle);
            break;
    }
    PushScanRequest();
}


void Overlay::DrawVisualsTab() {
    MenuColumns("visual_columns",[&](int column) {
        if (column==0) {
            if (panel::BeginRows("visual_rendering")) {
                panel::CheckRow("Box ESP",&visuals_.boxEsp);
                panel::CheckRow("Skeleton",&visuals_.skeleton);
                panel::CheckRow("Filled boxes",&visuals_.filledBox);
                panel::CheckRow("Tracers",&visuals_.tracers);
                panel::CheckRow("Name tags",&visuals_.nameTags);
                panel::CheckRow("Health bars",&visuals_.healthBars);
                panel::CheckRow("Distance labels",&visuals_.distanceText);
                panel::CheckRow("Target info",&visuals_.targetInfo);
                ImGui::EndTable();
            }
            MenuHeading("Name tags");
            if (panel::BeginRows("nametag_controls")) {
                ImGui::BeginDisabled(!visuals_.nameTags);
                panel::CheckRow("Show HP",&visuals_.nameTagHealth);
                panel::CheckRow("Player ping",&visuals_.nameTagPing);
                panel::CheckRow("Armour / held items",&visuals_.nameTagEquipment);
                ImGui::EndDisabled();
                ImGui::EndTable();
            }
            MenuHeading("Target effects");
            style::StyleScope tabs;
            tabs.Color(ImGuiCol_TabSelected,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
            tabs.Color(ImGuiCol_TabHovered,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
            tabs.Color(ImGuiCol_TabSelectedOverline,ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
            if(ImGui::BeginTabBar("target_effect_tabs",ImGuiTabBarFlags_FittingPolicyScroll)) {
                if(ImGui::BeginTabItem("Target aura")) {
                    DrawTargetAuraSettings(); ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Target marker")) {
                    DrawTargetMarkerSettings(); ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        } else {
            if (panel::BeginRows("visual_filters")) {
                panel::CheckRow("Players",&visuals_.showPlayers);
                panel::CheckRow("Hostile mobs",&visuals_.showHostiles);
                panel::CheckRow("Other entities",&visuals_.showOther);
                panel::SliderRow("Max distance",&visuals_.maxDistance,8,512,"%.0f m");
                panel::SliderRow("Box thickness",&visuals_.boxThickness,.5f,4,"%.1f px");
                panel::SliderRow("Box rounding",&visuals_.boxRounding,0,16,"%.0f px");
                panel::SliderRow("Skeleton thickness",&visuals_.skeletonThickness,.5f,3,"%.1f px");
                panel::SliderRow("Label size",&visuals_.labelSize,9,18,"%.0f px");
                ImGui::EndTable();
            }
            MenuHeading("Colours");
            if (panel::BeginRows("visual_colours")) {
                panel::ColorRow("Players",visuals_.playerColor);
                panel::ColorRow("Hostiles",visuals_.hostileColor);
                panel::ColorRow("Other entities",visuals_.otherColor);
                panel::ColorRow("Health accent",visuals_.healthColor);
                panel::ColorRow("Tracers",visuals_.tracerColor);
                ImGui::EndTable();
            }
        }
    });
}

void Overlay::DrawTargetAuraSettings() {
    if(panel::BeginRows("aura_controls")) {
        panel::CheckRow("Enable aura",&auraConfig_.enabled);
        ImGui::BeginDisabled(!auraConfig_.enabled);
        panel::SliderIntRow("Orbs",&auraConfig_.orbCount,1,AuraRenderer::kMaxOrbs,"%d");
        panel::SliderRow("Orbit radius",&auraConfig_.orbitRadius,.3f,2,"%.2f x");
        panel::SliderRow("Orb size",&auraConfig_.orbSize,.05f,.5f,"%.2f x");
        panel::SliderRow("Trail length",&auraConfig_.trailDegrees,10,220,"%.0f deg");
        panel::SliderRow("Intensity",&auraConfig_.opacity,0,1,"%.2f");
        panel::CheckRow("Animation",&auraConfig_.animate);
        ImGui::BeginDisabled(!auraConfig_.animate);
        panel::SliderRow("Rotation",&auraConfig_.rotationDegreesPerSecond,-90,90,"%.0f deg/s");
        panel::SliderRow("Pulse amplitude",&auraConfig_.pulseAmplitude,0,.3f,"%.2f");
        panel::SliderRow("Pulse frequency",&auraConfig_.pulseHz,0,3,"%.2f Hz");
        ImGui::EndDisabled();
        panel::SliderRow("Target fade",&auraConfig_.fadeSeconds,0,.5f,"%.2f s");
        panel::ColorRow("Orb glow",&auraConfig_.startColor.x);
        panel::ColorRow("Trail colour",&auraConfig_.endColor.x);
        ImGui::EndDisabled(); ImGui::EndTable();
    }
    ImGui::TextWrapped("Orbs follow the Kill Aura target, or the crosshair target when Kill Aura is off.");
}

void Overlay::DrawTargetMarkerSettings() {
    auto& c=targetCubeConfig_;
    if(panel::BeginRows("marker_controls")) {
        panel::CheckRow("Enable marker",&c.enabled);
        ImGui::BeginDisabled(!c.enabled);
        panel::SliderRow("Size",&c.sizeFraction,.3f,1.5f,"%.2f x");
        float width=c.strokeFraction*100;
        if(panel::SliderRow("Line width",&width,.5f,8,"%.1f %%")) c.strokeFraction=width/100;
        panel::SliderRow("Glow intensity",&c.glow,0,2,"%.2f");
        panel::SliderRow("Opacity",&c.opacity,0,1,"%.2f");
        panel::CheckRow("Animation",&c.animate);
        ImGui::BeginDisabled(!c.animate);
        panel::SliderRow("Rotation arc",&c.swingDegrees,20,180,"%.0f deg");
        float interval=c.directionCycleSeconds*.5f;
        if(panel::SliderRow("Direction switch",&interval,1,10,"%.1f s")) c.directionCycleSeconds=interval*2;
        ImGui::EndDisabled();
        panel::ColorRow("Frame colour",&c.lime.x);
        panel::ColorRow("Highlight colour",&c.yellow.x);
        // Die Details bleiben in derselben Tabelle, damit ihre Höhe auch unterhalb
        // des sichtbaren Scrollbereichs erfasst wird und alle Regler erreichbar sind.
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemOpen(markerDetailsOpen_,ImGuiCond_Always);
        markerDetailsOpen_=ImGui::CollapsingHeader("Shape & timing",ImGuiTreeNodeFlags_SpanAllColumns);
        if(markerDetailsOpen_) {
            float corners=c.cornerFraction*100,rounding=c.roundFraction*100;
            if(panel::SliderRow("Corner length",&corners,10,40,"%.0f %%")) c.cornerFraction=corners/100;
            if(panel::SliderRow("Corner rounding",&rounding,.5f,9,"%.1f %%")) c.roundFraction=rounding/100;
            panel::CheckRow("Cube depth",&c.showDepth);
            ImGui::BeginDisabled(!c.animate);
            panel::SliderRow("Target fade",&c.fadeSeconds,0,.5f,"%.2f s");
            ImGui::EndDisabled();
        }
        ImGui::EndDisabled(); ImGui::EndTable();
    }
    ImGui::TextWrapped("Follows the active Kill Aura target. Shorter direction intervals make it rotate faster.");
}


void Overlay::DrawCombatTab() {
    if(combatMenu_.DrawNavigation()!=CombatMenu::Section::Modules) { combatMenu_.DrawSettings(); return; }
    const bool auraActive=combatMenu_.Store().Read().values.enabled;
    if(auraActive) ImGui::TextWrapped("Aim assist and triggerbot are paused while Kill Aura is enabled.");
    MenuColumns("combat_columns",[&](int column) {
        if (column==0) {
            ImGui::BeginDisabled(auraActive);
            MenuHeading("Aim assist");
            if (panel::BeginRows("aim_controls")) {
                panel::CheckRow("Enable aim assist",&combat_.aimLock);
                const char* targets[]={"Living entities","Players"};
                int target=combat_.aimPlayersOnly ? 1 : 0;
                if (panel::ComboRow("Target selection",&target,targets,IM_ARRAYSIZE(targets))) combat_.aimPlayersOnly=target==1;
                ImGui::BeginDisabled(!combat_.aimLock);
                panel::SliderRow("Lock cone",&combat_.aimFov,2,90,"%.0f deg");
                panel::SliderRow("Lock range",&combat_.aimRange,1,8,"%.1f m");
                panel::SliderRow("Gain",&combat_.aimGain,.02f,1,"%.2f");
                panel::SliderRow("Max mouse step",&combat_.aimMaxStep,2,80,"%.0f px");
                ImGui::EndDisabled();
                ImGui::EndTable();
            }
            ImGui::EndDisabled();
            MenuHeading("Offhand");
            if (panel::BeginRows("totem_controls")) {
                panel::CheckRow("Auto totem",&combat_.autoTotem);
                ImGui::EndTable();
            }
            MenuHeading("Activation");
            ImGui::BeginDisabled(auraActive);
            if (panel::BeginRows("aim_activation")) {
                const char* keys[]={"Right mouse","Left mouse","ALT"};
                const int codes[]={VK_RBUTTON,VK_LBUTTON,VK_MENU};
                int key=combat_.aimKey==VK_MENU ? 2 : combat_.aimKey==VK_LBUTTON ? 1 : 0;
                if (panel::ComboRow("Aim assist key",&key,keys,IM_ARRAYSIZE(keys))) combat_.aimKey=codes[key];
                panel::ValueRow("Target priority","Angular distance");
                ImGui::EndTable();
            }
            ImGui::EndDisabled();
        } else {
            ImGui::BeginDisabled(auraActive);
            MenuHeading("Triggerbot");
            if (panel::BeginRows("trigger_controls")) {
                panel::CheckRow("Enable triggerbot",&combat_.triggerbot);
                ImGui::BeginDisabled(!combat_.triggerbot);
                panel::SliderRow("Clicks per second",&combat_.cps,1,20,"%.1f");
                panel::SliderRow("Click jitter",&combat_.jitter,0,.9f,"%.2f");
                panel::SliderRow("Max attack range",&combat_.maxDistance,1,6,"%.1f m");
                panel::CheckRow("Living targets only",&combat_.livingOnly);
                panel::CheckRow("Require hold key",&combat_.requireHoldKey);
                const char* keys[]={"Right mouse","Left mouse","ALT"};
                const int codes[]={VK_RBUTTON,VK_LBUTTON,VK_MENU};
                int key=combat_.holdKey==VK_MENU ? 2 : combat_.holdKey==VK_LBUTTON ? 1 : 0;
                if (panel::ComboRow("Activation key",&key,keys,IM_ARRAYSIZE(keys))) combat_.holdKey=codes[key];
                ImGui::EndDisabled();
                ImGui::EndTable();
            }
            ImGui::EndDisabled();
            MenuHeading("Activity");
            if (panel::BeginRows("trigger_activity")) {
                char count[32]; std::snprintf(count,sizeof(count),"%u",clickCount_);
                panel::ValueRow("Clicks sent",count);
                panel::ValueRow("Activation","Menu closed");
                ImGui::EndTable();
            }
        }
    });
}


void Overlay::DrawBlocksTab() {
    bool dirty=false;
    MenuColumns("block_columns",[&](int column) {
        if (column==0) {
            if (panel::BeginRows("block_filters")) {
                for (std::uint8_t kind=0; kind<kBlockKindCount; ++kind) {
                    const std::uint32_t bit=1u<<kind;
                    bool on=(blocks_.mask&bit)!=0;
                    if (panel::CheckRow(BlockKindLabel(kind),&on)) {
                        blocks_.mask=on ? blocks_.mask|bit : blocks_.mask&~bit;
                        dirty=true;
                    }
                }
                ImGui::EndTable();
            }
        } else {
            if (panel::BeginRows("block_scanning")) {
                dirty|=panel::CheckRow("Enable scanning",&blocks_.enabled);
                dirty|=panel::SliderIntRow("Scan radius",&blocks_.radius,4,48,"%d blocks");
                dirty|=panel::SliderIntRow("Scan interval",&blocks_.interval,200,3000,"%d ms");
                panel::SliderRow("Draw distance",&blocks_.maxDraw,8,128,"%.0f m");
                panel::CheckRow("Tracers to blocks",&blocks_.tracers);
                ImGui::EndTable();
            }
            ImGui::TextWrapped("A larger scan radius takes longer to finish. Results stay visible between scans.");
            if (haveSnapshot_) ImGui::TextDisabled("Found: %u blocks",lastSnapshot_.blockCount);
            if (haveSnapshot_ && lastSnapshot_.blockCount>=kMaxBlocks)
                ImGui::TextWrapped("Result limit reached. Reduce the scan radius to focus on nearby blocks.");
        }
    });
    if (dirty) PushScanRequest();
}


void Overlay::DrawHudTab() {
    MenuColumns("hud_columns",[&](int column) {
        if (column==0) {
            if (panel::BeginRows("hud_panels")) {
                panel::CheckRow("Armour & equipment",&hud_.equipment);
                panel::CheckRow("Status effects",&hud_.effects);
                panel::CheckRow("Coordinates",&hud_.coordinates);
                panel::CheckRow("Telemetry graphs",&hud_.performance);
                panel::CheckRow("Compass strip",&hud_.compass);
                panel::CheckRow("Reach indicator",&hud_.reachMarker);
                ImGui::EndTable();
            }
        } else {
            static const char* corners[]={"Top left","Top right","Bottom left","Bottom right"};
            if (panel::BeginRows("hud_layout")) {
                panel::ComboRow("Panel anchor",&hud_.corner,corners,IM_ARRAYSIZE(corners));
                float warn=100*hud_.durabilityWarn;
                if (panel::SliderRow("Durability warning",&warn,5,50,"%.0f%%")) hud_.durabilityWarn=warn/100;
                ImGui::EndTable();
            }
            ImGui::TextWrapped("Equipment bars pulse red below the durability threshold. The reach indicator lights up for a target within attack range.");
        }
    });
}


void Overlay::DrawStatsTab(const WorldSnapshot& snapshot) {

    if (!haveSnapshot_) {
        ImGui::TextDisabled("Waiting for the agent's first snapshot...");
        return;
    }

    SectionHeader("LOCAL PLAYER");
    ImGui::Text("Name       %s", snapshot.username[0] ? snapshot.username : "-");
    ImGui::Text("Position   %.1f, %.1f, %.1f", snapshot.posX, snapshot.posY, snapshot.posZ);
    ImGui::Text("Facing     yaw %.1f  pitch %.1f", snapshot.yawDeg, snapshot.pitchDeg);
    ImGui::Text("Dimension  %s", snapshot.dimension[0] ? snapshot.dimension : "-");
    ImGui::Text("Server     %s", snapshot.serverAddress[0] ? snapshot.serverAddress : "-");

    if (snapshot.maxHealth > 0.0f && snapshot.health >= 0.0f) {
        const float fraction = std::clamp(snapshot.health / snapshot.maxHealth, 0.0f, 1.0f);
        char label[48];
        std::snprintf(label, sizeof(label), "%.1f / %.1f", snapshot.health, snapshot.maxHealth);
        ImGui::ProgressBar(fraction, ImVec2(-1, 0), label);
    }
    ImGui::Text("Armour     %d", snapshot.armorValue);
    ImGui::Text("Absorption %.1f", snapshot.absorption);

    SectionHeader("CLIENT");
    // Values the agent deliberately does not sample come through as -1 rather
    // than as a plausible-looking zero.
    if (snapshot.fps  >= 0) ImGui::Text("FPS        %d", snapshot.fps);
    else                    ImGui::TextDisabled("FPS        not sampled");
    if (snapshot.ping >= 0) ImGui::Text("Ping       %d ms", snapshot.ping);
    else                    ImGui::TextDisabled("Ping       not sampled");
    ImGui::Text("FOV        %.0f deg", snapshot.fovDeg);
    ImGui::Text("Snapshot   #%llu", static_cast<unsigned long long>(snapshot.frameIndex));

    SectionHeader("ENTITIES");
    if (ImGui::BeginTable("##entities", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, 180))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        for (std::uint32_t i = 0; i < snapshot.entityCount && i < kMaxEntities; ++i) {
            const EntitySnapshot& entity = snapshot.entities[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entity.name[0] ? entity.name : "<unnamed>");

            ImGui::TableSetColumnIndex(1);
            if (entity.flags & kEntityIsPlayer)       ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "player");
            else if (entity.flags & kEntityIsHostile) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "hostile");
            else                                      ImGui::TextDisabled("other");

            ImGui::TableSetColumnIndex(2);
            if (entity.flags & kEntityIsLiving) ImGui::Text("%.0f / %.0f", entity.health, entity.maxHealth);
            else                                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.0f m", entity.distance);
        }
        ImGui::EndTable();
    }

}


void Overlay::DrawSettingsTab() {
    MenuColumns("settings_columns",[&](int column) {
        if (column==0) {
            if (panel::BeginRows("overlay_settings")) {
                panel::SliderRow("FOV trim",&settings_.fovTrim,-20,20,"%+.1f deg");
                if (panel::CheckRow("Click-through when closed",&settings_.clickThroughWhenIdle) && !menuVisible_)
                    ApplyExtendedStyles(settings_.clickThroughWhenIdle);
                panel::CheckRow("Hide when game is unfocused",&settings_.hideWhenUnfocused);
                panel::CheckRow("Show watermark",&settings_.showWatermark);
                panel::CheckRow("VSync",&settings_.vsync);
                if (panel::CheckRow("Exclude from screen capture",&settings_.excludeFromCapture)) ApplyCaptureAffinity();
                ImGui::EndTable();
            }
            MenuHeading("Focus & keys");
            static const char* focusLabels[]={"No focus","Activate","Unlock","Legacy"};
            if (panel::BeginRows("focus_settings")) {
                int focus=int(settings_.focusMode);
                if (panel::ComboRow("Focus mode",&focus,focusLabels,IM_ARRAYSIZE(focusLabels))) settings_.focusMode=FocusMode(focus);
                panel::ValueRow("Toggle menu","INSERT");
                panel::ValueRow("Quit overlay","END");
                ImGui::EndTable();
            }
            if (settings_.focusMode==FocusMode::Simple)
                ImGui::TextWrapped("If the menu does not accept clicks, choose Unlock foreground.");
            if (settings_.focusMode==FocusMode::AttachInput)
                ImGui::TextWrapped("Legacy attach input can crash GLFW games while the mouse is captured.");
        } else {
            if (panel::BeginRows("agent_details")) {
                char pid[32]; std::snprintf(pid,sizeof(pid),"%u",target_.pid);
                panel::ValueRow("Process",pid);
                panel::ValueRow("Status",statusLine_.c_str());
                panel::ValueRow("Minecraft",lastSnapshot_.gameVersion[0] ? lastSnapshot_.gameVersion : "detecting");
                panel::ValueRow("Profile",lastSnapshot_.mappingProfile[0] ? lastSnapshot_.mappingProfile : "detecting");
                ImGui::EndTable();
            }
            ImGui::Spacing();
            if (!agentConnected_) {
                if (ImGui::Button("Load agent into Minecraft",ImVec2(-1,0))) {
                    const auto directory=ExecutableDirectory();
                    std::uint32_t exitCode=0; std::string loadError;
                    if (LaunchAgentLoader(target_,directory+L"\\tools",directory+L"\\mcov_agent.dll",&exitCode,&loadError))
                        statusLine_="agent loaded; connecting";
                    else { statusLine_=loadError; MCOV_LOG_ERROR("agent load failed: %s",loadError.c_str()); }
                }
            } else if (ImGui::Button("Stop agent sampling",ImVec2(-1,0))) {
                if (auto* block=channel_.block()) {
                    block->controlFlags.fetch_or(kCtrlRequestUnload,std::memory_order_acq_rel);
                    statusLine_="unload requested";
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("Close overlay",ImVec2(-1,0))) running_=false;
        }
    });
}


} // namespace mcov
