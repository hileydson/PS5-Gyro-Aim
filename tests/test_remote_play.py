#!/usr/bin/env python3
"""Remote metadata and launcher rules; no PS5 or user credentials needed."""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from ps5_discovery import parse_response, query_console, StableTitle, SEARCH
import play_ps5


def packet(title='PPSA01234', name='Jogo: Edição especial', status=200):
    return (f'HTTP/1.1 {status} Ok\ndevice-discovery-protocol-version:00030010\n'
            f'host-type:PS5\nrunning-app-titleid:{title}\nrunning-app-name:{name}\n').encode()


class MetadataTests(unittest.TestCase):
    def test_game_menu_standby(self):
        self.assertEqual(parse_response(packet())['title'], 'PPSA01234')
        self.assertEqual(parse_response(packet())['name'], 'Jogo: Edição especial')
        self.assertEqual(parse_response(packet('CUSA00001'))['title'], 'CUSA00001')
        self.assertEqual(parse_response(packet(''))['title'], 'default')
        self.assertEqual(parse_response(packet(status=620))['title'], 'default')

    def test_invalid_or_unknown_metadata(self):
        for data in (packet('../wrong'), packet('NPXS40002'), packet() + b'running-app-titleid:PPSA99999\n',
                     packet().replace(b'00030010', b'00020020'), b'HTTP/1.1 500 Error\n', b'\x00', b'a' * 4097):
            with self.subTest(data=data[:40]):
                with self.assertRaises(ValueError):
                    parse_response(data)

    def test_stability_and_manual_override(self):
        t = StableTitle()
        a, b = parse_response(packet()), parse_response(packet('PPSA55555'))
        self.assertFalse(t.observe(a))
        self.assertTrue(t.observe(a))
        t.mark_applied()
        self.assertFalse(t.observe(a))  # manual selection is not repeatedly overwritten
        self.assertFalse(t.observe(b))
        self.assertFalse(t.observe(a))  # a transient candidate never replaces the profile
        self.assertFalse(t.observe(b))
        self.assertTrue(t.observe(b))
        t.mark_applied()
        self.assertFalse(t.observe(b))

    def test_discovery_is_search_only(self):
        with patch('ps5_discovery.socket.socket') as factory:
            sock = factory.return_value.__enter__.return_value
            sock.recv.return_value = packet()
            self.assertEqual(query_console('192.168.1.2')['title'], 'PPSA01234')
            sock.connect.assert_called_once_with(('192.168.1.2', 9302))
            sock.send.assert_called_once_with(SEARCH)
            self.assertNotIn(b'WAKEUP', SEARCH)
        for invalid in ('255.255.255.255', '0.0.0.0', '224.0.0.1', 'not-an-ip'):
            with self.assertRaises(ValueError):
                query_console(invalid)

    def test_client_environment_is_scoped(self):
        with patch.dict('os.environ', {'SDL_JOYSTICK_HIDAPI': '1'}):
            env = play_ps5.controller_env()
            self.assertEqual(env['SDL_JOYSTICK_HIDAPI'], '0')
            self.assertEqual(env['SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT'], '0x045e/0x028e')
            import os
            self.assertEqual(os.environ['SDL_JOYSTICK_HIDAPI'], '1')

    def test_dry_run_never_starts_a_process(self):
        with patch('play_ps5.client_command', return_value=['/fake/chiaki']), patch('subprocess.Popen') as spawn:
            with contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(play_ps5.main(['--dry-run', '--console', '192.168.1.2']), 0)
            self.assertIn('--remote-play', output.getvalue())
            spawn.assert_not_called()

    def test_panel_source_must_be_real_ps5(self):
        with patch('play_ps5.get_state', return_value={'platform': 'linux', 'simulated': 1, 'title': 'PPSA01234'}):
            with self.assertRaises(ValueError):
                play_ps5.watched_title('http://fake')


if __name__ == '__main__':
    unittest.main()
