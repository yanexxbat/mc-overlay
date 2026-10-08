---
name: mcov Compact Client
description: Compact translucent client controls with a separate Liquid Glass launcher.
colors:
  primary: "#EF8B70"
  primary-active: "#FFA388"
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
  border: "rgba(255,255,255,0.08)"
  separator: "rgba(255,255,255,0.065)"
  launcher-primary: "#818CF8"
  launcher-primary-ink: "#B2B9FF"
  launcher-text: "#F3F4FF"
  launcher-text-muted: "#D7DCEC"
  launcher-surface-window: "rgba(21,26,42,0.96)"
  launcher-surface-child: "rgba(16,21,34,0.24)"
  launcher-surface-popup: "rgba(34,43,66,0.99)"
  launcher-surface-button: "rgba(192,207,255,0.10)"
  launcher-surface-button-hover: "rgba(192,207,255,0.21)"
  launcher-border: "rgba(185,203,255,0.16)"
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
  launcher-title:
    fontFamily: "Inter"
    fontSize: "22.1px"
    fontWeight: 600
rounded:
  window: "12px"
  child: "8px"
  popup: "6px"
  control: "4px"
  tab: "5px"
  grab: "3px"
  launcher-window: "22px"
  launcher-child: "14px"
  launcher-popup: "12px"
  launcher-control: "8px"
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
  launcher-window: "16px"
  launcher-frame-x: "10px"
  launcher-frame-y: "6px"
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
  launcher-button:
    backgroundColor: "{colors.launcher-surface-button}"
    textColor: "{colors.launcher-text}"
    typography: "{typography.body}"
    rounded: "{rounded.launcher-control}"
    padding: "6px 10px"
  launcher-button-hover:
    backgroundColor: "{colors.launcher-surface-button-hover}"
    textColor: "{colors.launcher-text}"
    rounded: "{rounded.launcher-control}"
---

# Design System: mcov Compact Client

## Overview

**Creative North Star: "Compact Translucent Controls"**

The client follows the user's `design/reference-translucent-menu.png`: a wide,
compact anthracite menu with a narrow sidebar, module tabs and a muted coral
accent. This supersedes the spacious cyan example and the client's Liquid Glass
tile grid. The reference supplies visual direction; mcov retains its real module
names, settings and actions.

The client uses direct alpha compositing over the game viewport. Small native
controls and modest headings keep settings dense and readable. The launcher
remains a separate Liquid Glass surface with its existing lavender palette,
backdrop capture and larger heading. HUD and world effects retain their styles.

**Key Characteristics:**

- Compact anthracite client root with restrained coral interaction states.
- Seven categories, searchable module tabs and settings within one window.
- Native keyboard input, visible slider values and real module state.
- Local style/font ownership and direct transparency without client capture.
- A separate launcher whose main action stays outside scrolling content.

## Colors

Client primitives come from `src/common/compact_style.hpp`; launcher-prefixed
tokens come from `src/common/liquid_style.hpp`. The frontmatter is normative.

### Primary

- **Muted coral** (`primary`): checkmarks, slider fill, active category icons and
  selected tab tint. **Warm coral** (`primary-active`) marks slider activity,
  keyboard navigation and text links.
- **Launcher lavender** (`launcher-primary`, `launcher-primary-ink`): the
  launcher's existing selected and active controls only.

### Neutral

- **Soft white** and **muted gray** (`text`, `text-muted`): client labels, values
  and supporting text. Navigation has its own quiet and selected text tones.
- **Anthracite** (`surface-window`): the client root. The sidebar adds its darker
  overlay; children remain transparent. Popups use a nearly opaque dark fill.
- **Translucent white fills**: control, navigation and tab tokens distinguish
  rest and hover. Coral tint marks active controls and selected module tabs.
- **Dark value lozenge** (`surface-value`): keeps numeric slider text legible
  across the filled and unfilled portions.
- **Launcher cool neutrals**: the prefixed text, surface and border tokens remain
  confined to the separate launcher.

**The Material Scope Rule.** Client transparency uses the compact palette;
launcher glass uses its own tokens and renderer. Neither changes HUD/world style.

## Typography

Both surfaces use embedded Inter. The client uses medium body text and semibold
labels at the same compact size; its mcov/category header is not a display title.
The launcher retains the existing larger semibold heading. Fonts remain part of
the executable and existing HUD/ESP atlas entries remain intact.

### Hierarchy

- **Title:** the client mcov and category header.
- **Body:** controls, module tabs, category names and supporting text.
- **Label:** section headings.
- **Launcher title:** the separate launcher's mcov heading, derived from its
  default font size multiplied by 1.3.

**The Local Type Rule.** `CompactStyle` and `LiquidStyle` push their menu font and
restore the prior style and font on scope exit.

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

The launcher opens at 640 × 640. Start and Diagnose keep their existing content;
the 40-pixel primary start action remains outside the scrolling child. Diagnostics
stays separate from the primary operation.

## Elevation & Depth

The client root is an ordinary rounded ImGui fill composited directly over the
transparent overlay. A darker sidebar overlay and quiet separators establish
hierarchy. It adds no desktop capture, blur, refraction or shader shadow. The
client calls `LiquidGlass::BeginFrame` with visibility false. Settings → Overlay
reports **Translucent dark / 88% opacity** and **Not required**; the configured
screen-capture preference applies while the menu is open.

The launcher retains `LiquidGlass`: Dual Kawase blur, neutral dark tint,
restrained refraction, light rims and a soft shader shadow. Its material values
remain in `.impeccable/design.json` and `src/common/liquid_glass.cpp`. Its capture
sessions, exclusion and fallback are described in `docs/liquid-ui.md`. The
launcher has an opaque native base and rounded window region below the material.

**The Layer Order Rule.** World effects and HUD draw before client controls. UI
presentation adds no render-thread JNI and does not replace world-depth handling.

## Shapes

The client uses modest root rounding, smaller controls and tabs, zero window,
child and frame borders, and a one-pixel popup border. Its scrollbar is six pixels
wide. The rounded scale in the frontmatter is local to the client unless prefixed
with `launcher-`. The launcher retains its larger root and inset corner shapes.

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
client or returns to running Minecraft. Its material and capture remain separate
from the client's compact theme.

### Preserved HUD and world surfaces

Nineteen HUD elements retain visibility, position, scale, drag previews and reset.
Keyboard/mouse bindings retain modifier chords, Toggle/Hold, conflict detection
and focus restrictions. Module binds begin unbound; INSERT, F6 and END are defaults.
Combat, entity ESP, Aura, Marker, trails, particles, hats and depth controls retain
their real behavior. See `docs/client-ui.md`, `docs/aura.md`,
`docs/target-cube.md` and `docs/world-visuals.md`.

## Do's and Don'ts

### Do:

- **Do** use scoped `CompactStyle` for client controls and `LiquidStyle` for the launcher.
- **Do** preserve real module names, section identities, keybinds and native input behavior.
- **Do** keep module tabs outside content scroll and provide search recovery.
- **Do** keep the launcher start action outside scrolling content.
- **Do** report direct client transparency and preserve its configured capture preference.

### Don't:

- **Don't** reintroduce the rejected spacious cyan client or large module tile grid.
- **Don't** describe client transparency as live desktop capture or Liquid Glass blur.
- **Don't** globally restyle HUD/ESP or add render-thread JNI during menu work.
- **Don't** require loose shader/font/DLL files beside the portable EXE.
- **Don't** claim synthetic WARP images validate live Minecraft appearance or attach behavior.
