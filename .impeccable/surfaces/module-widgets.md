# Client module widgets

Mode: Operate. Native Windows x64 / C++20 / Dear ImGui.

## Authority and scope

The user's supplied Paint sketch, `design/reference-module-widgets.png`, is the
layout authority: categories in a left sidebar, module widgets arranged in the
main area, and settings in a second independent window opened with right-click.
Source: user attachment `codex-clipboard-3edaad67-1629-44e3-b87a-eaa9d64a3acc.png`.
The confirmed compact anthracite/coral theme, embedded Inter and translucent
rounded fills remain the visual authority. This replaces the module tabs and
inline settings composition. Injector, HUD, world effects and runtime behavior
remain within their existing approved scope.

## First viewport and interaction

- The category's modules are visible immediately in a scrollable widget grid;
  there is no upper module tab strip or settings panel inside the main window.
- Left-click or keyboard activation toggles real modules, runs existing actions,
  or opens a settings-only module. Right-click opens that module's settings
  without toggling it; Menu / Shift+F10 provides the keyboard equivalent.
- Settings are an independently draggable, nonmodal window with their own close
  control and scroll area. The same controls, widget IDs, key capture and real
  module state are retained. Selecting a different category closes stale details.
- Search filters widgets and supplies a working Clear search recovery. It resets
  on category changes. Settings scroll is retained for the same module and reset
  when another module is selected.
- The main window targets 740x520; settings target 480x580. Each clamps to the
  viewport minus 24px. The grid uses one to three columns. Desktop initial
  placement reserves room for settings. Below 1260px viewport width, the open
  settings pane replaces the visible grid so transparent UI text cannot overlap;
  its close control returns to the grid.

## Quality bar and verification

All 76 sections remain reachable with real right-clicks, including rows below
the visible grid. Toggle states and bindings reflect the actual stores. Separate
drag, close, search recovery, scroll behavior and keyboard operation must work.
There must be no global style/font leakage, ImGui layout errors, unreachable
controls or clipped setting values at 1280x800, 1920x1080 and 480x360.
Use native offscreen D3D11 WARP captures with clearly synthetic fixtures. They
establish ImGui composition and interactions, not live Minecraft appearance.
