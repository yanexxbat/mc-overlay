# Storage ESP textures

Storage ESP optionally renders selected loaded containers through walls using
Minecraft's own models, atlas UVs, orientation and current resource-pack textures.
**Container textures** is off by default. The external overlay draws transparent
full-cell boxes with soft outlines independently of that switch. Each container type's
configurable RGBA colour controls its fill, outline, optional names, distances and
tracers. Camera-facing sides fade from a faint tint at the bottom to a stronger
tint at the top; horizontal faces use a restrained uniform tint. Thin, slightly
lightened edges define the cell, with an adjustable halo on front edges and dimmer
rear edges without a halo. The tint and outlines inherit opacity and marker alpha,
keeping Minecraft's textures visible.

The settings show **Opacity**, **Soft glow**, **Container textures**,
**Block types and colours** and **Reset colours**. The **Visibility** row reads
**Filled boxes** when textures are off and **Filled boxes + textures** when on.
Opacity controls both the boxes
and the in-game texture composite. Soft glow ranges from 0 to 1.5 and reuses the
existing saved glow setting. Legacy pulse fields remain compatible with saved
configs; the current presentation has no scan bands or sequential pulse effect.
The texture choice persists in automatic preferences and named profiles; older
configs without it default to off. Turning it off immediately clears the storage
texture request while preserving the coloured gradient boxes and soft outlines.

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
coloured overlay box geometry. Synthetic captures cover the markers and settings
at desktop and small sizes. Actual appearance in a running Minecraft world still
needs an in-game check.
