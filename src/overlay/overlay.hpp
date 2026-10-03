// overlay.hpp - the external, transparent, click-through ImGui window.
//
// Window model
// ------------
// A borderless Win32 window sized to Minecraft's client rect, kept topmost, with
// a transparent framebuffer. Two Win32 extended styles do the real work:
//
//   WS_EX_LAYERED     - required for per-pixel alpha compositing. Without it the
//                       transparent framebuffer just composites against black.
//   WS_EX_TRANSPARENT - hit-testing passes straight through to whatever is
//                       underneath, so mouse clicks reach the game. This is
//                       toggled OFF while the menu is open so ImGui can be used.
//
// Two more that matter in practice:
//   WS_EX_TOOLWINDOW  - keeps the overlay out of the alt-tab list and taskbar.
//   WS_EX_NOACTIVATE  - stops the window stealing focus when it is shown or
//                       repositioned, which would otherwise yank the mouse out
//                       of the game every time the overlay tracked a resize.
#pragma once

#include <cstdint>
#include "../common/agent_health.hpp"
#include <string>

#include "../common/ipc.hpp"
#include "../common/shared_state.hpp"
#include "process.hpp"
#include "esp_geometry.hpp"
#include "esp_renderer.hpp"
#include "control_panel.hpp"
#include "combat_menu.hpp"
#include "aura_renderer.hpp"
#include "aura_blend_dx11.hpp"
#include "item_icon_atlas_dx11.hpp"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;
struct ImDrawList;
struct ImVec2;

namespace mcov {

// The always-on HUD widgets, independent of the ImGui menu.
struct HudSettings {
    bool equipment    = true;   // armour + hands, with durability bars
    bool effects      = true;   // active status effects
    bool coordinates  = true;   // XYZ, chunk, nether, facing
    bool compass      = true;   // directional strip
    bool performance  = false;  // fps / frametime / heap graphs
    bool reachMarker  = true;   // crosshair indicator when a target is in reach

    float scale       = 1.0f;
    int   corner      = 0;      // 0 TL, 1 TR, 2 BL, 3 BR - anchor for the stacked panels

    // Durability below this fraction turns the bar red and starts pulsing.
    float durabilityWarn = 0.20f;
};

// Automatic attack when the crosshair target is in reach. Off by default and
// gated behind an explicit hold key, because it injects real mouse input into
// whatever window has focus.
// Block ESP. The scan itself happens in the agent; these are the request
// parameters written into the shared block.
struct BlockEspSettings {
    bool          enabled  = false;
    std::uint32_t mask     = 0;      // bit per BlockKind
    int           radius   = 12;     // (2r+1)^3 lookups per scan - grows cubically
    int           interval = 900;    // ms between scans
    bool          tracers  = false;
    float         maxDraw  = 64.0f;  // do not draw boxes further than this
};

struct CombatSettings {
    bool  triggerbot    = false;

    // Aim lock: nudges the mouse toward the best target while a key is held.
    bool  aimLock       = false;
    int   aimKey        = 0x02;   // VK_RBUTTON
    float aimFov        = 30.0f;  // only lock onto targets within this cone
    float aimRange      = 5.0f;
    float aimGain       = 0.18f;  // proportional gain; tune until it feels right
    float aimMaxStep    = 22.0f;  // clamp per frame, so it cannot snap instantly
    bool  aimPlayersOnly= false;

    // Handled entirely in the agent; the overlay only raises the control bit.
    bool  autoTotem     = false;
    float cps           = 8.0f;   // clicks per second
    float jitter        = 0.25f;  // +/- fraction of the interval, so it is not metronomic
    float maxDistance   = 3.0f;   // survival attack range
    bool  livingOnly    = true;   // ignore item frames, boats, armour stands
    bool  requireHoldKey= true;
    int   holdKey       = 0x02;   // VK_RBUTTON - only fires while held
};

struct OverlaySettings {
    int   toggleKey            = 0x2D; // VK_INSERT
    int   panicKey             = 0x23; // VK_END - quits the overlay outright
    float fovTrim              = 0.0f; // manual FOV correction, degrees
    bool  clickThroughWhenIdle = true;
    bool  hideWhenUnfocused    = true;
    bool  showWatermark        = true;
    bool  vsync                = true;

    // Optional recording preference; flat panels do not require capture exclusion.
    bool excludeFromCapture = false;

    // Simple is the safe default. It is often refused by the foreground lock, in
    // which case the menu draws but cannot take input - switch to
    // UnlockForeground. AttachInput is the legacy path and crashes GLFW targets
    // while the cursor is captured; see FocusMode in process.hpp.
    FocusMode focusMode = FocusMode::Simple;
};

class Overlay {
    friend struct OverlayUiCheck; // Headless menu checks; no JVM/game attachment.
public:
    Overlay()  = default;
    ~Overlay();

