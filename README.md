# mcov — external overlay for Minecraft Java Edition

C++20 / Dear ImGui overlay with a JNI/JVMTI agent supplying live game state.
Runtime profiles cover **26.2, 1.21.11, 1.21.1 and 1.20.1**, including Forge's
version-specific member names. See [compatibility and verification](docs/compatibility.md).

**Start here:** open `build/client/mcov.exe`, select Minecraft and click
**Client starten**. This single portable file contains the overlay, native agent
and Java helpers. The launcher uses the same rounded Liquid Glass controls as the client.
See [launcher and portable package](docs/launcher.md) for runtime requirements and diagnostics.

The [Rockstar-inspired feature package](docs/rockstar-features.md) adds **29 functions**:
glowing wings/halo/ESP, jump and target rings, damage markers/vignette, crosshair,
offscreen arrows, storage/beacon/chunk guides, sound feedback, middle-click friends,
death coordinates/waypoints, privacy aliases, notifications and eight HUD widgets.
Every new feature has its own module tile and settings section. Toggleable features
start disabled with an unassigned keybind; Panic is an action.
The HUD now offers 19 movable/scalable elements. [Source-module coverage](docs/rockstar-catalog.md)
lists all 122 reference modules and distinguishes implemented behavior from remaining work.
Restart Minecraft before using this updated package: the agent/overlay now use **IPC v19**.

**World-effect occlusion:** on 1.21.11 Fabric with normal OpenGL, **Settings → Overlay →
Occlude world effects** hides aura orbs, trails, particles, rings, wings and hats behind
players and solid blocks. Other versions retain the previous behavior.
See [world depth and limitations](docs/world-depth.md).

The modular C++20 [Combat Assist / Kill Aura core](docs/combat-assist.md) includes
deterministic selection/rotation/cooldown/sync tests and an executable simulator
(`build/bin/mcov_combat_demo.exe`). Kill Aura executes player rotation and normal
game attacks synchronously on the instrumented Minecraft client tick. Combat
acquires the nearest living target in reach, follows its current hitbox directly
every tick and keeps the lock until death or loss of range/sight. Attacks repeat
when the game's cooldown is ready. The menu only needs activation, target type
and reach; old smoothing/timing settings are normalized by the live adapter.
Combat
rotation always keeps mouse-controlled camera angles separate from the player's
combat angles, including the third-person camera position. There is no camera
toggle or visible rotation mode. Existing
Aim Assist and Triggerbot pause while it is enabled. Live server-correction
capture is not implemented.

An active Kill Aura target automatically gets a glowing yellow/lime corner cube
that follows its hitbox and smoothly alternates rotation direction. It also works
with ESP and Aura orbs disabled. Configure it under **VISUALS → Target marker**: size, colours, glow, opacity, animation and direction interval.
**Shape & timing** contains corner geometry, depth and fade. See [target cube](docs/target-cube.md).

---

## The architectural correction you need to know about

The brief asked for a single external process that calls `OpenProcess` on
`javaw.exe`, then uses `JNI_GetCreatedJavaVMs` + `AttachCurrentThread` to get a
`JNIEnv*` for the target. **That cannot work, and the reason is worth
understanding before you read the code.**

`JNI_GetCreatedJavaVMs` enumerates VMs in the *calling* process's address space.
It has no PID parameter and no remote form. Even if you could obtain a `JavaVM*`
belonging to another process, it is a raw pointer into that process's heap — the
function-table dispatch behind `env->CallStaticObjectMethod(...)` would
dereference addresses that mean nothing in your own address space. There is no
cross-process JNI, in any language.

So the tool is split in two:

| | `mcov_overlay.exe` | `mcov_agent.dll` |
|---|---|---|
| Runs in | its own process | inside `javaw.exe` |
| Does | D3D11 + ImGui rendering, Win32 window work, `OpenProcess`, process/window discovery | JNI + JVMTI, samples coherent camera/entity frames in a daemon worker |
| Knows about JNI | nothing | everything |

