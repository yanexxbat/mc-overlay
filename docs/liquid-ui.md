# Liquid Glass launcher and compact client boundary

The launcher retains the material from
[poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11), adapted for
mcov's pinned Dear ImGui **1.91.9b** and native Win32/D3D11 renderer. The current
client uses a separate compact translucent style; its layout and operation are
documented in [compact-ui.md](compact-ui.md) and [client-ui.md](client-ui.md).

## Appearance and operation

The 640 × 640 launcher retains neutral dark tint, blurred backdrop, restrained
refraction, soft shadow, light edges and its lavender control palette. Embedded
Inter provides body text and the larger semibold heading. **Start** contains real
Minecraft instances, status and recovery; **Diagnose** contains connection details
and logs. The primary **Client starten / Zu Minecraft wechseln** action stays
outside scrolling content, including in smaller windows. Packaging and Java
requirements remain in [launcher.md](launcher.md).

The launcher's native shell uses an opaque base below the glass shader and a
rounded Windows region for exterior clipping. It stays hidden through
initialization and draws a frame before showing. Minimizing pauses capture.

The client instead draws ordinary ImGui geometry with an 88%-opaque anthracite
root, coral controls and direct alpha compositing over the game. Its
`LiquidGlass::BeginFrame` call receives visibility false. **Settings → Overlay →
Menu material** reports **Translucent dark / 88% opacity** and **Background capture**
reports **Not required**. The configured **Exclude from screen capture** preference
also applies while the client menu is open. Client tabs and in-window settings
replace the former glass tile grid and independent settings pane.

## Launcher backdrop and fallback

DXGI Desktop Duplication first attempts the launcher's monitor. After 750 ms
without a usable frame, Windows Graphics Capture (WGC) attempts monitor capture
on the existing render device, allowing a monitor on another GPU to reach the
material. WGC uses a free-threaded two-frame pool; a nonblocking poll at a 33-ms
cadence copies the newest frame to a retained GPU texture before releasing its
pool slot. Failed WGC starts retry after two seconds. Copy and blur stay on the GPU.

Closing/minimizing the glass surface, disabling desktop capture, supplying a
fixture texture or changing window/monitor ends capture sessions. DXGI duplication
also stops when WGC takes over. Ordinary capture stops retain reusable blur
resources; window/monitor changes rebuild the binding. Reopening starts a fresh
capture session.

While launcher glass is visible, temporary Windows capture exclusion prevents
recursive feedback and restores its prior display affinity afterward. This is
the launcher's material requirement; opening the compact client menu does not
temporarily override the overlay's configured capture preference.

The retained renderer distinguishes live DXGI/WGC capture, waiting/error state,
disabled capture and **Provided texture / no desktop capture**. A fixture texture
cannot be reported as a live desktop backdrop. No usable desktop frame, a locked
desktop, unsupported format or failure of both capture paths uses the stable
blue/violet fallback. DXGI rejects rotated outputs and monitors on another GPU
before the WGC attempt. A GPU material initialization failure uses a rounded
tinted ImDrawList panel. Neither fallback invents live capture or reads world data.

## Implementation

- `src/common/liquid_style.hpp`: scoped launcher palette, spacing, rounding and font.
- `src/common/liquid_glass.hpp/.cpp`: RAII GPU resources, capture affinity,
  monitor/window binding, cached backdrop and four retained pane slots.
- `src/common/windows_backdrop_capture.hpp/.cpp`: WGC monitor session,
  free-threaded frame pool and GPU upload through the shared backdrop.
- `third_party/glass/{glass,backdrop,shaders,spring}`: adapted renderer/material
  and blur source; not the upstream demo application or widget suite.
- `src/injector/launcher_ui.cpp`: launcher appearance and existing action results.
- `src/common/flat_app.cpp`: launcher GPU lifecycle and frame integration.
- `src/common/compact_style.hpp` / `src/overlay/control_panel.cpp`: client-local
  style, module tabs, two child areas and native inputs.
- `src/overlay/overlay.cpp`: direct client compositing and inactive glass frame;
  `overlay_modules.cpp` ties the catalog and activation to real module stores.

A glass pane is submitted during UI construction, rendered to its retained
texture after `ImGui::Render()`, then composed through its window's draw list.
The fixed pool supports four slots with independent texture/RTV/SRV resources
created on first use and recreated after viewport resize. The client no longer
submits root/settings panes. The material has no JNI dependency and does not
alter shared-memory layout or world-depth masking. Both style/font scopes restore
on exit. Embedded shaders and fonts add no loose runtime files. WGC uses the
Windows SDK `windowsapp` library; the portable package still needs only
`mcov.exe` and optional `START.txt`.

The launcher's material uses tint RGB 0.045/0.050/0.063, tint opacity 0.56,
blur mix 0.78, saturation 0.9, brightness offset −0.02 and contrast 0.94.
Backdrop blur uses three Kawase downsample passes, two intermediate upsample
passes and final full-resolution reconstruction. Launcher inset fill alpha is
0.24 and muted text is `#D7DCEC`; these are separate from the client tokens.

Upstream source: commit `00c2b2b19eb5523abde473b2c08220326d4146d4`.
The MIT notice remains in `third_party/glass/LICENSE.liquidDX11` and embedded
as RCDATA 103. See [third-party notices](../THIRD-PARTY-NOTICES.md).

## Verification boundary

Native launcher/glass checks exercise real D3D11 WARP shaders, clipping, resize,
resource ownership and portable packaging. The hidden native launcher test checks
the actual swapchain, opaque base, foreground pixels, resize and rounded HWND
region. Its `.impeccable/review/launcher-native-{start,diagnose}.png` images are
swapchain readbacks before OS clipping; they do not establish direct desktop
visibility.

The opt-in `mcov_backdrop_capture_check --live` probe passed WGC startup,
full LiquidGlass runtime, capture pause, display-affinity restoration and live
capture after reopening on this host. It neither saves desktop images nor
attaches to a JVM. `--live-sta` separately passed with the launcher's COM apartment
mode. These checks validate the retained capture renderer, not a client capture
requirement or every GPU/runtime combination.

The current client review images are
`.impeccable/review/compact-client/{combat-desktop,aim-desktop,aim-compact,visuals-desktop,keybinds-desktop}.png`.
They show direct ImGui alpha compositing over synthetic backgrounds. The older
`glass-grid-*` and `glass-settings-*` images show the superseded client material
over synthetic textures; older fallback images and historical `classic-*.png`
filenames are not authority for current client layout. Actual Minecraft appearance
and attach/focus behavior still require a game session.
