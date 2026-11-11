#!/usr/bin/env python3
"""Build native PS5 homebrew + resident etaHEN service. Never connects to a console."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TITLE_ID = 'PPSA94001'


def run(*args):
    subprocess.run([str(x) for x in args], cwd=ROOT, check=True)


def icon(path):
    # Code-native application icon: target + G/A, no external artwork.
    rows = bytearray()
    glyphs = [(112, [14, 17, 16, 23, 17, 17, 15]), (282, [14, 17, 17, 31, 17, 17, 17])]
    for y in range(512):
        rows.append(0)
        for x in range(512):
            r = math.hypot(x - 256, y - 225)
            c = (14, 24, 41)
            if 171 < r < 178 or (abs(x-256) < 3 and (30<y<82 or 370<y<423)) or (abs(y-225)<3 and (60<x<112 or 400<x<452)):
                c = (121, 182, 255)
            for left, pattern in glyphs:
                col, row = (x-left)//16, (y-170)//16
                if 0 <= col < 5 and 0 <= row < 7 and pattern[row] & (1 << (4-col)):
                    c = (234, 242, 255)
            rows.extend((*c, 255))
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB',512,512,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', type=Path, default=os.environ.get('PS5_PAYLOAD_SDK'))
    p.add_argument('--native-sdk', type=Path, default=os.environ.get('PS5_NATIVE_SDK'))
    p.add_argument('--native-deps', type=Path, default=os.environ.get('PS5_NATIVE_DEPS'))
    p.add_argument('--service-only', action='store_true')
    p.add_argument('--lab', action='store_true', help='Build the exact same settings UI for Linux only')
    args = p.parse_args()
    cache = ROOT / '.deps/ps5-native'
    sdk_path = cache / 'app-sdk-path.txt'
    app = args.native_sdk or (Path(sdk_path.read_text().strip()) if sdk_path.exists() else None)
    deps = args.native_deps or cache / 'libraries'
    output = ROOT / 'build/ps5'
    output.mkdir(parents=True, exist_ok=True)
    sources = ['src/native/main.c','src/native/client.c','src/core/native_protocol.c','src/core/profile.c','src/core/gyro_aim.c']
    if args.lab:
        if not (deps / 'include/SDL2/SDL.h').is_file():
            p.error('Execute make ps5-deps ou defina PS5_NATIVE_DEPS.')
        run(os.environ.get('CC','cc'), '-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Werror',
            '-I'+str(deps/'include'), *sources, '-o', ROOT/'build/gyroaim-native-lab', '-l:libSDL2-2.0.so.0','-lm')
        return
    if not args.sdk:
        p.error('Defina PS5_PAYLOAD_SDK ou --sdk.')
    sdk = args.sdk.resolve()
    cc = sdk / 'bin/prospero-clang'
    if not cc.is_file():
        p.error('prospero-clang não encontrado no SDK informado.')
    common = ['-O2','-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Wno-unused-parameter','-Wno-unreachable-code-generic-assoc']
    core = ['src/core/native_protocol.c','src/core/gyro_aim.c','src/core/profile.c','src/core/app.c','src/core/webui.c','src/core/webui_page.c']
    service = output / 'gyroaim-service.elf'
    run(cc,*common,*core,'src/ps5/main.c','-o',service,'-lSceSystemService','-lkernel_sys')
    elf = service.read_bytes()
    if elf[:6] != b'\x7fELF\x02\x01':
        raise ValueError('O serviço não é um ELF 64-bit little-endian.')
    plugin = output / 'etaHEN/plugins/gyroaim-service.plugin'
    plugin.parent.mkdir(parents=True, exist_ok=True)
    plugin.write_bytes(b'etaHEN_PLUGIN\x00GYRO00001\x001.00\x00' + elf)
    if args.service_only:
        print('Serviço etaHEN gerado:', plugin)
        return
    if not app or not (app/'tools/ps5-native-tool/native_app_builder.cpp').is_file():
        p.error('Execute make ps5-deps ou defina PS5_NATIVE_SDK e PS5_NATIVE_DEPS.')
    objdir = output / 'obj'; objdir.mkdir(exist_ok=True)
    tool_dir = output / 'tools'; tool_dir.mkdir(exist_ok=True)
    tool = tool_dir / 'ps5-native-tool'; runtime_tool = tool_dir / 'libc-prx-builder'
    host_cxx = os.environ.get('CXX','g++')
    cpp = sorted((app/'tools/ps5-native-tool').glob('*.cpp'))
    if not tool.exists() or any(x.stat().st_mtime > tool.stat().st_mtime for x in cpp):
        run(host_cxx,'-std=c++20','-O2',*cpp,'-o',tool,'-lz')
    rt_source = app/'tools/libc-prx-builder/libc_builder.cpp'
    if not runtime_tool.exists() or rt_source.stat().st_mtime > runtime_tool.stat().st_mtime:
        run(host_cxx,'-std=c++20','-O2',rt_source,'-o',runtime_tool,'-lz')
    stubs = []
    for name, suffix in [('agc',''), ('agc_driver','Driver')]:
        stub = objdir / ('libSceAgc'+suffix+'.so'); stubs.append(stub)
        run(cc,'-shared','-fPIC','-nostdlib','-Wl,-soname,libSceAgc'+suffix+'.sprx',app/'agc_stubs/src'/(name+'_link_stub.c'),'-o',stub)
    objects = []
    extra = ['-resource-dir='+os.environ['CLANG_RESOURCE_DIR']] if os.environ.get('CLANG_RESOURCE_DIR') else []
    for source in [*sources,'src/ps5/app_start.c','src/ps5/native_compat.c']:
        obj = objdir / (Path(source).stem+'.o'); objects.append(obj)
        run(cc,*common,*extra,'-D__PROSPERO__','-fPIC','-ffunction-sections','-fdata-sections','-I'+str(deps/'include'),'-c',source,'-o',obj)
    pie = output / 'gyroaim-native-pie.elf'
    run(cc,'-nostartfiles','-T'+str(app/'ps5-elf.ld'),'-Wl,--version-script='+str(app/'ps5-app-symbols.map'),
        '-Wl,--gc-sections','-Wl,-e,_start','-o',pie,*objects,'-L'+str(objdir),'-L'+str(deps/'lib'),
        '-Wl,--start-group',*sorted((deps/'lib').glob('*.a')),'-lc++','-lc++abi','-lunwind','-Wl,--end-group',
        '-lScePad','-lSceUserService','-lSceVideoOut','-lSceSystemService','-lSceAudioOut','-lSceImeDialog',
        '-lSceKeyboard','-lSceAgc','-lSceAgcDriver','-lkernel_sys')
    native = output / 'gyroaim-native.elf'
    run(tool,'link','--in',pie,'--out',native,'--stub-dir',sdk/'target/lib','--stub',stubs[0],
        '--stub',stubs[1],'--module-sdk','0x02000009','--companion-sdk','0x08050001','--file-name','eboot.elf')
    title = output / 'homebrew' / TITLE_ID
    (title/'sce_sys').mkdir(parents=True, exist_ok=True)
    (title/'sce_module').mkdir(exist_ok=True)
    run(tool,'self','--sign','--in',native,'--out',title/'eboot.bin','--app-version','0x01000000')
    runtime = output/'libc-runtime.elf'
    run(runtime_tool,app/'tools/libc-prx-builder/api-surface.txt',app/'tools/libc-prx-builder/imports.txt',runtime)
    run(tool,'self','--sign','--in',runtime,'--out',title/'sce_module/libc.prx')
    run(tool,'self','--inspect','--file',title/'eboot.bin')
    run(tool,'self','--inspect','--file',title/'sce_module/libc.prx')
    param = {
        'titleId':TITLE_ID,'conceptId':'94001','contentId':'UP9000-PPSA94001_00-GYROAIM000000001',
        'contentVersion':'01.000.000','masterVersion':'01.00',
        'applicationCategoryType':0,'applicationDrmType':'free','attribute':1644167168,
        'attribute2':0,'attribute3':1048576,'contentBadgeType':1,'downloadDataSize':256,
        'requiredSystemSoftwareVersion':'0x0000000000000000','sdkVersion':'0x0000000000000000',
        'localizedParameters':{'defaultLanguage':'pt-BR','pt-BR':{'titleName':'Gyro Aim'},'en-US':{'titleName':'Gyro Aim'}},
        'versionFileUri':''}
    (title/'sce_sys/param.json').write_text(json.dumps(param,indent=2)+'\n')
    icon(title/'sce_sys/icon0.png')
    manifest={'title_id':TITLE_ID,'service_id':'GYRO00001','native_app':True,'tested_on_console':False,
              'game_input_hook':False,'status':'experimental-configuration-build',
              'components':{str(f.relative_to(output)):hashlib.sha256(f.read_bytes()).hexdigest()
              for f in [title/'eboot.bin',title/'sce_module/libc.prx',plugin]}}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    install = output / 'INSTALL.txt'
    install.write_text(
        'GYRO AIM - BUILD EXPERIMENTAL NATIVA PS5\n\n'
        'Esta build ainda nao aplica gyro nos jogos. A interface e o servico foram compilados,\n'
        'mas instalacao/execucao no firmware 13.60 ainda nao foram validadas.\n\n'
        'homebrew/PPSA94001: pasta do aplicativo para um registrador homebrew compativel.\n'
        'etaHEN/plugins/gyroaim-service.plugin: servico para a Toolbox do etaHEN.\n'
        'O app se comunica com o servico no proprio PS5. Nao precisa de PC durante o uso.\n'
        'Celular: http://IP-DO-PS5:8090 na mesma rede, com o servico em execucao.\n'
        'Fontes, licencas e instrucoes: docs/PS5.md e docs/THIRD_PARTY.md do projeto.\n'
        'Nao e um PKG. Nao execute os exemplos de instalacao que alteram /system_ex.\n')
    notice = output / 'THIRD_PARTY.md'
    shutil.copyfile(ROOT/'docs/THIRD_PARTY.md', notice)
    archive = ROOT/'build/GyroAim-PS5-experimental.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for folder in [output/'homebrew',output/'etaHEN']:
            for file in sorted(folder.rglob('*')):
                if file.is_file(): z.write(file, str(file.relative_to(output)))
        for file in [output/'manifest.json', install, notice]: z.write(file, file.name)
        z.write(ROOT/'tools/ps5-native-lock.json','ps5-native-lock.json')
    print('App nativo e serviço gerados em', output)
    print('Pacote homebrew experimental:', archive)


if __name__ == '__main__':
    main()
