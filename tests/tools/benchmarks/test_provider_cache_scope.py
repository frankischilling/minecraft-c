"""Reject incomplete or semantically corrupt provider-scope receipts and files.

Synthetic protocol records below are reader controls, not timing observations.
"""
import copy
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    'provider_scope_public_runner', ROOT / 'scripts/benchmarks/provider_cache_scope.py')
RUN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUN)


def wire():
    # Literal declared eight-resident coordinate checksum, independently bound
    # by the native fixed-work observations. No synthetic timing is qualified.
    value = 1840867092695286659
    lines = ['16 achievements', '151 recipes',
             'PROVIDER_SCOPE_CONTEXT count=8 depth=2 calls=65536 samples=7 pointer_bits=64']
    for index in range(7):
        lines.append(f'PROVIDER_SCOPE_SAMPLE sample={index} calls=65536 ticks=1000 '
                     f'frequency=10000000 checksum={value} identity_guard=1')
    lines.extend([
        'PROVIDER_SCOPE_INVARIANTS count=8 depth=2 calls=458752 planes=123 '
        f'checksum={value} exact_identities=1 source_fields=1 random=1 cache=1 pending=1 '
        'diagnostics=1 sentinels=1 ownership=1 scopes=1 generic_dispatch=1',
        'PROVIDER_SCOPE_FINISH checks=81 cleanup=1 genuine_source_world=1'])
    return '\n'.join(lines) + '\n'


class ProtocolTests(unittest.TestCase):
    def test_complete_receipt(self):
        result = RUN.parse(wire(), 8, 2)
        self.assertEqual(len(result['samples']), 7)
        self.assertEqual(result['invariants']['calls'], 458752)
        self.assertGreater(result['median_ns_per_get'], 0)

    def test_order_count_and_completion(self):
        lines = wire().splitlines()
        reordered = list(lines)
        reordered[3], reordered[4] = reordered[4], reordered[3]
        damaged = [wire().rstrip('\n'), '\n'.join(lines[:-1]) + '\n',
                   '\n'.join(reordered) + '\n', wire() + lines[-1] + '\n',
                   wire().replace('count=8', 'count=64'),
                   wire().replace('depth=2', 'depth=1'),
                   wire().replace('sample=6', 'sample=5'),
                   wire().replace('calls=65536', 'calls=65535', 1)]
        for value in damaged:
            with self.subTest(value=value[-150:]), self.assertRaises(ValueError):
                RUN.parse(value, 8, 2)

    def test_transport_clean_semantic_corruption(self):
        for key in ('exact_identities', 'source_fields', 'random', 'cache', 'pending',
                    'diagnostics', 'sentinels', 'ownership', 'scopes', 'generic_dispatch',
                    'identity_guard', 'cleanup', 'genuine_source_world'):
            with self.subTest(key=key), self.assertRaises(ValueError):
                RUN.parse(wire().replace(key + '=1', key + '=0', 1), 8, 2)
        for old, new in (('checksum=1840867092695286659', 'checksum=1840867092695286660'),
                         ('ticks=1000', 'ticks=0'), ('planes=123', 'planes=0'),
                         ('frequency=10000000', 'frequency=9999999'),
                         ('checks=81', 'checks=0'), ('ownership=1', 'ownership=1 unknown=1'),
                         ('ownership=1', 'ownership=1 ownership=1'),
                         ('ticks=1000', 'ticks=01000')):
            with self.subTest(new=new), self.assertRaises(ValueError):
                RUN.parse(wire().replace(old, new, 1), 8, 2)


class AuthoredFileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.saving = RUN.load_module('provider_scope_test_seed',
                                     ROOT / 'tests/tools/world/test_source_saving.py')

    def test_sparse_source_inputs_and_file_continuity(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw) / 'owned'
            before = RUN.source_inputs(root, 256, self.saving)
            region = (root / 'world/region/r.0.0.mcr').read_bytes()
            locations = [struct.unpack_from('>I', region, slot * 4)[0] for slot in range(1024)]
            self.assertEqual(sum(bool(value) for value in locations), 256)
            seen = set()
            for location in filter(None, locations):
                self.assertEqual(location & 255, 1)
                offset = (location >> 8) * 4096
                length, = struct.unpack_from('>i', region, offset)
                self.assertEqual(region[offset + 4], 2)
                value = self.saving.Nbt(zlib.decompress(
                    region[offset + 5:offset + 4 + length])).root()['Level'][1]
                x, z = value['xPos'][1], value['zPos'][1]
                self.assertEqual(x % 2, 0)
                self.assertEqual(z % 2, 0)
                self.assertFalse(value['Entities'][1][1])
                self.assertFalse(value['TileEntities'][1][1])
                seen.add((x, z))
            self.assertEqual(len(seen), 256)
            self.assertIn((0, 0), seen)
            self.assertIn((30, 30), seen)
            (root / 'world/session.lock').write_bytes(bytes(8))
            after = RUN.inventory(root)
            RUN.check_files(before, after)
            for name, change in (
                    ('authored rewrite', lambda value: value['world/level.dat'].update(sha256='0' * 64)),
                    ('short session', lambda value: value['world/session.lock'].update(bytes=7)),
                    ('missing session', lambda value: value.pop('world/session.lock')),
                    ('unexpected output', lambda value: value.update({'world/extra': {'bytes': 0}}))):
                damaged = copy.deepcopy(after)
                change(damaged)
                with self.subTest(name=name), self.assertRaises(ValueError):
                    RUN.check_files(before, damaged)


if __name__ == '__main__':
    unittest.main()
