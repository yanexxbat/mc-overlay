# mcov — external overlay for Minecraft Java Edition

C++20 / Dear ImGui overlay with a JNI/JVMTI agent supplying live game state.
Runtime profiles cover **26.2, 1.21.11, 1.21.1 and 1.20.1**, including Forge's
version-specific member names. See [compatibility and verification](docs/compatibility.md).

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
The markers only update a volatile epoch; game-data queries remain in the worker.

`JNI_GetCreatedJavaVMs` *is* still implemented, in
`jvm::Runtime::DiscoverVm()`. It runs inside the agent, where it is meaningful,
and it dynamically resolves `jvm.dll` exactly as specified. The normal path
(`AdoptVm`) is cheaper because `Agent_OnAttach` already hands us the `JavaVM*`.

---

## Layout

```
CMakeLists.txt              overlay, injector, agent + FetchContent for ImGui
third_party/glass/          retained vendor/font assets; glass renderer is not built
scripts/build.ps1           configure + build wrapper
tools/AgentLoader.java      JVM attach helper (jdk.attach)
tools/frame-sync/           Java frame markers and audited bytecode transformer
src/
  main.cpp                  overlay entry point
  common/
    flat_style.hpp/.cpp     flat theme, shadows, module panels, scoped styles
    flat_app.hpp/.cpp       shared flat Win32/D3D11 injector shell
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

Artifacts land in `build/bin/`: `mcov_overlay.exe`, `mcov_injector.exe`,
`mcov_agent.dll`, `tools/AgentLoader.class`, `tools/frame-sync-agent.jar` and
`tools/frame-signal.jar`. **Keep the complete folder together** — the loader
installs both frame helpers before starting the native sampler.

Loading into a game requires an attach JDK matching the **running Java major**:
normally 25 for 26.2, 21 for the 1.21 releases and 17 for 1.20.1. The loader
skips wrong-major candidates; `MCOV_JDK_HOME` can point to a custom installation.

---

## Run

1. Start Minecraft and get into a world.
2. Run `mcov_overlay.exe`.
3. Press **INSERT** for the menu, **END** to quit.

The overlay finds the game, loads the agent, and connects. Pass `--no-agent` to
skip the automatic load and do it from the Settings tab instead.

---

## HUD

Always-on panels, independent of the INSERT menu. All toggleable under the
**HUD** tab, and anchorable to any corner (panels report their own height, so
hiding one closes the gap rather than leaving a hole).

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

## Rendering: rounded flat UI

The overlay and injector use a matte, opaque Dear ImGui theme: background
#121214, cards #1E1E24, hover #2A2A32, accent #5865F2, text #ECECEE.
Window/frame/popup/grab/scrollbar rounding is 10/6/8/5/6 pixels.

The common/flat_style.hpp/.cpp files own the theme, five-layer draw-list shadows,
scrollable module cards and scoped style changes. See [the flat UI guide](docs/flat-ui.md)
for C++ examples and the ownership rules for custom headers and buttons.

The current backend is **Win32 + Direct3D 11**. The new UI helpers use only ImGui
and are also usable with OpenGL 3.3; this redesign retains the existing backend.
The liquid-glass library, Desktop Duplication, blur and refraction passes are
removed from both UI targets. Inter's existing embedded font assets remain in use.

The framebuffer outside UI panels stays transparent so the game remains visible.
WS_EX_LAYERED and DwmExtendFrameIntoClientArea provide that compositing;
panels and controls themselves are opaque. Capture exclusion is now an independent
recording preference and defaults to off.

JNI/JVMTI sampling remains in the separate agent DLL. Rendering consumes the
published WorldSnapshot through shared memory; the redesign adds no JNI work.
The overlay menu reproduces the user's screenshot, saved in
[design/reference-ui.png](design/reference-ui.png). Its native implementation
lives in `src/overlay/control_panel.hpp/.cpp`: separate dark main panel and
right navigation, two compact setting columns, square gold checkboxes, thin
gold sliders, small dropdowns and clipped circular linework. The reference's
681 x 382 menu footprint scales to the game viewport, up to 1.5 times its
reference size. Narrow viewports stack settings with scrolling.

AIMBOT opens the existing combat modules, VISUALS entity rendering, LOOT block
ESP, and HUD equipment and overlays. The three small icons open player stats,
appearance/menu actions, and overlay/agent settings. Appearance includes the
four existing palettes, compact navigation, page reset, connection status and
menu closing. Font Awesome icons are embedded as an executable resource.
Aim/trigger activation keys and target filtering are bound to CombatSettings;
the reference's unrelated game features are not added. All six original pages
remain accessible. The injector retains its existing flat interface.

The menu uses the existing Win32/D3D11 backend and pinned ImGui version.
Its palette and smaller fonts are scoped to the menu, leaving the HUD intact.

Optional -DMCOV_BUILD_UI_CHECKS=ON enables headless `flat_ui` and `control_panel`
checks for style ownership, real settings binding, navigation input and small
viewports. The latter renders the actual menu with D3D11 WARP to PNG files in
`build/ui-preview` without opening a window or connecting to Minecraft.
`reference-size.png` uses the supplied image's 752 x 427 viewport; `combat.png`
shows the desktop version. The check also drags the real aim-cone slider and
verifies that the embedded icon glyphs are available.

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

`Settings → Focus mode` now picks the strategy:

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

- **Ping** — needs `getConnection() -> getPlayerInfo(UUID) -> getLatency()` plus
  the local UUID. Published as `-1` so the UI shows "not sampled" rather than a
  fabricated zero.
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

The four official client JARs were checksum-verified and all 70 mapped members
per profile audited. All 11 profiles pass the actual JNI binder/camera sampler
against isolated synthetic classes under `-Xcheck:jni`, including float/double
FOV calls, version rejection, mod-loader-style class resolution, entity geometry,
pose/gait sampling and view bob. Frame synchronization checks pass for future
class loading and dynamic attach to already loaded classes in all 11 profiles,
including repeated attach, camera methods with multiple return paths and
classes without the optional camera getter. Namespace tests check inherited
Fabric method names; connection checks cover world loading and high overlay FPS.
Flat UI and ESP projection/rig checks also pass.
Restart Minecraft before using shared-memory version 9 with the rebuilt binaries.

Live Minecraft/Forge sessions and actual Java 17/25 VMs have not been exercised
here. See [the compatibility matrix and reproducible checks](docs/compatibility.md).

---

## Third-party

| Component | Licence | Use |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) 1.91.9b | MIT | UI and draw lists |
| [liquidDX11](https://github.com/Pondot/liquidDX11) | MIT | Retained vendor/font assets in third_party/glass; renderer not compiled |

The existing vendor files and third_party/glass/LICENSE.liquidDX11 remain in
the tree. Only data/fonts.h is consumed by the flat UI; no glass shaders or
widgets are linked. ImGui remains pinned to **1.91.9b** to retain the current
API and font atlas behavior.

## Scope

Written for single-player, private servers you control, and modding/tooling
research. Using it on public multiplayer servers violates essentially every
server's rules and Minecraft's EULA.