They communicate over a lock-free shared-memory seqlock
(`src/common/shared_state.hpp`). The agent gets into the JVM through the
**standard attach API**. The helper first loads a small Java instrumentation
agent for frame markers, then calls `loadAgentPath(dll)` for the native sampler.
Frame markers update a volatile epoch. The separately armed combat hook invokes
JNI on the client tick; the external renderer only exchanges configuration and
diagnostics through shared memory.

`JNI_GetCreatedJavaVMs` *is* still implemented, in
`jvm::Runtime::DiscoverVm()`. It runs inside the agent, where it is meaningful,
and it dynamically resolves `jvm.dll` exactly as specified. The normal path
(`AdoptVm`) is cheaper because `Agent_OnAttach` already hands us the `JavaVM*`.

---

## Layout

```
CMakeLists.txt              overlay, injector, agent + FetchContent for ImGui
third_party/glass/          adapted liquidDX11 material/blur renderer and embedded fonts
scripts/build.ps1           configure + build wrapper
tools/AgentLoader.java      JVM attach helper (jdk.attach)
tools/frame-sync/           Java frame markers and audited bytecode transformer
src/
  main.cpp                  overlay entry point
  common/
    flat_style.hpp/.cpp     flat theme, shadows, module panels, scoped styles
    flat_app.hpp/.cpp       shared Win32/D3D11 injector shell with glass runtime
    liquid_style.hpp       scoped Liquid Glass menu/launcher theme
    liquid_glass.hpp/.cpp   RAII backdrop/material texture and capture fallback
    shared_state.hpp        POD wire format + seqlock (compiled into both)
    ipc.hpp/.cpp            named shared memory (Local\ namespace, no admin)
    log.hpp/.cpp            logger that works inside a console-less javaw.exe
  overlay/
    overlay.hpp/.cpp        Win32 window, D3D11 device, ImGui UI, ESP, HUD
    process.hpp/.cpp        OpenProcess, window discovery, jvm.dll path, agent launch
    render_math.hpp         view-projection reconstruction, world->screen
  agent/
    jvm_interface.hpp/.cpp  JNI layer: attach/detach, class cache, exceptions
    agent_main.cpp          Agent_OnAttach, JVMTI resolver, sampler thread
    frame_sync.hpp          sampler-only epoch checks and high-resolution wait
    mappings.hpp           semantic member schema
    generated_profiles.hpp audited version/namespace-specific JNI tables
    runtime_profile.hpp    version, FOV source and screen owner metadata
```

---

## Build

Requires **MSVC or clang-cl (x64)**, **CMake 3.20+**, and a **full JDK 17+** (not
a JRE — the agent needs `jvmti.h`, the loader needs `javac`).

```powershell
.\scripts\build.ps1
```

The script needs nothing on PATH. It locates Visual Studio via `vswhere`, uses
the **CMake and Ninja bundled inside VS Build Tools** if no standalone CMake is
installed, runs the build inside the `vcvars64` environment, and auto-detects a
JDK 17+ under `Eclipse Adoptium` / `Java` / `Microsoft`. Override with
`-JavaHome`, `-Config`, `-Generator`, or `-Clean`.

Or directly:

```bash
cmake -S . -B build -A x64 -DMCOV_JAVA_HOME="C:/Program Files/Eclipse Adoptium/jdk-21.0.11.10-hotspot"
```

The ready-to-use folder is **`build/client/`**, containing only **mcov.exe** and
the optional **START.txt** guide. To share it, send **`build/mcov-client.zip`**;
extract the ZIP and double-click mcov.exe. The EXE can also be copied alone to another
folder or machine. MSVC runtime libraries are linked statically; the Java runtime
is supplied separately. Developer artifacts remain in `build/bin/`:
`mcov_overlay.exe`, `mcov_injector.exe`, `mcov_agent.dll` and `tools/`.
Running these developer binaries directly requires keeping that folder together.

