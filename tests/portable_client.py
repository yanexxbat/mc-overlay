"""Single-file deployment check, with no game discovery, UI or JVM attachment."""
import concurrent.futures
import hashlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

exe, binaries, destination, archive = map(Path, sys.argv[1:])
destination.mkdir(parents=True, exist_ok=True)
destination = Path(tempfile.mkdtemp(prefix='run-', dir=destination))
with zipfile.ZipFile(archive) as package:
    assert sorted(package.namelist()) == ['START.txt', 'mcov.exe'], 'share ZIP must contain exactly two root files'
    assert package.read('mcov.exe') == exe.read_bytes(), 'share ZIP contains an outdated executable'
    assert 'INSERT' in package.read('START.txt').decode('utf-8'), 'start guide is missing controls'
    shared = destination / 'Entpackter Client'
    package.extractall(shared)
alone = destination / 'Nur eine Datei - Prüfung'
alone.mkdir(parents=True, exist_ok=True)
portable = alone / 'mcov.exe'
shutil.copyfile(shared / 'mcov.exe', portable)
assert list(alone.iterdir()) == [portable], 'portable folder has extra dependencies'
image = portable.read_bytes()
pe = struct.unpack_from('<I', image, 0x3c)[0]
assert image[pe:pe+4] == b'PE\0\0'
assert struct.unpack_from('<H', image, pe+24+68)[0] == 2, 'launcher must be a GUI executable'
cache = destination / 'Cache mit Leerzeichen und Umlauten ä'

def prepare():
    result = subprocess.run([str(portable), '--verify-package', str(cache)], timeout=30,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    assert result.returncode == 0, 'standalone resource extraction failed'

prepare()
assert list(alone.iterdir()) == [portable], 'standalone start left helper files next to the EXE'
payloads = ['mcov_overlay.exe', 'mcov_agent.dll', 'tools/AgentLoader.class',
            'tools/frame-sync-agent.jar', 'tools/frame-signal.jar']
versions = list((cache / 'runtime').iterdir())
assert len(versions) == 1, 'one package must use one deterministic cache version'
runtime = versions[0]
hash_file = lambda p: hashlib.sha256(p.read_bytes()).digest()
for name in payloads:
    assert hash_file(runtime / name) == hash_file(binaries / name), f'wrong bundled payload: {name}'
timestamps = [(runtime / name).stat().st_mtime_ns for name in payloads]
prepare()
assert timestamps == [(runtime / name).stat().st_mtime_ns for name in payloads], 'valid cache unnecessarily rewritten'
(runtime / 'tools/frame-signal.jar').write_bytes(b'corrupt cache')
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(lambda _: prepare(), range(2)))
for name in payloads:
    assert hash_file(runtime / name) == hash_file(binaries / name), f'cache repair failed: {name}'
for invalid in ['0', 'not-a-pid', '4294967296']:
    result = subprocess.run([str(runtime / 'mcov_overlay.exe'), '--no-agent', '--pid', invalid],
                            timeout=10, creationflags=subprocess.CREATE_NO_WINDOW,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert result.returncode == 64, 'invalid PID must stop before any game discovery or attachment'
print('PASS portable GUI: two-file ZIP, current EXE, standalone file, exact payloads, Unicode/spaces, idempotency, corruption repair and concurrent extraction')
