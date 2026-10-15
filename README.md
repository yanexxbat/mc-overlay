# mcov

A native Windows x64 overlay for Minecraft Java Edition, built with C++20,
Dear ImGui and Direct3D 11.

## Features

- Compact translucent menu and launcher, with a matching HUD.
- Entity and block visuals, target effects, particles and waypoints.
- Base ESP with Minecraft models and textures visible through walls (1.21.11).
- Storage ESP with individual block colours, Stash Finder and Spawner ESP.
- Chunkfinder marks possible bases from horizontal deepslate, tall kelp and large amethyst buds/clusters, with whole-chunk frames and individual block boxes.
- Combat and item-assist controls with friend management.
- Spinbot with independent camera, direction/pitch controls and Toggle/Hold binding.
- Freecam with independent flight, adjustable speed/boost and Toggle/Hold binding.
- 19 HUD elements with individual visibility, drag-and-drop placement and scaling.
- Custom keyboard/mouse bindings, modifier chords and Toggle/Hold modes.
- Named configs saved beside the client.
- A portable single-file client.
- KeyAuth license-only sign-in for mcov 1.0, required before startup.

Runtime profiles cover **26.2, 1.21.11, 1.21.1 and 1.20.1**.
See [compatibility](docs/compatibility.md) for supported profiles and verification limits.

## Quick start

1. Start Minecraft.
2. Open `build/client/mcov.exe`.
3. Enter your license key and click **Verify license**.
4. Select your Minecraft instance and click **Start client**.
5. Switch back to the game.

An internet connection and an active mcov license are required. Keys are held
in memory for sign-in and are not saved in configs or logs.

| Default key | Action |
|---|---|
| INSERT | Open or close the menu |
| F6 | Open or close the HUD editor |
| END | Quit the overlay |

Bindings can be changed under **Settings → Keybinds**. Automatic preferences
save in `client-settings.ini` beside `mcov.exe`. Under **Settings → Configs**,
enter a name and **Save config**, or choose a saved profile and **Load selected**.
Profiles live in `configs/<name>.ini` beside the client; copy that folder and
`client-settings.ini` when moving your setup.

The launcher needs a **64-bit JDK matching the game's Java major version**.
Use its JDK folder picker if no suitable installation is found. Restart Minecraft
when replacing an already loaded agent. The current client uses IPC v26; restart an
already running Minecraft instance before reattaching with the updated client.

Open **Blocks** for the SMP modules. Right-click **Storage ESP**, **Stash Finder**
or **Chunkfinder** to configure them. **Chunkfinder** combines deepslate and growth
clues as **Possible Base**, with separate per-chunk minimums. Whole-chunk frames
and individual block boxes are both enabled by default and independently adjustable.
These clues can also occur naturally. **Base ESP → Display → Minecraft textures**
renders loaded blocks around detected container clusters through walls. It also
works with Stash Finder off. Minecraft textures are the default display at full
opacity on 1.21.11. Texture opacity and area expansion are adjustable; detection
uses container bounds plus padding and does not trace a whole building.
The optional **Area marker** mode keeps the coloured frame and fill.
Scans use loaded client blocks within a
4–48 block cube radius. See [SMP features](docs/smp-features.md) for colours,
stash counting, saved waypoints and scan limits.

## CS Knife

Open **Visuals > CS Knife** for Butterfly, Karambit and Katana models with inspect
and deploy animations, detailed bevels and rounded grips, adjustable size/speed,
blade trails and soft additive glow. Glow colour, strength and width are adjustable. Assign an **Inspect
key** (such as R); tap once or hold to repeat. It changes your local main-hand
sword appearance on **1.21.11**. Other players see your normal sword. Restart
Minecraft after this update. See [CS Knife](docs/knives.md).

## Freecam

Open **Misc → Freecam** to toggle; right-click the widget for speed, boost and
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

The portable client is written to `build/client/mcov.exe`; the shareable package is
`build/mcov-client.zip`. Use `.\scripts\build.ps1 -Checks` to run regression checks.

## Documentation

- [Launcher and troubleshooting](docs/launcher.md)
- [Menu, HUD and keybinds](docs/client-ui.md)
- [SMP features and block colours](docs/smp-features.md)
- [Version compatibility](docs/compatibility.md)
- [Third-party notices](THIRD-PARTY-NOTICES.md)

Intended for single-player, private servers and development.
