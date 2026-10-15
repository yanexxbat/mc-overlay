# KeyAuth documentation finish

Date: 2026-10-15. Scope: license-only authentication added to the existing native
launcher in Operate mode.

The independent finish review returned **ship** for the reviewed launcher UI
and KeyAuth source integration, with no material findings or required fixes.
This is an ordinary extension of the inherited compact system. `DESIGN.md`,
`.impeccable/design.json`, shared tokens, theme and font ownership remain the
visual authority; no new visual world or token change is needed.

## Checked evidence and inherited-system match

Read `PRODUCT.md`, `DESIGN.md`, `.impeccable/design.json`, the KeyAuth surface
contract, `review.md`, the KeyAuth section of `docs/launcher.md` and the
Impeccable document reference. Opened all ten PNGs in this review directory:

| Captures | Size | Observed result |
| --- | --- | --- |
| `keyauth-empty-desktop.png`, `keyauth-empty-small.png` | 640 x 640; 480 x 380 | Labelled License key field, Show key control, English guidance and disabled Verify license action. |
| `keyauth-entry-desktop.png`, `keyauth-entry-small.png` | 640 x 640; 480 x 380 | Masked entry and enabled coral Verify license action retain the existing field and button treatment. |
| `keyauth-busy-desktop.png`, `keyauth-busy-small.png` | 640 x 640; 480 x 380 | Checking status and disabled entry/action communicate pending verification while Logs and Close remain visible. |
| `keyauth-rejected-desktop.png`, `keyauth-rejected-small.png` | 640 x 640; 480 x 380 | Clear rejection/retry copy fits both sizes and the fixed Verify license and Close actions remain reachable. |
| `launcher-ready.png`, `launcher-small.png` | 640 x 640; 480 x 380 | Existing Minecraft selection, status and fixed Start client action remain recognizable; compact content retains scrolling and row clipping. |

All captures preserve embedded Inter at the established compact scale, rounded
anthracite fills, restrained coral actions, muted supporting copy, quiet
separators and the Start/Logs sidebar. The smaller sidebar and wrapped guidance
follow the existing launcher composition. The fixed primary action and Close
footer remain outside scrolling content. No additional illustration, material,
motion system or shipping raster was introduced.

These are real Dear ImGui/D3D11 offscreen renders with synthetic fixtures.
They establish composition and displayed control states, not a live KeyAuth
login or Minecraft appearance. Busy/rejected fixtures deliberately retain a
seeded masked value for presentation coverage; production clears the submitted
input buffer in the launcher event handler. The images are review evidence,
not shipping assets, so no new shipping raster requires provenance.

## Behavior and trust boundary

The configured application is **mcov / Uhn5Z5WPCa / 1.0**, using KeyAuth API 1.3
with license-only sign-in. English copy, masked input with optional Show key,
background verification and rejection recovery extend the existing workflow.
After successful verification the Minecraft/JDK flow remains available. Keys
stay in memory, clear after submission and are excluded from disk, profiles,
logs, environment variables and command-line arguments.

The authentication documentation and independent source review record a fixed
HTTPS endpoint with certificate validation, rejected redirects, bounded
responses, Ed25519 body/timestamp verification, owner/success-code validation
and future subscription expiry before acceptance. The launcher rechecks the
session before connection. A restricted, one-use inherited pipe hands off the
session rather than the license; the overlay independently checks it online
before discovery or attach and refuses direct unauthenticated startup.
Background session checks run every 60 seconds. Expiry, failed/rejected checks
or the 90-second verification lease close the overlay while leaving Minecraft
running. The timer lifecycle was reviewed in source, not exercised end-to-end
by this documentation pass.

The implementation owner reports pinned **nlohmann JSON 3.12** and
**Monocypher 4.0.2** sources and hashes, with full third-party notices embedded
as resources **104/105**. These are dependency/license resources, not new
visual assets. This pass did not independently rebuild or inspect those
resources.

## Verification and limits

The implementation owner reports all seven affected checks passing:
`keyauth`, `launcher_window`, `control_panel`, `client_controls`,
`portable_client`, `agent_package_match` and `smp_features`. Signed protocol
fixtures cover altered/missing signatures, timestamp bounds, wrong owner/code,
malformed and oversized responses, expiry, rejection/redaction, cancellation,
parameter encoding and an inherited child-pipe round trip. These checks were
not rerun during documentation.

The configured application's live initialization passed. A further live guard
check initialized a session without supplying a license and then checked that
session: the backend returned `success=false`, `code=0`. This supports the
specific boundary that initialization alone cannot authorize startup; it does
not establish successful license authentication. Both live outcomes are
implementation-owner evidence, not independently repeated here.

No valid real-license login or live Minecraft run was tested. Dashboard
configuration, live subscription revocation, the full runtime timeout/closure
path, live Minecraft attach/focus and Windows/GPU composition compatibility
remain unverified by this evidence. No HTML/CSS detector applies to this native
ImGui surface. The ship disposition is bounded by these limits.

## Existing documentation drift

`PRODUCT.md` describes 82 reachable sections; `DESIGN.md` retains 77 and
historical module-widget verification also refers to 77. This pre-existing
catalog-count drift is unrelated to the KeyAuth extension. It is recorded
without repairing global documentation or regenerating the design sidecar.
