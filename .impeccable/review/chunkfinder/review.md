# Chunkfinder integration verification

2026-10-15. Bounded native UI review; implementation and review performed in this
chat. Existing compact anthracite/coral design and fixed settings header retained.

The standalone Base Finder widget is removed; Blocks > Chunkfinder contains all
three clue filters, independent minima, shared scan controls and appearance.
Both chunk-column and individual-block boxes default on. Legacy growth profiles
and bindings remain usable; the active-module HUD and notifications avoid double
counting the compatibility action.

Inspected real ImGui/D3D11 WARP captures with synthetic snapshots at 1280x800 and
480x360: settings, growth filters, discovery footer, whole chunk columns, per-block
kelp stems and two-line labels. All controls/results remain reachable by scrolling;
the fixed header does not overlap content. No new assets or global design changes.
Two bounded render inspection rounds completed; no material UI issue remains.

Seven focused checks passed after the final functional change: client_controls,
smp_features, control_panel, depth_rendering, esp_geometry, agent_package_match,
portable_client. Tests cover negative chunk coordinates, separate thresholds,
completed scans, filter changes, old/new profile formats, invalid partial fields,
preferences, legacy binding activation, master off, separate block/chunk toggles,
finite geometry and through-wall depth tags. Production EXE and ZIP rebuilt.

No live Minecraft attach, real-world scan or in-game appearance test was performed.
Vanilla build-height columns are used; custom dimension heights are not sampled.
