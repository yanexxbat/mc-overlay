disposition: ship

## 1. Disposition

Ship for the reviewed native launcher UI and KeyAuth source integration. No material blocker was found within this review. This is an extension of the established compact system; no comp, new visual world, or shipping raster is required.

## 2. Evidence validity and scope

Opened all ten required files in `.impeccable/review/keyauth/`: `keyauth-empty-desktop.png`, `keyauth-empty-small.png`, `keyauth-entry-desktop.png`, `keyauth-entry-small.png`, `keyauth-busy-desktop.png`, `keyauth-busy-small.png`, `keyauth-rejected-desktop.png`, `keyauth-rejected-small.png`, `launcher-ready.png`, and `launcher-small.png`. Every capture is nonblank, shows its named state, and has the expected 640 × 640 or 480 × 380 viewport.

These are the actual Dear ImGui/D3D11 offscreen renders with synthetic data, not live KeyAuth or Minecraft screenshots. Busy/rejected fixtures retain the seeded masked value to exercise presentation and retry; production clears the submitted buffer in the launcher event handler. These captures therefore establish layout and control states, not the entire production authentication transition.

Read PRODUCT.md, DESIGN.md, the KeyAuth direction contract, the authentication documentation, the craft floor and new-work reference, the auth client and inherited-handoff implementation, launcher UI and startup integration, overlay entry/runtime checks, signed fixture tests, LicenseChecks, and relevant CMake auth wiring. No detector ran: this is native Windows Dear ImGui, outside the HTML/CSS detector's scope. Existing Base Finder and English UI work is outside this review.

## 3. Material findings and supporting evidence

No material findings.

| Review area | Assessment | Evidence |
| --- | --- | --- |
| TYPE | Match | Embedded Inter and the established compact hierarchy remain readable in every supplied capture. |
| MATERIAL | Match | Existing native rounded fills and controls are preserved; no imitation illustration, decorative material, or new asset system appears. |
| GROUND | Match | Anthracite surfaces and restrained coral actions/feedback retain the incumbent DESIGN.md system. |
| License workflow | Match | Labelled masked entry, optional Show key, disabled empty/busy action, rejection recovery and fixed Verify license/Close actions remain visible at both sizes. Logs navigation remains available. |
| Authenticated workflow | Match | Supplied ready fixtures preserve instance selection, status, coral Start action and fixed footer. Small-view content uses the existing scrolling/clipping behavior. |
| Request trust | Pass by source | Fixed HTTPS endpoint, default TLS validation, no redirects, bounded response, Ed25519 timestamp/body verification, owner and success-code checks precede acceptance. Login additionally requires an active subscription expiry. |
| Startup gate | Pass by source | Launcher checks the session before package preparation/connection. Overlay consumes the one-use inherited pipe and checks online before Minecraft discovery or attach. The explicit handle list carries only the pipe; arguments carry its handle number. |
| Runtime/recovery | Pass by source | Background checks run at 60 seconds; local expiry, rejected/failed checks and the 90-second lease leave the overlay loop and call Shutdown. Minecraft is not terminated. Launcher rejection returns to license entry. |
| Secret handling | Pass at persistence boundary | Input is masked by default and cleared at submission; no license/session disk, command-line, environment or log path was found. Session and request strings receive explicit wiping. Test transport/key injection is constructor-only and is not a runtime override. |

The existing understated controls are appropriate for an operating launcher. Additional ornament or motion would not improve this task. The contract's compact English workflow is preserved, and the provided rendering evidence does not reveal a material craft-floor regression. DESIGN.md remains consistent with the incumbent visual tokens; the new license workflow should be recorded by the normal documentation handoff.

## 4. Required fix table

| Priority | Required fix | Status |
| --- | --- | --- |
| — | None | No blocking fix or recapture required. |

## 5. Actual verification limits

This review inspected source and opened the supplied captures; it did not execute a build, rerun tests, call KeyAuth, enter a real license, or attach to Minecraft. The parent reports a successful build and seven passing affected tests (`keyauth`, `launcher_window`, `control_panel`, `client_controls`, `portable_client`, `agent_package_match`, `smp_features`), plus successful configured-app live initialization. It also reports that a fresh initialized session without a license was rejected by the live `check` endpoint (`success=false`, `code=0`). These live results are parent-provided evidence, not independently repeated here.

The reviewed signed fixtures cover signature tampering/missing signatures, timestamp bounds, wrong owner/code, malformed and oversized responses, expiry, rejection/redaction, cancellation, encoding and an inherited child-pipe round trip. They do not prove successful login with an actual license, dashboard configuration, live subscription revocation, real Minecraft lifecycle behavior, or desktop composition across Windows/GPU combinations. The runtime timer loop was reviewed in source, not exercised end-to-end by these fixture tests.