Loading into a game requires an attach JDK matching the **running Java major**:
normally 25 for 26.2, 21 for the 1.21 releases and 17 for 1.20.1. The loader
skips wrong-major candidates; `MCOV_JDK_HOME` can point to a custom installation.

---

## Run

1. Start Minecraft; the main menu is sufficient.
2. Open **`build/client/mcov.exe`**.
3. Select your Minecraft instance and click **Client starten**.
4. Switch to Minecraft. **INSERT** opens the menu, **F6** opens the HUD editor,
   and **END** quits the overlay. These default bindings can be changed.

The launcher prepares its embedded files in `%LOCALAPPDATA%\mcov\runtime`,
loads the agent when needed and opens the overlay for the selected PID.
The window remains responsive during connection. Existing client instances are
reused. **Diagnose** contains details and logs; a missing JDK can be selected
with **JDK-Ordner auswaehlen**. Closing the launcher leaves Minecraft and the
overlay running. Developer runs of `mcov_overlay.exe` still support `--no-agent`.

---

## HUD

Independent HUD elements, individually enabled, moved and scaled under **HUD**.
Left-click **HUD → Editor** or use **F6**, drag elements, select them in the toolbar and
adjust Enabled/Scale. **Esc**, **Done** or F6 finishes. Watermark, Active modules
and Keybinds are configurable elements too. Empty/disabled elements preview in
the editor; compact editor viewports show the selected element only. Positions,
per-element scale and keybinds persist in `%LOCALAPPDATA%\mcov\client-settings.ini`
for the portable client (beside the executable for direct developer runs).
See [client UI](docs/client-ui.md).

**Equipment** — the six slots (helmet/chest/legs/boots/main/off) with durability
bars. `maxDamage == 0` means "has no durability" — a block, food, a totem — which
is deliberately distinguished from "full durability", so those items get no bar
at all. Bars go amber below 50% and pulse red below a configurable threshold.

**Status effects** — name, level and remaining time. Effect names come from
`getTranslationKey()` ("effect.minecraft.speed" → "Speed"), which avoids the
`RegistryEntry → StatusEffect → Text` chain entirely: one call, one string, no
extra classes to resolve. Effects under 10 seconds turn amber.

**Coordinates** — XYZ, chunk plus in-chunk offset (the same numbers F3 shows),
the 1:8 nether/overworld conversion in whichever direction is useful for the
dimension you're in, and facing.

**Compass** — a strip showing 90° of arc, with cardinal and intercardinal marks.
Bearing is derived from Minecraft's yaw convention (0 = south, +90 = west), and
marks wrap via shortest signed angular distance so they slide smoothly past 360.

**Telemetry** — rolling frametime and heap graphs. Frametime is measured in the
overlay; FPS is Minecraft's own counter. Heap comes from `java.lang.Runtime`,
which is a **bootstrap class** — no mapping involved, so it works on any client
regardless of mapping set. History is kept overlay-side so the sampler stays
cheap.

**Reach indicator** — brackets around the crosshair when an entity is in range,
with its name, health and distance.

This one is worth explaining: it reads `MinecraftClient.targetedEntity`, which
the game populates itself from the crosshair pick, already clamped to the
player's interaction range. So a non-null value means "in reach" *by the game's
own definition* rather than by a reimplementation of its raycast that would
drift out of agreement with the real hit registration.

## Rendering: Liquid Glass client UI

