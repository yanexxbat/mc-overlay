# CS Knife verification

2026-10-15. Self-reviewed native UI and rendering integration in this chat.

Shipped scope: Visuals > CS Knife, original procedural Butterfly/Karambit/Katana
models, material colours from the sword, tap/hold inspect, deploy and bounded
blade trails. Local main hand only, including left-handed players. Uses the real
Minecraft custom-geometry collector; no screen-space mock knife or copied Anubis
assets. 1.21.11 Mojmap, vanilla and intermediary signatures audited locally.

Inspected a batch of desktop/small native WARP captures at 1280x800 and 480x360:
model/binding/sliders/toggles, instructions/status and error recovery. Existing
compact tokens, fixed header and scrolling remain intact. The final second
capture round corrects the error fixture's scroll position and confirms the whole
message is reachable. No additional visual system or shipping raster asset.

The included model image is a synthetic software preview of exported production
meshes from KnifeGeometry, showing the actual generated shapes/material colours.
It does not establish the game's projection, lighting or resource-pack behavior.

All 32 available regression checks passed across the final runs. The broad run
passed tests 1–23, including runtime_profiles (90.27s); remaining checks passed
in focused runs. frame_attach failed inside the sandbox on its Windows result
pipe and passed separately outside it. The control_panel check passed again
after the final error-fixture scroll correction. No production code changed after
the complete passing set. Portable EXE and ZIP were rebuilt and packaging passed.

New checks: knives validates leases, settings/schema, old/partial profile handling
and input edges; knife_models exercises the production deferred submission
callback, all models, mirrored main hand, screen/item guards, inspect start/loop/
completion, attack/slot cancellation, trail generation/expiry and lease fallback;
knife_hooks loads real transformed bytecode in all three namespace signatures
and verifies stack maps, hook proofs and preserved Vanilla fallback. Existing
frame, depth, runtime, freecam, UI, combat, packaging and KeyAuth checks passed.

No live Minecraft attach or visual run was performed. First-person placement,
shader/mod compatibility and final in-game appearance remain unverified. IPC v25
requires restarting Minecraft and the previous client before using the new EXE.
