# Portable client and launcher

Open `build/client/Keim.exe`. Enter your license key and choose **Verify license**.
Then select an already running Minecraft window and click **Start client**.
The launcher automatically connects and starts the
overlay for that exact instance. Starting again switches back to Minecraft.
Only this EXE needs to be copied; all native and Java helpers are embedded.
The ready-to-share `build/Keim-client.zip` contains exactly `Keim.exe` and the
optional English `START.txt` guide, with no build directories or separate dependencies.
Both files are also available in `build/client/`. Extract the ZIP before starting.

All app-owned launcher and overlay text is English, including connection states,
errors, the debug HUD and named keys. Printable key legends follow your keyboard
layout. Minecraft window titles, player names and user-entered text are preserved.

The fixed footer shows **HWID** on Start and Logs, including before sign-in.
**Copy HWID** copies the complete ID for support; the read-only field supports
selection and horizontal scrolling, and hovering reveals the complete value.
This is the same Windows account SID used for KeyAuth verification. It is read
locally once at startup, never added to logs, and needs no internet connection
to display. If it cannot be read, the field shows **Unavailable** and copying
is disabled; restart Keim to retry.

## Runtime requirements

Windows x64, Minecraft Java Edition, and an x64 JDK matching the game's Java
major are required: normally Java 17 for 1.20.1, Java 21 for the 1.21 releases,
and Java 25 for 26.2. A JRE alone cannot attach the agent. Discovery checks the
build JDK, configured paths and installed JDKs. A missing match exposes
**Select JDK folder**. Select the installation root containing `bin/java.exe`,
`bin/javac.exe`, `bin/attach.dll` and `release`. Selection is saved for later starts.
The launcher does not install Java.

## KeyAuth license sign-in

The client uses KeyAuth API 1.3, application **mcov**, owner **Uhn5Z5WPCa**,
version **1.0**, with license-only authentication. No Seller API credential or
app secret is embedded. The endpoint is fixed to `https://keyauth.win/api/1.3/`.
**Show key** controls the masked input. Verification runs in the background;
Close and Logs remain usable. Invalid keys, expired subscriptions, clock skew,
unsigned or malformed responses and network errors leave startup locked.

Keys are not saved to disk, profiles, environment variables, command-line
arguments or logs. The input clears after submission. KeyAuth receives the
license and the Windows account SID, matching its C++ reference's hardware
identity convention; HWID enforcement follows the application's dashboard.
Restarting the launcher requires a fresh sign-in. After doing so, close an
already running overlay with END before starting it with the new session.

HTTPS certificate validation remains enabled and redirects are rejected.
Ed25519 verifies the response body and timestamp using KeyAuth's published
public key; successful responses must match the owner and protocol success code.
The session is accepted only with a future subscription expiry. The launcher
checks it again before connection; the overlay independently checks it before
any discovery/attach. A restricted inherited pipe carries the session token,
not the license. Direct overlay startup without a verified handoff is refused.

During use, the overlay checks the session in the background every 60 seconds.
Expiry, rejection, network failure or a 90-second verification lease timeout
closes the overlay, leaving Minecraft running. There is no offline unlock.
The next login creates a new session; abandoned sessions may remain visible in
the KeyAuth dashboard until replaced or expired.

Validation uses isolated signed fixtures for altered/missing signatures, wrong
owner/code, expiry, malformed data, rejection, cancellation, parameter encoding
and the inherited pipe. The configured app's live initialization succeeded.
A separate live check confirmed that a merely initialized session without a
license login is rejected. Successful real-license login and live Minecraft
use have not been tested.

## Data and lifetime

- `%LOCALAPPDATA%\Keim\runtime\<package hash>`: embedded overlay, agent and helpers.
  Extraction verifies SHA-256, skips valid files, repairs corrupt files and
  serializes concurrent preparation. The hash changes when packaged files change.
- `client-settings.ini` beside `Keim.exe`: automatic preferences, including
  keybinds, HUD layout, Rockstar parameters and server/dimension-scoped waypoints.