The menu and launcher now follow the user's requested
[poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11) material:
a rounded dark-blue glass root with backdrop blur, restrained refraction, soft
shadow and light edges. Embedded Inter keeps settings readable. The 900 × 680
menu places Combat/Visuals/Blocks/HUD/Stats/Settings/Misc categories on the left
and their module tiles in one to three columns. Left-click toggles real modules;
right-click opens a separate draggable settings window, up to 480 × 610.
Settings-only tiles also open on left-click; Editor and Panic run their actions.
Both windows clamp to the viewport minus 24 pixels and have scrolling content.
Changing category resets the grid scroll while keeping the selected module's
settings and scroll. Selecting a different module starts its settings at the
top; right-clicking the same module preserves scroll and brings settings forward.
Drag either title bar to move its window; Center restores root placement.
Ctrl-click a slider to enter its value.
The launcher keeps its main start action outside its scrolling content.
See [Liquid Glass integration](docs/liquid-ui.md) and [design tokens](DESIGN.md).

Desktop capture runs only while glass is visible, at a 33-ms cadence. The open
glass window is temporarily excluded from Windows captures to prevent recursive
feedback; closing the menu restores the configured capture preference. Settings
→ Overlay → Menu material reports live or fallback backdrop. Unavailable capture
uses a stable tinted material. Existing HUD/ESP/world effects retain their styles.

**Settings → Keybinds** assigns keyboard/mouse keys, modifier chords and Toggle/Hold
per module. A single modifier is captured on release. Esc cancels capture;
Backspace/Delete clears a module bind. Duplicates are rejected. System defaults:
INSERT/Menu, F6/HUD editor, END/Quit; all module binds initially unbound.
Hold restores the previous module state on release/focus loss. Module hotkeys
pause in menu/editor, and every bind is restricted to Minecraft/overlay focus.

The HUD uses individual position/scale/visibility instead of a fixed panel stack.
HUD settings, binds, new Rockstar feature parameters and waypoints save atomically
in **client-settings.ini** under `%LOCALAPPDATA%/mcov` for the portable client,
or beside mcov_overlay.exe for developer runs. Existing module settings and friends retain their session behavior.
Full usage and implementation: [client UI](docs/client-ui.md).

The backend remains **Win32 + Direct3D 11** with Dear ImGui. Only pixels occupied
by panels/effects are drawn; the surrounding framebuffer stays transparent.
JNI/JVMTI remains in the separate agent, and all UI/HUD rendering consumes the
published WorldSnapshot. This redesign adds no JNI or IPC fields. The scoped
menu/launcher style restores afterward. Existing flat helpers remain available
for HUD/developer surfaces: [flat UI](docs/flat-ui.md).

Combat has eight module tiles: **Kill Aura**, **Aim Assist**, **Triggerbot**,
**Auto Totem**, **Friends**, **Crystal macro**, **Anchor macro** and **Hit sounds**.
Every category uses the same tile navigation; all 76 existing sections remain
reachable. Visuals separates each effect, HUD provides a tile per element,
Blocks separates scanning/types/tracers, and Stats separates player/client/entities.
Settings has Overlay, Focus, Keybinds and Agent. Right-click a tile to configure
its existing controls. Left-click **Kill Aura** to enable it, or use
**Enable Kill Aura** in its settings, then close the menu and use F5. It aims and attacks
the nearest valid living hitbox in reach, including behind the camera, and
keeps that lock until death or loss of sight/reach. Players and mobs are included
by default; friends and occluded entities are skipped. The basic controls are
activation, target type and reach. Rotation always keeps the camera independent.
There is no preset step or selection-mode chooser. See [camera behavior and hooks](docs/silent-camera.md).
Optional live state is under the collapsed **Diagnostics** section;
the debug HUD is off by default. Aim Assist, Triggerbot and Auto Totem each
have their own tiles and settings; Aim Assist and Triggerbot visibly pause while Kill Aura is enabled.
Friends manages UUID exclusions and automatically marks those players with a
translucent conical hat. Its soft violet/turquoise glow uses the Target Aura
colors; it also works with Target Aura and ESP disabled. Removing the friend
removes the hat. UUIDs are cached by Java object identity on the sampler; drawing
adds no JNI calls. Changes apply on the next
client tick. Focus loss, either menu, world exit, shutdown or an expired 250 ms
overlay lease cancels combat. Restart Minecraft after updating the DLL/helper:
the wire protocol is now IPC v19. Existing settings and friends retain their
session behavior; new feature settings and waypoints persist. All seven pages
remain accessible, including the new Misc page.

