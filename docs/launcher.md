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

The injector uses the same approved compact design as the client. Scoped
`CompactStyle` restores the previous ImGui style and font after drawing. Embedded
Inter is 14 pixels, the root is anthracite `#222229` at alpha 0.88 with a 12-pixel
radius, and the sidebar adds `#15151B` at alpha 0.22. Controls use the shared
small radii and coral accent `#EF8B70`. This presentation replaces the launcher's
lavender Liquid Glass surface.

The launcher opens at 640 × 640. Its left **Start / Diagnose** sidebar is
142 pixels wide, or 112 pixels below 620 pixels viewport width, with simple
authored icons. **Start** contains the native selectable Minecraft list,
refresh, phase/status and JDK recovery. Long instance titles and PID/Java/client
details remain clipped to their row; hovering reveals the full text in a tooltip.
The list and content scroll while the 34-pixel primary action and close footer
remain fixed. Enabled primary text is near-black (`#221B1B`); disabled text is
soft white (`#ECECEF`) under the shared disabled alpha. Busy or absent selection
disables the action. **Diagnose** contains connection details, copy/open-folder
actions and logs, with its own fixed close footer. Actions wrap when needed.
The layout is also covered at 480 × 380.

The native shell clears to transparent RGBA (0, 0, 0, 0) and extends DWM over
the client area using margins of −1. A Windows rounded region clips the outer
12-pixel margin with a 24-pixel corner diameter matching the panel radius.
Initialization stays hidden; `Run` presents the first frame before `ShowWindow`.
Resize updates the render target and region. Native title drag and close/minimize
behavior remain unchanged. Backend, render-target, resize and presentation
failures are reported instead of leaving a silently blank window.

`FlatApp` no longer owns, initializes, begins frames for, renders or shuts down
LiquidGlass. The injector requires no desktop capture, blur or refraction.
[Retained material](liquid-ui.md) documents the legacy/standalone glass renderer,
historical capture checks and license. Its standard Windows SDK `windowsapp`
library adds no loose runtime files to the portable package.

Connection runs on a worker; the native window remains draggable and responsive.
Duplicate launcher/overlay instances are prevented by named mutexes. An
initialized overlay signals a readiness event before the launcher reports
success. Client discovery never falls back to a different game process when the
selected instance disappears. Launch, JDK selection, logs and packaging semantics
are unchanged by this presentation redesign.

## Verification

`portable_client` verifies that the share ZIP contains exactly two root files and
the current executable, then copies its EXE into an otherwise empty folder. It verifies
the GUI subsystem and exact embedded payloads, tests paths with spaces/Unicode,
idempotent extraction, concurrent corruption repair, and early rejection of
invalid overlay PIDs. It passed against the current `mcov.exe` and exact payloads.
The `mcov` and `mcov_injector` builds, native/UI checks and share ZIP passed.

`control_panel` renders the actual compact launcher offscreen at 640 × 640,
640 × 520 and 480 × 380, covering empty, selected, busy, missing-JDK and diagnostic
states. `LauncherChecks` checks style/font/window-stack restoration, selection,
start, running-instance action, duplicate-start prevention, both sidebar tabs,
fixed close footers and title minimize/close. These checks simulate input and do
not start processes or attach to Minecraft.

`launcher_window` adds a real hidden HWND and swapchain check at 640 × 640 and
480 × 380: translucent panel alpha, transparent margins, foreground control ink,
hidden initialization, resize, rounded window region and presentation. It
requires the fixture to remain invisible. It performs no desktop capture and
attaches to no JVM.

The nine current review images are in `.impeccable/review/compact-injector/`:

| Image | Size | Evidence |
| --- | --- | --- |
| `launcher-ready.png`, `launcher-empty.png` | 640 × 640 | Synthetic selected/empty UI states |
| `launcher-small.png` | 480 × 380 | Synthetic compact Start layout |
| `launcher-busy.png`, `launcher-error.png` | 640 × 520 | Synthetic busy/JDK recovery states |
| `launcher-diagnose.png` | 480 × 380 | Synthetic compact diagnostics |
| `launcher-diagnose-desktop.png` | 640 × 640 | Synthetic full-size diagnostics |
| `launcher-native-start.png` | 640 × 640 | Hidden HWND swapchain readback |
| `launcher-native-diagnose.png` | 480 × 380 | Hidden HWND swapchain readback after resize |

Independent finish review accepted all nine images with a ship disposition and
no material findings. Synthetic images show the real UI against fixture input;
native images are readbacks before OS clipping. Neither proves live desktop
composition or Minecraft appearance. Live Minecraft attach/focus behavior still
needs a real game session.

Retained `liquid_glass` shader and historical opt-in `--live`/`--live-sta` capture
checks are described in [liquid-ui.md](liquid-ui.md). They validate the retained
legacy renderer, not an active compact injector capture requirement.
