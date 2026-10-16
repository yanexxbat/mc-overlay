# Shared compact UI and retained LiquidGlass renderer

The client and injector use the same approved compact anthracite/coral design.
Both render ordinary ImGui geometry with direct alpha compositing and embedded
Inter at 14 pixels. The active launcher no longer uses LiquidGlass, backdrop
capture, blur, refraction or the former lavender palette. Current layout and
operation are documented in [compact-ui.md](compact-ui.md),
[client-ui.md](client-ui.md) and [launcher.md](launcher.md).

The retained renderer comes from
[poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11), adapted for
mcov's pinned Dear ImGui **1.91.9b** and Win32/D3D11. The material and capture facts
below describe that legacy/standalone capability, not current injector behavior.

## Active compact surfaces

Both roots use anthracite at alpha 0.88, a darker sidebar overlay, a 12-pixel
radius and compact coral controls. Scoped `CompactStyle` restores the previous
style and font after each surface renders. The injector opens at 640 × 640,
with a Start/Diagnose sidebar (142 pixels, or 112 below 620 pixels viewport
width). Its 34-pixel primary action and close footer stay outside scrolling
status, JDK recovery and diagnostics. Small-window coverage is 480 × 380.

`FlatApp` has removed LiquidGlass ownership, initialization, BeginFrame, render
and shutdown integration. Its render target clears to RGBA (0, 0, 0, 0), DWM
frame-extension margins are −1, and its rounded native region uses a 24-pixel
corner diameter matching the panel radius. Initialization stays hidden until
`Run` presents a frame before `ShowWindow`. Native drag, minimize/close and
existing launch semantics are unchanged.

The client glass renderer receives visibility false. **Settings → Overlay →
Menu material** reports **Translucent dark / 88% opacity** and **Background capture**
reports **Not required**. The configured **Exclude from screen capture** preference
also applies while the client menu is open. Client tabs and in-window settings
replace the former glass tile grid and independent settings pane.

## Retained backdrop and fallback capability

When the retained LiquidGlass renderer is explicitly used with a visible glass
surface, DXGI Desktop Duplication first attempts the bound monitor. After 750 ms
without a usable frame, Windows Graphics Capture (WGC) attempts monitor capture
on the existing render device, allowing a monitor on another GPU to reach the
material. WGC uses a free-threaded two-frame pool; a nonblocking poll at a 33-ms
cadence copies the newest frame to a retained GPU texture before releasing its
pool slot. Failed WGC starts retry after two seconds. Copy and blur stay on the GPU.

Hiding/minimizing a glass surface, disabling desktop capture, supplying a fixture
texture or changing window/monitor ends capture sessions. DXGI duplication also
stops when WGC takes over. Ordinary capture stops retain reusable blur resources;
window/monitor changes rebuild the binding. Reopening starts a fresh session.

A visible capture-backed glass surface temporarily excludes its window from
capture to prevent recursive feedback, restoring its previous display affinity
when that material stops. Neither active compact surface needs this material
exclusion; the client retains its configured capture preference.

The retained renderer distinguishes live DXGI/WGC capture, waiting/error state,
disabled capture and **Provided texture / no desktop capture**. A fixture texture
cannot be reported as a live desktop backdrop. No usable desktop frame, a locked
desktop, unsupported format or failure of both capture paths uses the stable
blue/violet fallback. DXGI rejects rotated outputs and monitors on another GPU
before the WGC attempt. A GPU material initialization failure uses a rounded
tinted ImDrawList panel. Neither fallback invents live capture or reads world data.

## Implementation and retained material

- `src/common/compact_style.hpp`: shared scoped client/injector palette, spacing,
  rounding and font.
- `src/injector/launcher_ui.cpp`: compact launcher navigation and real actions.
- `src/common/flat_app.cpp`: transparent native injector shell and D3D11 lifecycle,
  without LiquidGlass integration.
- `src/overlay/control_panel.cpp`: compact module widgets, independent settings window and native inputs.
- `src/overlay/overlay.cpp`: direct client compositing and inactive glass frame;
  `overlay_modules.cpp` ties catalog and activation to real module stores.
- `src/common/liquid_style.hpp`: retained legacy style; not the active injector theme.
- `src/common/liquid_glass.hpp/.cpp`: retained RAII GPU resources, capture affinity,
  monitor/window binding, cached backdrop and four pane slots.
- `src/common/windows_backdrop_capture.hpp/.cpp`: retained WGC monitor session,
  free-threaded frame pool and GPU upload through the shared backdrop.
- `third_party/glass/{glass,backdrop,shaders,spring}`: adapted renderer/material
  and blur source; not the upstream demo application or widget suite.

For retained glass callers, a pane is submitted during UI construction, rendered
to its retained texture after `ImGui::Render()`, then composed through its window's
draw list. The four-slot pool has independent texture/RTV/SRV resources created
on first use and recreated after resize. Neither current compact surface submits
these panes. The material has no JNI dependency and does not alter shared-memory
layout or world-depth masking. Embedded shaders and fonts add no loose runtime
files. WGC uses the Windows SDK `windowsapp` library; the portable package still
needs only `Keim.exe` and optional `START.txt`.

The retained material uses tint RGB 0.045/0.050/0.063, tint opacity 0.56,
blur mix 0.78, saturation 0.9, brightness offset −0.02 and contrast 0.94.
Backdrop blur uses three Kawase downsample passes, two intermediate upsample
passes and final full-resolution reconstruction. The legacy LiquidStyle inset
fill alpha is 0.24 and muted text is `#D7DCEC`. These values do not define the
active shared compact system.

Upstream source: commit `00c2b2b19eb5523abde473b2c08220326d4146d4`.
The MIT notice remains in `third_party/glass/LICENSE.liquidDX11` and embedded
as RCDATA 103. See [third-party notices](../THIRD-PARTY-NOTICES.md).

## Verification boundary

Current native/UI and portable packaging checks passed. `launcher_window` checks
an actual hidden HWND/swapchain for translucent alpha, transparent margins,
foreground control ink, resize, rounded clipping and presentation; it requires
the window to remain invisible. `control_panel` checks selection, start,
duplicate-start prevention, both sidebar tabs, footers and title controls.
Current injector images are in `.impeccable/review/compact-injector/`; its two
`launcher-native-*` files are hidden swapchain readbacks before OS clipping.
All nine injector images passed independent finish review with no material findings.

Retained `liquid_glass` checks exercise real D3D11 WARP shaders, clipping, resize
and resource ownership. Historical opt-in `mcov_backdrop_capture_check --live`
and `--live-sta` probes passed WGC startup, the full LiquidGlass runtime, capture
pause, display-affinity restoration and capture after reopening on this host.
They saved no desktop images and attached to no JVM. The STA probe used the COM
mode that the launcher continues to use. These results validate the retained
capture capability, not an active injector requirement or every GPU/runtime.

Current client images remain in
`.impeccable/review/compact-client/{combat-desktop,aim-desktop,aim-compact,visuals-desktop,keybinds-desktop,keybinds-compact,keybinds-compact-bottom}.png`.
They show direct ImGui alpha compositing over synthetic backgrounds. Older
`glass-grid-*` and `glass-settings-*` images show the superseded client material;
older launcher glass/fallback images also document the superseded presentation.
Historical `classic-*.png` filenames are not authority for current layout.
Current fixture images and hidden readbacks do not prove live desktop composition
or Minecraft appearance. Actual Minecraft attach/focus behavior requires a game session.
