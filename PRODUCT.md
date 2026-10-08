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

- The latest request replaces the classic menu with Liquid Glass based on
  [poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11).
  Preserve actual module controls, keybinds and HUD behavior. Categories live on
  the left; module tiles toggle on left-click and open independent draggable
  settings on right-click. Settings-only tiles and action tiles retain their
  real operation rather than presenting invented toggle state.
- The launcher shares the client menu's visual system and performs Minecraft
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
  category tiles and their settings windows.
- Preserve the clean rounded entity ESP, configurable Target Aura and Target
  Marker, soft particles/trails and the glowing friend/self hats.
- ThunderHack-Recode-inspired visuals prioritize particles and trails. The
  Rockstar-inspired catalog describes actual coverage rather than promising a
  complete Java port.

## Persistence and runtime boundaries

HUD positions, per-element scale/visibility, keybinds, Rockstar module parameters
and world/server-scoped waypoints save in `client-settings.ini`. Existing
combat/ESP/Aura settings and friend UUID relations retain their session behavior.

The glass material captures the desktop only while the menu or launcher is
visible. DXGI Desktop Duplication runs first; after 750 ms without a usable frame,
Windows Graphics Capture attempts monitor capture on the overlay's render device.
Closing the menu, disabling capture, supplying a fixture texture or changing the
monitor stops capture sessions. Temporary Windows capture exclusion prevents
recursive feedback, and closure restores the configured overlay capture preference.
Unavailable capture uses a stable tinted fallback. Settings → Overlay reports
the actual material and capture state. This is presentation behavior, not a change
to world-effect visibility or JNI data acquisition.

## Verification limits

Offscreen D3D11 WARP checks exercise the real shaders, UI input, small viewports,
HUD/keybind behavior and portable packaging. An explicit native `--live` probe
passed WGC startup, the full LiquidGlass runtime, capture pause, display-affinity
restoration and live capture after reopening on this host without saving desktop
images or attaching to a JVM. Synthetic review images establish material
and layout behavior, not live Minecraft appearance. These checks do not establish
live Minecraft capture/attach behavior on every GPU or runtime. Existing compatibility details
remain in [docs/compatibility.md](docs/compatibility.md).
