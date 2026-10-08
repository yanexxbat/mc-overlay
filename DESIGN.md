---
name: mcov Compact Controls
description: Shared compact translucent anthracite and coral controls for the client, injector and snapshot HUD.
colors:
  primary: "#EF8B70"
  primary-active: "#FFA388"
  primary-pressed: "#DC7D64"
  primary-ink: "#221B1B"
  positive: "#A5C7AD"
  warning: "#E8BB73"
  danger: "#F28080"
  text: "#ECECEF"
  text-muted: "#AEAEB8"
  navigation-text: "#C2C2CC"
  navigation-selected-text: "#F5D0C4"
  surface-window: "rgba(34,34,41,0.88)"
  surface-sidebar: "rgba(21,21,27,0.22)"
  surface-child: "rgba(0,0,0,0)"
  surface-popup: "rgba(38,38,45,0.98)"
  surface-control: "rgba(255,255,255,0.045)"
  surface-control-hover: "rgba(255,255,255,0.085)"
  surface-control-active: "rgba(255,255,255,0.12)"
  surface-button-hover: "rgba(255,255,255,0.09)"
  surface-button-active: "rgba(239,139,112,0.18)"
  surface-navigation-selected: "rgba(255,255,255,0.065)"
  surface-tab: "rgba(255,255,255,0.035)"
  surface-tab-selected: "rgba(239,139,112,0.16)"
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
  module-tab:
    backgroundColor: "{colors.surface-tab}"
    textColor: "{colors.text}"
    typography: "{typography.body}"
    rounded: "{rounded.tab}"
  module-tab-selected:
    backgroundColor: "{colors.surface-tab-selected}"
    textColor: "{colors.text}"
    rounded: "{rounded.tab}"
  content-panel:
    backgroundColor: "{colors.surface-child}"
    rounded: "{rounded.child}"
  client-root:
    backgroundColor: "{colors.surface-window}"
    rounded: "{rounded.window}"
    width: "1040px"
    height: "520px"
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

# Design System: mcov Compact Controls

## Overview

**Creative North Star: "Compact Translucent Controls"**

The client follows the user's `design/reference-translucent-menu.png`: a wide,
compact anthracite menu with a narrow sidebar, module tabs and a muted coral
accent. The user approved this design and requested the same system on the
injector and snapshot HUD. This replaces the spacious cyan example, the client's glass tile grid
and the launcher's lavender Liquid Glass presentation. The reference supplies
visual direction; mcov retains its real module names, settings and actions.

Client controls, injector and HUD editor use direct ImGui alpha compositing,
scoped `CompactStyle`, embedded Inter and modest headings. Snapshot HUDs share
the drawing tokens and font. The client organizes module settings; the injector
organizes instance selection and diagnostics. The shared palette and controls
keep these operations consistent. World effects retain their styles.

**Key Characteristics:**

- Shared compact anthracite roots with restrained coral interaction states.
- Seven categories, searchable module tabs and settings within one window.
- Native keyboard input, visible slider values and real module state.
- Local style/font ownership and direct transparency without backdrop capture.
- Start/Diagnose navigation with a fixed launcher action and close footer.

## Colors

Shared drawing tokens come from `src/common/compact_theme.hpp`; control styling
comes from `src/common/compact_style.hpp`. The injector's solid
primary button and list treatment come from `src/injector/launcher_ui.cpp`.
The frontmatter is normative; obsolete launcher lavender primitives are removed.

### Primary

- **Muted coral** (`primary`): checkmarks, slider fill, active category icons and
  selected tab tint, plus the launcher's main action. **Warm coral**
  (`primary-active`) marks slider activity, keyboard navigation, text links and
  primary-button hover, plus HUD selection and active states. **Pressed coral** (`primary-pressed`) marks that button's
  pressed state.

### Neutral

- **Soft white** and **muted gray** (`text`, `text-muted`): shared labels, values
  and supporting text. Navigation has its own quiet and selected text tones.
- **Anthracite** (`surface-window`): client/injector roots and HUD panels. The sidebar adds its darker
  overlay; children remain transparent. Popups use a nearly opaque dark fill.
- **Translucent white fills**: control, navigation and tab tokens distinguish
  rest and hover. Coral tint marks active controls and selected module tabs.
- **Dark value lozenge** (`surface-value`): keeps numeric slider text legible
  across the filled and unfilled portions.
- **Near-black primary ink** (`primary-ink`): enabled coral action text. Disabled
  actions use soft white with the shared disabled alpha (0.48).
- **Quiet instance surface** (`surface-instance-list`): the native selectable
  instance list within the launcher's transparent content area.

HUD feedback uses **soft green** (`positive`) for favorable health/friends,
**amber** (`warning`) for durability warnings and **soft red** (`danger`) for
danger or depleted durability. Coral continues to identify activity.

**The Material Scope Rule.** Client, injector and snapshot HUD use the same compact palette
and direct alpha compositing. World effects retain their styles.

## Typography

Client, injector and snapshot HUD use embedded Inter at the same compact size. Medium body text and
semibold labels keep controls and headings aligned; the mcov/category/page header
is not a display title. Fonts remain part of the executable and existing ESP
atlas entries remain intact.

