# English UI finish review

## Disposition

**Ship for the English translation scope.** No source fix, recapture or additional rebuild is required by this review. The supplied build and seven affected checks passed. This verdict covers app-owned English copy and its fit in the incumbent native interface.

## Scope and evidence validity

Reviewed PRODUCT.md, DESIGN.md, the `english-ui.md` direction contract, Impeccable craft-floor and clarify references, and the production launcher, package, combat debug HUD, key naming/input, IPC and process sources named in the handoff. The existing compact Inter/anthracite/coral system and native Windows Dear ImGui conventions are authoritative. No web or iOS design standard was imposed. Existing Base Finder implementation changes are outside this review.

Opened all ten actual offscreen captures: `launcher-empty.png`, `launcher-ready.png`, `launcher-small.png`, `launcher-busy.png`, `launcher-error.png`, `launcher-diagnose.png`, `launcher-diagnose-desktop.png`, `compact-keybinds.png`, `compact-keybinds-bottom.png`, and `classic-keybinds.png`. They cover launcher 640x640, 640x520 and 480x380 states, plus overlay 1280x800 and 480x360. These are synthetic native ImGui/D3D11 WARP fixtures, not approved concept images or live Minecraft captures. Their labels, layout and state composition agree with the reviewed production renderer.

The handoff reports successful production builds and passing `launcher_window`, `control_panel`, `combat_hud`, `client_controls`, `portable_client`, `agent_package_match`, and `smp_features` checks. It also reports an audit of 8,325 literals with no German candidates after excluding comments and raw shaders. These are supplied verification results, not rerun by this reviewer. The Impeccable detector did not run because its launcher engine was unavailable; no detector-clean claim is made. No new shipping raster assets were introduced.

## Material findings

**None.** No severity-ranked translation defect or translation-induced layout regression was found.

- The empty state names the missing Minecraft window and gives the Refresh recovery. Selection, busy and error states use clear English. The missing-JDK path supplies both `Select JDK folder` and `Show details`; `Logs` opens a `Diagnostics` page with connection details, `Open log folder` and `Copy log`. These names describe their respective navigation, page and actions without confusing the recovery path.
- `Start client`, `Preparing connection...` and `Switch to Minecraft` match the production action states. `Close` and the primary launcher action remain outside the scrolling content, including 480x380. Longer instance titles clip within their established row and retain full title/details in the production tooltip. At the smallest height, supporting status content scrolls above the fixed action, as specified.
- The compact keybind captures show readable English instructions, `None`, `Insert`, `End`, `Toggle`, `Hold` and `Reset keybinds`. The bottom capture confirms the final module rows and behavior explanation remain reachable. The desktop table preserves the same terminology.
- The key naming source supplies English named keys independently of Windows display language and preserves layout-dependent printable legends. User names, instance titles and typed content remain original. Both reviewed Win32 diagnostic formatters explicitly request English.
- Combat debug HUD copy is concise English: target validity, cooldown/adapter availability, activity, packet counts and rotation trace labels retain their technical meaning. The optional quick-start guide uses the shipping English control names and explains JDK recovery and portable settings locations.

## Fix table

No implementation fixes are required. The final documentation pass should replace historical `Start/Diagnose` navigation wording in DESIGN.md with the current `Start/Logs`; `Diagnostics` remains the page heading. This is a documentation alignment item, not a UI ship blocker.

## Verification limits

No live Minecraft attach, game appearance, foreground/focus behavior, localized Windows shell dialog, arbitrary GPU/runtime combination, or real desktop composition was tested by this review. Shell-owned controls and external Java/system output can follow their own environment language. The reviewed English Win32 diagnostic calls and app-owned strings do not establish the language of those external surfaces.

The captures establish fit and visible English for the represented states; source review supports the other named-key and debug-HUD states. They do not constitute a screenshot of every overlay setting, every keyboard layout or every error. Build, tests and the broader literal audit remain the complementary supplied evidence. No additional polishing of the existing visual identity was requested or performed.
