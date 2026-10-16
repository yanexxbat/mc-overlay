# Faster Storage ESP and extended radius

Mode: Operate. The user reports Storage ESP is still capped at 48 and scans too
slowly. Preserve the existing native controls, colours, 3D frames and pulse.
Extend the radius to 512 through UI, runtime clamps, preferences and named configs.
Use loaded chunk block-entity positions for a separate bounded scan instead of
querying the whole block volume. Other modules' large sweeps must not govern
Storage ESP range, progress, interval or result capacity. Keep known results
through rescans and the selected 13-type mask. Reuse existing block-state access,
audit chunk-cache and position signatures from verified cached game classes for
all profiles, and retain an independent bounded compatibility fallback.

Show Storage ESP's own progress/status and explain that scan pause and pulse
speed are separate. Default radius, pulse, types and colours stay unchanged.
Verify the actual slider at 512 in normal/compact native UI, profile/preferences,
dedicated-result rendering and independence from a full generic result list.
Use isolated JVM fixtures to prove loaded-cache traversal at both 24 and 512,
including distant/out-of-range containers and matching type filters. Keep only
one chunk's key snapshot and at most 1024 results; no render hook or game asset.
