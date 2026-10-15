# English injector and overlay copy

Mode: Operate. Narrow copy extension of the existing native Windows interface.
Authority: PRODUCT.md, DESIGN.md, module-widgets.md and docs/launcher.md.

## Direction contract

THESIS: Use consistent English throughout the injector and overlay, including
loading, recovery, errors, debug HUD and named keys. Preserve feature behavior.

OWN-WORLD: Keep embedded Inter, compact anthracite panels, coral controls,
existing spacing and fixed action footers. No visual-system change.

STORY: Select Minecraft, start the client, recover through Select JDK folder
or Show details, inspect Logs, then use English overlay settings and key labels.
Keep player names, instance titles and typed content intact.

FIRST VIEWPORT: The established launcher stays 640x640 with Start / Logs in
the existing sidebar. Start client and Close remain fixed; content scrolls.
At 480x380, English labels must fit or wrap without hiding actions.

FORM: Assigned local copy refinement; no concept seed applies. Reuse controls.

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance

## Finish status

Complete (2026-10-15). The independent
[finish review](../review/english-ui/review.md) returned **ship for the English
translation scope**, with no material findings. All ten supplied native
offscreen captures were also opened for the
[documentation finish](../review/english-ui/documentation.md).

The incumbent visual system is preserved. DESIGN.md and its sidecar have only
the validated historical label alignment to Start/Logs and Start client;
normative tokens, component styles and layout remain unchanged. No new shipping
rasters were introduced. Supplied production builds, seven affected checks,
the 8,325-literal audit and English portable guide/package verification passed.
Those checks were not rerun by the documentation pass.

Evidence is synthetic native ImGui/D3D11 WARP output, not live Minecraft or real
desktop composition. The documentation finish records those limits, the
unavailable detector, and preexisting 82-versus-77 section-count drift without
repairing unrelated context.

## Verification boundary

Use existing native offscreen launcher states and overlay keybinding captures.
Check English named keys independently of Windows language, and preserve
layout-specific printable legends. No live Minecraft or new shipping rasters.
