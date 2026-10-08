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
- The launcher retains its separate Liquid Glass visual system based on
  [poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11) and performs Minecraft
  selection, connection and overlay startup. Technical diagnostics remain separate
  from the primary start action.
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

The launcher's separate glass material captures the desktop only while its
glass surface is visible. DXGI Desktop Duplication runs first; after 750 ms without a usable frame,
Windows Graphics Capture attempts monitor capture on the overlay's render device.
Closing/minimizing the launcher, disabling capture, supplying a fixture texture or changing the
monitor stops capture sessions. Temporary Windows capture exclusion prevents
recursive feedback, and closure restores its prior capture affinity.
Unavailable capture uses a stable tinted fallback. This is presentation behavior, not a change
to world-effect visibility or JNI data acquisition.

## Verification limits

Offscreen D3D11 WARP checks exercise the real shaders, UI input, small viewports,
HUD/keybind behavior and portable packaging. Current compact-client review images
under `.impeccable/review/compact-client/` use synthetic backgrounds and direct
alpha compositing; they do not establish live Minecraft appearance. An explicit native `--live` probe
passed WGC startup, the full LiquidGlass runtime, capture pause, display-affinity
restoration and live capture after reopening on this host without saving desktop
images or attaching to a JVM. These glass checks concern the retained renderer
and launcher, not a client capture requirement. These checks do not establish
live Minecraft capture/attach behavior on every GPU or runtime. Existing compatibility details
remain in [docs/compatibility.md](docs/compatibility.md).
