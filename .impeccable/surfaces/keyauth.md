# KeyAuth license entry

Mode: Operate. Extend the established native launcher with license-only login.
Confirmed app: mcov; owner Uhn5Z5WPCa; version 1.0. English copy throughout.

## Direction contract

THESIS: Require a verified online license before the client starts, with clear
retry and rejection states. Preserve the existing Minecraft/JDK workflow.

OWN-WORLD: Reuse Inter, rounded anthracite panels, coral buttons, muted helper
text, existing Start/Logs navigation and fixed Close footer. No new theme.

STORY: Enter a masked license key, choose Verify license, then select Minecraft
and start. Invalid or unavailable verification leaves a clear retry action.
License keys remain in memory and are cleared after submission.

FIRST VIEWPORT: The existing launcher shows a License section, labelled masked
input, optional Show key, status and fixed Verify license action. After success,
the existing Minecraft controls appear. Close and logs remain reachable at
640x640 and 480x380; network work stays off the UI thread.

FORM: Assigned extension of existing controls; no concept seed applies.

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance

## Verification boundary

Use deterministic signed protocol fixtures and native offscreen UI captures.
Probe the configured application's initialization without a user license.
Successful live login requires an actual license; never fabricate that result.
No new shipping raster assets.
