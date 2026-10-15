# Minecraft versions and Forge

The external overlay and native agent now contain explicit profiles for these
exact releases. They select the profile using `SharedConstants.getCurrentVersion()`
and the game's `WorldVersion` ID, rather than guessing from the window title.

| Minecraft | Game / attach Java major | Forge runtime profile | Other profiles |
|---|---:|---|---|
| 26.2 | 25 | `mojmap-26.2` | Same unobfuscated names in vanilla |
| 1.21.11 | 21 | `mojmap-1.21.11` | Vanilla obfuscated, Fabric intermediary |
| 1.21.1 | 21 | `mojmap-1.21.1` | Vanilla obfuscated, Fabric intermediary |
| 1.20.1 | 17 | `forge-srg-1.20.1` | Mojmap development clients, vanilla obfuscated, Fabric intermediary |

Forge 1.20.1 uses Mojang class names with SRG field/method names (`f_â€¦`, `m_â€¦`).
Forge's production Mojmap transition began with Minecraft 1.20.6 / Forge 50.
Minecraft 26.1 and later ship unobfuscated classes and use Java 25.
Sources: [Forge maintainer](https://forums.minecraftforge.net/profile/150347-jonathing/content/page/2/?type=forums_topic_post),
[Minecraft 26.1 technical changes](https://www.minecraft.net/en-us/article/minecraft-java-edition-26-1),
[Forge 26.2 downloads](https://files.minecraftforge.net/net/minecraftforge/forge/index_26.2.html).

## Run

1. Restart Minecraft when replacing a previously loaded agent DLL. Windows keeps
   that library mapped even after sampling stops.
2. Keep `mcov_overlay.exe`, `mcov_injector.exe`, `mcov_agent.dll` and
   `tools/AgentLoader.class`, `tools/frame-sync-agent.jar` and
   `tools/frame-signal.jar` together in `build/bin`.
3. Start Minecraft with the desired version and Forge; use the injector or the
   overlay's existing attach action. This remains an EXE/DLL tool; installation
   does not use a Forge mods-folder JAR.
4. Open **Settings â†’ Agent** to see the detected Minecraft version and profile.

The attach helper needs a full **64-bit JDK with the same Java major as the
running game**. Discovery checks `MCOV_JDK_HOME`, `JAVA_HOME`, the build JDK,
the game runtime and common installed-JDK directories. Wrong-major candidates
are skipped. For a custom installation, set `MCOV_JDK_HOME` to its JDK root before
starting the injector. No launcher setting or system Java installation is modified.

The helper is compiled with `--release 17` (class version 61), so the same helper
file works with Java 17, 21 and 25. A build JDK 17+ is enough to compile the native
agent; the runtime attach JDK is selected independently.

## JNI and camera changes

After discovering the client via JVMTI, the agent pins its defining classloader.
Subsequent lookups use that loader's `loadClass`, which supports Forge/Fabric
classes that have not been loaded yet. The references and member IDs are cached.
Virtual method names also resolve through superclass/interface declarations in
the namespace mappings. In Fabric 1.21.11, `Camera.position()` inherits
`method_71156` from the tracked-waypoint camera interface; its raw name `b` is
not the runtime name. The same resolution handles inherited entity/bounds and
inventory methods instead of silently retaining their obfuscated names.
Core camera/entity version detection, binding and sampling run in the native
daemon worker. The external D3D11 renderer reads the shared POD snapshot and
never calls JNI. Base texture extraction has a separate game-render callback,
described below.

- 1.20.1 / 1.21.1: renderer `getFov(Camera,float,boolean)` returns **double**.
- 1.21.11: renderer FOV returns **float**; camera accessors are `position`, `yRot`, `xRot`.
- 26.2: renderer `mainCamera()`, camera `getFov()` with no arguments, active screen
  moved to `Minecraft.gui.screen`, container actions use `ContainerInput`.

Camera and entity bindings must succeed before a profile becomes active. Unknown
versions fail with a mapping diagnostic. Container IDs use the selected profile,
and the existing inventory action stays disabled if its screen guard cannot bind.
Shared-memory version is now **24**; restart Minecraft and use the newly built
agent, injector and overlay together. An already loaded DLL keeps the old layout.

Entity nametags use audited UUID â†’ PlayerInfo â†’ latency lookups and living-entity
equipment slots for every supported profile. Optional detail sampling is cached,
bounded and follows geometry publication. Missing player info remains unknown;
it is never replaced with the local client's ping. World changes clear the cache.

## ESP geometry

The sampler reads the actual AABB and render interpolation coordinates
`xOld/yOld/zOld`, rather than the different `xo/yo/zo` position history. Camera,
entity positions and renderer FOV share the same partial-tick fraction:

- 1.20.1: `Minecraft.getFrameTime()`.
- 1.21.1 / 1.21.11: `Camera.getPartialTickTime()`.
- 26.2: `Camera.getCameraEntityPartialTicks(Minecraft.getDeltaTracker())`.

All eight bounding-box corners are projected, with intersections at the near
plane. Depth, perspective, pitch and off-center entities affect box width and
height naturally. Double-precision camera subtraction prevents loss of precision
near the world border. Skeleton segments are projected individually and clipped
to the viewport. DPI awareness keeps the overlay aligned with game client pixels.
Skeleton, health-bar, distance-label and block-only rendering work independently
of the box/tracer/name toggles.

Players, zombies, skeletons, piglins, endermen and armor stands receive a humanoid
rig. Body yaw uses wrapped interpolation, limb angles use the sampled walk state,
and standing model height is independent of the current pose's hitbox height.
Crouching, riding and horizontal swimming/gliding/sleeping poses are represented.
Other entity types retain hitbox ESP without a fabricated humanoid skeleton.
The skeleton is a reconstruction: item/attack poses, renderer offsets, baby head
scaling, custom models and pose transitions may differ from the game's exact bones.

View bob uses the sampled camera entity's walk distance and bob amplitude. The
overlay applies the game's translation and Z/X rotations before camera rotation.
Modern profiles read `ClientAvatarState`; older profiles read Entity/Player fields.
Damage camera shake, camera roll and custom projection effects are not reconstructed.
Camera/entity visual sampling uses JNI in the sampler; the external renderer
only consumes POD snapshots.
The combat adapter has a separate, instrumented client-tick callback.

## Frame synchronization

The attach helper installs the Java frame agent before the native DLL. It adds
pure Java volatile epoch writes at `Minecraft.runTick(boolean)` entry and at
camera update entry/return. Camera/entity reads and their JNI calls remain in
the daemon sampler. The transformer uses the target JDK's bundled ASM, the audited version
signatures and an additional accessor/position-field signature to guard against
reused obfuscated class names. The optional camera getter is not required for
frame marking. Forge game modules receive access to the bootstrap
signal class. Classes that are already loaded are retransformed; future loads
use the same transformer.

An odd epoch means the camera or game frame is being updated. The sampler reads
the epoch before camera/entity capture and verifies it afterward; only a stable
even epoch can replace the previous ESP snapshot. Camera and entities therefore
share the same partial-tick timeline instead of mixing a completed camera with
entity coordinates from the next tick.

Geometry is published before equipment, effects, telemetry and block scans.
Slow HUD sampling runs every 100 ms; a high-resolution worker timer replaces the
fixed 16 ms sampling delay. The overlay also limits queued D3D11 frames to one.
This removes sampling drift, but independent game/overlay presentation still
allows a display-frame delay. There is no screen-space smoothing or extrapolation.

The worker also sends a heartbeat every 100 ms while waiting for coherent
geometry. World entry or camera loading therefore displays `waiting for camera
frame` instead of marking an active worker disconnected. Overlay liveness uses
a two-second clock timeout, independent of overlay FPS; it no longer counts 180
rendered frames without a snapshot. Skipped geometry is still not published.

Use the updated injector/attach helper and restart Minecraft after replacing
these files. Loading the DLL directly without the Java helper retains the weaker
camera double-read fallback and logs that frame synchronization is unavailable.

## Verification and limits

- All 24 class roles and 75 mapped members per profile were checked against the
  SHA1-verified official client files for all four releases. Namespace mapping
  artifacts are pinned by SHA256. See `compatibility-signatures.json`.
- MSVC x64 build passed for overlay, injector, native agent and helper.
- All 11 runtime profiles passed the actual binder and camera sampler under
  `-Xcheck:jni` in the installed Temurin Java 21 VM, using isolated synthetic
  classloaders and Java 17 fixture bytecode. Tests cover version rejection,
  defining-loader lookup, all three FOV call forms, rotations, position accessor
  and field-only camera classes, entity interpolation, actual AABB offsets, body/head angles,
  standing dimensions, pose enum identity, walk animation, view bob and Java-major
  helper selection. No game code is in fixtures.
- Flat UI regression checks passed.
- Java frame-marker checks passed for all 11 profiles in Java 21: future class
  loading, both camera return paths, dynamic attach after classes are loaded and
  repeated attach, including 11 additional classes without a camera getter.
  The native sampler's epoch checks also pass under `-Xcheck:jni`.
- Independent namespace inheritance tests cover the Fabric camera interface,
  private/static exclusions and unmapped methods. Liveness checks cover world
  loading at high overlay FPS, heartbeat wraparound and a stopped worker timeout.
- ESP geometry checks passed for distance scaling, off-center AABBs, steep camera
  angles, near-plane/viewport clipping, world-border precision and humanoid poses.

These checks validate signatures and JNI behavior. **Live Minecraft/Forge runs
and actual Java 17/25 VMs have not been tested here.** Forge mods that replace game
classes, development clients using Yarn names, snapshots and other releases are
outside the audited profile list. Before treating a loader/version combination as
tested in-game, check world entry, F5 camera modes, sprint/zoom FOV, equipment and
effects, dimension changes, a container screen and normal game shutdown.

Kill Aura additionally audits client tick, player rotation setters, eye position,
alive/LOS checks, attack-strength cooldown, interaction reach, block pick,
game-mode attack and hand swing from the official jars. All eleven profiles pass
the instrumented native combat adapter fixtures under `-Xcheck:jni`, including
automatic targeting/rotation/attacks, cooldown reset/rearm, UUID friends,
occlusion, Minecraft GUI, expired input lease and respawn. Silent-camera fixtures
also check that the player rotates while camera angles and a derived F5 orbit
remain steady through target switches, mouse look changes only the view,
foreign entities keep their original look and partial ticks stay stable.
Menu pause and disabling restore normal look without stale interpolation;
missing Camera hooks block Silent output. The transformer
wraps `Entity.getViewYRot/getViewXRot` calls inside Camera, including 26.2's
private `alignWithEntity`, and filters mouse deltas at `Entity.turn`.
These are synthetic call-site tests, not a live Minecraft renderer test.
See [silent camera](silent-camera.md). JNI for combat runs on the client tick;
camera/ESP sampling stays in
the daemon worker and the external renderer consumes IPC only. The helper and agent now
require IPC v22 and a full Minecraft restart after upgrading. Spinbot shares the
existing silent camera hooks. Its movement correction uses audited Entity,
LivingEntity, Player and LocalPlayer physics scopes; only getters inside those
scopes use camera yaw/pitch. Model/network getters outside keep the spin angles.
All eleven profiles test forward/sideways/diagonal acceleration, sprint-jump yaw,
travel pitch, nested/exception cleanup, foreign entities and pause/priority gates.
Dynamic attach also retransforms already loaded movement methods. Spinbot stays
disabled if camera or movement hooks are unavailable. These fixtures do not
replace movement testing in a running Minecraft client.

## SMP block scanning

Storage ESP, Stash Finder and Spawner ESP use the existing runtime profiles and
the worker's block-state sampling. IPC v22 adds distinct storage types and
coherent completed-scan metadata; restart already running Minecraft before
reattaching with the updated portable client or matching development binaries.
The overlay, injector and loaded agent must use the same shared-memory version.

The scanner reads loaded client-world blocks in a 4â€“48 block-radius cube and
currently visits Y = âˆ’64 through 319. Results are limited to 1024 matching blocks
across active block modules; truncated scans report lower-bound counts. Stash
Finder evaluates successful completed scans, records at most 128 session/world-
scoped discoveries and retains the highest observed count per chunk. It does
not read inventory contents. Spawner mob type and activation state are not sampled.
See [SMP features](smp-features.md) for controls, counting and persistence.

All 24 available regression checks passed across the final SMP runs. The
`frame_attach` check passed outside the sandbox after Windows denied access to
its disposable fixture JVM's pipe. SMP classification/state/profile/rendering,
native settings interaction and portable-payload checks passed. The six native
D3D11 WARP settings captures under `.impeccable/review/smp/` use synthetic data;
the independent finish review checked all six final captures and returned
**ship** after the caption-toggle fix. The production EXE and ZIP were rebuilt.
**No live Minecraft scanning, appearance or attach test was performed for SMP.**

## Base texture rendering (1.21.11)

**Base ESP â†’ Minecraft textures** is the default display, with texture opacity
1. It supports Mojmap, vanilla obfuscated and Fabric intermediary **1.21.11**.
The texture bridge's 38 classes and 73 mapped members were audited against
cached official 1.21.11 classes. Other supported versions retain the optional
**Area marker** display; the settings report an unsupported texture version or
missing renderer rather than substituting fabricated textures.

The existing detector still estimates storage-cluster bounds plus padding;
it does not trace an entire building. A dedicated JNI callback on the game's
render thread invokes the live game's block, liquid and sprite-based block-
entity renderers. Its Java reflection bridge captures quads, tint, light values
and atlas UVs. Native OpenGL draws the result with independent base depth,
then composites it into the main colour target while preserving graphics state.
The current game atlas and lightmap are sampled live. External UI/HUD rendering
stays outside the JVM.

Extraction targets 2 ms per slice and checks at least one cell; a single
expensive block can exceed that budget. Each rebuild is bounded to eight nearby
regions, 16,384 checked cells and 786,432 vertices. Overlapping areas are
deduplicated. Completed geometry is rebuilt after about one second. Requests
expire after 750 ms without renewal and are scoped to server and dimension.
Special text, portal and custom-renderer submissions are skipped and reported.
The menu reports loading, empty results, limits and failures, with toggle-to-retry
and Area marker recovery. **IPC v22 requires a full Minecraft restart after
upgrading** because an already loaded DLL retains the old layout.

All 26 available CTest checks passed across the final runs: 25 in the sandbox,
then `frame_attach` separately outside it after Windows fixture-pipe access
failed. After the final resource-release changes, `base_texture_gl`,
`base_texture_models`, `smp_features` and `portable_client` passed again;
`depth_hook` passed in the preceding focused run. The real WGL/OpenGL pixel/state
check covers atlas UVs, camera projection,
base self-occlusion, through-world compositing, live atlas updates, restored
state, opacity 0.5, translucent texel blending and renderer reinitialization
after resource release. Disabling Base ESP releases GL resources so off/on
can recreate a failed renderer. Java model fixtures cover
proxy defaults, quad UV/tint/light capture and sprite/model paths; the other
checks cover both render callbacks, mode/profile migration, leased world-scoped
requests and the embedded portable bundle.

The final five `-verified.png` settings captures under
`.impeccable/review/base-textures/` are native ImGui/D3D11 WARP synthetic fixtures
at 1280 Ã— 800 and 480 Ã— 360. The independent finish review returned **ship for
the UI scope**. Earlier `.impeccable/review/base-esp/` evidence covers the
coloured Area marker only. The EXE and ZIP were rebuilt. **No live Minecraft
texture, appearance or attach test was performed; shader and mod graphics
compatibility remains unverified.**

## Reproduce the checks

The normal build needs no game downloads. Optional regression tests need Python
3.10+ and a full test JDK:

```powershell
cmake -S . -B build -DMCOV_BUILD_UI_CHECKS=ON -DMCOV_BUILD_COMPAT_CHECKS=ON -DMCOV_BUILD_ESP_CHECKS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Fixture JARs are generated automatically from the checked-in profile table.
To audit/regenerate the tables from official inputs (downloads about 150 MB):

```powershell
python tools/compatibility/fetch_inputs.py build/compat-cache
python tools/compatibility/generate_profiles.py build/compat-cache
```

The generator reads signatures and inheritance without executing Minecraft code.
It stops on missing symbols, wrong static/instance kinds or checksum failures.
