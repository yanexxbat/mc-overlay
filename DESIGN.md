---
name: Keim Compact Controls
description: Shared compact translucent anthracite and lime-green controls for the client, injector and snapshot HUD.
colors:
  primary: "#8EDB1A"
  primary-active: "#A6EF42"
  primary-pressed: "#72B411"
  primary-ink: "#17200B"
  positive: "#A5C7AD"
  warning: "#E8BB73"
  danger: "#F28080"
  error-text: "#FFA6A6"
  text: "#ECECEF"
  text-muted: "#AEAEB8"
  navigation-text: "#C2C2CC"
  navigation-selected-text: "#D4F3B8"
  surface-window: "rgba(34,34,41,0.88)"
  surface-sidebar: "rgba(21,21,27,0.22)"
  surface-child: "rgba(0,0,0,0)"
  surface-popup: "rgba(38,38,45,0.98)"
  surface-control: "rgba(255,255,255,0.045)"
  surface-control-hover: "rgba(255,255,255,0.085)"
  surface-control-active: "rgba(255,255,255,0.12)"
  surface-button-hover: "rgba(255,255,255,0.09)"
  surface-button-active: "rgba(142,219,26,0.18)"
  surface-navigation-selected: "rgba(255,255,255,0.065)"
  surface-tab: "rgba(255,255,255,0.035)"
  surface-tab-selected: "rgba(142,219,26,0.16)"
  surface-widget-enabled: "rgba(142,219,26,0.10)"
  surface-widget-active: "rgba(142,219,26,0.22)"
  surface-value: "rgba(37,37,44,0.72)"
  surface-instance-list: "rgba(255,255,255,0.025)"
  border: "rgba(255,255,255,0.08)"
  separator: "rgba(255,255,255,0.065)"
typography:
  title:
    fontFamily: "Inter"
    fontSize: "14px"
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
  window: "12px"
  child: "8px"
  popup: "6px"
  control: "4px"
  tab: "5px"
  grab: "3px"
spacing:
  window-x: "16px"
  window-y: "14px"
  frame-x: "7px"
  frame-y: "3px"
  item-x: "9px"
  item-y: "6px"
  inner-x: "6px"
  inner-y: "4px"
  cell-x: "3px"
  cell-y: "4px"
  sidebar-x: "10px"
  launcher-inset: "12px"
components:
  button:
    backgroundColor: "{colors.surface-control}"
    textColor: "{colors.text}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "3px 7px"
  button-hover:
    backgroundColor: "{colors.surface-button-hover}"
    textColor: "{colors.text}"
    rounded: "{rounded.control}"
  button-active:
    backgroundColor: "{colors.surface-button-active}"
    textColor: "{colors.text}"
    rounded: "{rounded.control}"
  input:
    backgroundColor: "{colors.surface-control}"
    textColor: "{colors.text}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "3px 7px"
  category-navigation:
    backgroundColor: "{colors.surface-child}"
    textColor: "{colors.navigation-text}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "4px 32px"
    height: "29px"
  category-navigation-selected:
    backgroundColor: "{colors.surface-navigation-selected}"
    textColor: "{colors.navigation-selected-text}"
    rounded: "{rounded.control}"
  module-widget:
    backgroundColor: "{colors.surface-control}"
    textColor: "{colors.text}"
    typography: "{typography.title}"
    rounded: "{rounded.child}"
    padding: "12px"
    height: "66px"
  module-widget-hover:
    backgroundColor: "{colors.surface-control-hover}"
    textColor: "{colors.text}"
    rounded: "{rounded.child}"
  module-widget-enabled:
    backgroundColor: "{colors.surface-widget-enabled}"
    textColor: "{colors.text}"
    rounded: "{rounded.child}"
  module-widget-enabled-hover:
    backgroundColor: "{colors.surface-tab-selected}"
    textColor: "{colors.text}"
    rounded: "{rounded.child}"
  module-widget-active:
    backgroundColor: "{colors.surface-widget-active}"
    textColor: "{colors.text}"
    rounded: "{rounded.child}"
  content-panel:
    backgroundColor: "{colors.surface-child}"
    rounded: "{rounded.child}"
  client-root:
    backgroundColor: "{colors.surface-window}"
    rounded: "{rounded.window}"
    width: "740px"
    height: "520px"
  settings-root:
    backgroundColor: "{colors.surface-window}"
    rounded: "{rounded.window}"
    width: "480px"
    height: "580px"
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.primary-ink}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "3px 7px"
    height: "34px"
  button-primary-hover:
    backgroundColor: "{colors.primary-active}"
    textColor: "{colors.primary-ink}"
    rounded: "{rounded.control}"
  button-primary-active:
    backgroundColor: "{colors.primary-pressed}"
    textColor: "{colors.primary-ink}"
    rounded: "{rounded.control}"
  button-primary-disabled:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.text}"
    rounded: "{rounded.control}"
  launcher-root:
    backgroundColor: "{colors.surface-window}"
    rounded: "{rounded.window}"
    width: "640px"
    height: "640px"
  instance-list:
    backgroundColor: "{colors.surface-instance-list}"
    rounded: "{rounded.child}"
    padding: "8px"
  hud-panel:
    backgroundColor: "{colors.surface-window}"
    textColor: "{colors.text}"
    typography: "{typography.body}"
    rounded: "{rounded.child}"
