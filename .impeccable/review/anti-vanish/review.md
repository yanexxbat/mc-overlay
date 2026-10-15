# Anti Vanish / knife removal verification

2026-10-15. User requested removal of knife cosmetics after lag/RAM complaints,
then explicitly requested Anti Vanish from the local Anubis reference instead.
No subagents were used and no running game was stopped, launched or attached.

Knives are removed from UI, settings, input, native install/poll/cleanup, shared
transport, Java helpers/transformers, build/test sources and current product docs.
Generated Java class directories are cleaned before helper packing so incremental
builds cannot retain deleted classes. Portable verification checks both helper
JARs contain no knife classes. Existing cosmetic profile fields are discarded;
remaining module bindings keep their IDs. Retired IDs are reserved, hidden and
not serialized or reused by Anti Vanish.

Anti Vanish is an independent passive adaptation of the local Anubis TAB trust/
confirmation and optional invisible-entity checks. It is explicitly not every
Anubis sensor: no outgoing completion probes, staff database, sound/particle
correlation or ghost rendering. It reports evidence, never certainty about staff.
Server-hidden data is a stated limitation. Invisible-player evidence is off by
default because potions can match. The module also starts off/unbound.

Sampling runs on the existing client-tick hook, at most 2 Hz, with a 500 ms
settings lease and no render-frame geometry or packet hook. Limits are 512 cached
identities/roster entries, 256 nearby entities and 32 results, with 60 s cache
expiry, a 1.5 s TAB trust window and 1 s confirmation delay. Ordinary departures
alone are ignored; relisting and world/disconnect/disable clear current results.
Failures latch until off/on. Immutable Java views cross JNI into fixed-size POD
reports. Notifications reuse the existing HUD, honor Name protect, deduplicate by
identity/evidence and cap alerts at six per four seconds.

The native Misc tile, compact English settings, top/bottom states at 1280x800 and
480x360, evidence list and optional HUD enable action were inspected in a batch
of synthetic WARP captures. No new styling or shipping visual asset was added.
Profile/preference/binding/Panic tests cover actual native controls and schema.
Current IPC is v28: Minecraft and the old client must be restarted after update.

All 30 checks passed on final production sources: the 29-test sandbox run took
95.65 s, including runtime_profiles (86.79 s), then frame_attach passed separately
outside the sandbox in 0.44 s against an isolated local JVM. The sandbox blocks
that fixture's Windows result pipe. The earlier knife-removal-only state also
passed its complete 29-check set before the requested replacement was added.

New AntiVanishCheck exercises the production tracker and bridge with fake roster
APIs: trust/grace, relisting, departure rejection, opt-in invisibility, self/world
reset, bounded/expired caches, large-roster reporting, leases, failure containment
and explicit retry. Actual Fabric 1.21.11 client classes and its version-specific
cached libraries separately resolved all seven Minecraft classes/twelve members
and initialized the public bridge API successfully. Mojmap/vanilla/intermediary
profiles were audited against cached official signatures. Other versions report
unsupported. No live server detection, frame-time or RAM measurement is claimed.

EXE and ZIP match exactly; ZIP contains only mcov.exe and START.txt. Final EXE
SHA256: d28a305c64387f1ecc87bfdc592f56036ac4b31e17ba6daaee20b3431bf0172a

The old running EXE was preserved beside the standard client path to allow the
removal rebuild without interrupting it. The final rebuilt client is again at
build/client/mcov.exe. No production changes followed the passing test set.
