# Minecraft overlay menu

Mode: Operate. Visual authority: the user's `design/reference-ui.png`.
The user chose to retain Minecraft modules in the screenshot's layout.

The follow-up brief requests a cleaner, smaller, draggable menu with larger
text/controls and purposeful animations. Desktop footprint: 860 x 477 px,
approximately 16% smaller than the first implementation. Geometry uses a
shared scale capped at 1.25, while menu text remains 14 px independently of
that scale. Two setting columns appear above 540 px of content width; smaller
viewports stack and scroll without shrinking text. Checkboxes are 20 px at
desktop scale, with larger hit areas and 14 px labels.

The two floating panels and narrow internal tool strip retain the reference's
topology. A discreet grip in the empty upper edge moves the whole composition;
controls never initiate dragging. Position survives menu reopening, is clamped
when the game viewport changes, and can be restored with Center menu.

Reference palette: main #22201E, rows #1C1C19, fields #242420, border #44413A,
gold #F0D596, text #D6D5CC, muted #A5A299. The active navigation tile uses
#876B57. Rounded outer panels, nearly square controls, quiet inner row rules,
thin gold tracks and cream knobs retain the reference's material. Circular
linework is quieter and inactive labels remain readable.

Motion: shared cubic-bezier(0.23, 1, 0.32, 1), 120 ms press/toggle feedback,
140 ms hover feedback and 160 ms mouse page transitions. Transitions retarget
from their current value. Inputs and drag positions update immediately. Native
Windows animation preferences and the Reduced motion option disable movement;
keyboard menu opening and page selection have no entry delay.

Embedded Inter Medium/Semibold menu fonts and Font Awesome Solid icons travel
with the executable. Decorative rings are native clipped vector linework.
The actual game remains visible through the framebuffer around the panels.

Preserve all real module bindings, safe disabled states, mouse and keyboard
navigation, slider numeric input, resets and agent settings. The menu theme
must restore the HUD/global style and font stacks after rendering.

Pages: COMBAT -> Combat; VISUALS -> Visuals; LOOT -> Blocks; HUD -> Hud.
Combat contains Modules, Targets, Rotation, Attacks, Sync, Friends and Debug.
Modules owns the live Aim Assist, Triggerbot and Auto Totem controls. The other
sections edit the Combat Assist ConfigStore and UUID relations; their status
states that its Minecraft bridge is not connected. Live Assist activation is
disabled until that bridge exists. Page reset covers both settings stores.
Small icons: player -> Stats; sun -> appearance/actions; gear -> Settings.
INSERT toggles the menu; END quits. Theme and rail preferences last for the
current session. Background imagery from the screenshot is reference context,
not part of the shipped overlay.

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
