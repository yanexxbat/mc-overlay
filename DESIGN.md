# Minecraft overlay UI and HUD

Mode: Operate. Visual authority: the user's latest screenshot, committed as
`design/reference-classic-ui.png`. This explicitly replaces the prior amber
floating-panel/rail menu. Preserve the actual mcov module behavior and names;
the reference's game background and product title are not shipped assets.

A single rectangular 600 x 650 px window, constrained to the game viewport,
has a compact draggable title bar and six horizontal tabs: Combat, Visuals,
Blocks, HUD, Stats, Settings. Background #242424, fields #191919, black borders,
violet #AE69D1, text #ECECEC and muted #B6AFBE. Pixel-aligned embedded ImGui
ProggyClean is 13 px in menu/HUD; the existing Inter fonts remain for ESP and
shared injector helpers. No decorative rings, glossy highlights or rounded
floating navigation. Sliders fill their full width in violet with centered
numbers; labels sit above fields. Module groups use fine borders and violet
section rules. Two columns fit above 500 px content width; narrower menus stack
and scroll. Tab selection and input update immediately. Styles/fonts restore
before the rest of the frame. The shared injector flat theme is preserved.

Every main page has a consistent, horizontally scrolling function tab bar.
Visuals separates Box ESP, Skeleton, Name tags, Health bars, Distance labels,
Tracers, Target info, Target aura, Target marker, Trails, Hit particles,
Ambient particles, Hit bubbles, Kill effect, BreadCrumbs, China hat and Entity filters. Blocks has
Block ESP, Block types and Tracers. HUD has Editor plus a tab per element, each
with visibility, scale, position and individual layout reset. Stats separates
Local player, Client and Entities. Settings has Overlay, Focus, Keybinds and Agent. Every module bind is initially None, explicitly
chosen by the user; INSERT/Menu, F6/HUD editor and END/Quit remain defaults.
Keyboard/mouse keys and Ctrl/Shift/Alt chords support Toggle or Hold; single
modifiers are captured on release. Conflicts, capture cancellation and clearing
are visible. Binds act only with Minecraft/overlay focus and pause module changes
while menu/editor/capture is open. Hold restores prior state on release/focus loss.

HUD: nine independently enabled, positioned and scaled elements using dark
rectangular fills, violet top rules and the same compact font. The editor adds
outlines, empty/disabled previews, drag hit areas and a toolbar with selection,
visibility, scale and reset. Below 640 x 480 it isolates the selected element.
Coordinates are normalized within the viewport travel and clamped on resize;
free layouts may overlap intentionally. HUD placements/scale/visibility and binds
save beside the overlay executable in client-settings.ini. Other feature settings
and UUID relations retain their existing session behavior. Renderers consume
coherent snapshots; no new JNI calls, native hooks or IPC fields.

