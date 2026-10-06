# Portable client and launcher

Open `build/client/mcov.exe`. Select an already running Minecraft window and
click **Client starten**. The launcher automatically connects and starts the
overlay for that exact instance. Starting again switches back to Minecraft.
Only this EXE needs to be copied; all native and Java helpers are embedded.

## Runtime requirements

Windows x64, Minecraft Java Edition, and an x64 JDK matching the game's Java
major are required: normally Java 17 for 1.20.1, Java 21 for the 1.21 releases,
and Java 25 for 26.2. A JRE alone cannot attach the agent. Discovery checks the
build JDK, configured paths and installed JDKs. A missing match exposes
**JDK-Ordner auswaehlen**. Select the installation root containing `bin/java.exe`,
`bin/javac.exe`, `bin/attach.dll` and `release`. Selection is saved for later starts.
The launcher does not install Java.

## Data and lifetime

- `%LOCALAPPDATA%\mcov\runtime\<package hash>`: embedded overlay, agent and helpers.
  Extraction verifies SHA-256, skips valid files, repairs corrupt files and
  serializes concurrent preparation. The hash changes when packaged files change.
- `%LOCALAPPDATA%\mcov\client-settings.ini`: saved keybinds and HUD layout.
- `%LOCALAPPDATA%\mcov\launcher-jdk.txt`: selected JDK path.
- `%LOCALAPPDATA%\mcov\mcov_client.log` and `mcov_overlay.log`: launcher/overlay logs.
- `mcov_agent.log` beside the loaded DLL: agent log, also included under Diagnose.

On the first portable start, existing keybind/HUD settings beside the launcher
or in the build's sibling `bin` folder are imported if no portable settings exist.
Existing portable settings are never replaced by this import.

Closing the launcher leaves the overlay and Minecraft running. Closing during
connection cancels the launcher's attach helper; it never terminates Minecraft.
An agent already loaded into the JVM stays loaded until Minecraft exits.
An incompatible/stale agent requires restarting Minecraft before retrying.
The default **END** binding quits the overlay, **INSERT** opens its menu and
**F6** opens the HUD editor. Bindings remain configurable.

## Presentation and diagnostics

The injector shares `ClassicStyle` with the client: #242424 background,
#191919 fields, #AE69D1 accents, black borders and embedded ProggyClean.
The main page contains instance selection, refresh, one primary start action,
phase/status and recovery actions. Technical logs live under **Diagnose**,
with copy and open-folder actions. Connection runs on a worker; the native
window remains draggable and responsive. Duplicate launcher/overlay instances
are prevented by named mutexes. An initialized overlay signals a readiness event
before the launcher reports success. Client discovery never falls back to a
different game process when the selected instance disappears.

## Verification

`portable_client` copies only the EXE into an otherwise empty folder, verifies
the GUI subsystem and exact embedded payloads, tests paths with spaces/Unicode,
idempotent extraction, concurrent corruption repair, and early rejection of
invalid overlay PIDs. `control_panel` renders the actual launcher offscreen at
640 x 520 and 480 x 380, including empty, selected, busy, missing-JDK and diagnostic
states, and checks style restoration and button input. Neither check attaches to
Minecraft. Live attach/focus behavior still needs a real game session.
