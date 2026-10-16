# Auto Trident

Auto Trident is appended to Combat without shifting existing modules or keybind
IDs. Its native settings use the established compact rows, font and green accent:
enable, optional keybind, charge time (10–40 ticks), release count, live status and
held-Use instructions. Automatic preferences retain charge/binding and start off;
named profiles retain the full module state. Legacy profiles default off/unbound.

One batched native WARP inspection covered [1280×800](trident-desktop.png) and
[480×360](trident-small.png). Controls and wrapping are readable; the compact pane
scrolls to the remaining help. The existing shared settings-pane header overlap
at the lower scroll position is outside this feature's scope. No visual correction
or additional polish pass was needed. Captures show synthetic unavailable status.

The implementation uses the existing post-vanilla client-tick callback, the actual
Minecraft Use key, main-hand/active-stack identity, Riptide enchantment strength
and water/rain eligibility. It releases through MultiPlayerGameMode.releaseUsingItem
at the configured normal charge threshold. Vanilla begins the next held-Use cycle
and handles reuse/durability. No synthetic mouse input, world scan, render hook,
extra thread or retained item reference was added. Unsupported bindings fail closed.
Eleven version/namespace mappings were audited against checksum-verified cached
classes; the older integer Riptide API is handled separately from the float API.

Build succeeded. All 32 selected automated checks passed in 99.48 seconds; the
unrelated Java self-attach check was excluded. Tests cover the real module toggle,
normal/compact pane, preferences/profile migration, invalid partial profiles,
Panic, charge thresholds and input lease gates. The owned JNI JVM tests all eleven
profiles with both camera layouts, including repeated charging, the real mapped
release call, unenchanted/dry/offhand/changed stacks, key release, focus/menu/combat
pause and disabled behavior. Embedded-agent/package matching and portable archive
checks passed. No live Minecraft process was attached or started for verification.
IPC v31 requires a full game restart when replacing an older loaded agent.
