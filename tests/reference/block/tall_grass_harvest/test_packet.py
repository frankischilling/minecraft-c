"""Corruption controls for the finite original TallGrass harvest evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('tall_grass_harvest_audit', HERE / 'packet.py')
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)


class PacketTests(unittest.TestCase):
    def test_current_packet(self):
        corpus = P.read_packet()
        self.assertEqual(len(corpus['profiles']), 74)
        self.assertEqual(sum(p['mode'] for p in corpus['profiles']), 68)
        self.assertEqual(sum(len(corpus['by_profile'][p['index']])
                             for p in corpus['profiles'] if p['mode']), 192)

    def reject(self, change, filename='observations.tsv', reseal=True):
        with tempfile.TemporaryDirectory(prefix='tall-grass-packet-') as temporary:
            directory = Path(temporary)
            for p in HERE.iterdir():
                if p.is_file():
                    shutil.copyfile(p, directory / p.name)
            path = directory / filename
            change(path)
            if reseal:
                proof = json.loads((directory / 'provenance.json').read_text())
                proof['files'][filename] = {'bytes': path.stat().st_size, 'sha256': P.digest(path)}
                (directory / 'provenance.json').write_text(json.dumps(proof))
            with self.assertRaises((ValueError, KeyError, IndexError, UnicodeError)):
                P.read_packet(directory)

    def alter(self, tag, profile, key, value, occurrence=0):
        columns = {'PROFILE': P.PROFILE_COLUMNS, 'RESULT': P.RESULT_COLUMNS,
                   'ITEM': P.ITEM_COLUMNS, 'EVENT': P.EVENT_COLUMNS}[tag]
        def change(path):
            rows = path.read_text().splitlines()
            matches = [i for i, row in enumerate(rows) if row.startswith(f'{tag}\t{profile}\t')]
            i = matches[occurrence]
            cells = rows[i].split('\t')
            cells[1 + columns.index(key)] = str(value)
            rows[i] = '\t'.join(cells)
            path.write_text('\n'.join(rows) + '\n', encoding='utf-8', newline='\n')
        return change

    def test_transport_and_binding_corruption(self):
        mutations = (
            ('observations.tsv', lambda p: p.write_bytes(p.read_bytes() + b'EXTRA\n'), False),
            ('observations.tsv', lambda p: p.write_bytes(p.read_bytes().replace(b'\n', b'\r\n')), True),
            ('guards.tsv', lambda p: p.write_text('GUARD\tsource_network_attempts\t1\n'), True),
            ('observations.tsv', lambda p: p.write_bytes(p.read_bytes().rstrip(b'\n')), True),
        )
        for filename, change, reseal in mutations:
            with self.subTest(filename=filename, change=change):
                self.reject(change, filename, reseal)

    def test_result_roles_and_source_order(self):
        mutations = (
            ('RESULT', 1, 'status', 'NULL'), ('RESULT', 60, 'status', 'OK'),
            ('RESULT', 1, 'removed', 0), ('RESULT', 1, 'target_id', 31),
            ('RESULT', 1, 'mined_delta', 0), ('RESULT', 56, 'mined_delta', 1),
            ('RESULT', 1, 'used_delta', 1), ('RESULT', 1, 'broken_delta', 1),
            ('RESULT', 64, 'current_present', 1), ('RESULT', 66, 'current_present', 0),
            ('RESULT', 12, 'captured_damage', 239), ('RESULT', 1, 'item_count', 0),
            ('RESULT', 48, 'world48_after', 0), ('RESULT', 0, 'math48_after', 0),
            ('RESULT', 68, 'target_id', 0),
        )
        for tag, profile, key, value in mutations:
            with self.subTest(profile=profile, key=key):
                self.reject(self.alter(tag, profile, key, value))

    def test_seed_publication_and_aliases(self):
        for key, value in [('id', 31), ('count', 2), ('damage', 1), ('delay', 0),
                           ('age', 1), ('in_world', 0), ('in_chunk', 0), ('x64', '7ff0000000000000')]:
            with self.subTest(key=key):
                self.reject(self.alter('ITEM', 1, key, value))

    def test_callback_order_and_prefixes(self):
        for occurrence, key, value in [(0, 'kind', 'INSERT'), (0, 'mined_delta', 1),
                                       (0, 'target_id', 31), (0, 'item_id', 295),
                                       (1, 'mined_delta', 0), (1, 'item_id', 31),
                                       (1, 'in_chunk', 0), (1, 'math48', 0)]:
            with self.subTest(occurrence=occurrence, key=key):
                self.reject(self.alter('EVENT', 1, key, value, occurrence))

    def test_inherited_harvest_pin(self):
        def change(path):
            value = json.loads(path.read_text())
            value['classes']['ru']['methods'].append(copy.deepcopy(
                next(m for m in value['classes']['uu']['methods']
                     if m['name'] == 'a' and m['descriptor'] == '(Lfd;Lgs;IIII)V')))
            path.write_text(json.dumps(value))
        self.reject(change, 'source-pins.json')


if __name__ == '__main__':
    unittest.main()