Visuals has **Trails**, **Hit particles**, **Ambient particles**, **Hit bubbles**,
**Kill effect** and **BreadCrumbs**, inspired by ThunderHack-Recode. Enable effects
with their module tiles; they start disabled and unbound. Right-click opens their
settings. Every particle style has
Aura-style soft glow and an intensity control. Colours follow Target Aura or can
be edited separately. **China hat** enables a transparent glowing hat on your own
sampled head, on in F5 by default. Restart Minecraft and reattach the updated
agent for own head geometry. [Controls and snapshot limitations](docs/world-visuals.md).

Optional -DMCOV_BUILD_UI_CHECKS=ON enables headless UI/rendering checks, including
liquid_glass, flat_ui, client_controls, control_panel, aura_rendering and
depth_rendering. They verify actual D3D11 material/blur shaders, pixels, clipping,
resize, style ownership, settings input, key capture/conflicts, toggle/hold/focus,
preference roundtrip, HUD dragging and small viewports. D3D11 WARP renders
classic-*.png (historical names, current Liquid Glass UI) and modular-hud*.png
under build/ui-preview without opening or attaching Minecraft. Preview backdrops
are deterministic fallbacks; live Minecraft capture is not confirmed by them.
Current native grid/settings review captures are under `.impeccable/review`:
`desktop-grid.png`, `desktop-settings.png`, `compact-grid.png`,
`compact-settings.png` and `large-settings.png`; they use the same WARP fallback.

## Target aura

Entity ESP follows the supplied clean reference: rounded near-white boxes,
thin skeleton lines, a white-to-violet health bar and compact labels. Visuals
also offers a centered target info strip with distance, name, health and the
the selected player's ping when available. Box rounding, line widths, label size
and the health accent are configurable. The renderer uses the existing
coherent snapshot without extra JNI calls. Off-screen reference, health and
edge-case previews are in `build/ui-preview/esp-*.png`.

Nametags now show distance, name, HP and the player's actual tab-list ping.
An equipment row above shows worn armor and both held items with vanilla
inventory icons, durability bars and stack counts. Visuals → Name tags has
separate HP, ping and equipment switches. Missing player info shows `n/a`;
mobs have no player ping. Unsupported/modded icons use a `?` placeholder.
The embedded atlas uses 796 vanilla 26.2 item sprites; it does not read custom
resource packs or render 3D block/item models. Detail sampling runs only in the
agent worker, in a bounded four-entity/3 ms pass every 50 ms with a 300 ms cache
refresh interval per entity. Position/camera capture is published first.

The optional target aura is configured under Visuals → Target aura. Soft orbs
with white cores, violet halos and cyan fog trails orbit the current target in
3D. Every trail point uses the coherent entity/camera snapshot, with no added JNI
calls. Fixed arrays and one reusable Gaussian texture keep the effect inexpensive;
its D3D11 adapter scopes additive RGB blending and preserves coverage alpha.
Full C++ implementation, render-loop example and caveats: [Aura guide](docs/aura.md).
`MCOV_BUILD_UI_CHECKS` also enables `aura_rendering` for real WARP pixels and
allocation/projection checks.

The shareable executable is `build/client/mcov.exe`; developer binaries are in
`build/bin` and previews in `build/ui-preview`. The adapted liquidDX11 runtime
and retained font assets are under `third_party/glass`. The portable EXE embeds
its payload, shader strings, font assets and the glass renderer's MIT notice.
`scripts/build.ps1` always uses `build`, with `-Checks` for the regression suite
and `-Clean` to remove old outputs while retaining dependency/mapping caches.

## Mappings: read this before trusting any field

