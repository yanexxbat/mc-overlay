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
#include "../common/liquid_glass.hpp"

#include <cstdint>
#include "rockstar_features.hpp"
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
#include "depth_renderer.hpp"
#include "friend_hat_renderer.hpp"
#include "target_cube_renderer.hpp"
#include "item_icon_atlas_dx11.hpp"
#include "keybinds.hpp"
#include "hud_layout.hpp"
#include "world_visuals.hpp"
#include "smp_features.hpp"
#include <filesystem>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;
struct ImDrawList;
struct ImVec2;

namespace mcov {
namespace client_configs { struct Snapshot; }
std::string BindName(keys::Bind bind);

// The always-on HUD widgets, independent of the ImGui menu.

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

struct ChunkFinderSettings {
    bool enabled=false,blocks=true,labels=true,chunks=true,deepslate=true;
    int radius=20,interval=900,minHints=1;
    float maxDraw=64;
    ImVec4 color{.48f,.85f,1.f,.95f};
};

struct CombatSettings {
    combat::SpinConfig spin{};
    freecam::Config freecam{};
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
    items::TotemConfig autoTotem{};
    float cps           = 8.0f;   // clicks per second
    float jitter        = 0.25f;  // +/- fraction of the interval, so it is not metronomic
    float maxDistance   = 3.0f;   // survival attack range
    bool  livingOnly    = true;   // ignore item frames, boats, armour stands
    bool  requireHoldKey= true;
    int   holdKey       = 0x02;   // VK_RBUTTON - only fires while held
};

struct OverlaySettings {
    bool occludeWorldEffects = true;
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
    void ProcessKeyInput(const keys::Input& input,bool focused);
    void SetMenuVisible(bool visible);
    void SetHudEditing(bool editing);
    void DrawHudEditor();
    void DrawKeybindSettings();
    void DrawBind(keys::Action action,bool inlineRow=false);
    bool ModuleEnabled(keys::Action action) const;
    void SetModuleEnabled(keys::Action action,bool enabled);
    bool HudEnabled(int module) const;
    void SetHudEnabled(int module,bool enabled);
    void SavePreferences();
    void LoadPreferences();
    void DrawConfigSettings();
    void RefreshConfigs();
    bool SaveConfig(const std::string& name,bool overwrite=false);
    bool LoadConfig(const std::string& name);
    client_configs::Snapshot CaptureConfig() const;
    void ApplyConfig(const client_configs::Snapshot& config);

    void DrawMenu();
    int ReadModules(control_panel::Page,std::span<control_panel::ModuleView>) const;
    void ActivateModule(control_panel::Page,int);
    void ResetMenuPage(control_panel::Page page);
    void DrawEsp(const WorldSnapshot& snapshot);
    void DrawAura(const WorldSnapshot& snapshot, const CameraProjection& projection, float width, float height);
    void DrawTargetCube(const WorldSnapshot& snapshot,const CameraProjection& projection,ImVec2 viewport);

    // HUD-Widgets zeichnen lokal; Layout und Skalierung übernimmt DrawHud.
    void  DrawHud(const WorldSnapshot& snapshot);
    float DrawEquipmentPanel(const WorldSnapshot& snapshot, float x, float y);
    float DrawEffectsPanel(const WorldSnapshot& snapshot, float x, float y);
    float DrawCoordinatesPanel(const WorldSnapshot& snapshot, float x, float y);
    float DrawPerformancePanel(const WorldSnapshot& snapshot, float x, float y);
    void  DrawCompassStrip(const WorldSnapshot& snapshot,float stripWidth);
    void  DrawReachMarker(const WorldSnapshot& snapshot);
    void  UpdateTriggerbot(const WorldSnapshot& snapshot);
    void  UpdateAimLock(const WorldSnapshot& snapshot);
    void  DrawBlockEsp(const WorldSnapshot& snapshot, const CameraProjection& projection,
                       float width, float height);
    void  PushScanRequest();
    void  PushCombatRequest();
    items::Request BuildItemRequest(std::uint64_t now,bool inputAllowed,bool combatActive) const;
    void  DrawItemAssistSettings(int section);
    void  DrawChunkFinderSettings();
    smp::BaseFinderSettings ChunkDetection() const;
    void NormalizeChunkFinder();
    void  DrawChunkFinder(const WorldSnapshot&,const CameraProjection&);
    void  DrawBlocksTab();
    void DrawSmpSettings(int section);
    void DrawScanStatus();
    void DrawRockstarSettings(rockstar::Feature feature);
    void DrawMiscTab();
    void UpdateRockstar();
    void Panic();
    void PlayModuleSound(bool enabled,bool hit=false);

