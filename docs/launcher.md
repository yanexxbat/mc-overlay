# Portable client and launcher

Open `build/client/mcov.exe`. Select an already running Minecraft window and
click **Client starten**. The launcher automatically connects and starts the
overlay for that exact instance. Starting again switches back to Minecraft.
Only this EXE needs to be copied; all native and Java helpers are embedded.
The ready-to-share `build/mcov-client.zip` contains exactly `mcov.exe` and the
optional German `START.txt` guide, with no build directories or separate dependencies.
Both files are also available in `build/client/`. Extract the ZIP before starting.

## Runtime requirements

Windows x64, Minecraft Java Edition, and an x64 JDK matching the game's Java
major are required: normally Java 17 for 1.20.1, Java 21 for the 1.21 releases,
and Java 25 for 26.2. A JRE alone cannot attach the agent. Discovery checks the
build JDK, configured paths and installed JDKs. A missing match exposes
**JDK-Ordner auswaehlen**. Select the installation root containing `bin/java.exe`,
`bin/javac.exe`, `bin/attach.dll` and `release`. Selection is saved for later starts.
The launcher does not install Java.

## Data and lifetime

- `%LOCALAPPDATA%\mcov\runtime\<package hash>`: embedded overlay, agent and helpers.
  Extraction verifies SHA-256, skips valid files, repairs corrupt files and
  serializes concurrent preparation. The hash changes when packaged files change.
- `%LOCALAPPDATA%\mcov\client-settings.ini`: saved keybinds, HUD layout,
  Rockstar feature parameters and server/dimension-scoped waypoints.
- `%LOCALAPPDATA%\mcov\launcher-jdk.txt`: selected JDK path.
- `%LOCALAPPDATA%\mcov\mcov_client.log` and `mcov_overlay.log`: launcher/overlay logs.
- `mcov_agent.log` beside the loaded DLL: agent log, also included under Diagnose.

On the first portable start, existing keybind/HUD settings beside the launcher
or in the build's sibling `bin` folder are imported if no portable settings exist.
Existing portable settings are never replaced by this import.

Closing the launcher leaves the overlay and Minecraft running. Closing during
connection cancels the launcher's attach helper; it never terminates Minecraft.
An agent already loaded into the JVM stays loaded until Minecraft exits.
An incompatible/stale agent requires restarting Minecraft before retrying.
The default **END** binding quits the overlay, **INSERT** opens its menu and
**F6** opens the HUD editor. Bindings remain configurable.

## Presentation and diagnostics

The injector shares `LiquidStyle` and `LiquidGlass` with the client: a rounded
neutral dark glass root, GPU backdrop blur/refraction, subtle light edges and embedded
Inter controls. The launcher opens at 640 × 640. The primary 40-pixel start action
stays outside scrolling content so smaller windows retain it. Capture failure
uses a stable tinted fallback; temporary Windows capture exclusion prevents the
glass from capturing itself. DXGI capture runs first; after 750 ms without a
usable frame, WGC attempts monitor capture on the launcher's render device.
Closing the window ends capture and restores its previous display affinity.
[Material behavior](liquid-ui.md) describes capture restoration and live/fallback
status. The standard Windows SDK `windowsapp` library adds no portable runtime
files beside `mcov.exe` and optional `START.txt`.

The native shell has an opaque render base beneath the shader material, with
zero DWM frame-extension margins. A Windows rounded region clips the outer
12-pixel padding to the glass panel's 22-pixel corners. Initialization stays
hidden; `Run` draws the first frame before showing the window. Minimizing ends
desktop capture until the launcher returns. Backend, render-target, resize and
presentation failures are reported instead of leaving a silently blank window.

The main page contains instance selection, refresh, one primary start action,
phase/status and recovery actions. Technical logs live under **Diagnose**,
with copy and open-folder actions. Connection runs on a worker; the native
window remains draggable and responsive. Duplicate launcher/overlay instances
are prevented by named mutexes. An initialized overlay signals a readiness event
before the launcher reports success. Client discovery never falls back to a
different game process when the selected instance disappears.

## Verification

`portable_client` verifies that the share ZIP contains exactly two root files and
the current executable, then copies its EXE into an otherwise empty folder. It verifies
the GUI subsystem and exact embedded payloads, tests paths with spaces/Unicode,
idempotent extraction, concurrent corruption repair, and early rejection of
invalid overlay PIDs. `control_panel` renders the actual launcher offscreen at
640 x 520 and 480 x 380, including empty, selected, busy, missing-JDK and diagnostic
states, and checks style restoration and button input. `liquid_glass` also runs
the real material and Dual Kawase blur shaders with synthetic inputs on WARP,
checking pixels, clipping, layering and resize. These checks do not attach to
Minecraft. Synthetic glass-grid/settings review images under `.impeccable/review`
exercise the real material with synthetic GPU input, not Minecraft screenshots.
The explicit native `mcov_backdrop_capture_check --live` probe passed WGC startup,
the full LiquidGlass runtime, capture pause, display-affinity restoration and
live capture after reopening on this host without saving desktop images or using
a JVM. Live Minecraft appearance and attach/focus behavior still need a real game session.

`launcher_window` adds a real hidden HWND and swapchain check at 640 × 640 and
480 × 380: opaque background, foreground controls, resize, rounded window region
and presentation. It performs no desktop capture and attaches to no JVM.
`launcher-native-start.png` and `launcher-native-diagnose.png` under
`.impeccable/review` are swapchain readbacks before OS clipping; they are not
desktop screenshots and do not prove user-visible desktop composition.
The capture probe also passed `--live-sta`, using the launcher's STA COM mode.
