# mcov

A native Windows x64 overlay for Minecraft Java Edition, built with C++20,
Dear ImGui and Direct3D 11.

## Features

- Compact translucent menu and launcher, with a matching HUD.
- Entity and block visuals, target effects, particles and waypoints.
- Combat and item-assist controls with friend management.
- 19 HUD elements with individual visibility, drag-and-drop placement and scaling.
- Custom keyboard/mouse bindings, modifier chords and Toggle/Hold modes.
- A portable single-file client.

Runtime profiles cover **26.2, 1.21.11, 1.21.1 and 1.20.1**.
See [compatibility](docs/compatibility.md) for supported profiles and verification limits.

## Quick start

1. Start Minecraft.
2. Open `build/client/mcov.exe`.
3. Select your Minecraft instance and click **Client starten**.
4. Switch back to the game.

| Default key | Action |
|---|---|
| INSERT | Open or close the menu |
| F6 | Open or close the HUD editor |
| END | Quit the overlay |

Bindings can be changed under **Settings → Keybinds**. HUD layouts, bindings and
waypoints are saved in `%LOCALAPPDATA%\mcov\client-settings.ini`.

The launcher needs a **64-bit JDK matching the game's Java major version**.
Use its JDK folder picker if no suitable installation is found. Restart Minecraft
when replacing an already loaded agent.

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
- [Version compatibility](docs/compatibility.md)
- [Third-party notices](THIRD-PARTY-NOTICES.md)

Intended for single-player, private servers and development.
