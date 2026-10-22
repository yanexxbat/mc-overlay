# Keim

## Platform

Native Windows x64 desktop overlay attached to the Minecraft Java client window.

## Stack

C++20, Dear ImGui 1.91.9b, Win32 and Direct3D 11. A separate JNI/JVMTI agent
publishes coherent world snapshots and executes existing client-tick actions
through IPC v31. The game's OpenGL renderer and the overlay's D3D11 renderer are
separate. UI rendering stays outside the JVM; textured Base ESP uses a dedicated
JNI callback on the game's render thread.

## User and purpose

The user configures Minecraft visual/combat modules, observes game information,
and operates features through an external menu and keyboard/mouse bindings.
Success means working features with clear controls, a modular HUD, and preserved
camera/entity projection stability.

## Confirmed constraints

- The product is **Keim**, with the lowercase **keim** wordmark and the user's
  green germ mascot in the launcher, client menu, HUD watermark and Windows icon.
  The portable deliverables are `Keim.exe` and `Keim-client.zip`. Preserve the
  approved compact anthracite/lime-green controls; the shared #8EDB1A accent matches the mascot. Bright primary actions
  use dark green-black text; errors stay red and warnings amber.
  Legacy IPC/JNI identifiers, settings headers and the configured KeyAuth backend
  application `mcov` remain compatible with existing installations and licenses.
- The latest client layout follows the user-supplied Paint sketch,
  `design/reference-module-widgets.png`: narrow left categories and searchable
  module widgets, with right-click opening settings in an independent window.
  All settings remain reachable through 76 module tiles, including 17 in Visuals.
  Left-click or Space operates the real
  toggle/action; settings-only widgets open their controls. Menu / Shift+F10
  opens settings without toggling. The approved compact translucent anthracite,
  lime-green, embedded Inter and rounded fills remain the visual identity.
  This replaces the module tabs, inline settings and Quick toggles.
- The injector uses the same approved compact lime-green design as the client:
  scoped `CompactStyle`, embedded Inter at 14 pixels, an 88%-opaque anthracite
  root and a narrow Start/Logs sidebar. It performs Minecraft selection,
  connection and overlay startup. The fixed primary action and close footer
  remain reachable while status, JDK recovery and diagnostics scroll.
- All app-owned injector and overlay text is English, including connection
  states, package errors, debug HUD and named keys. Printable key legends follow
  the keyboard layout. Minecraft titles, player names and user content retain
  their original text. The portable quick-start guide uses the same English labels.
- The 19 snapshot HUD elements and optional Combat debug HUD use the same
  compact anthracite/lime-green design and embedded Inter at 14 pixels. Rounded fills,
  quiet headings and semantic green/amber/red feedback preserve readability.
  The F6 editor retains movable/scalable elements, visibility, reset and persistence.
- Keep the simplified Kill Aura workflow and independent mouse-controlled camera.
- External UI/HUD renderers consume snapshots; their presentation adds no
  render-thread JNI. Base texture extraction has its own game-render callback.
- Keep one canonical `build` directory and existing version/Forge compatibility.
- Hole ESP sits under Blocks, using independent bounded short-range block scans
  for 1x1 bedrock/obsidian/mixed holes with three air blocks of clearance. Preserve
  native settings, type colours, profile/preferences, unbound defaults and Panic.
- Ship one portable `Keim.exe` containing the client payload. The optional
  `START.txt` guide is the only other file in the share ZIP.
- Require license-only KeyAuth 1.3 sign-in for mcov / Uhn5Z5WPCa / 1.0 before
  startup. Preserve the compact English launcher, masked input and background
  verification. Never persist or log license keys. Validate signed responses,
  owner, subscription expiry and session before launch and independently in
  the overlay; periodic online checks close the overlay on invalidation.
  Session handoff uses a restricted inherited pipe; direct unauthenticated
  overlay startup is refused. Minecraft remains running on license failure.
- Keep the current KeyAuth HWID visible in the launcher's fixed footer, before
  login and on both tabs. Provide a read-only field and **Copy HWID** for support,
  using the existing Windows account SID without logging or persisting it.
- Module keybinds start unbound, as explicitly chosen by the user. Menu INSERT,
  HUD editor F6 and Quit END are defaults; all can be rebound.
- Named profiles and automatic preferences live directly beside the portable
  client. **Settings Ã¢â€ â€™ Configs** saves, explicitly overwrites and loads profiles;
  loading restores saved module states only after full validation. No named
  profile is selected or loaded automatically at startup.
- Support keyboard/mouse bindings, modifier chords, Toggle/Hold operation and
  movable/scalable HUD elements. All existing sections remain reachable through
  category module widgets and their independent settings controls.
- Preserve the clean rounded entity ESP, configurable Target Aura and Target
  Marker, soft particles/trails and the glowing friend/self hats.
