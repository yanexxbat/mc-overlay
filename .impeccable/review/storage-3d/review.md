# Storage ESP 3D and pulse verification

Outcome: ready for in-game validation. Existing compact native settings remain
legible at both supported fixture sizes. The first viewport exposes scan radius,
scan interval, pulse toggle/cycle and appearance. Type filters and explanatory
copy remain reachable in the scrolling body. World captures show six spatial
cube frames with independent type colours and a rising pulse band.

| Fixture | Desktop | Small |
|---|---|---|
| Storage controls | [1280x800](storage-3d-settings-desktop.png) | [480x360](storage-3d-settings-small.png) |
| 3D world frames | [1280x800](storage-3d-world-desktop.png) | [480x360](storage-3d-world-small.png) |

These are the real ImGui/D3D11 WARP renderer with synthetic snapshots, not
Minecraft screenshots. One inspection batch and one correction/confirmation
batch were used. The world fixture initially had a player position outside its
own scan range; its position and nonempty-geometry assertion were corrected.
The projected-edge test accounts for ImGui's half-pixel line alignment.

All 30 tests passed: the 29-test sandbox run took 93.40 seconds; the isolated
frame_attach fixture passed outside the sandbox in 0.45 seconds. New coverage
checks stable incremental scan results, committed removal/empty scans, bounded
pools, sequential fades/pulses, configured speed, reduced motion, world/session
resets, twelve exact projected edges, clipping, through-wall depth policy,
named profiles, automatic preferences and migration of older settings.
Portable packaging also passed; both the client EXE and share ZIP were rebuilt.

Scanner work retains its existing 2 ms slice target and 1024-result limit.
The storage presentation also has at most 1024 entries and reuses its lookup
index while the set is stable. No added model extraction or render-thread JNI.
Successful completed scans remain the only source of cluster counts.

No live Minecraft scan, actual frame-rate/RAM measurement or all-GPU validation
was performed. Known matches can remain outdated until a successful sweep
finishes; other active block modules may request a faster shared scan cadence.
