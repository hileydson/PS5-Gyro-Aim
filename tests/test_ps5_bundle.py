#!/usr/bin/env python3
"""Validate only the produced homebrew layout/identity/integrity; not hardware compatibility."""
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

root=Path(__file__).resolve().parents[1]
build=root/'build/ps5'
manifest=json.loads((build/'manifest.json').read_text())
assert manifest['native_app'] and not manifest['tested_on_console'] and not manifest['game_input_hook']
title=build/'homebrew'/manifest['title_id']
param=json.loads((title/'sce_sys/param.json').read_text())
assert param['titleId']==manifest['title_id']=='PPSA94001'
assert len(param['contentId'].rsplit('-',1)[1])==16
assert param['localizedParameters']['pt-BR']['titleName']=='Gyro Aim'
for name,digest in manifest['components'].items():
    data=(build/name).read_bytes()
    assert hashlib.sha256(data).hexdigest()==digest, name
for name in ('eboot.bin','sce_module/libc.prx'):
    assert struct.unpack('<I',(title/name).read_bytes()[:4])[0] in (0xEEF51454,0x1D3D154F)
plugin=(build/'etaHEN/plugins/gyroaim-service.plugin').read_bytes()
parts=plugin.split(b'\0',3)
assert parts[:3]==[b'etaHEN_PLUGIN',b'GYRO00001',b'1.00']
assert parts[3].startswith(b'\x7fELF\x02\x01')
png=(title/'sce_sys/icon0.png').read_bytes()
assert png[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',png[16:24])==(512,512)
with zipfile.ZipFile(root/'build/GyroAim-PS5-experimental.zip') as z:
    assert z.testzip() is None
    assert 'homebrew/PPSA94001/eboot.bin' in z.namelist()
    assert 'etaHEN/plugins/gyroaim-service.plugin' in z.namelist()
    assert 'INSTALL.txt' in z.namelist()
print('Pacote PS5: identidade, contêineres, hashes, ícone e layout passaram; execução no console pendente')
