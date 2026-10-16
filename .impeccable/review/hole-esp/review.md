# Hole ESP verification

Hole ESP is available under Blocks, after Base ESP, using the next appended,
unbound action ID. It recognises 1x1 bedrock, obsidian and mixed holes with an
intact floor, four side walls and three air blocks of clearance. Low 3D frames
and translucent floors use lime/purple/amber, with configurable colours, type
filters, labels, height, opacity, distance and radius. Preferences, named configs,
legacy disabled defaults, key bindings, category Reset and Panic are integrated.

An independent 1 ms slice uses the existing JNI block-state bindings and material
cache. Radius is 4–16, default 8, with a 250 ms post-completion pause and 128-result
cap. The long Block ESP scanner cannot delay it. World/radius/disable changes
clear its state; invalid reads fail closed and old reports are suppressed after
two seconds. IPC v29 carries the bounded request/results. No texture/model,
render hook or network work is added. Replacing an attached v28 agent requires
restarting Minecraft and the client.

| Surface | Normal | Compact |
|---|---|---|
| Settings | [1280x800](hole-settings-desktop.png) | [480x360](hole-settings-small.png) |
| Scrolled colours/actions | — | [480x360](hole-colours-small.png) |
| World markers | [1280x800](hole-world-desktop.png) | [480x360](hole-world-small.png) |

One batched inspection of native synthetic ImGui/D3D11 fixtures confirmed
readable values and distinct, projected markers, with reachable compact colour
controls. The compact scrolled settings table retains the existing shared-pane
title overlap visible in earlier module captures; this feature does not change
the shared shell. No visual polish loop was used.

All 32 selected checks passed in 96.15 seconds, including hole_esp, control_panel,
runtime_profiles, portable_client, agent_package_match and rendering/IPC-dependent
regressions. The unrelated Java self-attach check was excluded. Hole tests cover
exact materials, all seven neighbour/headroom failures, bedrock/obsidian/mixed,
open/double-hole rejection, config bounds/round-trip/legacy/partial corruption,
3D geometry, disabled/type/distance filters, native module activation, preferences
and Panic. Existing JNI profiles pass with the revised cache and v29 IPC. There
was no live Minecraft attach or live-game performance measurement.

Keim.exe and Keim-client.zip were rebuilt; exact portable payloads, resources,
extraction, corruption repair and concurrent preparation checks passed.
