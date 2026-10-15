# Knife detail and glow verification

2026-10-15. Self-reviewed implementation and one batched visual inspection plus
one confirmation batch. No live Minecraft attach or rendering run was performed.

The three original models now use cached solid geometry: Butterfly 18,512,
Karambit 10,256 and Katana 13,472 vertices. Details include bevelled satin blades,
rounded grooved grips, machined pocket rims, recessed screw heads, a 96-segment
finger ring and raised Katana wraps. Static parts are bounded to 21 canonical
model/material combinations. Butterfly parts retain independent inspect pivots.

Glow defaults on at 1.2x strength, 1x width and RGB 69ddd5. It follows the actual
animated blade edges with joined Gaussian ribbons and rounded tips; blade trails
receive additive cores and soft edge ribbons. Strength controls both the edge and
trail core. Zero strength or disabled glow skips additive submissions. The custom
Minecraft pipeline inherits the audited position/colour snippet, uses additive
blending, no culling or depth writes, and explicitly selects the main output.
No post-process bloom or world lighting is claimed. The three namespaces audit
31 classes and 42 member signatures from cached official 1.21.11 bytecode.

The existing native settings pane adds Glow, strength, width and RGB colour.
Controls fit and scroll at 1280x800 and 480x360, with the fixed header intact.
Captured desktop/small glow states, top controls, status and failure recovery are
included. The mesh image renders exported production vertices and their actual
alpha gradients using a software depth buffer/additive blend; it is a synthetic
orthographic diagnostic, not a game screenshot. The single correction batch
smooths curve joins/tips and brings every glow control into the small capture.

Settings reach the Java renderer through the native leased request. IPC v26
requires restarting Minecraft and the previous client. Profiles and preferences
retain all four glow fields; complete older knife settings migrate to defaults,
while partly present glow settings reject the candidate before application.

Release build succeeded. All 32 regression checks passed on the final production
sources: the 31-test sandbox run passed in 99.08s, including runtime_profiles
(86.80s), then frame_attach passed against an isolated local JVM outside the
sandbox (0.63s). The Windows sandbox is known to block that fixture's result pipe;
no live game was attached. Checks cover actual deferred callbacks, additive main
output configuration, toggling, alpha falloff, trail intensity, finite edge-on
inspect joins, bounded geometry/cache, old/partial profiles, preference persistence,
hand mirroring, original fallback, hooks, frames, depth, packaging and KeyAuth.

Portable EXE: build/client/mcov.exe (16,764,416 bytes).
ZIP: build/mcov-client.zip (6,239,736 bytes), containing mcov.exe and START.txt.
The packed and standalone EXE hashes match:
SHA256 d6b6a91aab91c63f6b80e9bad323f38d00fa512b20ea557aab319872c2dfdc29.
No production changes followed the complete passing set. Live placement, shader
compatibility and in-game frame cost remain unverified.
