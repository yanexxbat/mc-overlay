"""Fetch official signature audit inputs into a build cache, with checksums."""
import hashlib
import json
import sys
import urllib.request
from pathlib import Path
from generate_profiles import VERSIONS


def download(url, path, algorithm, expected):
    if path.exists() and hashlib.new(algorithm, path.read_bytes()).hexdigest() == expected:
        return
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read()
    if hashlib.new(algorithm, data).hexdigest() != expected:
        raise ValueError(f'checksum mismatch: {url}')
    path.write_bytes(data)


def fetch(cache):
    cache.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen('https://piston-meta.mojang.com/mc/game/version_manifest_v2.json', timeout=30) as r:
        versions = {v['id']: v for v in json.load(r)['versions']}
    for version in VERSIONS:
        entry = versions[version]
        metadata_path = cache / (version + '.json')
        download(entry['url'], metadata_path, 'sha1', entry['sha1'])
        metadata = json.loads(metadata_path.read_text(encoding='utf-8-sig'))
        for kind, suffix in [('client', '.jar'), ('client_mappings', '-mappings.txt')]:
            if kind not in metadata['downloads']: continue
            artifact = metadata['downloads'][kind]
            download(artifact['url'], cache / (version + suffix), 'sha1', artifact['sha1'])
    inputs = json.loads(Path(__file__).with_name('mapping_inputs.json').read_text())
    for name, artifact in inputs.items():
        download(artifact['url'], cache / name, 'sha256', artifact['sha256'])
    print('Official client and namespace audit inputs verified.')


if __name__ == '__main__':
    fetch(Path(sys.argv[1]))
