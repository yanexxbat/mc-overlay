"""Encode the approved transparent PNG as native icon and atlas resources.

This only scales and encodes the existing asset; it does not generate or redraw
the character. Run with Python and Pillow from the repository root.
"""
from pathlib import Path
from PIL import Image

resources = Path(__file__).resolve().parents[2] / 'src/resources'
with Image.open(resources / 'keim-logo.png') as source:
    image = source.convert('RGBA')
    (resources / 'keim-logo.rgba').write_bytes(
        image.resize((128, 128), Image.Resampling.LANCZOS).tobytes())
    image.resize((256, 256), Image.Resampling.LANCZOS).save(
        resources / 'keim.ico', format='ICO',
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
