# Keim green accent verification

The user's follow-up selects green to match the mascot. The shared accent is
#8EDB1A, hover #A6EF42, pressed #72B411, primary ink #17200B and selected text
#D4F3B8. Launcher, module widgets, navigation, controls, snapshot HUD, reach marker,
Combat debug HUD and fallback style use the shared tokens. Red errors, amber
warnings and the existing world-effect/type palettes retain their semantic roles.

| Surface | Normal | Compact |
|---|---|---|
| Primary action | [Launcher](keyauth-entry-desktop.png) | [Launcher](keyauth-entry-small.png) |
| Modules | [Menu](module-grid-combat.png) | [Menu](module-grid-small.png) |
| Settings | [Sliders and checks](storage-3d-settings-desktop.png) | Covered by the existing native control-panel fixture |
| Error feedback | — | [Red error text](keyauth-rejected-small.png) |
| HUD | [Snapshot HUD](modular-hud.png) | Existing layout bounds checks pass |

These are actual native ImGui/D3D11 fixtures with synthetic data, not live
Minecraft screenshots. The batched inspection found the green treatment aligned
with the unchanged mascot and compact layout. No screenshot polish loop was used.

Computed contrast for the changed roles: primary ink against normal/hover/pressed
actions is 9.85:1 / 12.06:1 / 6.61:1. Primary green against the 88%-opaque root
over an all-white backdrop is 6.36:1. Pale selected text on the strongest pressed
widget tint is 5.70:1; error text on the root over white is 5.82:1. Selected text
over its highlight is 5.17:1. Before capture, the computed pressed-widget accent
text contrast (4.05:1) was corrected by using pale selected text; pressed inactive
labels use the normal text colour.

The client and ZIP were rebuilt. All 14 relevant UI/rendering/package checks pass
in 6.69 seconds: launcher_window, flat_ui, client_controls, region_map,
world_visuals, smp_features, rockstar_features, control_panel, aura_rendering,
depth_rendering, portable_client, esp_rendering, freecam and combat_hud.
The palette adds no runtime assets or game work. No live Minecraft, real license
login or repeated JVM compatibility run was required for this colour-only change.
