# English UI documentation finish

Status: **complete; ship for the English translation scope** (2026-10-15).
The independent [finish review](review.md) found no material translation defect
or translation-induced layout regression. No further source fix, recapture or
rebuild was requested by that review.

## Scope and visual authority

This is a narrow English-copy extension of the existing native Windows Operate
interface. PRODUCT.md, DESIGN.md, `.impeccable/design.json` and the
[English UI direction contract](../../surfaces/english-ui.md) remain the context.
No palette, typography, spacing, shape, layout or interaction redesign was
approved. Embedded Inter, compact anthracite roots, muted coral controls,
direct alpha compositing, narrow navigation and fixed launcher actions remain
the incumbent visual system. No token or component style was changed for this
documentation finish.

The implementation handoff covers injector strings, `combat/debug_hud.cpp`,
`overlay/keybinds.hpp`, `overlay/overlay_input.cpp`, `common/ipc.cpp` and
`overlay/process.cpp`. App-owned labels, states, recovery, errors, diagnostics,
debug HUD and named keys use English. Minecraft window titles, player names,
typed content and layout-dependent printable keyboard legends remain original.
The English `docs/client-start.txt` uses the shipping control names.

## Design documentation alignment

Inspected the parent change to DESIGN.md and the sidecar against HEAD.
DESIGN.md differs only in historical `Start/Diagnose` references becoming
`Start/Logs`, including the corresponding footer sentence. The sidecar differs
only in `Client starten` becoming `Start client`, `Start / Diagnose` becoming
`Start / Logs`, and the matching navigation narrative. Both literal-only
comparisons passed; the sidecar also parsed as valid JSON. Normative frontmatter,
color metadata, typography, component CSS, layout and all other design data are
unchanged. `Diagnostics` remains the current page heading reached through Logs.
This pass did not rewrite either design file.

Preexisting context drift is recorded without repair: PRODUCT.md describes
82 reachable sections, while DESIGN.md and historical module-widget verification
still refer to 77. The sidecar retains its existing `generatedAt` timestamp of
2026-10-09T11:31:47Z; this copy alignment did not regenerate the visual system.
These facts do not alter the English translation verdict or establish a fresh
audit of the wider product catalog.

## Capture evidence and provenance

Opened all ten supplied PNGs during this documentation pass:

| Capture | Evidence |
| --- | --- |
| `launcher-empty.png` | English empty state, Refresh recovery, disabled Start client and fixed Close. |
| `launcher-ready.png` | English instance metadata and enabled primary action; original Minecraft titles retained. |
| `launcher-small.png` | 480x380 selection and status content with reachable fixed Start client and Close. |
| `launcher-busy.png` | English connection progress and Preparing connection... disabled action. |
| `launcher-error.png` | Missing-JDK explanation, Select JDK folder and Show details recovery. |
| `launcher-diagnose.png` | Compact Logs navigation and Diagnostics page with Open log folder and Copy log. |
| `launcher-diagnose-desktop.png` | Desktop Diagnostics layout and fixed Close. |
| `compact-keybinds.png` | Compact English instructions, Insert, End, F6, None and Toggle. |
| `compact-keybinds-bottom.png` | Reachable final rows, English Toggle/Hold explanation and Reset keybinds. |
| `classic-keybinds.png` | Desktop module/key/mode table in the existing client and independent settings layout. |

These are actual offscreen native Dear ImGui/D3D11 WARP captures with synthetic
fixtures, covering launcher 640x640, 640x520 and 480x380, plus overlay 1280x800 and
480x360. They are review evidence, not concept art or live Minecraft screenshots.
The historical `diagnose` filenames identify the current English Diagnostics
states; they do not prescribe visible navigation copy. No new shipping raster
was introduced, so there is no new shipping-image provenance obligation.

## Supplied implementation verification

The implementation handoff and finish review report successful production
builds and seven passing affected checks: `launcher_window`, `control_panel`,
`combat_hud`, `client_controls`, `portable_client`, `agent_package_match` and
`smp_features`. The source lexical audit examined 8,325 string literals and found
no German candidates after excluding comments and raw shaders. The packaged ZIP
was verified to contain `mcov.exe` and a matching English `START.txt`.
These results were supplied to this documentation pass and were not rerun here.

## Verification limits

No live Minecraft attach, game appearance, foreground/focus behavior, arbitrary
GPU/runtime combination or real desktop composition was tested here. The images
establish visible English and fit for their represented states; they do not
cover every setting, keyboard layout or failure. Named-key and debug-HUD coverage
also relies on the supplied source review and checks. Shell-owned dialogs and
external Java/system output can retain their environment language.

The Impeccable detector did not run because its launcher engine was unavailable;
this is not a detector-clean claim. Existing Base Finder implementation work and
broader visual-system drift remain outside this translation finish.
