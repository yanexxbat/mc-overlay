# Larger Block ESP and Chunkfinder scan range

Mode: Operate. The user asks for a much higher scan radius in both modules.
Keep the existing compact Keim settings and shared scan pipeline. Raise both
radius sliders, agent validation and profile schemas from 48 to 512 blocks.
Allow 1024 m draw distance, including Block ESP tracers and legacy finder import;
raise draw distance as needed when the user enlarges the radius. Keep defaults.
Show existing scan progress on Block ESP and explain loaded-block availability
and longer completion times in the native settings.

Preserve the sampler's 2 ms slice budget, 1024 results, retained visible matches,
successful-completion requirement for clusters, IPC v28 and all other modules'
individual limits. Walk nearby columns first with a constant-memory cursor,
clipping invalid vertical slabs before iteration and using wide progress math.
Verify actual radius-slider input and normal/compact native settings, config
round-trips and out-of-range rejection, far clues, complete unique scan-volume
coverage and the scanner in isolated JVM profile fixtures. No live game attach.
