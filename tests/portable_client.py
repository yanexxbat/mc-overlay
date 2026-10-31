"""Single-file deployment check, with no game discovery, UI or JVM attachment."""
import concurrent.futures
import ctypes
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
    assert sorted(package.namelist()) == ['Keim.exe', 'START.txt'], 'share ZIP must contain exactly two root files'
    assert package.read('Keim.exe') == exe.read_bytes(), 'share ZIP contains an outdated executable'
    assert 'INSERT' in package.read('START.txt').decode('utf-8'), 'start guide is missing controls'
    shared = destination / 'Entpackter Client'
    package.extractall(shared)
alone = destination / 'Nur eine Datei - Prüfung'
alone.mkdir(parents=True, exist_ok=True)
portable = alone / 'Keim.exe'
shutil.copyfile(shared / 'Keim.exe', portable)
assert list(alone.iterdir()) == [portable], 'portable folder has extra dependencies'
image = portable.read_bytes()
for binary in (image, (binaries / 'mcov_overlay.exe').read_bytes()):
    for marker in ('keyauth.win', '--auth-pipe', 'Verify license'):
        assert marker.encode() not in binary and marker.encode('utf-16le') not in binary, 'obsolete authentication remains in the shipped binaries'
pe = struct.unpack_from('<I', image, 0x3c)[0]
assert image[pe:pe+4] == b'PE\0\0'
assert struct.unpack_from('<H', image, pe+24+68)[0] == 2, 'launcher must be a GUI executable'
# Branding must travel inside the single executable, with no loose image files.
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint32]
kernel.LoadLibraryExW.restype = ctypes.c_void_p
kernel.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
kernel.FindResourceW.restype = ctypes.c_void_p
kernel.LoadResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.LoadResource.restype = ctypes.c_void_p
kernel.LockResource.argtypes = [ctypes.c_void_p]
kernel.LockResource.restype = ctypes.c_void_p
kernel.SizeofResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.SizeofResource.restype = ctypes.c_uint32
kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
module = kernel.LoadLibraryExW(str(portable), None, 0x22)  # resource-only; no entry point executes
assert module, 'could not inspect embedded branding'
try:
    logo = kernel.FindResourceW(module, 106, 10)
    assert logo, 'embedded Keim mascot is missing'
    pixels = ctypes.string_at(kernel.LockResource(kernel.LoadResource(module, logo)), kernel.SizeofResource(module, logo))
    expected = Path(__file__).resolve().parents[1] / 'src/resources/keim-logo.rgba'
    assert pixels == expected.read_bytes(), 'embedded Keim mascot is outdated'
    assert kernel.FindResourceW(module, 107, 14), 'Keim Windows icon is missing'
finally:
    kernel.FreeLibrary(module)
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
for name in ['tools/frame-sync-agent.jar', 'tools/frame-signal.jar']:
    with zipfile.ZipFile(runtime / name) as helper:
        assert not any('knife' in entry.lower() for entry in helper.namelist()), 'removed knife renderer is still bundled'

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
