# Large Block ESP and Chunkfinder scans

Both modules now allow 4–512 blocks of scan radius, up from 48, with 8–1024 m
draw distance. Increasing a radius raises draw distance to at least the scan
cube's diagonal; the user can lower draw distance independently. Defaults remain
12/20 blocks. The agent, profile schema, legacy finder import/clamp and automatic
Chunkfinder preferences all accept the new limits. Other modules keep their own
limits. Block ESP also exposes the existing scan progress/status.

The scanner retains its 2 ms slices and 1024-result cap. A constant-memory cursor
walks nearby columns first and starts each column near the player's height.
Invalid vertical slabs are clipped before iteration. Cursor/progress arithmetic
uses 64-bit integers; a 512-radius sweep covers at most 403,440,000 valid cells
within the existing -64..319 world-height bounds. No position array proportional
to the scan volume, protocol layout change or new game binding is introduced.
Large scans take longer, remain limited to locally loaded blocks and preserve
known results. Cluster detection still waits for successful completion.

| Surface | Normal, 1280x800 | Compact, 480x360 |
|---|---|---|
| Block ESP | [512 blocks](block-radius-512-desktop.png) | [512 blocks](block-radius-512-small.png) |
| Chunkfinder | [512 blocks](chunk-radius-512-desktop.png) | [512 blocks](chunk-radius-512-small.png) |

Native synthetic ImGui/D3D11 fixtures exercise the actual radius sliders in both
sizes, asserting 512 and automatic distance 887 m, then capture the real controls.
An input diagnostic and one batched confirmation found readable values, reachable
sliders and normal compact scrolling. No product layout/palette correction was
needed. Captures do not use a live game.

Nine relevant checks pass across final verification runs: block_scan,
smp_features, control_panel, runtime_profiles, portable_client, client_controls,
world_visuals, rockstar_features and depth_rendering. The isolated JNI profile
run passed in 86.85 s; the final block_scan/smp_features/control_panel/package
run passed in 4.53 s. Tests cover unique full scan coverage, clipped heights,
all 1,050,625 horizontal columns at max radius, nearby priority, resumption,
progress, bounded results, far evidence, profile bounds and round-trips, automatic
preferences and native slider input. Keim.exe and Keim-client.zip are rebuilt;
the exact payload/resource/extraction package checks pass. No live Minecraft
attach or live-game performance measurement was performed.

Restart Minecraft and the client when replacing an already attached older agent;
the existing loaded DLL otherwise retains its old scan cap.
