# Minecraft overlay menu

Mode: Operate. Visual authority: the user's `design/reference-ui.png`.
The user chose to retain Minecraft modules in the screenshot's layout.

At the 752 x 427 reference viewport, the main panel starts at (26, 17) and
measures 590 x 382. The separate rail starts at (634, 17) and measures 73 x 382.
The gap is 18 px. The main inset frame is 14 px; the internal icon strip is
29 px wide with 31 px buttons. Navigation tiles are 53 x 51 with 8 px gaps.
All these dimensions share a scale, capped at 1.5. Small viewports use a
scrolling single column rather than clipping settings or losing controls.

Reference palette: main #22201E, rows #1C1C19, fields #242420, border #44413A,
gold #F0D596, text #D6D5CC, muted #77766E. The active navigation tile uses
#876B57. Rounded outer panels, nearly square controls, quiet inner row rules,
thin gold tracks and cream knobs reproduce the reference's material.

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