- Visuals groups entity controls in one **Player ESP** tile with its own master
  toggle and initially unbound keybind. Its settings selector contains Boxes,
  Skeleton, Name tags, Health / armor, Distance, Tracers, Target info,
  Entity filters / colours, Glow and Offscreen arrows. The master suppresses
  these visuals while retaining component choices; other cosmetic tiles remain
  separate. Automatic preferences and named profiles retain the master and
  component settings. Legacy profiles default the master to enabled, and existing
  binding indices remain compatible.
- SMP controls extend the existing **Blocks** category: Storage ESP selects
  13 storage block types with independent RGBA colours, labels and tracers.
  Transparent full-cell gradients and soft clipped outlines use each container
  type's colour. **Container textures** optionally adds the through-wall Minecraft
  texture pass on 1.21.11; it defaults off, including for older configs without
  this field. Disabling it clears the texture request immediately while the
  coloured gradient boxes remain. Both preferences and named profiles save it.
  Opacity controls boxes and textures; Soft glow adjusts the front-edge halo
  using the existing saved glow setting. Type colours also apply to annotations.
  Known matches persist during rescans; discoveries and removals fade
  progressively. Scan pause (200-3000 ms) persists; legacy pulse fields
  remain compatible without adding scan bands or a sequential pulse effect.
  Both scan and presentation are bounded to 1024 matches. Storage ESP uses its
  own 4-512 radius and a 1 ms loaded-chunk
  block-entity traversal; ordinary Block ESP/Chunkfinder sweeps cannot delay it.
  Preserve known results and type colours. Audit existing JNI signatures
  across all runtime profiles, retain a bounded compatibility fallback and save
  the extended radius in both automatic preferences and named configs.
  Stash Finder records container clusters by chunk; Spawner ESP has its own
  colour, labels and tracers. Right-click opens the same independent settings
  window. These controls preserve the incumbent compact visual system.
- Base ESP independently enables the shared container-cluster detector. Its
  default 1.21.11 display uses actual game block models, atlas textures and
  lightmap through walls, with independent depth for the base itself. The
  optional Area marker keeps the coloured fill/frame on other supported profiles.
  Texture opacity, appearance and the unbound module key persist. Scan radius,
  minimum and filters are shared with Stash Finder. Detection estimates storage
  extent plus an adjustable margin; it does not trace entire building bounds.
- Chunkfinder contains the former Base Finder: raw horizontal deepslate, tall
  kelp and large amethyst buds/clusters have independent per-chunk thresholds.
  Block ESP and Chunkfinder allow scan radii of 4Ã¢â‚¬â€œ512 blocks and draw distances
  up to 1024 m; increasing a radius raises draw distance as needed. Keep existing
  default radii and the shared 2 ms sampler slices / 1024-result cap. Traverse
  nearby columns and the player's height first, clipping invalid world heights
  before traversal and keeping cursor/progress arithmetic wide. Chunk evidence
  still requires completed successful scans. Only locally loaded blocks exist.
  One master toggle and scan/appearance controls serve all three clue types.
  Both full 16x16 chunk-column frames and individual evidence-block boxes are
  on by default and independently configurable. Markers remain visible through
  walls. Vanilla column height is used; custom dimension height is not sampled.
  Session results retain reasons, coordinates, copy and waypoint actions.
  Legacy growth profiles and bindings remain compatible. Natural clues can
  produce markers, so detections are never confirmation.
- Misc > Anti Vanish adapts the Anubis TAB/invisible-player hints on 1.21.11.
  The client-tick sampler runs at 2 Hz with bounded identities/results, a trust
  window and confirmation grace. It reports possible evidence, not confirmed
  staff. Alerts reuse Notifications HUD and Name protect. No render geometry or
  outgoing probes are added. Preferences/profiles retain choices and binding.
- First-person knife cosmetics, inspect animations, blade trails and knife glow
  have been removed. No item-render hook or knife geometry ships. Old cosmetic
  profile fields are ignored without changing surviving bindings.
- ThunderHack-Recode-inspired visuals prioritize particles and trails. The
  Rockstar-inspired catalog describes actual coverage rather than promising a
  complete Java port.

## Persistence and runtime boundaries

Automatic preferences remain partial and save in `client-settings.ini` beside
`Keim.exe`, including HUD positions, per-element scale/visibility, keybinds,
Rockstar parameters and world/server-scoped waypoints. Existing AppData preferences
are imported once when local preferences are absent, without replacing local files.
Runtime helpers and logs use `%LOCALAPPDATA%\Keim`. The old mcov JDK selection
and AppData preferences are imported when the corresponding new/local file is
absent; existing portable settings and named profiles remain in place.

