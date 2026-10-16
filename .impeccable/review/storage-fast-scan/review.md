# Storage ESP range and faster traversal

Storage ESP now accepts 4–512 blocks in its actual slider, runtime clamp,
automatic preferences and named config schema. The prior 48-block storage
clamp and generic 256-block module limit no longer override it. Default radius
24, existing colours, 3D frames, glow, filters, pulse and scan pause are preserved.

The new independent scanner walks the already-loaded client chunk cache and
block-entity keys. It queries block states only for candidate positions within
the requested radius and applies the existing 13-type mask. Other modules no
longer determine Storage ESP radius, completion, interval, capacity or progress.
Its own mailbox/results use IPC v30. Known results are retained throughout
rescans and merged incrementally; only successful completion replaces the set.
The existing renderer/pulse consumes the dedicated results without copying an
extra WorldSnapshot each frame. A full generic result pool cannot crowd them out.

Each step has a 1 ms budget checked every 16 traversal operations. Individual
Java calls may overrun it. At most 1024 results, one chunk key snapshot and one
loaded-cache reference are retained. Keys are released as chunks finish and
all references are released on completion, cancellation, world change or unload.
A bounded compatibility block scan remains available if a modified runtime
prevents the direct path. Requests expire after three seconds without refresh;
the lease clock is sampled after reading the mailbox so work done earlier in
the frame cannot falsely reject a newly published request.

The profile generator audits seven members across all eleven supported runtime
profiles from checksum-verified cached official clients and namespace artifacts.
Generated symbols and fixture metadata are committed source; no network, extra
runtime file, Java helper, texture or render hook is required.

| Actual native radius controls | Capture |
|---|---|
| 1280x800 | [512 blocks](storage-radius-512-desktop.png) |
| 480x360 | [512 blocks](storage-radius-512-small.png) |

One batched normal/compact inspection confirmed readable unchanged controls at
the maximum. Real slider input reaches 512 and remains there after runtime
clamping, automatic save/load and named profile round-trip. No visual correction
or polish loop was needed. Captures use synthetic state, without a live game.

All 32 selected regression checks passed in 99.17 seconds, excluding the
unrelated Java self-attach check. After the lease-clock correction, the five
relevant checks (smp_features, control_panel, agent_package_match, runtime_profiles,
portable_client) passed in 90.81 seconds. The JNI fixture traverses 4225 chunk
slots with three candidate container positions: radius 24 performs one block-state
read, radius 512 performs two, and the out-of-range third position is excluded.
The same assertions run for all eleven profiles and both camera fixture variants.
Type filters are exercised through the actual mapped methods. This establishes
query count/coverage rather than promising a measured live-game scan duration.

The pulse test uses dedicated results at distance 500, then fills the generic
pool with 1024 unrelated containers and changes its scan ID. The dedicated
marker remains independent; an empty dedicated result correctly removes it.
Existing animation, type colours, world isolation, limits and rendering checks
also pass. Keim.exe and Keim-client.zip were rebuilt with verified payloads.
Restart Minecraft and Keim when replacing the previously attached v29 agent.
