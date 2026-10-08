# mcov

## Platform

Native Windows x64 desktop overlay attached to the Minecraft Java client window.

## Stack

C++20, Dear ImGui 1.91.9b, Win32 and Direct3D 11. A separate JNI/JVMTI agent
publishes coherent world snapshots and executes existing client-tick actions
through IPC v19. The game's OpenGL renderer and the overlay's D3D11 renderer are
separate. UI rendering introduces no render-thread JNI calls.

## User and purpose

The user configures Minecraft visual/combat modules, observes game information,
and operates features through an external menu and keyboard/mouse bindings.
Success means working features with clear controls, a modular HUD, and preserved
camera/entity projection stability.

## Confirmed constraints

- The latest client direction follows the user-supplied
  `design/reference-translucent-menu.png`: compact translucent anthracite,
  narrow left categories, searchable module tabs and muted coral controls.
  The 76 real sections render inside the main window. Tabs select settings;
  the header and available Quick toggles operate real modules and actions.
  Preserve actual controls, keybinds and HUD behavior. The spacious cyan example,
  large module tile grid and independent module settings window are superseded.
- The injector uses the same approved compact coral design as the client:
  scoped `CompactStyle`, embedded Inter at 14 pixels, an 88%-opaque anthracite
  root and a narrow Start/Diagnose sidebar. It performs Minecraft selection,
  connection and overlay startup. The fixed primary action and close footer
  remain reachable while status, JDK recovery and diagnostics scroll.
- Keep the simplified Kill Aura workflow and independent mouse-controlled camera.
- Renderers consume snapshots; UI/HUD changes add no render-thread JNI.
- Keep one canonical `build` directory and existing version/Forge compatibility.
- Ship one portable `mcov.exe` containing the client payload. The optional
  `START.txt` guide is the only other file in the share ZIP.
- Module keybinds start unbound, as explicitly chosen by the user. Menu INSERT,
  HUD editor F6 and Quit END are defaults; all can be rebound.
- Support keyboard/mouse bindings, modifier chords, Toggle/Hold operation and
  movable/scalable HUD elements. All existing sections remain reachable through
  category module tabs and their in-window controls.
- Preserve the clean rounded entity ESP, configurable Target Aura and Target
  Marker, soft particles/trails and the glowing friend/self hats.
- ThunderHack-Recode-inspired visuals prioritize particles and trails. The
  Rockstar-inspired catalog describes actual coverage rather than promising a
  complete Java port.

## Persistence and runtime boundaries

HUD positions, per-element scale/visibility, keybinds, Rockstar module parameters
and world/server-scoped waypoints save in `client-settings.ini`. Existing
combat/ESP/Aura settings and friend UUID relations retain their session behavior.

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

## Verification limits

The `mcov` and `mcov_injector` builds, native/UI checks and portable share ZIP
passed after the redesign. Offscreen D3D11 WARP checks exercise real UI input,
small viewports, HUD/keybind behavior and portable packaging. Updated launcher
checks cover selection, start, duplicate-start prevention, both sidebar tabs,
fixed footer and title controls. Hidden native HWND/swapchain checks cover alpha,
control ink, hidden initialization, resize and rounded clipping.

Current images under `.impeccable/review/compact-client/` and
`.impeccable/review/compact-injector/` use synthetic fixtures. The two injector
`launcher-native-*` images are hidden swapchain readbacks before OS clipping.
These images do not establish live desktop composition or Minecraft appearance.
Historical explicit `--live` and `--live-sta` probes passed WGC startup, the full
LiquidGlass runtime, capture pause, affinity restoration and capture after
reopening on this host without saving desktop images or attaching to a JVM.
Those results concern the retained legacy renderer, not current injector capture.
Live Minecraft attach/focus and every GPU/runtime combination remain outside
these checks. Existing compatibility details remain in
[docs/compatibility.md](docs/compatibility.md).
