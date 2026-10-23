# Chunkfinder

Left-click **Blocks > Chunkfinder** to enable it; right-click to configure it.
**Method** selects Activity (default), Block clues or Combined. Activity watches
received light and redstone changes below Y 0. Block clues keeps the original
placed-block/base/farm/geode scanner; Combined uses both. The DonutSMP setup
button selects Combined, enables sky/redstone updates and fully grown amethyst
clusters, disables placed-block/base/farm clues and sets Rotated min to Off.
This is a preset, not a guarantee about the current server implementation.

Detection settings appear only for the selected method. Block rules, display,
colours, alerts and performance have collapsible groups. Preferences and named
profiles persist method and options. Older profiles start in Activity while
retaining their previous block-scanner values. `Off` denotes a disabled rotation
rule. Detailed world labels explain the cause and avoid overlap.

## Activity detection

The implementation was informed by the local Anubis 0.9.9.3
`PrimeChunkFinderModule` (labelled Chunk Finder), which compares underground light
updates and redstone states. Keim implements its own bounded tracker and adapts
the approach to the existing read-only packet hook and client-tick bridge.
It does not copy Minecraft game data or recover containers withheld by a server.

The shared packet tap works even with Anti Vanish disabled. It queues references
only; parsing and all world-context reads happen on the client tick. Full chunk
packets establish/reset light baselines. The first incremental light section or
first observed block state also establishes a baseline and cannot flag activity.
A redstone position needs two different received states. No polling comparison
of terrain or initial deepslate blocks is used for Activity.

Block and optional sky light are compared below Y 0. Unknown sections, all-dark
transitions, your own chunk (block light/redstone), and its eight neighbours
(sky light) are excluded. A bounded local light-gradient check suppresses known
amethyst, redstone ore and ordinary torch/lantern/candle sources. Recently
observed removed lamps receive a five-second exclusion. These filters rely on
client-visible source blocks; hidden or unusual sources can still cause clues.

Activity markers use amber, include the reason and last evidence coordinate, and
share the existing discoveries, alerts, orbs, copy and waypoint controls.
Light-change positions are not rendered as physical material blocks. Combined
mode preserves activity finds when a later static scan has no result.

Bounds: 128 queued packets, 32 top-level packets per tick with a 2 ms soft drain
budget, 512 tracked chunks, 32 negative-Y light sections per layer/chunk (64 MiB
maximum light arrays), 512 block-state observations per chunk and 2048 finds.
Candidate/filter work is capped too. Overflow discards the incomplete baseline
and requires fresh data; it does not compare across dropped updates. Oversized
bundles, deep custom dimensions or budgets can skip observations and show a limit
notice. Existing unloaded discoveries may remain stale, as in Block clues mode.
Clearing, disabling, changing method/settings or world resets observation history.

Packet members are generated/audited for all 11 existing Minecraft namespace
profiles. The UI reports an unavailable listener instead of silently claiming
Activity is operating. Live detection on the latest DonutSMP has not been verified.

## Scanning (Block clues / Combined)

The client copies loaded chunk palettes on the Minecraft client tick, including
available neighbouring chunks. A bounded pool of 1–4 daemon workers analyzes
copies without accessing the live world. Defaults: two workers, up to three
chunks queued per tick, a 2 ms soft snapshot budget (one copy can overrun it),
and at most twelve pending jobs. Unseen chunks precede rescans. Previously
sampled chunks become eligible again after ten seconds. Unloading cancels
pending results; world changes, disconnecting, disabling, clearing finds or
changing detection settings discard old analysis. Thread-count changes apply
on the next enable.

Only chunks in the client cache are inspected. There is no forced chunk loading,
server request, network chat message or world modification. The scan uses the
dimension's actual section height, independently of Block ESP.

## Detection

- Placed blocks: raw deepslate above Y 8, horizontal raw deepslate, cobbled
  deepslate above Y 0, and end stone outside the End. Defaults: 1 / 2 / 4 / 2.
  Ignore exposed excludes clues adjacent to air, fluids, waterlogged blocks or
  unknown neighbours. This filter applies only to placed-block clues.
