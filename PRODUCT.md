# mcov

## Platform

Native Windows x64 desktop overlay attached to the Minecraft Java client window.

## Stack

C++20, Dear ImGui 1.91.9b, Win32 and Direct3D 11. A separate JNI/JVMTI agent
publishes coherent world snapshots and executes existing client-tick actions
through IPC v20. The game's OpenGL renderer and the overlay's D3D11 renderer are
separate. UI rendering introduces no render-thread JNI calls.

## User and purpose

The user configures Minecraft visual/combat modules, observes game information,
and operates features through an external menu and keyboard/mouse bindings.
Success means working features with clear controls, a modular HUD, and preserved
camera/entity projection stability.

## Confirmed constraints

- The latest client layout follows the user-supplied Paint sketch,
  `design/reference-module-widgets.png`: narrow left categories and searchable
  module widgets, with right-click opening settings in an independent window.
  All 77 real sections remain reachable. Left-click or Space operates the real
  toggle/action; settings-only widgets open their controls. Menu / Shift+F10
  opens settings without toggling. The approved compact translucent anthracite,
  muted coral, embedded Inter and rounded fills remain the visual identity.
  This replaces the module tabs, inline settings and Quick toggles.
- The injector uses the same approved compact coral design as the client:
  scoped `CompactStyle`, embedded Inter at 14 pixels, an 88%-opaque anthracite
  root and a narrow Start/Diagnose sidebar. It performs Minecraft selection,
  connection and overlay startup. The fixed primary action and close footer
  remain reachable while status, JDK recovery and diagnostics scroll.
- The 19 snapshot HUD elements and optional Combat debug HUD use the same
  compact anthracite/coral design and embedded Inter at 14 pixels. Rounded fills,
  quiet headings and semantic green/amber/red feedback preserve readability.
  The F6 editor retains movable/scalable elements, visibility, reset and persistence.
- Keep the simplified Kill Aura workflow and independent mouse-controlled camera.
- Renderers consume snapshots; UI/HUD changes add no render-thread JNI.
- Keep one canonical `build` directory and existing version/Forge compatibility.
- Ship one portable `mcov.exe` containing the client payload. The optional
  `START.txt` guide is the only other file in the share ZIP.
- Module keybinds start unbound, as explicitly chosen by the user. Menu INSERT,
  HUD editor F6 and Quit END are defaults; all can be rebound.
- Named profiles and automatic preferences live directly beside the portable
  client. **Settings → Configs** saves, explicitly overwrites and loads profiles;
  loading restores saved module states only after full validation. No named
  profile is selected or loaded automatically at startup.
- Support keyboard/mouse bindings, modifier chords, Toggle/Hold operation and
  movable/scalable HUD elements. All existing sections remain reachable through
  category module widgets and their independent settings controls.
- Preserve the clean rounded entity ESP, configurable Target Aura and Target
  Marker, soft particles/trails and the glowing friend/self hats.
- ThunderHack-Recode-inspired visuals prioritize particles and trails. The
  Rockstar-inspired catalog describes actual coverage rather than promising a
  complete Java port.

## Persistence and runtime boundaries

Automatic preferences remain partial and save in `client-settings.ini` beside
`mcov.exe`, including HUD positions, per-element scale/visibility, keybinds,
Rockstar parameters and world/server-scoped waypoints. Existing AppData preferences
are imported once when local preferences are absent, without replacing local files.
Runtime helpers and logs remain in `%LOCALAPPDATA%\mcov`.

Named `configs/<name>.ini` profiles beside the client contain user-facing module
states and settings, visual colors, combat and macro settings, blocks, HUD and
layout, binds, friend UUID relations and waypoints. Profiles are self-contained,
fully validated before application and saved atomically within the local folder.
Copy `configs` and `client-settings.ini` with the client to retain the setup.

The client menu composites an 88%-opaque dark ImGui fill directly over the
transparent overlay. It requires no desktop capture, blur or refraction; the
client glass renderer receives visibility false. The configured overlay capture
preference applies while the menu is open. Settings → Overlay reports
`Translucent dark / 88% opacity` and `Not required`.

The injector also uses direct ImGui alpha compositing. Its native shell clears
to transparent RGBA, extends DWM across the client area and clips to a rounded
Windows region matching the 12-pixel panel radius. Initialization stays hidden
until the first frame is presented. `FlatApp` no longer owns or calls LiquidGlass;
injector presentation requires no desktop capture, blur or refraction. Native
drag, close/minimize, async launch, JDK selection and logs retain their behavior.

The retained LiquidGlass renderer and capture checks remain a legacy/standalone
capability based on
[poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11).
Their material, capture lifecycle and license remain documented in
[docs/liquid-ui.md](docs/liquid-ui.md). They are not active injector requirements.
The redesign changes presentation only, not world-effect visibility or JNI acquisition.
Snapshot HUDs use the shared rounded ImGui fill and font; their editor applies
`CompactStyle` locally. HUD presentation adds no capture, blur or new renderer.

## Verification limits

The `mcov` and `mcov_injector` builds, native/UI checks and portable share ZIP
passed after the redesign. Offscreen D3D11 WARP checks exercise real UI input,
small viewports, HUD/keybind behavior and portable packaging. Updated launcher
checks cover selection, start, duplicate-start prevention, both sidebar tabs,
fixed footer and title controls. Hidden native HWND/swapchain checks cover alpha,
control ink, hidden initialization, resize and rounded clipping.

Current client images under `.impeccable/review/module-widgets/` use native
ImGui/D3D11 WARP with synthetic fixtures at 1280 × 800, 1920 × 1080 and 480 × 360.
The independent finish review opened all 11 images and returned **ship** with no
material findings. Native checks cover real right-clicks for all 77 sections,
actual toggle stores, Space / Shift+F10, independent drag/close, compact close
returning to the grid, search recovery, settings controls, key capture,
persistence and style/font ownership. Production build, native check and
`portable_client` passed; `mcov.exe` and the share ZIP were updated.
Earlier `.impeccable/review/compact-client/` tab images are historical.
Five Configs captures under `.impeccable/review/configs/` cover empty, loaded,
error and small scrolling states at 1280 × 800 and 480 × 360. The finish review
returned **ship** after confirming the explicit-overwrite fix. Native profile
round-trips, invalid-file preservation, local storage and portable exact-payload
checks passed; HUD and Rockstar regression checks remained green.
Current `.impeccable/review/compact-injector/` images use synthetic fixtures. Seven HUD images
under `.impeccable/review/compact-hud/` likewise use the native ImGui/D3D11 WARP
renderer with synthetic fixtures; `hud-catalog.png` is a test gallery of all
19 elements, not a production layout. HUD input/layout, style/font and clipping
checks, `combat_hud`, `rockstar_features` and `portable_client` passed; the portable
EXE and ZIP were rebuilt. The two injector
`launcher-native-*` images are hidden swapchain readbacks before OS clipping.
These images do not establish live desktop composition or Minecraft appearance.
Historical explicit `--live` and `--live-sta` probes passed WGC startup, the full
LiquidGlass runtime, capture pause, affinity restoration and capture after
reopening on this host without saving desktop images or attaching to a JVM.
Those results concern the retained legacy renderer, not current injector capture.
Live Minecraft attach/focus and every GPU/runtime combination remain outside
these checks. Existing compatibility details remain in
[docs/compatibility.md](docs/compatibility.md).