### Hierarchy

- **Title:** mcov, client category and launcher page headers.
- **Body:** controls, module tabs, category names and supporting text.
- **Label:** section headings.

**The Local Type Rule.** Both surfaces use `CompactStyle` to push their menu font
and restore the prior style and font on scope exit. The HUD editor uses the same
scope; snapshot HUD rendering pushes and restores the shared `MenuFont` locally.

## Layout

The client targets 1040 × 520 UI pixels, clamps to the viewport minus 24 pixels
and stays within it when dragged. The sidebar is 150 pixels wide, reducing to
112 pixels below 620 pixels root width. It holds Search, the seven categories
and connection status. Category changes clear search; each category retains its
selected section.

All 76 existing sections are reached through module tabs above the scrolling
content. Tabs scroll horizontally with native ImGui overflow arrows. Search
matches category module names without case sensitivity; zero results show
**Clear search**. Selecting a tab chooses settings without toggling the module.
Category or module changes reset content scroll.

There are two children: navigation and content. When content width is at least
660 pixels and the category has more than two toggle modules, a 60/40 table puts
details left and **Quick toggles** for other modules right. Otherwise content
uses one column. The current module's real **Enabled** checkbox or **Run action**
remains in the header. Center, category Reset and Close remain in the footer.
There is no separate module settings window.

The launcher opens at 640 × 640 with 12-pixel exterior insets and a 58-pixel native
title strip. Its Start/Diagnose sidebar is 142 pixels wide, reducing to 112 pixels
below 620 pixels viewport width. Simple authored line icons and selected states
match the client navigation. At 480 × 380 the same controls remain reachable.

The native instance list scrolls within Start and exposes full titles and details
in a tooltip. Status, JDK recovery and diagnostics use the scrolling content
child. A 34-pixel coral primary action remains outside it, followed by the fixed
close footer. Diagnose has the same close footer. Diagnostic actions wrap to
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
controls and tabs, zero window, child and frame borders, and a one-pixel popup
border. Scrollbars are six pixels wide. The launcher's native window region uses
a 24-pixel corner diameter to match the 12-pixel panel radius and clips the outer
transparent margin. Viewport resize updates that region.
HUD panels reuse the gently rounded child radius (8 pixels); only editor
selection adds a coral outline. A disabled badge appears on the selected element.

## Components

### Buttons and navigation

Small native buttons use neutral fills, coral activity and a warm keyboard cursor.
Selected categories use a quiet white fill, warm text and coral line icons.
Module tabs use the coral selected tint and native tab overflow behavior.

### Inputs and sliders

Settings use label/value table columns. Checkboxes and color swatches align at
the right; clicking a checkbox label also toggles its real value. Existing
keybind fields remain beside module activation controls. Combos and color editors
retain native popups and IDs.

Sliders retain ImGui mouse and keyboard interaction with a compact coral fill
and centered value lozenge. Ctrl-click and keyboard direct entry show a native
numeric field with visible text and caret. Existing bounds and clamping apply.
Below 420 pixels content width, Keybinds places each module name above its
binding/mode row. Binding fields fit the available width and retain widget IDs
when the layout changes.

### Containers and module state

Details render inside the main scrolling content. **Quick toggles** operate real
modules in the current category. Settings-only sections expose their controls
without invented enabled state; action sections retain their real action.
`RenderMenu()` is production code; `Render()` is the compatible entry point.
`ApplyCustomStyle()` is applied locally through `CompactStyle`.

### Launcher

The launcher retains instance selection, refresh, busy/disabled state, missing-JDK
recovery, log actions and close/minimize controls. Its main action starts the
client or returns to running Minecraft. Its compact controls and Start/Diagnose
navigation use the shared theme. The primary action uses near-black text when
enabled and soft-white text when disabled; busy or absent selection disables it.
Long instance labels remain clipped within their row, with full title/details on
hover. Existing async connection, selected-instance identity and duplicate-start
prevention remain unchanged.

### Snapshot HUD and editor

All 19 snapshot HUD elements and the optional Combat debug HUD share compact
tokens and the 14-pixel `MenuFont`. Active modules and pressed keys use coral;
health, friends and durability use semantic feedback. Long name, item, effect
and keybind text is clipped within its row, including after element scaling.
The Reach readout has an anthracite surface below the open crosshair brackets;
its name clips before the separate health/distance column.

Nineteen HUD elements retain visibility, position, scale, drag previews and reset.
The F6 editor uses local `CompactStyle`; positions, scales and visibility retain
their existing persistence. Small-view selection does not hide enabled elements
from the normal HUD.

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
- **Do** keep module tabs outside content scroll and provide search recovery.
- **Do** keep the launcher primary action and close footer outside scrolling content.
- **Do** report direct client transparency and preserve its configured capture preference.

### Don't:

- **Don't** reintroduce the rejected spacious cyan client or large module tile grid.
- **Don't** describe compact client, injector or HUD transparency as live desktop capture or Liquid Glass blur.
- **Don't** restyle world ESP/effects or add render-thread JNI during compact UI work.
- **Don't** require loose shader/font/DLL files beside the portable EXE.
- **Don't** claim synthetic WARP images validate live Minecraft appearance or attach behavior.
