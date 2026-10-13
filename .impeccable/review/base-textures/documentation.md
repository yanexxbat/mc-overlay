# Base textures: documentation and preserved-system evidence

This is an ordinary extension of the native Windows x64 / C++20 / Dear ImGui
Operate surface. The implemented texture controls extend Base ESP in **Blocks**;
they introduce no new visual system. The final [review](review.md) disposition is
**ship for the UI scope only**. There is no live Minecraft capture or validation.

## Ground truth checked

The documenter read `PRODUCT.md`, `DESIGN.md`,
`.impeccable/surfaces/module-widgets.md`, the Impeccable `reference/document.md`
and documenter role guidance. Source sampling covered
`src/common/compact_theme.hpp`, `src/common/compact_style.hpp`,
`src/overlay/control_panel.cpp`, `src/overlay/overlay_smp.cpp`,
`src/overlay/smp_features.hpp`, `tools/frame-sync/BaseTextureBridge.java` and
`tests/base_texture_gl.cpp`. The existing finish review supplies the validated
visual evidence. This documentation pass checked capture dimensions and source
contracts; it did not edit source, recapture the UI or rerun runtime tests.

## Extension compared with the incumbent system

| Incumbent rule | Implemented extension and evidence | Result |
|---|---|---|
| Compact anthracite/coral palette | Shared `compact_theme.hpp` and scoped `CompactStyle` supply the existing grounds, control ink, coral activity and muted status copy. Base settings add no palette or material tokens. | Preserved |
| Embedded Inter with quiet 14-pixel headings | `RenderMenu` and the independent settings header keep `MenuFont`; native settings rows and wrapped status text reuse the established type hierarchy. | Preserved |
| Direct rounded ImGui alpha compositing | Root/child/control rounding remains 12/8/4 pixels in `compact_style.hpp`; Base controls use the existing pane and row components. | Preserved |
| Independent settings and compact scrolling | `control_panel.cpp` retains the fixed draggable header, enabled/close controls, 480 × 580 target and viewport clamping. At 480 × 360 the content scrolls below that header. | Preserved |
| Truthful controls and recovery | `overlay_smp.cpp` exposes Minecraft textures / Area marker, mode-specific opacity, shared detection controls, Through walls visibility, wrapped status/error copy and off/on retry or Area marker recovery. | Preserved |
| UI rendering outside the JVM | External controls remain D3D11/ImGui. The dedicated Minecraft render callback belongs to the texture feature, not a new UI presentation requirement. | Preserved |

The Material Scope, Local Type and Layer Order rules remain applicable. The
texture implementation changes the world rendering feature while its settings
retain the established control system.

## Final visual evidence

All five required `-verified.png` captures were validated by the finish reviewer;
their dimensions were checked in this pass. They use native ImGui/D3D11 WARP
with synthetic fixtures and establish settings composition and status legibility.

| Capture | Size | Coverage |
|---|---|---|
| [Desktop](base-esp-settings-desktop-verified.png) | 1280 × 800 | Blocks integration and separate Base settings |
| [Small top](base-esp-settings-small-top-verified.png) | 480 × 360 | Header, activation and texture controls |
| [Small scrolled](base-esp-settings-small-verified.png) | 480 × 360 | Remaining controls and detector-bound explanation |
| [Loading](base-textures-loading-small-verified.png) | 480 × 360 | 1.21.11 build progress |
| [Error](base-textures-error-small-verified.png) | 480 × 360 | Wrapped error and actionable recovery |

## Product and verification record

- Minecraft textures are the default (`BaseSettings.textured = true`) with
  opacity 1; the optional Area marker retains coloured fill/frame controls.
- Supported texture bindings target 1.21.11 Mojmap, vanilla and Fabric
  intermediary. The primary implementation pass audited 38 classes and 72
  members against cached official classes.
- Detection remains storage-cluster extent plus adjustable padding, independent
  of Stash Finder activation. It does not trace whole-building bounds.
- The game-render bridge captures block, liquid and sprite-based block-entity
  quads, tint, light and atlas UVs. Native GL uses independent base depth and
  composites into the main colour target with preserved state and live
  atlas/lightmap sampling.
- Extraction targets 2 ms per slice, with at least one checked cell; one expensive
  block may exceed the budget. Bounds are eight regions, 16,384 checked cells and
  786,432 vertices, with overlapping areas deduplicated, rebuild after about one
  second, a 750 ms request lease and server/dimension scope.
- Special text, portal and custom-renderer submissions are skipped and reported.
  Status covers loading, empty results, limits, errors and version fallback.
  Disabling releases GL resources so off/on can recreate a failed renderer.
- The primary implementation pass reported all 26 CTest checks passing across
  the final runs: 25 in the sandbox and `frame_attach` separately outside it
  after Windows fixture-pipe access failed. After final resource-release
  hardening, `base_texture_gl`, `base_texture_models`, `smp_features` and
  `portable_client` passed; `depth_hook` passed in the preceding focused run.
  WGL pixel/state assertions include opacity 0.5, translucent texel blending
  and reinitialization after resource release. Native control checks cover the
  synthetic settings/status captures. The EXE and ZIP were rebuilt.
- IPC v22 requires a full Minecraft restart after upgrading an already loaded
  agent. No live Minecraft texture, appearance or attach validation was
  performed. Shader and mod graphics compatibility remains unverified.

The earlier `.impeccable/review/base-esp/` captures and D3D11 filled-area checks
are historical Area marker evidence; the documentation now distinguishes them
from the texture bridge, native GL and final settings evidence.

## Written and preserved artifacts

Updated `PRODUCT.md`, `README.md`, `docs/smp-features.md`, `docs/client-ui.md` and
`docs/compatibility.md`, and wrote this evidence record. The existing `review.md`
ship disposition remains unchanged.

`DESIGN.md` and `.impeccable/design.json` remain byte-identical to the supplied
baseline, with SHA256 hashes:

- `DESIGN.md`: `996B8396F5EFF555931F5915FB666F5A65AD2EAF9F43F58E9622C6A10CD672FB`
- `.impeccable/design.json`: `73A2E6D1425E367A7F906413C644910664F13907B49FDEA15B234087B107A3E4`

Existing section-count drift is not canonized or repaired: `DESIGN.md` and the
module-widgets surface brief retain the historical 77-section count while
`PRODUCT.md` records 81. The extension preserves the visual system, and an
unrequested refresh of design/surface metadata is outside this write boundary.
