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

Pages: AIMBOT -> Combat; VISUALS -> Visuals; LOOT -> Blocks; HUD -> Hud.
Small icons: player -> Stats; sun -> appearance/actions; gear -> Settings.
INSERT toggles the menu; END quits. Theme and rail preferences last for the
current session. Background imagery from the screenshot is reference context,
not part of the shipped overlay.
