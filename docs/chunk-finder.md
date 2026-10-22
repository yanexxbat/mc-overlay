# Chunkfinder

Left-click **Blocks > Chunkfinder** to enable it; right-click to configure it.
**General** and **Options** contain placed-block, base, farm, geode, display,
performance and alert controls. Automatic preferences and named profiles persist
the settings. Older profiles receive defaults for the new controls. The former
kelp/crystal-growth detector has been replaced.

## Scanning

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
- Geodes: connected amethyst/budding-amethyst components must be completely
  observed, large enough and contain no budding amethyst. Unknown boundaries
  and components exceeding 8192 blocks are skipped. Sensitivity 5 requires 62
  amethyst blocks; 1 requires 102 and 10 requires 12. The brief's “one in fifteen
  natural geodes” rate has **not** been measured for this implementation.

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

The shared protocol is now v37. Restart Minecraft before attaching the updated
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
