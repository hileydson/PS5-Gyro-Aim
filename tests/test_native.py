#!/usr/bin/env python3
"""Exercise console UI/service communication without a console or real controller."""
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    port = sock.getsockname()[1]
with tempfile.TemporaryDirectory(prefix='gyroaim_native_') as directory:
    proc = subprocess.Popen([str(ROOT/'build/gyroaim-linux'),'--port',str(port),'--title','NATIVE_TEST',
                             '--profiles',directory,'--no-controller','--no-virtual'],
                            stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic()+3
        while True:
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{port}/api/native',timeout=.2) as response:
                    assert response.headers['Content-Type']=='application/octet-stream'
                    assert len(response.read())==2732
                break
            except OSError:
                if time.monotonic()>deadline:
                    raise
                time.sleep(.02)
        subprocess.run([str(ROOT/'build/test_native_client'),str(port)],check=True)
    finally:
        proc.terminate()
        proc.wait(timeout=3)
