# Anti Vanish source adaptation

The 1.21.11 bridge now adapts the supplied Anubis `AntiVanishModule.java` to
Keim's injected agent and external HUD. Named checks use only the 21 fixed names
from the supplied `StaffListModule.java`, case-insensitively. There is no setting
to extend the list to every player. Anonymous sound/particle hints identify only
a location, never a staff member.

Enable **Misc > Anti Vanish**. With the HUD enabled, up to four current hints
appear at the top centre, plus a critical line for strong nearby evidence.
Notifications are optional. Entries show their reason, and relisting or leaving
spectator mode clears the old alert and briefly shows `Visible again`. Normal
observations expire 20 seconds after their latest evidence; critical lines last
7 seconds with a 12-second cooldown. Name Protect also covers named hints.

## Evidence and filtering

- Require 1.5 seconds of previously listed trust for named TAB/spectator/entity
  checks. Ignore the local account, including when a large roster is truncated.
- Observe TAB removal/listed events and spectator transitions. Delay removal
  checks one second; cancel them when a matching departure message is received.
  Ignore bulk removals or hidden batches of four or more entries. A disappearance
  without a remaining entity is meaningful only after learning that the server
  sends leave messages. Plain English/German and vanilla translated leaves work.
- Request `minecraft:msg ` suggestions every 5 seconds. This sends a completion
  request only, not an executed command or a chat message. Accept only responses
  to our outstanding IDs (15-second timeout, at most 8 outstanding), and only
  one to three missing fixed-list staff names. Ordinary user completion requests
  and duplicate/late responses are ignored.
- Observe invisible entities, suspicious container/mechanism sounds and selected
  particles. Suppress visible player/projectile causes, villager doors, own
  actions/placements, open containers, explosions, powered mechanisms and smoke
  near natural sources. Weak smoke/block particles require a repeated burst.
- Combining anonymous hints alone cannot identify a watcher. A critical combined
  alert requires at least two signal categories, score 35, and a trusted named
  nearby entity as supporting evidence.

## Runtime and scope

The packet transformer checks three audited network signatures in the Mojmap,
vanilla and intermediary namespaces. It preserves packet contents and delivery.
Network hooks only enqueue references; game access occurs on client ticks. The
queue holds at most 4,096 entries; draining processes at most 512 entries including
bundle children per tick. Environmental inspections are additionally limited to
16 events and a 4-ms admission budget (a started scan can finish after the budget).
Roster/entity scans and signal caches are bounded. Limits are reported in settings.

World, connection and local-player changes, disable, panic and expired leases
clear monitoring state. Sampling faults disable the bridge until an off/on toggle.
The UI explicitly reports when only roster sampling is available without packet
hooks. Shared-memory version 35 requires restarting Minecraft with the new package.

This is an adaptation, not a full Anubis client port: its `StaffGhost` animation,
custom chime, Minecraft chat renderer and Donut-specific map/region tracking are
not included. Keim uses its own HUD/notifications and loaded nearby entities for
regional evidence. Server-hidden information remains unknowable; ordinary potion
invisibility and server-specific TAB behaviour can still produce hints.

## Validation

`anti_vanish` covers staff filtering, trust, removal grace, leave-message order,
bulk suppression, spectator return, completion correlation, self exclusion,
combined evidence, expiry, queue bounds, disconnect and failure recovery.
`anti_vanish_sensors` exercises visible causes, own actions, container grace,
explosions, redstone memory, villagers, natural smoke, unloaded chunks and bursts.
`anti_vanish_hooks` loads and executes transformed network bytecode for all three
namespaces and verifies original delivery and callback arguments. The control-panel
test renders the settings and five-row HUD at desktop and small viewport sizes,
checks disabled/stale HUD guards and retains the preferences/panic checks.

These are automated/fixture checks. Behaviour on a live Minecraft server has not
been verified in this task.