    void SampleTelemetry(const WorldSnapshot& snapshot);

    void DrawVisualsTab();
    void DrawTargetAuraSettings();
    void DrawTargetMarkerSettings();
    void DrawWorldVisualSettings(int section);
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
    LiquidGlass glass_;
    bool glassWasVisible_ = false;
    bool                    quitRequested_ = false;

    VisualSettings  visuals_{};
    AuraConfig auraConfig_{};
    AuraRenderer auraRenderer_{};
    WorldVisualsConfig worldVisualsConfig_{};
    WorldVisualsRenderer worldVisuals_{};
    TargetCubeRenderer targetCube_{};
    TargetCubeConfig targetCubeConfig_{};
    bool markerDetailsOpen_=false;
    AuraBlendDX11 auraBlend_{};
    DepthChannel depthChannel_;
    DepthRenderer depthRenderer_;
    std::unique_ptr<DepthFrame> depthFrame_;
    bool depthActive_{},depthFresh_{};
    std::uint64_t depthConnectAt_{};
    FriendHatRenderer friendHats_{};
    FriendHatConfig friendHatConfig_{};
    LocalHatConfig localHatConfig_{};
    ItemIconAtlasDX11 itemIcons_{};
    bool auraPrepared_ = false;
    control_panel::State menu_{};
    rockstar::Config rockstarConfig_{};
    rockstar::Pack rockstarPack_{};
    int miscSection_=0;
    char waypointName_[kNameLen]="Waypoint";
    bool panicActive_=false;
    std::array<bool,keys::kCount> observedModules_{};
    bool moduleObservationReady_=false;
    OverlaySettings settings_{};
    HudSettings     hud_{};
    keys::Bindings bindings_=keys::Defaults();
    keys::Engine keyEngine_{};
    keys::Input keyInput_{},capturePrevious_{};
    int captureAction_=-1;
    int captureModifier_=0;
    unsigned captureModifierMask_=0;
    bool captureArmed_=false;
    bool hudEditing_=false;
    int hudSelected_=0;
    std::array<HudRect,kHudModules> hudRects_{};
    std::filesystem::path preferencesPath_{};
    bool preferencesDirty_=false;
    std::uint64_t preferenceSaveAt_=0;
    std::string preferenceStatus_{};
    std::vector<std::string> configNames_{};
    char configName_[49]{};
    std::string selectedConfig_{},configStatus_{};
    bool configListReady_=false,configError_=false;
    int visualsSection_=0;
    int blocksSection_=0;
    int hudSection_=0;
    int statsSection_=0;
    int settingsSection_=0;
    CombatSettings   combat_{};
    CombatMenu       combatMenu_{};
    CombatReport    combatReport_{};
    BlockEspSettings blocks_{};
    ChunkFinderSettings chunkFinder_{};
    smp::Config smpConfig_{};
    base::Report baseReport_{};
    smp::StashFinder stashFinder_{};
    smp::BaseFinder baseFinder_{};
    items::MacroConfig crystalMacro_{},anchorMacro_{};
    items::Report itemReport_{};

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

    int windowX_ = 0, windowY_ = 0, windowW_ = 0, windowH_ = 0;

    WorldSnapshot lastSnapshot_{};
    bool          haveSnapshot_ = false;
    std::uint64_t snapshotReceivedAt_{};

    AgentHeartbeat heartbeatClock_{};
    int           reconnectCountdown_ = 0;
    std::string   statusLine_         = "starting up";
};

} // namespace mcov
