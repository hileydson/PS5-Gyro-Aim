#!/usr/bin/env python3
"""Exercise the real server, including slow clients and transaction boundaries."""
import http.client
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.parse

ROOT = Path(__file__).resolve().parents[1]


def main():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix='gyroaim_http_') as directory:
        proc = subprocess.Popen([str(ROOT / 'build/gyroaim-linux'), '--port', str(port),
                                 '--profiles', directory, '--no-controller', '--no-virtual'],
                                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        def request(path='/api/state', data=None, headers=None):
            c = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
            body = None if data is None else urllib.parse.urlencode(data)
            hs = {'Content-Type': 'application/x-www-form-urlencoded', 'X-GyroAim': '1'}
            if headers:
                hs.update(headers)
            c.request('GET' if data is None else 'POST', path, body, hs)
            r = c.getresponse()
            raw = r.read()
            status = r.status
            c.close()
            return status, json.loads(raw) if r.getheader('Content-Type', '').startswith('application/json') else raw
        def state():
            code, s = request()
            assert code == 200
            return s
        def post(path, **fields):
            s = state()
            return request(path, {'title': s['title'], 'revision': s['revision'], **fields})
        try:
            deadline = time.monotonic() + 3
            while True:
                try:
                    initial = state()
                    break
                except OSError:
                    if time.monotonic() > deadline:
                        raise
                    time.sleep(.02)
            assert initial['input_ready'] == 0 and initial['output_ready'] == 0
            assert request('/')[0] == 200 and b'Assistente deste jogo' in request('/')[1]
            assert request('/compact')[0] == 200
            assert request('/practice')[0] == 200
            assert b'navigator.getGamepads' in request('/practice')[1]
            assert post('/api/title', next_title='GAME_A')[0] == 200
            assert post('/api/set', sensitivity=2.4, activation='R1')[0] == 200
            s = state()
            old = s['config']['sensitivity']
            # A bad key must not commit earlier valid fields.
            assert post('/api/set', sensitivity=5, missing_key=1)[0] == 400
            assert state()['config']['sensitivity'] == old
            assert post('/api/set', sensitivity='nan')[0] == 400
            assert post('/api/set', sensitivity='2junk')[0] == 400
            assert post('/api/action', action='calibrate')[0] == 400
            # Another device or a title switch invalidates previously captured state.
            stale = state()
            assert post('/api/title', next_title='GAME_B')[0] == 200
            code, _ = request('/api/set', {'title': stale['title'], 'revision': stale['revision'], 'sensitivity': 8})
            assert code == 409
            assert state()['config']['sensitivity'] != 8
            assert post('/api/title', next_title='GAME_A')[0] == 200
            assert state()['config']['sensitivity'] == 2.4
            assert post('/api/action', action='begin_test')[0] == 200
            assert post('/api/set', sensitivity=3.7)[0] == 200
            assert post('/api/action', action='revert_test')[0] == 200
            assert state()['config']['sensitivity'] == 2.4
            assert post('/api/action', action='undo')[0] == 200
            assert state()['config']['sensitivity'] == 3.7
            assert post('/api/action', action='pause')[1]['paused'] == 1
            s = state()
            code, _ = request('/api/set', {'title': s['title'], 'revision': s['revision'], 'enabled': 0},
                              {'Origin': 'https://unrelated.invalid'})
            assert code == 400 and state()['config']['enabled'] == 1
            # An incomplete body occupies a slot without blocking other requests.
            slow = socket.create_connection(('127.0.0.1', port), timeout=2)
            slow.sendall(b'POST /api/set HTTP/1.1\r\nHost: localhost\r\nContent-Length: 900\r\n\r\nx=')
            start = time.monotonic()
            assert state()['config']['sensitivity'] == 3.7
            assert time.monotonic() - start < .25, 'slow client stalled gyro loop'
            slow.close()
            # Fragmented headers/body must be accumulated and committed exactly once.
            s = state()
            body = urllib.parse.urlencode({'title': s['title'], 'revision': s['revision'], 'sensitivity': 4.1}).encode()
            raw = (f'POST /api/set HTTP/1.1\r\nHost: localhost\r\nX-GyroAim: 1\r\n'
                   f'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: {len(body)}\r\n\r\n').encode() + body
            fragmented = socket.create_connection(('127.0.0.1', port), timeout=2)
            for piece in (raw[:40], raw[40:-5], raw[-5:]):
                fragmented.sendall(piece)
                time.sleep(.025)
            response = http.client.HTTPResponse(fragmented)
            response.begin()
            assert response.status == 200
            response.read()
            fragmented.close()
            assert state()['config']['sensitivity'] == 4.1
            # Make the profile directory unwritable without relying on UID permissions.
            profile = Path(directory) / 'GAME_A.ini'
            profile.unlink()
            profile.mkdir()
            before = state()
            assert post('/api/set', sensitivity=9)[0] == 503
            after = state()
            assert after['config'] == before['config'] and after['save_error'] == 1
            assert after['revision'] == before['revision']
            print('HTTP: perfis, conflitos, transações, falhas de gravação e clientes lentos passaram')
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


if __name__ == '__main__':
    main()