All SMP settings and their module bindings also persist in automatic preferences
and named profiles. Stash discoveries are session data, scoped to the current
server and dimension; **Save waypoint** retains a location through the existing
waypoint store. Complete older profiles without an SMP block receive SMP defaults
and unbound new actions. A partly present or invalid SMP block rejects a named
profile before any settings apply.

Named `configs/<name>.ini` profiles beside the client contain user-facing module
states and settings, visual colors, combat and macro settings, blocks, HUD and
layout, binds, friend UUID relations and waypoints. Profiles are self-contained,
fully validated before application and saved atomically within the local folder.
Copy `configs` and `client-settings.ini` with the client to retain the setup.

Block scans cover a cube with a 4Ã¢â‚¬â€œ48 block radius and only blocks available in
the client world. Each result set is bounded to 1024 matching blocks. Stash Finder
evaluates successful completed scans, defaults to eight chest, trapped-chest,
shulker-box or barrel blocks per chunk, and counts a double chest as two blocks.
It keeps at most 128 discoveries and the highest observed count for each chunk.
Truncated scans report lower-bound counts. The scan does not reveal inventory
contents, spawner mob types or activation state. See [SMP features](docs/smp-features.md).
IPC v31 changes the shared layout: restart already running Minecraft before
reattaching with the updated client and agent.

The client menu composites an 88%-opaque dark ImGui fill directly over the
transparent overlay. It requires no desktop capture, blur or refraction; the
client glass renderer receives visibility false. The configured overlay capture
preference applies while the menu is open. Settings Ã¢â€ â€™ Overlay reports
`Translucent dark / 88% opacity` and `Not required`.

The injector also uses direct ImGui alpha compositing. Its native shell clears
to transparent RGBA, extends DWM across the client area and clips to a rounded
Windows region matching the 12-pixel panel radius. Initialization stays hidden
until the first frame is presented. `FlatApp` no longer owns or calls LiquidGlass;
injector presentation requires no desktop capture, blur or refraction. Native
drag, close/minimize, async launch, JDK selection and logs retain their behavior.