Combat shows five tabs: Kill Aura, Aim Assist, Triggerbot, Auto Totem and Friends. Kill Aura exposes
activation, target type and reach, plus status and the last target.
It acquires the nearest valid living hitbox all around the player, then keeps
that target until death or loss of range/sight. Players and mobs are included
by default; friends and occluded targets are skipped. Rotation follows the
current hitbox directly every client tick, with no smoothing or reaction controls.
Attacks use Minecraft's cooldown, with the hit ray refreshed after rotation.
There is no preset or selection-mode chooser. Diagnostics is collapsed by
default and exposes HUD toggles and read-only live state. Old technical settings
are normalized in the client adapter and cannot delay this behavior.
The debug HUD is off by default. Aim Assist, Triggerbot and Auto Totem each
own a function tab; conflicting controls are visibly paused while Kill Aura is enabled.
Friends uses one action to add a UUID exclusion, with no enemy-priority chooser.
Adding a friend also applies a translucent conical hat automatically. Its violet
and turquoise rim shares the Target Aura colors and Gaussian glow resource.
The hat follows the interpolated humanoid head and pose in world space, scales
with distance, and renders below ESP labels. It remains visible when Target Aura
and ESP are disabled; removing the UUID removes the hat. No extra menu controls.
The player's combat angles and
mouse-controlled view are always independent. Camera decoupling has no toggle.
An active Kill Aura target automatically receives a neon yellow/lime corner
cube inspired by the user's target-marker screenshot. Four thick front corner
brackets have rounded joins/end caps and a soft Gaussian glow; faint rear corners
and connectors give depth. The frame uses the interpolated hitbox center and
perspective size, faces the camera and reverses its roll smoothly every 3.5 s.
Visuals exposes Target aura and Target marker directly in its function tab bar.
The marker exposes enable, size, stroke, glow, opacity, animation, rotation arc,
direction interval and two colours using the existing row controls. Shape & timing
folds away corner length/rounding, rear depth and fade duration. Values update live;
Visuals Reset restores all three visual configs. Animation pause keeps the current
angle, and interval edits retain the phase. It works with ESP/orbs off. Only a fresh active Combat
report can show it; target loss, death or pause removes it. Explicit Reduced motion
freezes its roll; Windows menu-animation settings do not block the game effect.
No new render-thread JNI, texture creation or shader passes. It draws below ESP
labels and restores ordinary alpha blending after its glow.
Native callbacks run on the client tick;
ImGui only publishes settings and consumes diagnostics. Page reset covers both stores.
## Entity ESP

Mode: Operate. The user's ESP screenshot is the visual authority for this
surface: thin near-white rounded boxes and skeleton lines, a narrow vertical
white-to-violet health bar on the left and compact information above the entity.
Default box stroke: 1.6 px; corner radius: 7 px, capped on small boxes;
skeleton stroke: 1.1 px; labels: 12 px from the embedded Inter font.
Filled boxes stay off by default. The violet health accent matches the Aura.

Nametags now group distance, player ping, name and red HP in a compact dark
rounded strip. Worn armor, main hand and offhand appear as a centered row of
16 px vanilla inventory icons directly above it, with violet durability tracks
and small stack counts. Unknown ping reads n/a; mobs omit it. Empty equipment
slots use no space. Individual HP/ping/equipment toggles live under Name tags.

The selected entity has a quiet dark info pill near the upper center with
distance, available selected-player ping, name and health. Unknown ping is
omitted. The info pill yields when it would overlap an entity nametag at the
upper viewport edge. With nametags off, distances appear below boxes and HP
above the health bar. Colors, label size,
rounding, stroke widths and target info remain configurable under Visuals.

EspRenderer draws only from the coherent snapshot and existing world-space
geometry. It introduces no JNI calls, pixel smoothing or delayed anchor state.
Aura renders below ESP, and HUD/menu above it. Text and stroke sizing use UI
pixels while entity bounds and bones retain their actual perspective scale.


## Trails and particles

ThunderHack-Recode is the visual reference for Trails, Hit particles and Ambient
particles; the user prioritizes particles and trails. Preserve the current menu,
ESP and Aura identity. New effects start disabled and unbound, with dedicated
Visuals tabs and standard keybinds. Ribbon/line/orb trails fade in world space;
HP loss triggers orb/star bursts and billboard glow rings; positive-to-zero HP
triggers a ground ring and death burst. Fireflies/Stars/Snow form a local ambient
field; BreadCrumbs leaves fading points at the local feet. Own China hat starts
enabled in F5, uses sampled self head geometry and remains unbound.
All share Aura colours by default or accept separate palettes. Reuse the Gaussian
glow and additive blend resource, bounded pools and the existing coherent snapshot.
The agent samples the own body once in the existing JNI worker as a flagged Self
entity; no render-thread JNI, new hooks or IPC layout changes. Reduced motion
keeps particle fades and removes drift, spin and ring growth. Glow uses the Aura's
Gaussian halo, middle colour and bright core, with intensity controls per module.
See docs/world-visuals.md for exact controls, bounds and external-depth limitations.
