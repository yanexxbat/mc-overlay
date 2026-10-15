# Freecam

Enable **Misc → Freecam**; right-click for settings and its initially unbound
Toggle/Hold key. Flight speed is 1–40 m/s (default 8); Ctrl boost is 1–10×
(default 3). WASD follows the camera's yaw and pitch, Space rises, Shift descends,
and the mouse controls look. Combined movement is normalized.

The real player stays in place. Minecraft attack, continuous block breaking and
item use are suppressed; Kill Aura, Spinbot, external aim/trigger assist and
item assists pause. The player's normal tick and network connection remain
active. Disabling, menus, focus loss, world/player changes, unload and the
250 ms client lease release the camera. Resuming starts at the current player.
Freecam can move through blocks but only displays already loaded chunks.

Automatic preferences retain speed, boost and binding with Freecam off on
startup. Named configs also retain its enabled state. Complete older profiles
receive disabled/unbound defaults; partial or invalid Freecam fields reject
the profile before application. Panic disables Freecam with the other modules.

## Runtime

The existing client-tick JNI bridge reads the leased request, advances flight
using elapsed time and publishes coordinates to `FrameSignal`. Its existing
mouse hook separates view angles from entity angles. Camera hooks apply a
linearly interpolated position through Minecraft's own `setPosition`, update
rotation through `setRotation`, and set detached mode for the player model.
The renderer's hand pass is separately skipped while Freecam is active.
Minecraft overwrites the camera normally on the next setup.
For 26.2 the hook runs at the end of `alignWithEntity`, before camera culling
and cached matrices are prepared; earlier profiles use `Camera.setup`.
No render-frame JNI callback is added.

While the Java lease is live, the local player's `aiStep` is skipped; other
entities keep normal updates. Client `startAttack`, `continueAttack` and
`startUseItem` return early. All hooks use signatures generated from locally
checksum-verified official clients for the eleven existing profiles (26.2,
1.21.11, 1.21.1, 1.20.1 and their supported mapping namespaces).
Missing camera, mouse, local-step or interaction hooks prevent activation and
show a restart diagnostic. IPC v24 requires a full Minecraft restart before
using the new agent and helper JARs.

## Verification limits

Core tests exercise camera-relative movement, pitch, diagonal normalization,
boost, elapsed time, invalid parameters and named-profile migration. Java
fixtures verify real transformed camera setters, mouse separation, local-only
movement freezing, blocked/restored interactions, lease expiry and dynamic
attach to already loaded classes. JNI profile fixtures verify Freecam priority,
movement, menus/focus pause, stale requests, world exit and missing-hook gating.
Native ImGui fixtures exercise the module toggle, settings, automatic preference
persistence and Panic, with desktop, compact and missing-hook captures.

Fixtures contain no Minecraft code. No live Minecraft attach, rendering or
server check was performed. Mods that replace camera or player movement,
server corrections, visibility through terrain and chunk loading remain
live-game verification limits.

All 29 regression checks passed across the final verification runs. The dynamic
attach check passed separately outside the sandbox after Windows denied access
to its disposable fixture pipe. The final focused run passed the Freecam core,
all 22 JNI profiles, frame hooks, depth hook, native menu and portable package.

### Attach correction

The first Freecam build failed Java verification when a camera return was inside
an exception scope with live local variables. Its injected branch described only
the method parameters; an existing handler could still require the omitted
locals. The transformer now tracks the actual local-variable state with the
JDK's ASM `AnalyzerAdapter`, including two-slot long/double values, and preserves
it in each inserted stack-map frame. Unreachable returns are left alone.

The synthetic cameras now retain object, long and double locals in a surrounding
handler. This reproduces the original `VerifyError` with the old transformer and
passes with the correction, including attach to already loaded classes. The
updated helper is embedded in the rebuilt portable client. Fully restart
Minecraft before retrying, since a failed Java-agent initialization can leave
the old helper classes loaded. Live Minecraft reconnection remains unverified.

The installed Fabric 1.21.11 classes exposed a second case: the original camera
stack maps discard all locals, including the receiver, immediately before the
terminal return. A return hook cannot load slot 0 from that frame. The first
transformer pass now proves that the camera method never overwrites receiver
slot 0; its original frames then retain the receiver type. Methods that reuse
that slot remain ineligible for Freecam. Both handler locals and terminal
receiver chopping are covered by the synthetic cameras.

`tools/diagnostics/RealFrameVerify.java` additionally verifies the installed game
classes without initializing Minecraft. With a file containing local dependency
JAR paths (one per line), it can either verify transformed definitions or attach
the helper to a separate JVM containing already verified game classes. The
installed intermediary 1.21.11 client, Camera, Entity, LivingEntity, Player,
LocalPlayer and GameRenderer passed both verification and dynamic attach after
the receiver correction. These checks do not launch Minecraft, apply the
profile's mixins or establish live-game rendering behavior.
