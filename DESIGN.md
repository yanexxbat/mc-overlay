---
name: mcov Liquid Glass
description: A native, readable control surface over the Minecraft viewport.
colors:
  primary: "#818CF8"
  primary-ink: "#B2B9FF"
  primary-grab-active: "#E3E6FF"
  text: "#F3F4FF"
  text-muted: "#D7DCEC"
  surface-window: "rgba(21,26,42,0.96)"
  surface-child: "rgba(16,21,34,0.24)"
  surface-popup: "rgba(34,43,66,0.99)"
  surface-control: "rgba(192,207,255,0.09)"
  surface-control-hover: "rgba(192,207,255,0.17)"
  surface-control-active: "rgba(192,207,255,0.23)"
  surface-button: "rgba(192,207,255,0.10)"
  surface-button-hover: "rgba(192,207,255,0.21)"
  surface-tab: "rgba(192,207,255,0.06)"
  surface-tab-selected: "rgba(129,140,248,0.32)"
  border: "rgba(185,203,255,0.16)"
  separator: "rgba(203,213,255,0.10)"
typography:
  title:
    fontFamily: "Inter"
    fontSize: "22.1px"
    fontWeight: 600
  body:
    fontFamily: "Inter"
    fontSize: "14px"
    fontWeight: 500
  label:
    fontFamily: "Inter"
    fontSize: "14px"
    fontWeight: 600
rounded:
  window: "22px"
  child: "14px"
  popup: "12px"
  navigation: "10px"
  module: "12px"
  control: "8px"
  grab: "6px"
spacing:
  window: "16px"
  content: "13px"
  frame-x: "10px"
  frame-y: "6px"
  item-x: "10px"
  item-y: "8px"
  inner-x: "7px"
  inner-y: "5px"
components:
  button:
    backgroundColor: "{colors.surface-button}"
    textColor: "{colors.text}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "6px 10px"
  button-hover:
    backgroundColor: "{colors.surface-button-hover}"
    textColor: "{colors.text}"
    rounded: "{rounded.control}"
  input:
    backgroundColor: "{colors.surface-control}"
    textColor: "{colors.text}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "6px 10px"
  input-hover:
    backgroundColor: "{colors.surface-control-hover}"
    textColor: "{colors.text}"
    rounded: "{rounded.control}"
  category-navigation:
    textColor: "{colors.text}"
    typography: "{typography.body}"
    rounded: "{rounded.navigation}"
    padding: "7px 4px"
  module-tile:
    textColor: "{colors.text}"
    typography: "{typography.label}"
    rounded: "{rounded.module}"
    height: "96px"
  content-panel:
    backgroundColor: "{colors.surface-child}"
    rounded: "{rounded.child}"
    padding: "13px"
---

# Design System: mcov Liquid Glass

## Overview

**Creative North Star: "Liquid Glass"**

Mode: Operate. The user's latest visual authority is
[poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11), replacing
the earlier classic dark/violet menu. This system describes the implemented
native Windows control surface, not a web dashboard or a copy of the reference
demo's content.

A dark, neutral-tinted glass root provides blur, restrained refraction and subtle
light edges. Crisp Inter text and quiet translucent controls keep real settings
readable over changing game imagery. The client menu and launcher share this
material; existing world effects, entity ESP and modular HUD retain their own
visuals and behavior.

**Key Characteristics:**

- A softly rounded glass root with calmer, inset setting surfaces.
- Seven left-side categories, a responsive module grid and independent settings glass.
- Visible values, conventional checkbox interaction and keyboard-editable sliders.
- A primary launcher action that remains outside its scrolling details.
- Scoped style/font changes and a deterministic capture fallback.

## Colors

The palette combines cool neutral text with a restrained lavender-blue accent.
The frontmatter records the actual shared style values; translucent colors depend
on the dark root underneath them.

### Primary

- **Lavender blue** (`primary`): selected categories, enabled modules and active buttons.
- **Pale lavender** (`primary-ink`): checkmarks, slider rails, values and keyboard
  navigation outlines. Active slider grabs use `primary-grab-active`.

### Neutral

