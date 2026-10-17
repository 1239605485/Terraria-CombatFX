from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import struct

root = Path(__file__).resolve().parents[1]
version = json.loads((root / 'Info.json').read_text())['version']
required = {}
for abi, elf_class, machine in [('arm64', 2, 183)]:
    name = f'libcombatfx.android.{abi}.so'
    p = root / 'build' / 'package-stage' / name
    data = p.read_bytes()
    assert data[:4] == b'\x7fELF' and data[4] == elf_class and data[5] == 1, name
    assert struct.unpack_from('<H', data, 16)[0] == 3, f'{name}: not shared ELF'
    assert struct.unpack_from('<H', data, 18)[0] == machine, f'{name}: wrong machine'
    required[f'Resources/lib/{name}'] = data
for name in ['slash-tbbk.png']:
    data = (root / 'assets' / 'textures' / name).read_bytes()
    assert data[:8] == bytes([137,80,78,71,13,10,26,10]), name
    w,h,depth,color = struct.unpack_from('>IIBB',data,16)
    assert w > 0 and h > 0 and depth == 8 and color == 6, name
    required[f'Resources/textures/{name}'] = data
required['Resources/LICENSES/tbbk-CC0.txt'] = (root / 'assets/textures/LICENSE-tbbk.txt').read_bytes()
required['Resources/LICENSES/Dobby-Apache-2.0.txt'] = (root / 'include' / 'Dobby-LICENSE').read_bytes()
for name in ['Info.json', 'Manifest.json', 'mod.json']:
    required[name] = (root / name).read_bytes()
    json.loads(required[name])
assert json.loads(required['Manifest.json'])['parentLoader'] == 'eternal.future.kernelloader'
assert json.loads(required['mod.json'])['lib_name'] == 'combatfx'
(root / 'dist').mkdir(exist_ok=True)
package = root / 'dist' / f'CombatFX-{version}-Android.zip'
with ZipFile(package, 'w', ZIP_DEFLATED) as z:
    for name, data in required.items():
        z.writestr(name, data)
with ZipFile(package) as z:
    assert set(z.namelist()) == set(required)
    assert z.testzip() is None
print(f'{package.name} SHA256 {hashlib.sha256(package.read_bytes()).hexdigest()}')
