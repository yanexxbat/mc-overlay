# Hole ESP

Open **Blocks â†’ Hole ESP**. Left-click enables the module; right-click opens its
settings. A key can be assigned in the module or **Settings â†’ Keybinds**.

The module marks 1Ã—1 holes whose floor and four side walls consist entirely of
bedrock and/or obsidian. The feet position and two blocks above it must be air.
Green marks bedrock, purple obsidian and amber a mixture. Water, missing blocks,
other wall/floor materials, obstructed headroom and open/double holes are excluded.
These labels describe the sampled blocks, rather than guaranteeing player safety.

Markers are low 3D frames with a translucent floor visible through walls. Each
type can be hidden and given its own RGBA colour. Labels, floor opacity, marker
height, scan radius (4â€“16, default 8) and draw distance (4â€“64 m, default 32) are
adjustable. Settings and the unbound-by-default key save automatically and in
named configs. Legacy configs start with Hole ESP disabled. Panic disables it.

The agent uses the existing mapped block-state methods in an independent scan.
It gets a 1 ms slice per sampler iteration, with a deadline check every 32 cells
(individual game calls may overrun it), a 250 ms pause after completion and at
most 128 results. It does not inherit Block ESP's potentially 512-block sweep.
Known matches remain while a new sweep progresses; successful completion removes
vanished holes. Failed reads clear the hole results. World changes, disabling the
module and radius changes reset the scan. Only locally loaded blocks are visible.
The overlay suppresses reports older than two seconds.

IPC is now **v31** for the separate request and bounded hole results. Restart
Minecraft and the client before using this build with a previously attached
agent. No new Java helper, game-render hook, texture or network request is added.
