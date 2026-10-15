# Anti Vanish

Open **Misc > Anti Vanish** on Minecraft Java **1.21.11**. The module starts off
and its key starts unbound. It observes player-list and nearby-entity hints on
the Minecraft client tick, at most twice per second. It does not render a model,
send chat/commands, modify packets or scan blocks.

A player must first be present in the listed TAB roster for at least 1.5 seconds.
If that known player remains in the connection roster or near the local player
but is hidden from TAB, a one-second confirmation delay produces an indicator.
A normal disconnect alone produces no warning. Relisting clears a detection.
Changing worlds/connections, disabling the module or losing its settings lease
clears transient state. NPCs that were never established in TAB are not trusted.

The optional **Include invisible players** check is off by default. It adds
metadata-based hints for known nearby players; ordinary invisibility potions
can match. **Nearby range** is 8-128 m (default 64). TAB-only hints are server-wide.
Names are reported with their evidence, never as proven vanished staff.

The TAB trust/debounce and invisible-entity ideas were inspected in the user's
local Anubis AntiVanishModule. This is an independent, bounded adaptation of
those signals, not a port of every Anubis sensor. It does not include Anubis's
command-completion probes, staff-name database, sound/particle/environment
correlation, spectator announcements or ghost visualization. Servers that do
not transmit player evidence cannot be reliably detected by a client.

Enable **Alerts in Notifications HUD** and the existing Notifications HUD to
see messages outside the list. The pane includes a button to enable that HUD.
Alerts are deduplicated by UUID/evidence and limited to six per four seconds.
Name protect applies to the list and notifications. Panic and Misc reset disable
Anti Vanish. All choices and the module binding save to preferences/profiles.
Older profiles receive disabled defaults; obsolete knife fields/bindings are
ignored, and the retired binding IDs are not reused.

Runtime limits: 512 trusted identities, 512 roster entries, 256 nearby entities
and 32 result rows. Cached identities expire after 60 seconds without a sample.
Large rosters show an explicit limit notice. The sampler uses a 500 ms settings
lease; failures latch until toggled off/on. No per-render-frame geometry or
packet hook is added. Memory/FPS improvements are not measured in a live game.

The bridge's seven Minecraft classes and twelve members are audited for Mojmap,
vanilla and Fabric intermediary 1.21.11. Other versions show unsupported status.
IPC v28 requires a full Minecraft/client restart after updating. Knives, inspect,
blade trails and knife glow have been removed from both native code and helper
JARs; the packaging check verifies that no knife class is shipped.
