<p align="center">
  <img src="src/resources/keim-logo.png" width="96" alt="Keim's green mascot">
</p>

<h1 align="center">Keim</h1>

<p align="center">
  An external overlay for Minecraft Java Edition on Windows x64.<br>
  Highlight entities and blocks, arrange your HUD and switch between saved setups.
</p>

<p align="center">
  <strong><a href="https://github.com/yanexxbat/mc-overlay/releases/latest/download/Keim.exe">Download Keim.exe</a></strong>
  &nbsp;·&nbsp; <a href="https://github.com/yanexxbat/mc-overlay/releases/latest">Release notes &amp; checksums</a>
  &nbsp;·&nbsp; <a href="#build-from-source">Build from source</a>
</p>

## First launch

1. Start **Minecraft Java Edition** and open **Keim.exe**. The launcher lists your running Minecraft instances; no Keim account or license key is required.
2. Select the instance you want and click **Start client**. If the launcher asks for Java, choose **Select JDK folder** and point it at a full 64-bit JDK matching your game version.
3. Switch back to Minecraft and press <kbd>Insert</kbd>. Left-click a module to toggle it, or right-click it to change its settings.

| Minecraft version | Matching 64-bit JDK |
| :--- | :--- |
| 26.2 | JDK 25 |
| 1.21.11 or 1.21.1 | JDK 21 |
| 1.20.1 | JDK 17 |

Prefer a ZIP? [Keim-client.zip](https://github.com/yanexxbat/mc-overlay/releases/latest/download/Keim-client.zip) contains the same executable and a short start guide. Extract it before launching. To update, quit Keim with <kbd>End</kbd>, replace the executable and restart Minecraft so the new agent can load.

## Inside the overlay

| Area | What you can do |
| :--- | :--- |
| **HUD** | Drag and scale 21 elements, including the active module list, target info, radar and process memory readout. |
| **Visuals** | Highlight players, health bars, storage containers and holes. Add waypoints, trails or target effects. |
| **Exploration** | Use Chunkfinder to inspect activity and block clues in loaded chunks, or follow your position on the DonutSMP region map. Chunk findings are clues, not confirmed bases. |
| **Combat & movement** | Set an Auto-Totem health threshold, manage friends or explore loaded terrain with Freecam while your player stays in place. |

Some features depend on the Minecraft version. On **1.21.11**, Base ESP can show Minecraft textures, and Anti Vanish can flag visible player-list or entity clues. It cannot reveal information the server never sends.

### Keyboard shortcuts

| Key | Action |
| :--- | :--- |
| <kbd>Insert</kbd> | Open or close the module menu |
| <kbd>Ctrl</kbd> + <kbd>F</kbd> | Search modules in the open menu |
| <kbd>F6</kbd> | Open the HUD editor to move or resize elements |
| <kbd>F12</kbd> | **Panic:** disable all modules and hide the UI |
| <kbd>End</kbd> | Quit Keim |

Use **Settings → Keybinds** for custom keyboard and mouse bindings. <kbd>F12</kbd> stays reserved for Panic.

<details>
<summary><strong>Move your settings to another PC</strong></summary>

Copy `client-settings.ini` and the `configs/` folder alongside `Keim.exe`. The INI holds automatic preferences; **Settings → Configs** lets you save and load named profiles. Logs, runtime helpers and the launcher's JDK selection live in `%LOCALAPPDATA%\Keim`.

</details>

## Build from source

Keim uses **C++20**, **Dear ImGui** and **Direct3D 11**. To build it on Windows x64, install Git, Visual Studio Build Tools 2022 with the x64 C++ toolchain and Windows SDK, CMake 3.20+ (the Visual Studio copy works), and a full JDK 17 or newer.

Run this in PowerShell from the repository root:

```powershell
.\scripts\build.ps1 -Config Release
```

The first build downloads the pinned ImGui sources. It produces **`build/client/Keim.exe`** to run and **`build/Keim-client.zip`** to share. The native and Java helpers are embedded in the executable.

<details>
<summary><strong>Offline builds, clean rebuilds and JDK selection</strong></summary>

- `-Offline` reuses the ImGui source cache after the first build.
- `-Clean` recompiles the build targets while preserving client settings and profiles.
- `-JavaHome` selects a specific JDK installation.

If PowerShell blocks the unsigned build script, use a process-only override:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Config Release
```

</details>

[Portable start guide](docs/client-start.txt) · [Launcher source](src/injector/) · [Overlay source](src/overlay/) · [JVM agent](src/agent/) · [Third-party notices](THIRD-PARTY-NOTICES.md)

Intended for single-player, private servers and development.