    Overlay(const Overlay&)            = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool Initialize(const TargetProcess& target, std::string* error);
    void Shutdown();

    // Renders exactly one frame. Returns false when the overlay should exit
    // (panic key pressed, or the game window disappeared).
    bool RunFrame();

private:
    bool CreateOverlayWindow(std::string* error);
    bool CreateDeviceD3D(std::string* error);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void PumpMessages();

    void ApplyExtendedStyles(bool clickThrough);
    void ApplyCaptureAffinity();
    void TrackTargetWindow();
    void PollHotkeys();
    void SetMenuVisible(bool visible);

    void DrawMenu();
    void ResetMenuPage(control_panel::Page page);
    void DrawEsp(const WorldSnapshot& snapshot);
    void DrawAura(const WorldSnapshot& snapshot, const CameraProjection& projection, float width, float height);
    void DrawWatermark(const WorldSnapshot& snapshot);

    // HUD widgets. Each returns the height it consumed so they can stack.
    void  DrawHud(const WorldSnapshot& snapshot);
    float DrawEquipmentPanel(const WorldSnapshot& snapshot, float x, float y);
    float DrawEffectsPanel(const WorldSnapshot& snapshot, float x, float y);
    float DrawCoordinatesPanel(const WorldSnapshot& snapshot, float x, float y);
    float DrawPerformancePanel(const WorldSnapshot& snapshot, float x, float y);
    void  DrawCompassStrip(const WorldSnapshot& snapshot);
    void  DrawReachMarker(const WorldSnapshot& snapshot);
    void  UpdateTriggerbot(const WorldSnapshot& snapshot);
    void  UpdateAimLock(const WorldSnapshot& snapshot);
    void  DrawBlockEsp(const WorldSnapshot& snapshot, const CameraProjection& projection,
                       float width, float height);
    void  PushScanRequest();
    void  PushCombatRequest();
    void  DrawBlocksTab();

    void SampleTelemetry(const WorldSnapshot& snapshot);

    void DrawVisualsTab();
    void DrawStatsTab(const WorldSnapshot& snapshot);
    void DrawCombatTab();
    void DrawHudTab();
    void DrawSettingsTab();

    // --- state ---
    TargetProcess target_{};
    SharedChannel channel_{};

    void*                   hwnd_        = nullptr; // HWND
    ID3D11Device*           device_      = nullptr;
    ID3D11DeviceContext*    context_     = nullptr;
    IDXGISwapChain*         swapChain_   = nullptr;
    ID3D11RenderTargetView* backBuffer_  = nullptr;
    bool                    quitRequested_ = false;

    VisualSettings  visuals_{};
    AuraConfig auraConfig_{};
    AuraRenderer auraRenderer_{};
    AuraBlendDX11 auraBlend_{};
    ItemIconAtlasDX11 itemIcons_{};
    bool auraPrepared_ = false;
    control_panel::State menu_{};
    OverlaySettings settings_{};
    HudSettings     hud_{};
    CombatSettings   combat_{};
    CombatMenu       combatMenu_{};
    CombatReport    combatReport_{};
    BlockEspSettings blocks_{};

    // Triggerbot pacing. Stored as a raw tick count so no clock type leaks into
    // this header.
    unsigned long long nextClickTick_ = 0;
    unsigned int       clickCount_    = 0;

    // Rolling telemetry history, overlay-side. The agent publishes instantaneous
    // values; the graphs are built here so the sampler stays cheap.
    static constexpr int kHistory = 180;
    float fpsHistory_[kHistory]   = {};
    float frameMsHistory_[kHistory] = {};
    float heapHistory_[kHistory]  = {};
    int   historyCursor_          = 0;
    std::uint64_t lastTelemetryFrame_ = 0;

    bool menuVisible_    = false;
    bool clickThrough_   = true;
    bool running_        = true;
    bool agentConnected_ = false;

    // Edge detection for GetAsyncKeyState polling.
    bool toggleKeyDown_ = false;
    bool panicKeyDown_  = false;

    int windowX_ = 0, windowY_ = 0, windowW_ = 0, windowH_ = 0;

    WorldSnapshot lastSnapshot_{};
    bool          haveSnapshot_ = false;

    AgentHeartbeat heartbeatClock_{};
    int           reconnectCountdown_ = 0;
    std::string   statusLine_         = "starting up";
};

} // namespace mcov