`generated_profiles.hpp` contains 11 audited profiles for the four requested
releases: Mojmap, older vanilla obfuscated names, Fabric intermediary and Forge
1.20.1 SRG. Minecraft 26.2 ships unobfuscated names. Forge 1.20.1 keeps Mojang
class names but uses SRG member names; newer Forge uses production Mojmap.

The agent reads the game's version ID before binding a profile. Camera/entity
members must bind before sampling starts; optional modules can fail independently.
Unknown releases report `MappingsFailed`. Settings → Agent shows the detected
version and namespace. Generation inputs and test limits are documented in
[docs/compatibility.md](docs/compatibility.md).

---

## JNI lifecycle and classloader handling

Each of these is commented at its site; collected here as a checklist.

**`FindClass` uses the wrong loader.** On a thread you attached yourself,
`FindClass` searches the *system* class loader. Minecraft's classes live under
the launcher's or mod loader's loader, so it throws `NoClassDefFoundError` for
every one of them. JVMTI `GetLoadedClasses` discovers the client, then the agent
pins that client's defining loader and uses `loadClass` for subsequent lookups.
This also resolves classes not yet loaded by Forge or Fabric.

**`AttachCurrentThread` creates a non-daemon thread.** The JVM will not exit
while one exists — Minecraft hangs at shutdown until you detach. `ScopedEnv`
uses `AttachCurrentThreadAsDaemon` instead.

**`DetachCurrentThread` on a thread you did not attach aborts the VM.**
`ScopedEnv` calls `GetEnv` first and only detaches what it owns.

**You cannot detach another thread.** `DetachCurrentThread` only affects the
caller. "Detach all threads on shutdown" necessarily means "signal every worker
and wait for each to detach itself" — `BeginShutdown()` +
`WaitForThreadsDetached()`.

**Local refs are scarce and must be scoped.** The spec guarantees 16; HotSpot
gives ~512 before it aborts. Any loop touching objects needs
`PushLocalFrame`/`PopLocalFrame` — hence `LocalFrame`, used per entity.

**Cached refs must be global.** Local refs die when the frame unwinds, so the
class cache holds `NewGlobalRef` results, and releases them *while still
attached* — deleting a global ref without a valid env corrupts the ref table.

**Almost every JNI call is UB with an exception pending.** Only
`ExceptionOccurred`/`Describe`/`Clear`, `DeleteLocalRef` and a few others are
legal. `CheckAndClearException` clears *first*, then inspects. Note also that
`ExceptionDescribe()` clears the exception as a side effect, which is why it is
not used for "log and continue".

**A pending exception at detach time aborts the VM.** `~ScopedEnv` clears before
detaching.

**`GetStringUTFChars` returns *modified* UTF-8**, not standard UTF-8, and pins
memory until `ReleaseStringUTFChars`.

**Signatures must match exactly.** `getInstance` resolves only when the return
descriptor matches the mapping set in play — hence the descriptor is built from
whichever class name actually resolved.

**JDK 21 warns on dynamic agent loading.** Since Java 21 the target prints
`WARNING: A JVM TI agent has been loaded dynamically`. Harmless; silence it with
`-XX:+EnableDynamicAgentLoading` on the game.

**Iterating the entity list off-thread can throw `ConcurrentModificationException`.**
The sampler runs on its own thread while the client mutates the list. The loop
catches and clears exceptions. Frame markers invalidate a capture if the game
starts another frame or updates the camera during sampling, so a partially
updated entity list cannot replace the previous coherent ESP frame.

---

## Windows API flags the code handles

**`WS_EX_LAYERED`** — required for per-pixel alpha. Without it a transparent
framebuffer just composites against black.

**`WS_EX_TRANSPARENT`** — hit-testing falls through to the game. Toggled *off*
while the menu is open so ImGui can receive clicks.

**`WS_EX_NOACTIVATE`** — stops the overlay stealing focus when it is shown or
repositioned, which would otherwise yank the mouse out of the game every time it
tracked a resize.

