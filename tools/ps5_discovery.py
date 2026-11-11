#!/usr/bin/env python3
"""Read PS5 Remote Play discovery metadata over LAN; no wakeup or credentials.
Protocol constants/fields: chiaki-ng lib/src/discovery.c and discovery.h.
A missing running title is treated as unknown/menu, never guessed from a name.
"""
import argparse
import ipaddress
import json
import re
import socket

SEARCH = b'SRCH * HTTP/1.1\ndevice-discovery-protocol-version:00030010\n'
PORT = 9302


def parse_response(data):
    if len(data) > 4096 or b'\x00' in data:
        raise ValueError('Resposta de descoberta inválida.')
    lines = data.decode('utf-8', errors='strict').splitlines()
    if not lines or not re.fullmatch(r'HTTP/1\.[01] (?:200|620)(?: .*)?', lines[0]):
        raise ValueError('Estado de descoberta inválido.')
    headers = {}
    for line in lines[1:]:
        if not line:
            continue
        key, sep, value = line.partition(':')
        key, value = key.strip().lower(), value.strip()
        if not sep or key in headers:
            raise ValueError('Cabeçalho de descoberta inválido.')
        headers[key] = value
    if headers.get('device-discovery-protocol-version') != '00030010':
        raise ValueError('Resposta não identificada como PS5.')
    title = headers.get('running-app-titleid', '')
    if title and not re.fullmatch(r'(?:PPSA|CUSA)[0-9]{5}', title):
        raise ValueError('Title ID inválido ou aplicativo não suportado.')
    ready = lines[0].split()[1] == '200'
    return {'title': title if ready and title else 'default',
            'name': headers.get('running-app-name', '')[:120] if ready and title else '',
            'status': 'ready' if ready else 'standby',
            'source': 'discovery'}


def query_console(host, timeout=.5):
    # Numeric unicast IPv4 avoids DNS changes and probing an unrelated machine.
    address = ipaddress.IPv4Address(host)
    if address.is_multicast or address.is_unspecified or int(address) == 0xffffffff:
        raise ValueError('Use o IPv4 individual do PS5 na rede local.')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        for port in range(9303, 9320):
            try:
                sock.bind(('', port))
                break
            except OSError:
                continue
        sock.settimeout(timeout)
        # connect() filters replies to the requested endpoint, without a TCP session.
        sock.connect((str(address), PORT))
        sock.send(SEARCH)
        return parse_response(sock.recv(4097))


class StableTitle:
    """Require two consistent reads; apply once, preserving manual overrides.

    A new title must stabilize before replacing the current profile. On a lookup
    error the caller immediately selects the passive default and resets this class.
    """
    def __init__(self):
        self.candidate = None
        self.count = 0
        self.applied = None

    def observe(self, result):
        signature = (result['title'], result.get('name', ''), result['source'])
        self.count = self.count + 1 if self.candidate == signature else 1
        self.candidate = signature
        return self.count >= 2 and self.applied != signature

    def mark_applied(self):
        self.applied = self.candidate


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('console', help='IPv4 do PS5')
    args = p.parse_args()
    try:
        print(json.dumps(query_console(args.console), ensure_ascii=False, indent=2))
    except (OSError, ValueError) as e:
        p.exit(1, f'Sem metadados de jogo: {e}\n')