---

# Design System: Keim Compact Controls

## Overview

**Creative North Star: "Compact Translucent Controls"**

The approved visual system is compact anthracite with a narrow sidebar and a
lime-green accent, shared with the injector and snapshot HUD. The user selected
this green on 2026-10-15 to match the Keim mascot; it replaces the earlier coral
in `design/reference-translucent-menu.png` while preserving the compact layout. The client's current layout follows
the user's Paint sketch, `design/reference-module-widgets.png`: categories contain
module widgets, and right-click opens an independent settings window. This
replaces the client's module tabs, inline settings and Quick toggles. Keim
retains its real module names, settings and actions.

Client controls, injector and HUD editor use direct ImGui alpha compositing,
scoped `CompactStyle`, embedded Inter and modest headings. Snapshot HUDs share
the drawing tokens and font. The client organizes module settings; the injector
organizes instance selection and diagnostics. The shared palette and controls
keep these operations consistent. World effects retain their styles.

**Key Characteristics:**

- Lowercase **keim** wordmark with the user's green germ mascot. The transparent
  master is `src/resources/keim-logo.png`; the packed 128px RGBA atlas cell and
  multi-size Windows ICO ship as executable resources. The mascot is 28px in
  the launcher/menu header and 24px in the HUD watermark, without a white plate.
- Shared compact anthracite roots with restrained lime-green interaction states.
- Seven categories, searchable module widgets and independently movable settings.
- Native keyboard input, visible slider values and real module state.
- Local style/font ownership and direct transparency without backdrop capture.
- Start/Logs navigation with a fixed launcher action and close footer.

## Colors

Shared drawing tokens come from `src/common/compact_theme.hpp`; control styling
comes from `src/common/compact_style.hpp`. The injector's solid
primary button and list treatment come from `src/injector/launcher_ui.cpp`.
The frontmatter is normative; obsolete launcher lavender primitives are removed.

### Primary

- **Lime green** (`primary`): checkmarks, slider fill, active category icons and
  enabled widget tint, plus the launcher's main action. **Light lime green**
  (`primary-active`) marks slider activity, keyboard navigation, text links and
  primary-button hover, plus HUD selection and active states. **Deep lime green** (`primary-pressed`) marks that button's
  pressed state.

### Neutral

- **Soft white** and **muted gray** (`text`, `text-muted`): shared labels, values
  and supporting text. Navigation has its own quiet and selected text tones.
- **Anthracite** (`surface-window`): client/injector roots and HUD panels. The sidebar adds its darker
  overlay; children remain transparent. Popups use a nearly opaque dark fill.
