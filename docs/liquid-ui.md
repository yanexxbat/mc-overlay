# Liquid Glass menu and launcher

The client menu and launcher now use the material from
[poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11), adapted for
mcov's pinned Dear ImGui **1.91.9b** and native Win32/D3D11 renderer. The menu's
real controls, section identities, keybinds and HUD behavior are preserved.

## Appearance and operation

The root has neutral dark tint, blurred background, restrained refraction, soft
shadow and subtle light edges. Inter replaces the classic menu's pixel font;
existing HUD/ESP fonts and rendering styles remain unchanged. The default menu
is 900 × 680 UI pixels, clamped to the game viewport minus 24 pixels. Drag its header, use
**Center**, or close it with **Close** or the configured menu bind.

Seven left-side categories remain: **Combat, Visuals, Blocks, HUD, Stats,
Settings, Misc**. The sidebar is 146 pixels wide, or 98 pixels below 620 pixels
root width. Its module grid uses one to three columns of 96-pixel-high tiles
with 12-pixel corners. Left-click toggles real modules; right-click opens their
native controls in a second draggable settings window. Tiles display real enabled
state and available bindings. Settings-only tiles also open on left-click;
Editor and Panic execute their actions. All 76 existing sections are reachable.

The settings window is up to 480 × 610 UI pixels, also clamped to the viewport
minus 24 pixels. It keeps its own position, close button and scrolling content.
Category changes reset the root grid scroll and leave the selected settings
module and its scroll intact. A different module starts at the top; right-clicking
the same module retains scroll and brings settings forward. Closing the root
closes settings. Slider values appear beside their labels; **Ctrl-click** enters
a number directly. Keybind and HUD-editor operation is documented in
[client-ui.md](client-ui.md).

The 640 × 640 launcher uses the same glass root. **Start** contains real Minecraft
instances, status and recovery; **Diagnose** contains connection details and logs.
Its primary **Client starten / Zu Minecraft wechseln** action remains outside
the scrolling content, including smaller windows. Packaging and Java requirements
remain in [launcher.md](launcher.md).

Its native shell uses an opaque base below the glass shader and a rounded
Windows region for exterior clipping, rather than a full transparent DWM frame.
It stays hidden through initialization and draws a frame before showing.
This shell change does not alter the shared glass material or game overlay.

## Live backdrop and fallback

**Settings → Overlay → Menu material** reports either **Liquid glass / live
backdrop** or **Liquid glass / fallback backdrop**. **Background capture** reports
**Live / DXGI Desktop Duplication**, **Live / Windows Graphics Capture**, waiting
for a frame, capture disabled, or the capture error state. A provided GPU fixture
texture reports **Provided texture / no desktop capture** and cannot be reported
as a live desktop backdrop.

DXGI Desktop Duplication first attempts the window's monitor. After 750 ms without
a usable frame, Windows Graphics Capture (WGC) attempts monitor capture on the
existing render device, allowing a monitor on another GPU to reach the material.
WGC uses a free-threaded two-frame pool; a nonblocking poll at a 33-ms cadence
copies the newest frame to a retained GPU texture before releasing its pool slot.
Failed WGC starts retry after two seconds. The copy and blur stay on the GPU.

Closing the menu, disabling desktop capture, supplying a fixture texture or
changing window/monitor ends the capture sessions. DXGI duplication also stops
when WGC takes over. Ordinary capture stops retain reusable blur resources;
window/monitor changes rebuild the binding. Reopening starts a fresh capture session.

While the glass menu is open, Windows capture exclusion applies temporarily to
the overlay window so the material cannot capture itself recursively. This also
excludes the window's other overlay content from captures that honor Windows
display affinity. On menu closure, the configured **Exclude from screen capture**
preference applies again. The launcher likewise restores its previous affinity.
This is required for the desktop-backed material and is not a promise about every
recording tool.

No usable desktop frame, a locked desktop, unsupported capture format or failure
of both capture paths can result in the stable blue/violet fallback. DXGI rejects
rotated outputs and monitors on another GPU before the WGC attempt. The UI stays
usable; fallback does not read world data or pretend to be live. A GPU material
initialization failure uses a rounded tinted ImDrawList panel instead.