- `configs/<name>.ini` beside `Keim.exe`: named profiles saved under
  **Settings → Configs**, including module states and settings, bindings,
  HUD layout, friend relations and waypoints.
- `%LOCALAPPDATA%\Keim\launcher-jdk.txt`: selected JDK path.
- `%LOCALAPPDATA%\Keim\Keim_client.log` and `Keim_overlay.log`: launcher/overlay logs.
- `mcov_agent.log` beside the loaded DLL: agent log, also included under Logs.

On the first portable start without local `client-settings.ini`, existing
automatic preferences from `%LOCALAPPDATA%\Keim\client-settings.ini` are imported.
If absent there, the old `%LOCALAPPDATA%\mcov` location is checked as well. The
previous JDK selection is copied once when Keim has no saved selection.
The build's sibling `bin` folder is a fallback when starting from `build/client`.
This import never replaces local settings. Runtime helpers and logs continue
to use AppData independently of the client-folder configs.

Open **Settings → Configs**, enter a name and choose **Save config**. An existing
name changes the button to **Overwrite config**. Choose a profile in the list
and press **Load selected** to restore its saved module states and settings;
profiles are not selected or loaded automatically at startup. **Refresh** updates
the list after copying a profile into `configs`. Copy `client-settings.ini` and
the `configs` folder with the client to retain your setup.

Closing the launcher leaves the overlay and Minecraft running. Closing during
connection cancels the launcher's attach helper; it never terminates Minecraft.
An agent already loaded into the JVM stays loaded until Minecraft exits.
An incompatible/stale agent requires restarting Minecraft before retrying.
Reconnection compares the loaded agent DLL and both frame/helper JARs with the
selected client package. A healthy heartbeat and matching IPC version alone do
not establish that the loaded code is current. The check also runs when an old
overlay is already open; different code requires a full Minecraft restart.
The default **END** binding quits the overlay, **INSERT** opens its menu and
**F6** opens the HUD editor. Bindings remain configurable.

## Presentation and diagnostics

The injector uses the same approved compact design as the client. Scoped
`CompactStyle` restores the previous ImGui style and font after drawing. Embedded
Inter is 14 pixels, the root is anthracite `#222229` at alpha 0.88 with a 12-pixel
radius, and the sidebar adds `#15151B` at alpha 0.22. Controls use the shared
small radii and coral accent `#EF8B70`. This presentation replaces the launcher's
lavender Liquid Glass surface.

The launcher opens at 640 × 640. Its left **Start / Logs** sidebar is
142 pixels wide, or 112 pixels below 620 pixels viewport width, with simple
authored icons. **Start** contains the native selectable Minecraft list,
refresh, phase/status and JDK recovery. Long instance titles and PID/Java/client
details remain clipped to their row; hovering reveals the full text in a tooltip.
The list and content scroll while the 34-pixel primary action and close footer
remain fixed. Enabled primary text is near-black (`#221B1B`); disabled text is
soft white (`#ECECEF`) under the shared disabled alpha. Busy or absent selection
disables the action. **Logs** contains connection details, copy/open-folder
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
invalid overlay PIDs. It passed against the current `Keim.exe` and exact payloads.
The `mcov` and `mcov_injector` builds, native/UI checks and share ZIP passed.

`control_panel` renders the actual compact launcher offscreen at 640 × 640,
640 × 520 and 480 × 380, covering empty, selected, busy, missing-JDK and diagnostic
states. `LauncherChecks` checks style/font/window-stack restoration, selection,
start, running-instance action, duplicate-start prevention, both sidebar tabs,
fixed close footers and title minimize/close. These checks simulate input and do
not start processes or attach to Minecraft.

The HWID fixture verifies complete copying before sign-in, while verification
is busy and in Logs at both sizes, using synthetic IDs and an isolated in-memory
clipboard. Unavailable IDs leave that clipboard unchanged. The signed KeyAuth
fixture checks that the login request uses exactly the ID exposed to the launcher.

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
