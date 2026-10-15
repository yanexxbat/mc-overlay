# CS Knife

Open **Visuals > CS Knife** and enable the module. This cosmetic replaces vanilla
swords in the local main hand in first person on **Minecraft Java 1.21.11**.
Butterfly, Karambit and Katana are original procedural models inspired by the
Anubis feature requested by the user. No Anubis source, binaries or textures ship.
Vanilla, Mojmap and Fabric intermediary profiles are audited against cached game
classes. Other versions keep the original item and show an unavailable message.

Choose the model, size (0.5–2x), animation speed (0.5–2x), deploy animation and
blade trails. The blade material follows wooden/stone/iron/golden/diamond/netherite
or copper swords. This changes local appearance only, with no inventory, attack
range, damage or server changes. Other players see the original sword.

Assign **Inspect key** inside the module. It starts unbound, like the module key;
choose a Minecraft-unused key, such as R, to avoid Minecraft also handling it.
Tap to play once; hold to repeat until the current inspect completes. Attacking,
using an item, changing slots/items/world, focus changes, menus or a stale lease
stop or suppress inspection. Blade ribbons use a bounded 48-sample history and
fade after 0.05–0.35 seconds. Left-handed main hands are mirrored; offhand and
non-sword items retain their normal appearance. Models are not a skin change for
third-person views or other players.

The toggle, model, speed, size, deploy/trail choices and bindings save to automatic
preferences and named profiles. Complete old profiles receive disabled defaults;
partly damaged new fields reject a named profile before application. Panic and
Visuals reset disable CS Knife.

The Java hand-render hook verifies the complete item-render signature, tick method
and held-stack field before installation. It preserves the original bytecode when
unsupported. The bridge submits immutable per-frame POSITION_COLOR geometry
through Minecraft's own deferred custom-geometry collector and restores Vanilla
item rendering when disabled, stale, unsupported or failed. A failed optional
callback is contained; toggle off/on retries. Settings travel in a 500ms lease;
JNI only updates settings and status outside the game rendering callback.

This update uses **IPC v25** and a new frame helper: restart Minecraft and the
old client before launching the rebuilt portable EXE.

Verification is documented in `.impeccable/review/knives/review.md`. Synthetic
native UI, bytecode and game-API fixtures do not establish live Minecraft appearance
or shader/mod compatibility. No live Minecraft attach has been performed.