- **Cool white** (`text`): labels, values and actions.
- **Muted cool white** (`text-muted`): supporting instructions and inactive state.
- **Night blue** (`surface-window`, `surface-child`, `surface-popup`): shared window
  defaults, inset content and nearly opaque popups. The shader supplies the root
  material instead of painting the window default over it.
- **Light translucent control fills**: frame, button and tab tokens distinguish
  rest, hover and active state without competing with the root.
- **Quiet light edges** (`border`, `separator`): popup boundaries and section
  divisions and module boundaries.

**The Readable Glass Rule.** Root refraction and blur remain behind text and
controls. Dense settings use the darker inset content surface; they do not
inherit a more transparent root merely to emphasize the effect.

## Typography

Menu and launcher use embedded Inter: medium body and controls, semibold section
labels and the existing larger semibold font for the mcov heading. The larger
heading derives from the default font size multiplied by 1.3. The pinned atlas
and HUD/ESP fonts remain intact.

### Hierarchy

- **Title:** the `title` role is used for the root's mcov heading.
- **Body:** the `body` role is used for controls, category names and supporting text.
- **Label:** the `label` role is used for section headings.

**The Local Type Rule.** `LiquidStyle` pushes the menu font and restores it on
scope exit. Do not change global font defaults to restyle the menu or launcher.

## Layout

The client menu targets 900 × 680 UI pixels and clamps to the viewport minus
24 pixels, with a minimum requested size of 160 pixels. Its draggable header and
footer frame a left category sidebar and independently scrolling module grid.
The seven categories remain Combat, Visuals, Blocks, HUD, Stats, Settings and Misc.
The sidebar is 146 pixels wide, reducing to 98 pixels below 620 pixels root width.
The grid uses one to three equal-width columns, 96-pixel-high tiles and the
existing 10-pixel horizontal item spacing. Changing category resets grid scroll.
Center, Reset and Close remain in the root footer; Reset applies to that category.

Right-clicking a tile opens that module's existing controls in a second draggable
glass window, up to 480 × 610 pixels and clamped to the viewport minus 24 pixels.
It has its own retained position, close button and scrolling inset content. It
remains on the selected module when the root category changes. Selecting a
different module resets settings scroll; selecting the same module preserves it
and brings the settings window forward. Closing the root closes settings too.
All 76 existing sections remain reachable through their category tiles.

The launcher opens at 640 × 640. Its Start and Diagnose views share the root
material. The instance list, status and recovery instructions scroll inside a
child. The 40-pixel primary start action is outside that child, above the footer,
so small windows retain the main operation. Diagnostics remains a separate view.

## Elevation & Depth

`LiquidGlass` retains a fixed pool of four shader-backed D3D11 panes. Each visible
glass window uses its own texture, render target and shader resource view,
allocated on first use and recreated after viewport resize. The
adapted liquidDX11 renderer combines Dual Kawase backdrop blur, a neutral dark tint,
moderate edge refraction, subtle chromatic separation, light rims and a soft
shadow. Root and settings panes reuse the same captured and blurred desktop;
nested content surfaces use ordinary ImGui fills. Independent pane images retain
ImGui window order without composing both glass windows into each image.

The material's shared parameters live in `src/common/liquid_glass.cpp`: tint RGB
0.045/0.050/0.063 with opacity 0.56, blur mix 0.78, saturation 0.9, brightness
offset −0.02 and contrast 0.94. Backdrop blur uses three Kawase downsample passes,
two intermediate upsample passes and final reconstruction to full resolution,
preserving recognizable background contours.

Capture is polled at a 33-ms cadence only while the menu or launcher is visible.
DXGI Desktop Duplication runs first; after 750 ms without a usable frame,
Windows Graphics Capture attempts monitor capture on the existing render device.
Failed WGC starts retry after two seconds. Capture sessions stop when glass closes,
capture is disabled, a fixture texture is supplied or the monitor changes; DXGI
also stops at the WGC handoff. Blur resources survive ordinary capture stops.
Capture failure uses an immutable blue/violet backdrop; failure of the GPU
material uses the rounded tinted draw-list fallback. Neither fallback nor a
provided GPU fixture texture invents a live desktop capture status.