## Implementation

- `src/common/liquid_style.hpp`: scoped palette, spacing, rounding and font.
- `src/common/liquid_glass.hpp/.cpp`: RAII GPU resources, capture affinity,
  monitor/window binding, cached backdrop and four retained pane slots.
- `src/common/windows_backdrop_capture.hpp/.cpp`: WGC monitor session,
  free-threaded frame pool and GPU upload through the shared backdrop.
- `third_party/glass/{glass,backdrop,shaders,spring}`: adapted renderer/material
  and blur source; not the upstream demo application or widget suite.
- `src/overlay/control_panel.cpp`: root, category sidebar, module grid, independent
  settings window and real inputs.
- `src/overlay/overlay_modules.cpp`: catalog and activation tied to real module stores.
- `src/injector/launcher_ui.cpp`: launcher appearance and existing action results.
- `src/common/flat_app.cpp` / `src/overlay/overlay.cpp`: GPU lifecycle and frame
  integration before ordinary ImGui compositing.

Each glass pane is submitted during UI construction, rendered to its own retained
texture after `ImGui::Render()`, then composed through its window's draw list.
The fixed pool has four slots, with independent texture/RTV/SRV resources created
on first use and recreated after viewport resize. Root and settings reuse the
same captured and blurred desktop. Their separate images preserve window order
without drawing both panes again in each window. World effects/HUD remain below
the menu; text and controls remain above its material. The material has no
JNI dependency and does not alter shared-memory layout or world-depth masking.
Style/font changes restore on scope exit. Embedded shader strings and existing
font assets add no loose runtime files to the portable package. WGC uses the
standard Windows SDK `windowsapp` library; the portable package still needs only
`mcov.exe` and optional `START.txt`.

The shared material uses tint RGB 0.045/0.050/0.063, tint opacity 0.56,
blur mix 0.78, saturation 0.9, brightness offset −0.02 and contrast 0.94.
The backdrop uses three Kawase downsample passes, two intermediate upsample
passes and final full-resolution reconstruction instead of the previous six/five
chain. Inset child fill alpha is 0.24, and muted text is `#D7DCEC`.

Upstream source: commit `00c2b2b19eb5523abde473b2c08220326d4146d4`.
The MIT notice is retained in `third_party/glass/LICENSE.liquidDX11` and embedded
as RCDATA 103 in the executable. See [third-party notices](../THIRD-PARTY-NOTICES.md).

## Verification boundary

The targeted `launcher_window`, `liquid_glass`, `flat_ui`, `client_controls`, `control_panel`,
`aura_rendering`, `depth_rendering` and `portable_client` checks exercise real
D3D11 WARP shaders/pixels, clipping, resize, style ownership, menu input,
keybind/HUD behavior and packaging. Preview PNGs under `build/ui-preview` retain
historical `classic-*.png` names but show the current material. Their deterministic
backdrop is a fallback, not live Minecraft capture.

Current native review images are
`.impeccable/review/{glass-grid-desktop,glass-settings-desktop,glass-grid-compact,glass-settings-compact}.png`.
They render the real GPU material and UI over synthetic scene textures; they are
not Minecraft screenshots and do not establish live capture. Earlier
`{desktop-grid,desktop-settings,compact-grid,compact-settings,large-settings}.png`
review images use the deterministic WARP fallback.

The opt-in native `mcov_backdrop_capture_check --live` probe passed WGC startup,
the full LiquidGlass runtime, capture pause, display-affinity restoration and
live capture after reopening on this host. It neither saves desktop images nor
attaches to a JVM. Actual Minecraft appearance, attach/focus behavior and other
GPU/runtime combinations still need a game session.

The hidden native launcher test checks the actual swapchain, opaque base,
foreground pixels, resize and rounded HWND region. Its
`.impeccable/review/launcher-native-{start,diagnose}.png` images are swapchain
readbacks before OS clipping; direct desktop visibility is not established by
these fixtures. `mcov_backdrop_capture_check --live-sta` separately passed with
the launcher's COM apartment mode.