- **Translucent white fills**: control and navigation tokens distinguish rest
  and hover. Module widgets reuse control fills; lime-green tint distinguishes enabled,
  enabled-hover and pressed widget states. Retained tab tokens serve native tabs
  used by other callers.
- **Dark value lozenge** (`surface-value`): keeps numeric slider text legible
  across the filled and unfilled portions.
- **Near-black primary ink** (`primary-ink`): enabled lime-green action text. Disabled
  actions use soft white with the shared disabled alpha (0.48).
- **Quiet instance surface** (`surface-instance-list`): the native selectable
  instance list within the launcher's transparent content area.

HUD feedback uses **soft green** (`positive`) for favorable health/friends,
**amber** (`warning`) for durability warnings and **soft red** (`danger`) for
danger or depleted durability. Lime green continues to identify activity.

**The Material Scope Rule.** Client, injector and snapshot HUD use the same compact palette
and direct alpha compositing. World effects retain their styles.

## Typography

Client, injector and snapshot HUD use embedded Inter at the same compact size. Medium body text and
semibold labels keep controls and headings aligned; the Keim/category/page header
is not a display title. Fonts remain part of the executable and existing ESP
atlas entries remain intact.

### Hierarchy

- **Title:** Keim, client category and launcher page headers.
- **Body:** controls, widget state labels, category names and supporting text.
- **Label:** section headings.

**The Local Type Rule.** Both surfaces use `CompactStyle` to push their menu font
and restore the prior style and font on scope exit. The HUD editor uses the same
scope; snapshot HUD rendering pushes and restores the shared `MenuFont` locally.

## Layout

The client targets 740 × 520 UI pixels; the independent settings window targets
480 × 580. Both clamp to the viewport minus 24 pixels and stay within it when
dragged. Initial main-window placement reserves up to 496 pixels to the right
for settings. The sidebar is 150 pixels wide, reducing to 112 pixels below
620 pixels root width. It holds Search, seven categories and connection status.
Category changes clear search and close details.

All 77 sections are reached through a scrolling `module_grid` child.
Its equal-width table uses one to three columns: floor of available width divided
by 174 pixels, clamped to that range, with 5 × 5 pixel cell padding. Search matches
category module names without case sensitivity; zero results show **Clear search**.
Widgets are 66 pixels high. Their title starts at a 12-pixel inset; the second
line starts 39 pixels from the top. Toggle indicators sit at the bottom right.

Settings have their own draggable header, X close control and scrolling content
inset 24 pixels horizontally and 52 pixels from the top. Selecting another module
resets settings scroll; reopening the same module preserves it. On desktop both
windows can remain visible side by side. Below 1260 pixels viewport width, open
settings replace the visible grid; X returns to the grid. Center resets both
placements, Reset operates on the current category, and Close closes the menu
and details.

The launcher opens at 640 × 640 with 12-pixel exterior insets and a 58-pixel native
title strip. Its Start/Logs sidebar is 142 pixels wide, reducing to 112 pixels
below 620 pixels viewport width. Simple authored line icons and selected states
match the client navigation. At 480 × 380 the same controls remain reachable.

The native instance list scrolls within Start and exposes full titles and details
in a tooltip. Status, JDK recovery and diagnostics use the scrolling content
child. A 34-pixel lime-green primary action remains outside it, followed by the fixed
HWID and close footer. Start and Logs both show the read-only HWID field and
**Copy HWID** before the Keim/Close row, including before sign-in. The field
scrolls horizontally at compact widths and exposes the full ID in a tooltip;
copying always uses the full value. Unavailable IDs disable copying. Diagnostic actions wrap to
another row when required. Native title drag, minimize and close remain available.

The HUD editor toolbar is at most 540 pixels wide, clamped to viewport width
minus 24 pixels, with 14 × 10 pixels of padding. It is 90 pixels high, or 110
below 640 pixels viewport width or 480 pixels height; that compact editor shows
only the selected element while keeping all 19 available through its selector.

## Elevation & Depth

