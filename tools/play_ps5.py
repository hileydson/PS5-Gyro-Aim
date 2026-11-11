#!/usr/bin/env python3
"""Run Gyro Aim + a local Chiaki client without patching the PS5.

Optional --ps5-panel reads the PS5 companion daemon for automatic game profiles.
No account IDs, pairing keys or passwords are read or changed by this launcher.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

from ps5_discovery import query_console, StableTitle

ROOT = Path(__file__).resolve().parents[1]
VIRTUAL_PAD = '0x045e/0x028e'


def client_command(explicit=None):
    if explicit:
        found = shutil.which(explicit)
        if not found:
            raise ValueError('Cliente não encontrado: ' + explicit)
        return [found]
    for name in ('chiaki-ng', 'chiaki'):
        if found := shutil.which(name):
            return [found]
    if shutil.which('flatpak'):
        for app in ('io.github.streetpea.Chiaki4deck', 're.chiaki.Chiaki'):
            if subprocess.run(['flatpak', 'info', app], stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL).returncode == 0:
                return ['flatpak', 'run', app]
    raise ValueError('Instale e pareie o Chiaki/chiaki-ng ou informe --client CAMINHO.')


def controller_env():
    env = os.environ.copy()
    # SDL2 and SDL3 use the same VID/PID allowlist hint. Scope to child only.
    env['SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT'] = VIRTUAL_PAD
    env['SDL_GAMECONTROLLER_IGNORE_DEVICES'] = ''
    env['SDL_JOYSTICK_HIDAPI'] = '0'
    env['SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS'] = '1'
    return env


def get_state(base):
    with urllib.request.urlopen(base.rstrip('/') + '/api/state', timeout=.75) as r:
        data = r.read(16385)
        if len(data) > 16384:
            raise ValueError('Resposta grande demais do painel.')
        return json.loads(data)


def select_title(base, title, name="", source="manual"):
    current = get_state(base)
    if current['title'] == title and current.get('title_name', '') == name and current.get('title_source') == source:
        return
    body = urllib.parse.urlencode({'title': current['title'], 'revision': current['revision'],
                                   'next_title': title, 'next_name': name, 'source': source}).encode()
    request = urllib.request.Request(base + '/api/title', body, headers={
        'Content-Type': 'application/x-www-form-urlencoded', 'X-GyroAim': '1'})
    with urllib.request.urlopen(request, timeout=.75) as r:
        r.read()


def watched_title(base):
    state = get_state(base)
    title = state.get('title', '')
    if state.get('platform') != 'ps5' or state.get('simulated') != 0 or not (
            title == 'default' or re.fullmatch(r'(?:PPSA|CUSA)[0-9]{5}', title)):
        raise ValueError('A origem não é um painel PS5 válido.')
    return {'title': title, 'name': state.get('title_name', ''), 'source': 'ps5_panel'}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', type=int, default=8090)
    p.add_argument('--title', default='default', help='Perfil manual; ex.: PPSA01234')
    p.add_argument('--profiles', default=str(ROOT / 'profiles'))
    p.add_argument('--console', help='IPv4 do PS5: tenta detectar o jogo pela rede, sem payload')
    p.add_argument('--ps5-panel', help='http://IP-DO-PS5:8090; leitura do título, sem hook')
    p.add_argument('--client', help='Executável Chiaki/chiaki-ng; argumentos depois de --')
    p.add_argument('--dry-run', action='store_true', help='Mostra os comandos sem iniciar nada')
    p.add_argument('client_args', nargs=argparse.REMAINDER)
    args = p.parse_args(argv)
    if not 1 <= args.port <= 65535 or not re.fullmatch(r'[A-Za-z0-9_-]{1,63}', args.title):
        p.error('Porta ou identificador de jogo inválido.')
    if args.console:
        import ipaddress
        try:
            ipaddress.IPv4Address(args.console)
        except ValueError:
            p.error('--console deve ser o IPv4 do PS5.')
    if args.ps5_panel:
        url = urllib.parse.urlsplit(args.ps5_panel)
        if url.scheme not in ('http', 'https') or not url.hostname or url.username or url.password:
            p.error('--ps5-panel deve ser o endereço http(s) do painel no console.')
    try:
        client = client_command(args.client)
    except ValueError as e:
        p.error(str(e))
    extra = args.client_args[1:] if args.client_args[:1] == ['--'] else args.client_args
    client += extra
    gyro = [str(ROOT / 'build/gyroaim-linux'), '--remote-play', '--port', str(args.port),
            '--profiles', args.profiles, '--title', args.title]
    if args.dry_run:
        print(json.dumps({'gyroaim': gyro, 'client': client, 'client_environment': {
            k: v for k, v in controller_env().items() if k.startswith('SDL_')},
            'title_source': args.ps5_panel or args.console or 'manual'}, indent=2, ensure_ascii=False))
        return 0
    if not Path(gyro[0]).is_file():
        p.error('Compile primeiro com make.')
    with socket.socket() as probe:
        if probe.connect_ex(('127.0.0.1', args.port)) == 0:
            p.error('Já existe um servidor nessa porta. Encerre a outra sessão ou use --port.')
    base = f'http://127.0.0.1:{args.port}'
    def remote_title():
        if args.ps5_panel:
            return watched_title(args.ps5_panel)
        return query_console(args.console)

    processes = []
    try:
        # No grab until explicitly launched by the user; this routine owns its children.
        log_path = ROOT / 'build/remote-play.log'
        with log_path.open('w') as log:
            bridge = subprocess.Popen(gyro, stdin=subprocess.DEVNULL, stdout=log, stderr=log)
            processes.append(bridge)
            deadline = time.monotonic() + 8
            while True:
                if bridge.poll() is not None:
                    raise RuntimeError('Gyro Aim encerrou. Veja build/remote-play.log.')
                try:
                    state = get_state(base)
                    if state['input_ready'] and state['output_ready']:
                        break
                except (OSError, ValueError):
                    pass
                if time.monotonic() >= deadline:
                    raise RuntimeError('Sensor ou /dev/uinput indisponível. Veja build/remote-play.log e encerre outras sessões que usam o controle.')
                time.sleep(.15)
            tracker = StableTitle()
            if args.ps5_panel or args.console:
                select_title(base, 'default', source='ps5_panel' if args.ps5_panel else 'discovery')
            player = subprocess.Popen(client, env=controller_env())
            processes.append(player)
            print(f'Painel: {base} · no celular: http://IP-DESTE-PC:{args.port}', flush=True)
            print('Veja o jogo na TV; conecte no Chiaki. Fechar o cliente encerra esta sessão.', flush=True)
            print('Controle virtual não encaminha háptica, gatilhos adaptáveis ou touchpad. Ctrl+C encerra.', flush=True)
            last_warning = False
            while player.poll() is None:
                if bridge.poll() is not None:
                    raise RuntimeError('Gyro Aim encerrou durante a sessão.')
                if args.ps5_panel or args.console:
                    try:
                        result = remote_title()
                        if tracker.observe(result):
                            select_title(base, result['title'], result['name'], result['source'])
                            tracker.mark_applied()
                        last_warning = False
                    except (OSError, ValueError):
                        # Unknown game -> safe menu profile; never use an old title indefinitely.
                        tracker = StableTitle()
                        try:
                            select_title(base, 'default', source='ps5_panel' if args.ps5_panel else 'discovery')
                        except (OSError, ValueError):
                            pass
                        if not last_warning:
                            print('Leitura do jogo indisponível: gyro suspenso até identificar o jogo. Para perfil manual, reinicie sem --console/--ps5-panel.', flush=True)
                            last_warning = True
                time.sleep(.5)
            return player.returncode
    except KeyboardInterrupt:
        return 130
    except (OSError, RuntimeError, ValueError) as e:
        print(str(e), file=sys.stderr)
        return 1
    finally:
        for child in reversed(processes):
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


if __name__ == '__main__':
    sys.exit(main())
