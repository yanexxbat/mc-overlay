# Knife initialization fix

2026-10-15. The user's current agent log confirmed intermediary-1.21.11,
working frame/depth/base bridges, and failed knife installation. No game process
was still running when diagnosis began; no live Minecraft was started or attached.

The actual Modrinth Fabric 1.21.11 remapped client and its version-specific cached
libraries reproduced the constructor failure: RenderPipeline.Builder.withLocation
(String) prefixes the default namespace and rejects the colon in mcov:knife_glow
as an invalid path (minecraft:mcov:knife_glow). The old synthetic builder accepted
all strings, so the previous tests missed the real API contract.

Changed the path to mcov/knife_glow. Added root-cause initialization logging and
contained optional runtime setup errors. The test builder now validates the real
path contract: it failed against the previous production constructor, then passed
with the fix. Model geometry, rendering scope, settings and IPC v26 are unchanged.

Independent real-class verification succeeded twice: the actual Fabric symbol
and pipeline constructor check reported API ready; an isolated JVM with the real
frame agent transformed the actual Fabric hand class and the public configure
entry point returned true, with the hook bit and ready status verified. This
uses cached game bytecode, not a fake API; no world or game window was launched.
First-person visual appearance remains unverified live.

The release EXE was initially locked by the running old launcher. Built into
client-fixed with a local CMake override, then preserved the previous executable
beside the standard client path and copied the byte-identical repaired EXE to
build/client/mcov.exe. The local build override was removed afterwards. No process
was stopped and no user game/profile files were modified. ZIP contains the exact
repaired EXE plus START.txt; the helper includes the corrected path constant.

All eight focused checks passed: knives, agent_package_match, base_texture_models,
knife_models, knife_hooks, portable_client, frame_sync and depth_hook. The prior
full suite was not repeated for this isolated Java initialization fix.

Repaired portable EXE SHA256: 4e2d8fe58ea90468b4488461815b44109bd8122b228178f6efabf896768b86c3

Restart Minecraft and the running old client before opening the repaired EXE.
