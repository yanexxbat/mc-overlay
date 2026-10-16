# Keim mascot assets

The user supplied `keim-reference.png` and selected this character for the client.
The transparent master is [keim-logo.png](../../src/resources/keim-logo.png).
The built-in Imagegen tool produced the transparent cutout; no CLI/API fallback
was used. The original reference is retained unchanged.

Prompt used:

> Edit target: the attached green cartoon germ mascot, which is the user's chosen
> logo for the Keim client. Produce a clean transparent PNG cutout of this SAME
> mascot. Remove only the white background, black screenshot border and rounded
> screenshot corners. Preserve the exact character design, silhouette, eyes,
> pupils, huge yellow-toothed grin, antennas, lime-green shading, dark-green
> outline and green floating bubbles. Do not redesign, do not change its
> expression, do not add lettering or any new objects. Keep all existing whites
> inside the eyes, highlights and teeth; only the surrounding background is
> transparent. Square canvas with modest transparent padding around the complete
> mascot and bubbles, no cropping. High-resolution crisp edges suitable for a
> desktop application logo and small window icon.

`tools/brand/pack_logo.py` scales/encodes this approved master to a 128x128 RGBA
font-atlas cell and a Windows ICO with 16, 24, 32, 48, 64, 128 and 256px frames.
The master retains alpha transparency. The executable resources require no image
downloads or additional files when running the client. Regenerate the native
assets with Python and Pillow after replacing the approved master.