**The Layer Order Rule.** World effects and HUD precede the menu glass; controls
and text follow it. `RenderTexture()` plus the root's `AddImage` retain this order.
Each window's own image retains that order when windows overlap. The glass pass
introduces no JVM calls and does not replace world-depth handling.

## Shapes

The rounded scale in the frontmatter differentiates the root, inset content,
popups and controls. Category buttons use the navigation radius; module tiles use
the module radius. Root and settings glass use the window radius. Shadow and edge shading follow the
same rounded shape. Fields remain rectangular enough to read as settings, with
small slider grabs instead of the old full-width violet value bars.

## Components

### Buttons and navigation

Ordinary buttons use translucent neutral fills; selected left-side categories
use lavender-blue fills. Category labels and existing section identities remain
stable. ImGui keyboard navigation retains its visible cursor color.

### Module tiles and settings

Tiles show the genuine module name and, for toggles, Enabled or Disabled plus the
available binding. Enabled modules receive the existing lavender-blue accent;
hover and keyboard focus remain visible. Left-click changes the real module
state. Right-click opens or raises the separate settings window, including when
the same module is already selected. Friends, Entity filters, Block types and
the Stats/Settings sections instead say Open settings and open on left-click too.
Editor and Panic say Run action: left-click opens the HUD editor or invokes
Panic. Settings and actions do not present a fabricated enabled state.

### Inputs and sliders

Checkboxes use actual ImGui checkbox items. Combos and color editors retain their
native popups and IDs. Sliders have a thin rail, pale grab and right-aligned value
beside the label; hovering/dragging adds a restrained grab halo. Ctrl-click enters
numeric input with visible text and caret. Focus receives a visible outline.
Bounds and clamping remain those of each existing setting.

### Containers and section labels

The root is the signature material. Content children use the darker inset fill
and independent clipping/scrolling. Semibold section labels and quiet separators
group settings. `StyleScope` contains local changes; `LiquidStyle` restores the
entire prior ImGui style and font after menu/launcher rendering.

### Launcher

The launcher retains real instance selection, refresh, busy/disabled state,
missing-JDK recovery, log-copy/open actions and close/minimize controls. The
primary action selects either client startup or return to the running Minecraft
instance. No borrowed sign-in form, recorder or reference-demo widgets are added.

The launcher shell keeps an opaque base below the shader material and clips its
outer padding with a native rounded window region. It does not use a full
transparent DWM sheet. Initialization remains hidden and a frame is drawn before
the window is shown; minimizing pauses desktop capture. The shared material,
palette, typography and controls remain unchanged.

### Preserved HUD and world surfaces

Nineteen HUD elements retain independent visibility, position and scale. The
editor keeps drag previews, selection, reset and its small-viewport behavior.
Keyboard/mouse bindings retain Toggle/Hold, modifier chords, conflict detection
and focus restrictions. Module binds remain initially unbound; INSERT, F6 and END
remain defaults.

Entity ESP retains the user's clean rounded box/skeleton treatment, health bar,
nametags and equipment row. Target Aura, Marker, trails, particles and hats keep
their existing glow, world projection and depth-occlusion controls. Combat retains
the simplified Kill Aura controls and separate module settings. Detailed existing
behavior lives in `docs/client-ui.md`, `docs/aura.md`, `docs/target-cube.md` and
`docs/world-visuals.md`.

## Do's and Don'ts

### Do:

- **Do** reuse the shared glass root and scoped `LiquidStyle` for client controls.
- **Do** preserve real module names, section identities, keybinds and input behavior.
- **Do** keep the launcher start action outside scrolling content.
- **Do** keep HUD/ESP rendering styles and JNI acquisition separate from menu material.
- **Do** report live versus fallback material and the actual Background capture state under Settings → Overlay.

### Don't:

- **Don't** globally replace the HUD/ESP style or fonts during a menu redesign.
- **Don't** capture an unexcluded glass window into its own backdrop.
- **Don't** require loose shader/font/DLL files beside the portable EXE.
- **Don't** claim offscreen WARP previews validate live Minecraft capture on every GPU.
- **Don't** reintroduce reference-demo controls as placeholders for mcov modules.
