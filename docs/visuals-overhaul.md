# In-game visuals overhaul

The C++20 renderer consumes the existing coherent `WorldSnapshot` and
`CameraProjection`. The agent, IPC layout, menu drawing and HUD drawing are
unchanged. `Overlay::PushScanRequest` also requests the existing cached entity
details when armor bars are active; no game reads occur inside the renderer.

## Files

| File | Responsibility |
|---|---|
| `src/overlay/esp_renderer.hpp` | Existing module switches plus render style configuration |
| `src/overlay/esp_renderer.cpp` | Entity filtering, render ordering, world labels and meters |
| `src/overlay/visual_draw.hpp/.cpp` | AA corner brackets, RGBA gradients, outlines, bounded glow and outlined text |
| `src/overlay/visual_geometry.hpp` | Cuboids, camera/viewport clipping and approximate humanoid highlight geometry |
| `tests/esp_rendering.cpp` | Actual Direct3D 11 WARP pixel checks, state checks, load measurement and preview |

## Configuration without menu changes

The existing menu switches still enable boxes, fills, tracers, health bars,
names and distance labels. Advanced choices are fields on `VisualSettings`;
this change does not add menu controls or persistence for those fields.

| Setting | Options / meaning |
|---|---|
| `boxStyle` | `Corners2D` (default), `Full2D`, `Wire3D` |
| `cornerFraction` | Bracket length relative to the shorter box side |
| `fillAlpha`, `fillBottomAlpha`, `fillGradient` | Multipliers for the entity color's alpha; solid fill or top-to-bottom fade |
| `boxRounding`, `boxThickness` | Existing menu controls; rounding affects 2D boxes |
| `barPlacement` | `Vertical` (HP left, armor right), `Horizontal` (stacked below the box) |
| `healthGradient` | Red/yellow/healthy ramp; disable to use the existing health accent uniformly |
| `healthLow`, `healthMiddle`, `healthColor` | Ramp endpoints; the existing Health accent control changes the healthy endpoint |
| `armorBars`, `armorColor`, `barThickness` | Armor points from existing cached details, clamped to 0–20; shares the health-bars switch |
| `tracerOrigin` | `Bottom`, `Center`, `Top`, `Custom` |
| `customTracerOrigin` | Normalized viewport coordinates, clamped to `[0,1]` |
| `tracerColor`, `tracerEndColor`, `tracerStartAlpha` | First/last color and origin fade; the existing tracer alpha controls the whole line |
| `textOutline` | Outlines on entity labels; disable to use the existing shadow rendering |
| `targetHighlight` | `Off`, `Bounds`, `Humanoid`; shares box ESP, only the snapshot's selected target |
| `targetGlow`, `glowRadius`, `glowOpacity`, `targetColor` | Three bounded AA geometry layers on the selected target's box |
| `highlightAlpha` | Opacity of the selected target's projected volume |

`Humanoid` reconstructs ten vanilla-sized body parts from the sampled rig.
It follows the existing head/body pose and limb animation. Entities without a
usable rig fall back to their bounds. This is an external approximate highlight,
not a Minecraft mesh/material replacement or shader silhouette. The existing
aura, target cube and world-depth pipeline remain unchanged; the ESP geometry
stays in its existing screen-overlay layer and does not claim scene occlusion.

## References

This is an independent C++ implementation inspired by these Java renderers:

- [Rockstar ESP](https://github.com/ConeTin/rockstar-free/blob/main/ru/rockstar/client/features/impl/visuals/ESP.java): world bounds, clear outlines, health-dependent coloring and equipment labels.
- [Rockstar Tracers](https://github.com/ConeTin/rockstar-free/blob/main/ru/rockstar/client/features/impl/visuals/Tracers.java): world-anchored, anti-aliased target lines.
- [Rockstar TargetESP](https://github.com/ConeTin/rockstar-free/blob/main/ru/rockstar/client/features/impl/visuals/TargetESP.java): target-specific color and transparency emphasis.
- [ThunderHack ESP](https://github.com/Pan4ur/ThunderHack-Recode/blob/main/src/main/java/thunder/hack/features/modules/render/ESP.java): projected AABB bounds and vertex color gradients.
- [ThunderHack Chams](https://github.com/Pan4ur/ThunderHack-Recode/blob/main/src/main/java/thunder/hack/features/modules/render/Chams.java): colored translucent model emphasis, adapted here to external rig geometry.

## Performance and verification

Gradients recolor one AA primitive in place instead of drawing line segments.
Geometry uses fixed stack arrays; the skeleton and target highlight share one
rig reconstruction. Normal name tags keep the viewport scissor and only use a
separate scissor for overflowing fields, reducing command fragmentation.
All ordinary primitives use the font atlas and the existing blend state.
Equipment images retain the existing atlas batching. Glow is limited to the
selected target. Text outlines use four offsets; `textOutline=false` provides
the lighter two-pass shadow path.

The Release load check used 256 visible entities with names, fill, skeleton,
health/armor bars and tracers, plus one selected target. On this host it measured
about 2.1 ms CPU/frame, 361,736 vertices and six draw commands, with zero new
ImGui allocations after warm-up. This is a synthetic CPU measurement, not an
in-game FPS result or a hardware GPU timing guarantee.

Build the existing CMake project with `MCOV_BUILD_ESP_CHECKS=ON`, then run
`ctest --test-dir build --output-on-failure -R "esp_geometry|esp_rendering"`.
The new check runs GPU pixel checks with Direct3D 11 WARP and writes a synthetic
preview to `build/visual-preview/esp.bmp`. It checks RGBA gradients, health and
armor fractions, all box modes, target-only highlights, transparent untouched
pixels, near/viewport clipping, 16-bit vertex offsets, bounded strings and
draw-list state. The existing menu/HUD, aura, depth and world-visual checks also
passed. Live rendering in Minecraft has not been exercised in this task.
