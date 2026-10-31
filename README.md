# Keim

A native Windows x64 overlay for Minecraft Java Edition, built with C++20,
Dear ImGui and Direct3D 11.

Keim uses the green germ mascot in the launcher, client menu, HUD watermark and
Windows icon. Portable settings and named configs remain
compatible after the rebrand. The client and share package are now `Keim.exe`
and `Keim-client.zip`.

## Features

- Graphite / Sage ImGui workspace with a responsive two-column module grid, All/Enabled filters, Ctrl+F search and direct settings buttons; matching Keim branding in the launcher and HUD.
- Entity and block visuals, target effects, particles and waypoints.
- Base ESP with Minecraft models and textures visible through walls (1.21.11).
- 3D Storage ESP with individual block colours, adjustable scan timing and sequential pulses, plus Stash Finder and Spawner ESP.
- Storage ESP scans loaded container positions directly with its own 4â€“512 block radius, independently of long Block ESP/Chunkfinder sweeps.
- Chunkfinder offers Activity (received underground light/redstone changes), Block clues (background terrain analysis) and Combined modes, with grouped settings, a DonutSMP preset, material highlights and orbiting orbs. See [Chunkfinder](docs/chunk-finder.md).
- Block ESP supports a 512-block scan radius and 1024 m draw distance. Chunkfinder independently inspects loaded chunks across their full height.
- Hole ESP marks 1Ã—1 bedrock, obsidian and mixed holes with coloured 3D frames and a separate short-range scan. See [Hole ESP](docs/hole-esp.md).
- Combat and item-assist controls with friend management.
- Spinbot with silent packet rotation, independent camera/movement, direction/pitch controls and Toggle/Hold binding. See [rotation behavior and limits](docs/spinbot.md).
- Freecam with independent flight, adjustable speed/boost and Toggle/Hold binding.
- 20 HUD elements with individual visibility, drag-and-drop placement and scaling.
- DonutSMP region map covering -225,000 to +225,000, with six server regions and live Overworld position.
- Custom keyboard/mouse bindings, modifier chords and Toggle/Hold modes.
- Named configs saved beside the client.
- A portable single-file client.

Runtime profiles cover **26.2, 1.21.11, 1.21.1 and 1.20.1**.
See [compatibility](docs/compatibility.md) for supported profiles and verification limits.

## Quick start

1. Start Minecraft.
2. Open `build/client/Keim.exe`.
3. Select your Minecraft instance and click **Start client**.
4. Switch back to the game.

Keim starts directly without a login or an online license check.

| Default key | Action |
|---|---|
| INSERT | Open or close the menu |
| F6 | Open or close the HUD editor |
| END | Quit the overlay |

Bindings can be changed under **Settings â†’ Keybinds**. Automatic preferences
save in `client-settings.ini` beside `Keim.exe`. Under **Settings â†’ Configs**,
enter a name and **Save config**, or choose a saved profile and **Load selected**.
Profiles live in `configs/<name>.ini` beside the client; copy that folder and
`client-settings.ini` when moving your setup.

The launcher needs a **64-bit JDK matching the game's Java major version**.
Use its JDK folder picker if no suitable installation is found. Restart Minecraft
when replacing an already loaded agent. The current client uses IPC v39; restart an
already running Minecraft instance before reattaching with the updated client.

Open **Blocks** for the SMP modules. Right-click **Storage ESP**, **Stash Finder**
or **Chunkfinder** to configure them. Storage ESP uses world-aligned 3D frames;
known containers stay visible throughout rescans. **Scan interval** sets the pause
between sweeps (200â€“3000 ms); **Pulse cycle** sets the visual cycle (500â€“6000 ms).
**Chunkfinder** groups evidence into placed blocks, bases, farms and mined geodes.
Its default display tints the nearest findings at feet height. Detailed mode adds
outlines, evidence labels and individual clue blocks. Findings, alerts and scanning
limits are configurable in General and Options. These are heuristic clues.
**Base ESP â†’ Display â†’ Minecraft textures**
renders loaded blocks around detected container clusters through walls. It also
works with Stash Finder off. Minecraft textures are the default display at full
opacity on 1.21.11. Texture opacity and area expansion are adjustable; detection
uses container bounds plus padding and does not trace a whole building.
The optional **Area marker** mode keeps the coloured frame and fill.
Scans use loaded client blocks within a
4â€“48 block cube radius. See [SMP features](docs/smp-features.md) for colours,
stash counting, saved waypoints and scan limits.

## DonutSMP region map

Open **HUD â†’ Region map** to show the full 9 Ã— 9 server map, with colours for
EU Central, EU West, NA East, NA West, Asia and Oceania. Right-click its widget
for labels, grid lines, coordinates, scale, position and an optional Toggle/Hold
keybind. **F6** opens the HUD editor for dragging the map. Settings are saved in
automatic preferences and named configs; older configs load with the map off.

The white dot shows your live player position in the DonutSMP Overworld.
Other servers, dimensions, an expired connection and out-of-bounds positions
show a status instead. The full map stays visible for reference.
The geographic reference is [Donut.Auction](https://donut.auction/regions),
checked on 15 October 2026; server boundaries can change. Region names are
inferred from coordinates, not verified against the server scoreboard.
Restart Minecraft and Keim after updating so the agent publishes complete
dimension names for the position indicator.

## Anti Vanish

Open **Misc > Anti Vanish** for passive TAB-list and optional invisible-player
hints on **1.21.11**. Uses a bounded sampler twice per second, with a confirmation
delay, a result list and optional Notifications HUD alerts. Hints are not proof
of vanished staff; fully hidden server data cannot be detected. Knife cosmetics
have been removed. Restart Minecraft and Keim after updating.
See [Anti Vanish](docs/anti-vanish.md).

## Freecam

Open **Misc â†’ Freecam** to toggle; right-click the widget for speed, boost and
its initially unbound key. Close the menu to fly with **WASD**, look with the
mouse, rise with **Space**, descend with **Shift**, and boost with **Ctrl**.
The player stays in place; attacks, block breaking, item use and combat/item
assists pause. Disable Freecam to return. Menus, focus loss, world changes and
an expired client connection release the camera; resuming starts at the player.
Flight only shows chunks already loaded by Minecraft. Settings and the binding
persist; automatic startup keeps Freecam off. Named profiles retain its state.
See [Freecam implementation and checks](docs/freecam.md).

## Build

Requires Visual Studio Build Tools with C++ x64 support, CMake 3.20+ and a full JDK 17+.

```powershell
.\scripts\build.ps1
```

The portable client is written to `build/client/Keim.exe`; the shareable package is
`build/Keim-client.zip`. Use `.\scripts\build.ps1 -Checks` to run regression checks.

## Documentation

- [Launcher and troubleshooting](docs/launcher.md)
- [Menu, HUD and keybinds](docs/client-ui.md)
- [SMP features and block colours](docs/smp-features.md)
- [Version compatibility](docs/compatibility.md)
- [Third-party notices](THIRD-PARTY-NOTICES.md)

Intended for single-player, private servers and development.
