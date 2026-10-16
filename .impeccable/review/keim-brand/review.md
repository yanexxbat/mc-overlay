# Keim branding verification

Outcome: Keim.exe and Keim-client.zip are built and ready for use. The user's
green germ mascot replaces the former circular brand mark in launcher/client
headers and the HUD watermark. The lowercase keim wordmark, native window
titles, user-facing license/errors/config guidance, Windows icon and PE product
metadata use Keim. The approved anthracite/coral controls remain intact.

| Surface | Normal fixture | Compact fixture |
|---|---|---|
| Native launcher | [640x640](launcher-native-start.png) | [480x380](launcher-native-diagnose.png) |
| License entry | [640x640](keyauth-empty-desktop.png) | [480x380](keyauth-empty-small.png) |
| Module menu | [1280x800](module-grid-combat.png) | [480x360](module-grid-small.png) |
| HUD watermark | [1280x800](modular-hud.png) | Small-view HUD bounds/input covered by the existing fixture |

These are actual ImGui/D3D11 native and WARP fixtures with synthetic data, not
Minecraft screenshots. One batched inspection found no branding layout defects;
no visual polish/rebuild loop was needed. The native launcher test verifies
green mascot pixels and a loaded Windows class icon. The control-panel test
verifies that the embedded colour glyph exists. Windows version metadata was
inspected: ProductName Keim, FileDescription Keim Client.

All 31 tests passed: the 30-test sandbox run took 95.65 seconds and the isolated
frame_attach test passed outside the sandbox in 0.55 seconds. The portable test
checks exactly Keim.exe and START.txt in the ZIP, matching EXE/payload hashes,
the exact embedded mascot resource and the Windows icon resource, standalone
extraction, cache repair and concurrent preparation. Existing auth/config/HUD,
Region Map and runtime compatibility checks also pass.

The new logo uses one 128x128 colour cell in the existing font atlas. It adds no
separate game texture, model extraction, render-thread JNI or per-frame upload.
The ICO includes 16, 24, 32, 48, 64, 128 and 256px sizes. Transparent source and
the Imagegen edit prompt are retained in [the brand asset notes](../../../design/brand/README.md).

Existing KeyAuth application identifiers, IPC/JNI names, settings/config headers
and module bindings stay compatible. Portable preferences/configs remain beside
the client. Runtime data now uses the Keim AppData folder, with old JDK/preferences
imported when the new/local file is absent. Pre-existing Region Map changes were
preserved. No live Minecraft session, real KeyAuth login or domain purchase was
performed for this change.

Keim.exe SHA-256:
`ECC9375AD9E3F630293AFF72698D172872F5A27A2F3B94979D6FECD381B67F7F`

Keim-client.zip SHA-256:
`2F320273A11902F44195B9A6E4CB98EBC9764876CC00D25B48673ABBD56DF19C`