- Base: cobblestone rows, deepslate masonry, buried obsidian, pistons, stairs,
  walled vertical shafts, flat rooms, optional ore columns, storage, dropped
  items and clustered mobs. Contributions are capped per detector; default
  threshold 8 points. Stacked ore defaults off and contributes one point.
  Item and mob evidence has no block outline.
- Farm: repeaters (powered weigh more), occupied hives or hives with bees in
  the chunk, cobbled floors, underground flowers/vines and workshops. Confidence
  combines contributions as `1 - product(1 - clue)`. This is a heuristic score,
  not a calibrated statistical probability.
- Amethyst crystals: only `minecraft:amethyst_cluster` (the fully grown stage)
  triggers this rule. Shell blocks, budding amethyst and small/medium/large buds
  are ignored. Evidence highlights only the actual cluster positions in purple.
  Available in Block clues and Combined; Activity remains packet-based.
  The old geode toggle is retained as the cluster toggle for saved settings;
  legacy geode sensitivity is ignored. A cluster alone is not proof of a player.

Structure exclusions are local heuristics. Spawners suppress nearby cobble and
storage; sculk/reinforced deepslate suppress ancient-city-like masonry, stairs,
workshops and storage; rails/cobwebs suppress mineshaft-like storage/stairs;
bells/paths suppress village-like workshops/storage. Exposed obsidian and nearby
portal clues are excluded. Plants require a roof and exclude lush caves, jungles
and swamps. Client worlds do not reliably expose server structure metadata:
these filters cannot remove every natural false positive and may suppress player
builds near structures.

## Display and alerts

Defaults: Color only, 30% tint at feet height, 0.15-block thickness, 96 nearest
chunks and 200 evidence blocks. Detailed adds outlines, evidence labels and
individual blocks using the shared Storage ESP gradient fills and soft edges.
Material colors are independent of chunk colors: gray deepslate, purple amethyst,
pale yellow end stone, red repeaters, honey-colored hives and green plants.
Air/fluid geometry clues do not draw solid evidence cells.
Orbiting orbs default on and reuse Target Aura glow/trails along the perimeter
of the nearest 24 displayed chunks. They work in both display modes; Reduce
motion freezes their positions. The Orbiting orbs toggle is saved in profiles. Fixed defaults to Y 64; Column and optional beacons use the
sampled world height. Colors are yellow (placed), red (base), green (farm), purple
(geode). For overlapping categories, base precedes farm, geode and placed.

The session cache retains up to 2048 discoveries, including unloaded ones, and
reports when full. Loaded chunks are re-evaluated; unloaded finds can be outdated.
Clear discoveries resets the cache. Save waypoints for persistence across restarts.
Evidence output is bounded to 1000 nearest-chunk positions and 64 samples per
detector per chunk.

New finds share a rolling alert limit, default six per minute. Suppressed alerts
do not hide discoveries. Chat alerts are local Minecraft system messages. Toasts
use independent four-second Keim notifications; sound uses the Windows notification sound. All
three have independent controls, enabled by default.

## Verification and upgrade

`tools/compatibility/generate_chunk_profiles.py` resolves API accessors against
official 26.2, 1.21.11, 1.21.1 and 1.20.1 classes and matching vanilla,
intermediary and Forge mappings. Detector fixtures cover natural exclusions,
coordinates, boundary exposure, thresholds, toggles, shafts, farms and geodes.
UI checks cover compact/desktop settings, both display modes and profile migration.
This does not substitute for a live Minecraft performance and accuracy check.

The shared protocol is now v38. Restart Minecraft before attaching the updated
Keim client so the new agent and bundled Java helper load together.

## Diagnosing widespread placed-block hits

Detailed labels and the discovery list distinguish high deepslate, rotated
raw deepslate, high cobbled deepslate and End stone outside the End. The list
also shows up to three sampled evidence coordinates when available. World labels
are limited to 12 nearest non-overlapping entries; this does not filter finds.
Only evidence from a category that actually reached its threshold appears in labels.
Set Rotated min to 0 to disable rotation alone on custom-world servers. Normal
vertical deepslate below Y -2 is covered by a negative regression and must not
trigger Placed. Client snapshots cannot establish whether server-supplied block
states reflect actual terrain or obfuscation; widespread hits alone do not prove
a server cause. Restart Minecraft to load updated Java diagnostics.
