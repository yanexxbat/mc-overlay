# 1.21.11 packed-colour repair

The running client's log showed that frame hooks and the texture bridge installed,
then mesh extraction raised a Java exception. The earlier native handler cleared
that exception without logging its underlying cause, so those live logs alone do
not identify the failing operation.

An audit of the actual cached 1.21.11 `VertexConsumer` interface found an omitted
abstract operation: `setColor(int)`. Minecraft's bulk vertex methods call this
packed ARGB overload. The old proxy rejected it, stopping mesh extraction. The
old test used the four-component overload directly and therefore missed this path.

The corrected test reproduces the failure against the old implementation and
passes with packed-colour handling. It also checks unequal RGB channels and alpha.
Profile generation now checks that every abstract vertex operation has a binding;
the audited table covers 38 classes and 73 members in three namespaces. Future
bridge exceptions retain their nested causes in the native log.

`base_texture_models`, `base_texture_gl`, `smp_features` and `depth_hook` passed.
The repair client and ZIP also passed the standalone portable-package check,
including equality with the newly built DLL and helper JARs.

The open old launcher locked `build/client/mcov.exe`. The repair was linked using
the generated client link command into `build/fixed-client/mcov.exe`, packaged as
`build/mcov-client-fixed.zip`, and verified there. Existing preferences and configs
were copied into the repair folder. Default builds still use `build/client` once
the old launcher is closed.

Minecraft had exited before an attempted live consumer repair; no class was
redefined. Successful texture appearance in Minecraft has not yet been verified.
The development-only hotfix/probe tools are not bundled in the client.

## Reconnection follow-up

The next live log showed the repaired launcher selecting cache `33ee65a277e9299f9d01`
while reusing the agent already loaded from `f5f56baf201064da30bc`. The old DLL and
frame-signal JAR differ from the repaired package by SHA-256. Thus that session
still executed the previous consumer and its exception handler.

The launcher now checks all three loaded runtime files before either reconnect
shortcut: the native agent, frame-signal helper and frame synchronization agent.
An already running overlay also enters this verification. A mismatch uses the
existing restart diagnostic instead of launching new UI over obsolete code.

`agent_package_match` verifies identical payloads in different cache directories,
same-sized differences beyond a read-buffer boundary, each helper separately,
and missing packages. It passed along with `base_texture_models`,
`base_texture_gl`, `depth_hook` and the rebuilt portable package check. The normal
`build/client/mcov.exe` and `build/mcov-client.zip` were rebuilt successfully.
This confirms version rejection and packaging; successful in-game texture
appearance remains unverified until Minecraft starts with the repaired runtime.