Client and injector roots are ordinary rounded ImGui fills composited directly over transparent
render output. A darker sidebar overlay and quiet separators establish hierarchy.
There are no shader shadows on these controls. Neither surface requires desktop
capture, blur or refraction. The client calls `LiquidGlass::BeginFrame` with
visibility false. Settings → Overlay
reports **Translucent dark / 88% opacity** and **Not required**; the configured
screen-capture preference applies while the menu is open.

The injector's `FlatApp` clears RGBA to zero, uses DWM frame-extension margins
of −1 and applies a rounded native region matching the panel silhouette. It
presents the first frame before `ShowWindow`. LiquidGlass ownership, initialization,
frame/render calls and shutdown are removed from this shell. Retained glass
renderer values, capture behavior and license are legacy/standalone context in
`docs/liquid-ui.md`, rather than requirements of the active compact surfaces.

HUD panels use one rounded anthracite fill with no border, stripe or shadow.
Quiet headings and separators supply hierarchy; the HUD requires no desktop
capture, blur or new renderer.

**The Layer Order Rule.** World effects and HUD draw before client controls. UI
presentation adds no render-thread JNI and does not replace world-depth handling.

## Shapes

Both surfaces use the shared rounded scale: modest root rounding, smaller
controls, 8-pixel module widgets, zero window, child and frame borders, and a one-pixel popup
border. Scrollbars are six pixels wide. The launcher's native window region uses
a 24-pixel corner diameter to match the 12-pixel panel radius and clips the outer
transparent margin. Viewport resize updates that region.
HUD panels reuse the gently rounded child radius (8 pixels); only editor
selection adds a lime-green outline. A disabled badge appears on the selected element.

## Components

### Buttons and navigation

Small native buttons use neutral fills, lime-green activity and a light green keyboard cursor.
Selected categories use a quiet white fill, pale green text and lime-green line icons.
Module widgets use shared neutral control fills at rest and hover. Enabled
widgets use a quiet lime-green fill, the retained selected-tab tint on hover and a
stronger lime-green fill while pressed. The native button supplies keyboard focus.

### Inputs and sliders

Settings use label/value table columns. Checkboxes and color swatches align at
the right; clicking a checkbox label also toggles its real value. Existing
keybind fields remain beside module activation controls. Combos and color editors
retain native popups and IDs.

Sliders retain ImGui mouse and keyboard interaction with a compact lime-green fill
and centered value lozenge. Ctrl-click and keyboard direct entry show a native
numeric field with visible text and caret. Existing bounds and clamping apply.
Below 420 pixels content width, Keybinds places each module name above its
binding/mode row. Binding fields fit the available width and retain widget IDs
when the layout changes.

### Containers and module state

Left-click or Space on a widget toggles the real module or runs its action.
Settings-only widgets open their controls. Right-click, Menu or Shift+F10 opens
the independent settings window without toggling. Widget status reads **Enabled**,
**Disabled**, **Open settings** or **Run action**, and its tooltip includes the
current binding when present. The details header retains the real **Enabled**
checkbox or **Run action**. Settings-only sections expose their controls without
invented enabled state. Existing controls, stores, key capture and native IDs
remain the source of truth.
`RenderMenu()` is production code; `Render()` is the compatible entry point.
`ApplyCustomStyle()` is applied locally through `CompactStyle`.

Block ESP and Chunkfinder retain the existing native slider rows with a 4–512
block scan radius and 8–1024 m draw distance. Increasing scan radius raises the
draw distance as needed to cover the cube diagonal. The user can then reduce it
independently. Their existing default radii stay unchanged. Plain help text
explains nearby-first scanning, loaded-block availability and longer completion
times; Block ESP reuses the existing scan status. No new theme or layout is added.

Hole ESP is the last Blocks module, with the same toggle/settings interaction.
Low 3D frames and a translucent floor distinguish bedrock (lime), obsidian
(purple) and mixed (amber) holes. Radius, distance, height, opacity, type colours
and optional labels use the incumbent native rows and compact scrolling pane.

