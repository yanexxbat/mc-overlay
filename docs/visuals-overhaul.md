# In-game visuals overhaul

The C++20 renderer consumes the existing coherent `WorldSnapshot` and
`CameraProjection`. The agent, IPC layout, menu design and HUD drawing are
unchanged. New style controls use the existing Visuals settings rows and widgets.
`Overlay::PushScanRequest` also requests the existing cached entity
details when armor bars are active; no game reads occur inside the renderer.

## Files

| File | Responsibility |
|---|---|
| `src/overlay/esp_renderer.hpp` | Existing module switches plus render style configuration |
| `src/overlay/esp_renderer.cpp` | Entity filtering, render ordering, world labels and meters |
| `src/overlay/visual_draw.hpp/.cpp` | AA corner brackets, RGBA gradients, outlines, bounded glow and outlined text |
| `src/overlay/visual_geometry.hpp` | Cuboids, camera/viewport clipping and approximate humanoid highlight geometry |
| `src/overlay/visual_preferences.hpp` | Validated style preference save/load without changing module activation |
| `tests/esp_rendering.cpp` | Actual Direct3D 11 WARP pixel checks, state checks, load measurement and preview |

## Configuration in the existing menu

The existing menu switches still enable boxes, fills, tracers, health bars,
names and distance labels. Advanced choices remain fields on `VisualSettings`
and are exposed in the existing menu sections:

- **Box ESP:** box style, corner length, outline thickness/rounding, filled boxes,
  fill gradient and top/bottom opacity, target highlight mode, highlight opacity,
  target glow, glow radius/opacity and target color.
- **Health bars:** vertical/horizontal placement, thickness, health gradient and
  its colors, armor bar and armor color. Armor shares the health-bars switch.
- **Tracers:** bottom/center/top/custom origin, normalized custom X/Y, thickness,
  origin/target colors and origin opacity.
- **Name tags / Distance labels:** shared text outline and label size.

Inactive dependent controls are disabled. The existing content scroll reaches
all controls even at a 480x360 viewport. Style and filter choices are saved in
`client-settings.ini`; module activation retains its previous startup behavior.
Existing files without the new `visual-` rows still load normally. Reset restores
the visual defaults. No navigation, theme, shared widget or HUD layout is changed.

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
The new render check runs GPU pixel checks with Direct3D 11 WARP and writes a synthetic
preview to `build/visual-preview/esp.bmp`. It checks RGBA gradients, health and
armor fractions, all box modes, target-only highlights, transparent untouched
pixels, near/viewport clipping, 16-bit vertex offsets, bounded strings and
draw-list state. The existing menu/HUD, aura, depth and world-visual checks also
passed. The menu check clicks the new mode/origin/placement controls, changes the
custom origin and armor checkbox, verifies scrolling at desktop/small viewports,
checks style save/load, rejects invalid modes and clamps out-of-range values.
Live rendering in Minecraft has not been exercised in this task.
