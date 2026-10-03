"""Generate a deterministic item atlas from a checksum-verified local client.

Run: python tools/equipment/generate_icons.py build/compat-cache
No download, game code or resource-pack/runtime access is performed.
"""
import hashlib
import io
import json
import math
from pathlib import Path
import sys
import zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
VERSION = '26.2'


def generate(cache):
    metadata = json.loads((cache / f'{VERSION}.json').read_text(encoding='utf-8-sig'))
    source = cache / f'{VERSION}.jar'
    if hashlib.sha1(source.read_bytes()).hexdigest() != metadata['downloads']['client']['sha1']:
        raise ValueError('Client checksum mismatch')
    prefix = 'assets/minecraft/textures/item/'
    with zipfile.ZipFile(source) as jar:
        entries = sorted((Path(name).stem, name) for name in jar.namelist()
                         if name.startswith(prefix) and name.endswith('.png') and
                         '/' not in name[len(prefix):])
        columns, cell, size = 32, 20, 16
        width, height = columns * cell, math.ceil((len(entries)+1)/columns)*cell
        atlas = Image.new('RGBA', (width, height))
        for index, (name, path) in enumerate(entries, 1):
            icon = Image.open(io.BytesIO(jar.read(path))).convert('RGBA')
            icon = icon.crop((0, 0, icon.width, min(icon.width, icon.height)))
            icon = icon.resize((size, size), Image.Resampling.NEAREST)
            atlas.paste(icon, ((index % columns)*cell+2, (index // columns)*cell+2))
        output = ROOT / 'src/resources/equipment_atlas.png'
        output.parent.mkdir(exist_ok=True)
        atlas.save(output, optimize=True)
    lines = ['// Generated from the audited 26.2 vanilla item textures. Do not edit.',
             '#pragma once', '#include <algorithm>', '#include <array>',
             '#include <cstdint>', '#include <string_view>', 'namespace mcov::equipment_icons {',
             f'inline constexpr int kWidth={width}, kHeight={height}, kColumns={columns}, kCell={cell}, kSize={size};',
             f'inline constexpr std::array<std::string_view,{len(entries)}> kNames={{']
    lines.extend(f'    "{name}",' for name, _ in entries)
    lines += ['};', 'inline std::uint16_t Find(std::string_view key) noexcept {',
              '    constexpr std::string_view prefix="item.minecraft.";',
              '    if (!key.starts_with(prefix)) return 0;',
              '    key.remove_prefix(prefix.size());',
              '    const auto found=std::lower_bound(kNames.begin(),kNames.end(),key);',
              '    return found!=kNames.end() && *found==key ? std::uint16_t(found-kNames.begin()+1) : 0;',
              '}', '} // namespace mcov::equipment_icons', '']
    (ROOT / 'src/common/equipment_icons.hpp').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
    print(f'Generated {len(entries)} vanilla icons, atlas {width}x{height}: {output}')


if __name__ == '__main__':
    generate(Path(sys.argv[1]))