Storage ESP uses the same native radius row up to 512 blocks. Its status reflects
the independent loaded-container scan, with a clearly named compatibility fallback.
Scan interval controls the post-sweep pause; Pulse cycle controls the existing
visual pulse. Preserve type colours, 3D frames and known matches between sweeps.

### Launcher

The launcher retains instance selection, refresh, busy/disabled state, missing-JDK
recovery, log actions and close/minimize controls. Its main action starts the
client or returns to running Minecraft. Its compact controls and Start/Logs
navigation use the shared theme. The primary action uses near-black text when
enabled and soft-white text when disabled; busy or absent selection disables it.
Long instance labels remain clipped within their row, with full title/details on
hover. Existing async connection, selected-instance identity and duplicate-start
prevention remain unchanged.

### Snapshot HUD and editor

All 20 snapshot HUD elements, Anti Vanish, target information and the optional
Combat debug HUD share `common/hud_style.hpp`: translucent charcoal surfaces,
4px corners, a 2px white right rail, white text with a 1px shadow and muted grey
secondary values. Snapshot text uses the embedded 17px font. In-game surfaces
do not inherit the menu's lime accent. Health, friends and durability retain
semantic feedback. Long name, item, effect and keybind text is clipped within
its row, including after element scaling. Effects and keybinds use stepped
content-width rows; notifications use separate compact strips. Keystrokes use
individual dark keycaps with white pressed states. The region map retains its
six geographic categories in a desaturated palette with white shadowed labels.
The Reach readout has an anthracite surface below the open crosshair brackets;
its name clips before the separate health/distance column.

All HUD elements retain visibility, position, scale, drag previews and reset.
The F6 editor uses local `CompactStyle`; positions, scales and visibility retain
their existing persistence. Small-view selection does not hide enabled elements
from the normal HUD.

The HUD editor shows a 16px grid with stronger 64px lines behind the widgets.
Dragging snaps to the grid and, within 6px, to matching edges or centres of
enabled peers. Alignment guides appear during a snap. Grid visibility and
snapping have separate editor toggles; holding Alt temporarily enables free
movement. Pointer offsets are measured from drag start so sub-grid mouse motion
accumulates. These editor aids never appear in the normal HUD.

### In-game module list and watermark

The active module HUD follows the supplied reference: compact names sorted by
rendered width, right-aligned stepped charcoal backgrounds, white shadowed text
and a continuous 2px white rail. Relevant configured values use smaller muted
text. Rows use 18px type and a 25px line height; there is no enclosing panel or
section heading. The watermark is a plain 20px `keim / player` label using the
snapshot username (or just `keim` before a username is available). New layouts
place both at the top right; existing placements and editor scaling are retained.
Content changes preserve the list's top and nearest horizontal edge. An empty
list is hidden in game and has a selectable placeholder in the editor.

### Preserved world surfaces and bindings

Keyboard/mouse bindings retain modifier chords, Toggle/Hold, conflict detection
and focus restrictions. Module binds begin unbound; INSERT, F6 and END are defaults.
Combat, entity ESP, Aura, Marker, trails, particles, hats and depth controls retain
their real behavior. See `docs/client-ui.md`, `docs/aura.md`,
`docs/target-cube.md` and `docs/world-visuals.md`.

## Do's and Don'ts

### Do:

- **Do** use scoped `CompactStyle` for client, injector and HUD editor controls.
- **Do** preserve real module names, section identities, keybinds and native input behavior.
- **Do** keep category widgets reachable, provide search recovery and preserve independent settings drag/close.
- **Do** keep the launcher primary action and close footer outside scrolling content.
- **Do** report direct client transparency and preserve its configured capture preference.

### Don't:

- **Don't** reintroduce the superseded client tab strip, inline settings or Quick toggles.
- **Don't** describe compact client, injector or HUD transparency as live desktop capture or Liquid Glass blur.
- **Don't** restyle world ESP/effects or add render-thread JNI during compact UI work.
- **Don't** require loose shader/font/DLL files beside the portable EXE.
- **Don't** claim synthetic WARP images validate live Minecraft appearance or attach behavior.
