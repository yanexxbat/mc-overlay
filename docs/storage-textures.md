# Storage ESP textures

Storage ESP renders the selected loaded containers through walls using Minecraft's
own models, atlas UVs, orientation and current resource-pack textures. The external
overlay draws only optional names, distances and tracers; it no longer draws boxes
or scan bands. Per-type colours affect those annotations. Texture opacity controls
the in-game composite. Legacy pulse/glow settings remain readable in saved configs.

The texture bridge currently supports the audited 1.21.11 Mojmap, vanilla and
intermediary profiles. Other versions display an availability message. Restart
Minecraft and use the rebuilt client to load the updated agent and Java helpers.

Storage uses its own leased request, report, model cache and OpenGL resources, so
Base ESP can run at the same time. Requests contain up to 1,024 individual container
cells from the dedicated storage scan, filtered by type and distance, deduplicated
and sorted so scan ordering does not restart extraction. Requests expire when the
overlay stops refreshing them and are checked against the current world/server.
Water in waterlogged containers is excluded from the storage pass.

The shared-memory version is 35 (including the Anti Vanish report update). Large request buffers are kept off the menu and
JVM render-thread stacks. Tests cover selection and capacity, world/lease checks,
independent cache resets, native atlas/model capture, through-wall compositing and
the absence of overlay box geometry. Actual appearance in a running Minecraft
world still needs an in-game check.
