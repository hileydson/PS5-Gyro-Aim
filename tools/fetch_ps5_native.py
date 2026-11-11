#!/usr/bin/env python3
"""Fetch pinned public native SDK/graphics dependencies into .deps, never system folders."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def download(item):
    data = urllib.request.urlopen(item['url'], timeout=90).read()
    if hashlib.sha256(data).hexdigest() != item['sha256']:
        raise ValueError('SHA-256 inesperado: ' + item['url'])
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / '.deps/ps5-native')
    args = parser.parse_args()
    directory = args.directory.resolve()
    lock = json.loads((ROOT / 'tools/ps5-native-lock.json').read_text())
    marker = directory / 'verified-lock.json'
    if marker.exists() and json.loads(marker.read_text()) == lock:
        print('Dependências nativas já verificadas:', directory)
        return
    directory.mkdir(parents=True, exist_ok=True)
    data = download(lock['app_sdk'])
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
        members = archive.getmembers()
        prefix = members[0].name.split('/')[0]
        archive.extractall(directory / 'source', filter='data')
    (directory / 'app-sdk-path.txt').write_text(str(directory / 'source' / prefix))
    deps = directory / 'libraries'
    deps.mkdir(exist_ok=True)
    for item in lock['libraries']:
        print('Verificando', item['name'], item['tag'], flush=True)
        with zipfile.ZipFile(io.BytesIO(download(item))) as archive:
            for entry in archive.infolist():
                if not (deps / entry.filename).resolve().is_relative_to(deps):
                    raise ValueError('Caminho inválido no arquivo de dependência.')
            archive.extractall(deps)
    marker.write_text(json.dumps(lock, indent=2) + '\n')
    print('Dependências prontas:', directory)


if __name__ == '__main__':
    main()