The retained LiquidGlass renderer and capture checks remain a legacy/standalone
capability based on
[poncippg-spec/liquidDX11](https://github.com/poncippg-spec/liquidDX11).
Their material, capture lifecycle and license remain documented in
[docs/liquid-ui.md](docs/liquid-ui.md). They are not active injector requirements.
The redesign changes presentation only, not world-effect visibility or JNI acquisition.
Snapshot HUDs use the shared rounded ImGui fill and font; their editor applies
`CompactStyle` locally. HUD presentation adds no capture, blur or new renderer.

## Verification limits

For the Player ESP consolidation and optional Storage textures, the seven focused
checks `client_controls`, `smp_features`, `rockstar_features`, `control_panel`,
`depth_rendering`, `portable_client` and `esp_rendering` passed. The portable
`build/client/Keim.exe` and `build/Keim-client.zip` were rebuilt. An independent
finish review returned **ship** after reviewing the relevant source and four
native synthetic captures under `.impeccable/review/player-esp/`:
`player-esp-desktop.png`, `player-esp-small.png`,
`storage-texture-settings-desktop.png` and `storage-texture-settings-small.png`.
These checks establish native UI behavior, not appearance in a running Minecraft
world; no live Minecraft validation was performed for this update.

For the Base texture update, all 26 available regression checks passed across
the final runs. The sandbox run passed 25; `frame_attach` passed separately
outside the sandbox after a Windows fixture-pipe access failure. After the final
resource-release changes, `base_texture_gl`, `base_texture_models`,
`smp_features` and `portable_client` passed again; `depth_hook` passed in the
preceding focused run. The real WGL/OpenGL pixel test
covers live atlas sampling, UVs, camera projection, independent base depth,
compositing, restored graphics state, opacity 0.5, translucent texel blending
and renderer reinitialization after resource release.
Java fixtures cover quad/tint/light capture, sprite/model paths and proxy defaults;
state/profile checks cover mode migration and leased, world-scoped requests.
The 1.21.11 texture bridge's 38 classes and 73 members were audited against
cached official classes for Mojmap, vanilla and Fabric intermediary.

Native UI checks cover the existing Base settings and texture loading/error
states. The five final `-verified.png` captures under
`.impeccable/review/base-textures/` use synthetic ImGui/D3D11 WARP fixtures at
1280 Ãƒâ€” 800 and 480 Ãƒâ€” 360. The independent finish review returned **ship for the
UI scope**. The EXE and ZIP were rebuilt. Earlier coloured-marker checks and
captures under `.impeccable/review/base-esp/` remain historical Area marker
evidence, not texture-rendering evidence. No live Minecraft texture, appearance
or attach check was performed; shader and mod graphics compatibility remains
unverified.

For the original Storage ESP, Stash Finder and Spawner ESP extension, the
production client and portable ZIP were rebuilt. All 24 available
regression checks passed across the final runs; `frame_attach` required a run
outside the sandbox after Windows denied fixture pipe access. The SMP checks
cover exact block classification, completed-scan gating, filters, negative chunk
coordinates, session/world isolation, discovery bounds, highest-count retention,
profile compatibility and per-type rendering. Native UI checks cover the actual
storage-type caption clicks, colour picker, settings controls and persistence.
Six SMP captures under `.impeccable/review/smp/` use the real ImGui/D3D11 WARP
renderer with synthetic fixtures. They do not establish live Minecraft scanning,
appearance or attach behavior; no live Minecraft check was performed for SMP.
The independent finish review opened all six regenerated captures and returned
**ship** after the storage-type caption-toggle fix, with no visible regression.

The `mcov` and `mcov_injector` builds, native/UI checks and portable share ZIP
passed after the redesign. Offscreen D3D11 WARP checks exercise real UI input,
small viewports, HUD/keybind behavior and portable packaging. Updated launcher
checks cover selection, start, duplicate-start prevention, both sidebar tabs,
fixed footer and title controls. Hidden native HWND/swapchain checks cover alpha,
control ink, hidden initialization, resize and rounded clipping.

Current client images under `.impeccable/review/module-widgets/` use native
ImGui/D3D11 WARP with synthetic fixtures at 1280 Ãƒâ€” 800, 1920 Ãƒâ€” 1080 and 480 Ãƒâ€” 360.
The independent finish review opened all 11 images and returned **ship** with no
material findings. Native checks cover real right-clicks for all 77 sections,
actual toggle stores, Space / Shift+F10, independent drag/close, compact close
returning to the grid, search recovery, settings controls, key capture,
persistence and style/font ownership. Production build, native check and
`portable_client` passed; `Keim.exe` and the share ZIP were updated.
Earlier `.impeccable/review/compact-client/` tab images are historical.
Five Configs captures under `.impeccable/review/configs/` cover empty, loaded,
error and small scrolling states at 1280 Ãƒâ€” 800 and 480 Ãƒâ€” 360. The finish review
returned **ship** after confirming the explicit-overwrite fix. Native profile
round-trips, invalid-file preservation, local storage and portable exact-payload
checks passed; HUD and Rockstar regression checks remained green.
Current `.impeccable/review/compact-injector/` images use synthetic fixtures. Seven HUD images
under `.impeccable/review/compact-hud/` likewise use the native ImGui/D3D11 WARP
renderer with synthetic fixtures; `hud-catalog.png` is a test gallery of all
19 elements, not a production layout. HUD input/layout, style/font and clipping
checks, `combat_hud`, `rockstar_features` and `portable_client` passed; the portable
EXE and ZIP were rebuilt. The two injector
`launcher-native-*` images are hidden swapchain readbacks before OS clipping.
These images do not establish live desktop composition or Minecraft appearance.
Historical explicit `--live` and `--live-sta` probes passed WGC startup, the full
LiquidGlass runtime, capture pause, affinity restoration and capture after
reopening on this host without saving desktop images or attaching to a JVM.
Those results concern the retained legacy renderer, not current injector capture.
Live Minecraft attach/focus and every GPU/runtime combination remain outside
these checks. Existing compatibility details remain in
[docs/compatibility.md](docs/compatibility.md).

## Freecam

**Misc Ã¢â€ â€™ Freecam** extends the existing module grid and independent settings
window. Its unbound Toggle/Hold action, flight speed (1Ã¢â‚¬â€œ40 m/s, default 8) and
Ctrl boost (1Ã¢â‚¬â€œ10Ãƒâ€”, default 3) use existing controls. WASD follows camera yaw/pitch;
Space rises and Shift descends. Native client ticks own flight state and pause
combat/item assists. Java camera hooks apply the interpolated position and
rotation before culling; local player movement and game interactions are gated
while active. Menus, focus loss, missing world, unload and stale lease release
the camera. Automatic preferences keep parameters/binding, with Freecam off at
startup; named profiles preserve its enabled state. See docs/freecam.md for
verification and live-game limits. IPC v24 requires restarting Minecraft.

Auto Trident appears after Spinbot in Combat. Hold Minecraft's Place / Use with
a main-hand Riptide trident in water/rain to repeat normal charged releases.
Charge time is 10–40 game ticks, default 10. Randomize charge defaults to an
additional 0–3 ticks, chosen once per charge (10–13 ticks with defaults).
The extra range is configurable from 0–10 ticks, capped at 40 total; disabling
randomization restores the fixed charge time. No new scan, render hook or timer.
Native action gating uses the existing client-tick lease. Automatic preferences
retain charge time/randomization/binding and start disabled; named profiles retain
all settings. Automatic releases wait for the previous Riptide boost to finish
using the game's isAutoSpinAttack state, retaining the sampled charge duration
while waiting. IPC v33 requires restarting Minecraft after updating the client.
