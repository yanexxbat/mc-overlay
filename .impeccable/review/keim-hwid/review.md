# Launcher HWID verification

The fixed launcher footer displays the existing KeyAuth hardware identity: the
Windows account SID. The launcher reads it once at startup through the same
function used by license verification. The read-only field supports selection
and horizontal scrolling; its tooltip shows the full value. Copy HWID copies
the complete value on explicit activation, including before sign-in, during
verification and in Logs. Unavailable IDs disable copying without blocking the UI.
No new fingerprint, network request, persistence, log entry or game work is added.

| Surface | Normal | Compact |
|---|---|---|
| Before login | [640x640](keyauth-empty-desktop.png) | [480x380](keyauth-empty-small.png) |
| Native Logs | — | [480x380](launcher-native-diagnose.png) |
| Unavailable | — | [480x380](hwid-unavailable-small.png) |

These are native ImGui/D3D11 fixture captures with a synthetic SID. One batched
inspection confirmed a readable, fixed footer with reachable Copy HWID and Close
controls; compact content continues to scroll above the primary action. No
visual correction loop was needed. Captures contain no actual account identifier.

All six relevant checks passed in 4.16 seconds: keyauth, launcher_window,
flat_ui, client_controls, control_panel and portable_client. Input checks copy
the complete synthetic ID at both sizes before login, while busy and in Logs,
and ensure unavailable copying leaves the clipboard untouched. Clipboard tests
use in-memory callbacks and never modify the user's OS clipboard. The signed
protocol fixture checks that login submits exactly the locally exposed HWID.
The fixture reads a local SID without printing or recording its value and
uses no live KeyAuth request. No live Minecraft was started or attached.

Keim.exe and Keim-client.zip were rebuilt. Package checks confirm the exact
portable payloads, embedded mascot/icon and standalone extraction.