**`WS_EX_TOOLWINDOW`** — keeps the overlay out of alt-tab and the taskbar.

**`LWA_ALPHA`, not `LWA_COLORKEY`** — colour-keying discards the framebuffer's
own alpha and gives hard, aliased edges.

**`SetForegroundWindow` fails silently** for a process that does not already own
the foreground. `ForceForeground()` does the `AttachThreadInput` dance that makes
it work — this is what lets the menu take focus from a focused Minecraft.

**`GetAsyncKeyState`, not GLFW key callbacks** — while click-through, the
overlay is unfocused and receives no keyboard events at all, so a toggle key can
only be read from global key state.

**`PROCESS_QUERY_LIMITED_INFORMATION`** — the minimum right that still allows
`QueryFullProcessImageNameW`, and granted between same-user processes without
elevation. If Minecraft runs elevated and the overlay does not, `OpenProcess`
fails and the code says so explicitly.

**`Local\` not `Global\`** for the shared section — `Global\` needs
`SeCreateGlobalPrivilege` (an elevated shell). Same user, same session, so
`Local\` is both sufficient and the only unelevated option.

**`DisableThreadLibraryCalls` + an empty `DllMain`** — joining a thread or
touching JNI under the loader lock is a classic self-inflicted deadlock. All
teardown happens in `OnVmDeath`/`Agent_OnUnload`.

**`Agent_OnUnload` is not reliably called for dynamically attached agents**, so
the agent also registers a JVMTI `VMDeath` callback as the dependable shutdown
signal.

---

## Troubleshooting

### The game crashes when I press INSERT

Fixed — but here is what it was, because the failure mode is instructive.

Minecraft only releases the mouse when it loses focus, so the menu has to take
the foreground. Windows refuses `SetForegroundWindow` from a background process
while the foreground lock is active (default `ForegroundLockTimeout = 200000`),
so the original code used the classic `AttachThreadInput` trick to satisfy it.

That merges our input queue with Minecraft's **render thread**. Focus changes
then propagate through a shared queue into GLFW's `WM_KILLFOCUS` →
`enableCursor()` path, and GLFW is not reentrant there. Result:
`EXCEPTION_ACCESS_VIOLATION` inside `glfw.dll`, on the Render thread, under
`RenderSystem.limitDisplayFPS` → `glfwWaitEventsTimeout`.

The tell is that it only crashes **in-world**. With the cursor captured
(`GLFW_CURSOR_DISABLED`) the focus handler does real work; in menus it does
almost nothing, so the menu opens fine and the whole thing looks like it works.

`Settings → Focus → Focus mode` now picks the strategy:

| Mode | Behaviour |
|---|---|
| **Simple** (default) | Plain `SetForegroundWindow`. Never touches the target's input queue. Often refused by the lock — the menu then draws but won't take clicks. |
| **Unlock foreground** | Clears the foreground lock for one call, then restores it. Minecraft sees an ordinary alt-tab style deactivation. **Use this if Simple won't focus.** |
| **None** | Never take focus at all. |
| **Attach input** | The legacy path that caused the crash. Kept, labelled, off by default. |

### "auto-load failed" / the agent never attaches

Minecraft launchers ship a **JRE**, not a JDK. Modrinth's is
`zulu21.52.15-ca-jre21.0.12` — 51 modules, no `javac`, no `jdk.attach`. Running
the attach helper on the game's own runtime therefore died with
`NoClassDefFoundError: com/sun/tools/attach/AttachNotSupportedException`.

The overlay now searches `MCOV_JDK_HOME` → `JAVA_HOME` → the JDK it was built
against → the game's runtime, and picks the first with `bin/attach.dll` (the
precise signal for attach support; `javac` is only a proxy). The helper does
**not** need to be the same JDK as the target — the attach handshake is
HotSpot's and works across vendors. Verified: a Temurin 21 helper attaches to a
Zulu 21 JRE target.

The helper's stdout/stderr is now mirrored into `mcov_overlay.log` as
`[helper]` lines, so failures name themselves instead of reporting a bare exit
code.

### Status says "mapping lookup failed" on Fabric

Shipped Fabric clients use intermediary names. Separate profiles now cover
1.21.11, 1.21.1 and 1.20.1, translated from official client symbols through the
matching Fabric intermediary artifact. Names, member descriptors and FOV return
types all vary by release. A Yarn development client is outside this audited
runtime list. To regenerate, use the commands in `docs/compatibility.md`.

## Known gaps

Deliberately left unwired, each marked in the source:

- **Ping** — sampled from `getPlayerInfo(UUID).getLatency()` per player in the
  bounded detail worker. Missing player info stays `-1`; nametags show `n/a`.
- **Food / saturation** — one more class chain (`getFoodData()` /
  `getHungerManager()`). Wire it like the FOV chain in `SampleCamera`.
- **FPS** — reads `Minecraft.fps`, a private static that has moved between
  versions. Falls back to `-1`.
- **Projection timing** — sampled off-thread. The actual camera/renderer FOV is
  read using the active partial tick, and vanilla view bob is reconstructed.
  Volatile frame markers reject mixed camera/entity captures, and geometry is
  published before slower HUD/block work. The external window can still present
  a frame behind the game; camera roll, hurt shake and custom
  projection effects need additional adapters. Keep FOV trim at zero for vanilla.
- **Skeleton animation** — world-space joints follow interpolated position,
  body yaw, head pitch, walk state and basic pose for recognised humanoids.
  Item/attack animations, custom model bones and some pose transitions are
  approximated. Box ESP uses the entity's actual AABB for all entity types.
- **Occlusion / visibility checks** — no raycast, so ESP draws through walls.

---

## Verification status

Built with MSVC x64, Ninja and Temurin JDK 21. Overlay, injector, agent and the
Java 17-compatible attach helper compile successfully.

The four official client JARs were checksum-verified and all 75 mapped members
per profile audited. All 11 profiles pass the actual JNI binder/camera sampler
against isolated synthetic classes under `-Xcheck:jni`, including float/double
FOV calls, version rejection, mod-loader-style class resolution, entity geometry,
pose/gait sampling and view bob. Frame synchronization checks pass for future
class loading and dynamic attach to already loaded classes in all 11 profiles,
including repeated attach, camera methods with multiple return paths and
classes without the optional camera getter. Namespace tests check inherited
Fabric method names; connection checks cover world loading and high overlay FPS.
Flat UI and ESP projection/rig checks also pass.
Restart Minecraft when updating an already attached agent to IPC v19. This UI
redesign itself does not change the shared-memory protocol.

Live Minecraft/Forge sessions and actual Java 17/25 VMs have not been exercised
here. See [the compatibility matrix and reproducible checks](docs/compatibility.md).

---

## Third-party

| Component | Licence | Use |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) 1.91.9b | MIT | UI and draw lists |
| [liquidDX11](https://github.com/poncippg-spec/liquidDX11) | MIT | Adapted D3D11 material renderer, backdrop capture and blur shaders in third_party/glass |

The original-source notice remains in `third_party/glass/LICENSE.liquidDX11` and
is embedded as executable RCDATA 103. The renderer is adapted from commit
`00c2b2b19eb5523abde473b2c08220326d4146d4`; the reference demo's recorder,
custom-widget suite and ImGui upgrade are not adopted. ImGui remains pinned to
**1.91.9b** with the existing embedded font assets. See
[third-party notices](THIRD-PARTY-NOTICES.md).

## Scope

Written for single-player, private servers you control, and modding/tooling
research. Using it on public multiplayer servers violates essentially every
server's rules and Minecraft's EULA.

Chunkfinder, Place/Use-driven Crystal/Anchor cycles and the Auto Totem client-tick fix are documented in [item assist](docs/item-assist.md).
