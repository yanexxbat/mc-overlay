<p align="center">
  <img src="src/resources/keim-logo.png" alt="Keim mascot" width="112">
</p>

<h1 align="center">Keim</h1>

<p align="center">
  A native Minecraft Java client with an external overlay, configurable visuals and a movable HUD.
</p>

<p align="center">
  <strong>Windows x64</strong> · C++20 · Dear ImGui · Direct3D 11<br>
  One portable executable. No account or license key required.
</p>

<p align="center">
  <a href="#quick-start">Quick start</a> ·
  <a href="#features">Features</a> ·
  <a href="#build-from-source">Build</a> ·
  <a href="#documentation">Documentation</a>
</p>

## Quick start

1. Start **Minecraft Java Edition**.
2. Open **`Keim.exe`**. After building from source, it is in `build/client/`.
3. Select your Minecraft instance and click **Start client**.
4. Switch back to the game and press **INSERT** to open the menu.

The launcher opens directly to the Minecraft selector. KeyAuth, license prompts,
HWID collection and periodic online license checks have been removed.

Only `Keim.exe` is needed to run the client; its native and Java helpers are embedded.
The shareable `Keim-client.zip` contains the executable and an optional `START.txt` guide.
Extract the ZIP before launching.

### Controls

| Key | Action |
| :--- | :--- |
| **INSERT** | Open or close the client menu |
| **F6** | Open or close the HUD editor |
| **F12** | Panic: disable all modules and hide the UI |
| **END** | Quit the overlay |
| **Ctrl+F** | Focus module search while the menu is open |

Left-click a module to toggle it; right-click to open its settings. Configure
keyboard and mouse bindings under **Settings > Keybinds**, including modifier
chords and Toggle/Hold modes. **F12 stays reserved for Panic.**

## Features

| Area | Included |
| :--- | :--- |
| **Workspace** | Searchable module grid, category navigation, All/Enabled filters and independent settings windows |
| **Visuals** | Player and block ESP, health bars, target effects, particles, trails and waypoints |
| **Exploration** | Storage ESP, Stash Finder, Spawner ESP, Base ESP, Hole ESP and Chunkfinder |
| **Combat & items** | Combat controls, friend management, Auto-Totem health threshold and item assists |
| **HUD** | 21 movable and scalable elements, including Watermark, ArrayList, Target Info, radar and performance statistics |
| **Movement** | Freecam with adjustable speed and boost; configurable Spinbot |
| **World information** | DonutSMP region map and passive Anti Vanish hints |
| **Personalization** | Saved HUD positions, automatic preferences and named configuration profiles |

**Chunkfinder** supports Activity, Block clues and Combined modes, with configurable
findings and a DonutSMP preset. Findings are heuristic clues based on available
client data. [Read the Chunkfinder guide](docs/chunk-finder.md).

**Freecam** uses WASD to move, Space/Shift to rise or descend and Ctrl to boost.
The player stays in place, and combat/item assists pause while it is active.
Only chunks already loaded by Minecraft are visible. [Freecam details](docs/freecam.md).

**Anti Vanish** provides passive TAB-list and optional invisible-player hints on
1.21.11. Hints are not proof of vanished staff and cannot reveal fully hidden
server data. [Detection behavior and limits](docs/anti-vanish.md).

## Compatibility

Runtime profiles cover **26.2, 1.21.11, 1.21.1 and 1.20.1**. Feature support varies
by profile; Minecraft-textured Base ESP is supported on **1.21.11**.
See the [compatibility guide](docs/compatibility.md) for tested coverage and limits.

The launcher requires a **full 64-bit JDK matching the game's Java major version**.
If one is missing, use **Select JDK folder** and choose the installation root.
A JRE alone cannot attach the agent.

**Updating:** quit the old client with END, replace `Keim.exe`, and restart Minecraft
before reconnecting so the updated agent can load.

## Settings that travel with you

```text
client/
├── Keim.exe
├── client-settings.ini
└── configs/
    └── My profile.ini
```

Automatic preferences are saved beside the executable. Under **Settings > Configs**,
use **Save config**, **Overwrite config** or **Load selected** to manage named profiles.
Copy `client-settings.ini` and `configs/` along with `Keim.exe` when moving your setup.

Runtime helpers, logs and the saved JDK selection live in `%LOCALAPPDATA%\Keim`.
See [launcher and troubleshooting](docs/launcher.md) for details.

## Build from source

### Requirements

- **Windows x64** and **Git**.
- **Visual Studio Build Tools 2022** with C++ x64 tools, a Windows SDK and CMake tools.
- **CMake 3.20+**; the build script can use the version bundled with Visual Studio.
- A full **JDK 17+** for compilation. Runtime attachment still needs the game's matching JDK.
- **Python 3.10+** when running the full regression checks.

From the repository root in PowerShell:

```powershell
.\scripts\build.ps1 -Config Release
```

The script locates the installed JDK and Visual Studio toolchain. The first build
fetches the pinned Dear ImGui sources and needs network access.

| Output | Purpose |
| :--- | :--- |
| `build/client/Keim.exe` | Portable client to run |
| `build/Keim-client.zip` | Package to share |

### Checks and build options

```powershell
# Build and run the regression suite
.\scripts\build.ps1 -Config Release -Checks

# Reuse the existing ImGui source cache without downloading
.\scripts\build.ps1 -Config Release -Offline
```

Use `-JavaHome` or `-PythonExecutable` to specify an installation explicitly.
If PowerShell blocks the unsigned script, run it with a process-only policy override:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Config Release
```

The suite covers native UI rendering, controls, animation, portable packaging,
JNI compatibility and feature logic. Automated checks do not replace testing
inside a live Minecraft session.

## Documentation

| Guide | Contents |
| :--- | :--- |
| [Launcher](docs/launcher.md) | Startup, JDK selection, logs and troubleshooting |
| [Client UI](docs/client-ui.md) | Menu, HUD and keybinds |
| [Compatibility](docs/compatibility.md) | Runtime profiles and verification limits |
| [SMP features](docs/smp-features.md) | Storage, bases, spawners, colours and waypoints |
| [Chunkfinder](docs/chunk-finder.md) | Activity detection, block clues and scan settings |
| [Hole ESP](docs/hole-esp.md) | Hole types, range and appearance |
| [Freecam](docs/freecam.md) | Flight controls and camera behavior |
| [Spinbot](docs/spinbot.md) | Rotation settings and implementation limits |
| [Anti Vanish](docs/anti-vanish.md) | Passive hints, notifications and limitations |
| [Third-party notices](THIRD-PARTY-NOTICES.md) | Credits and dependency licenses |

---

Intended for single-player, private servers and development.
