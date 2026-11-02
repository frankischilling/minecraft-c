"""Corruption controls for direct genuine-owner original held geometry."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def module(name):
    spec = importlib.util.spec_from_file_location('geometry_packet_' + name, HERE / (name + '.py'))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


P, A = module('compare'), module('audit')


def read(name):
    return json.loads((HERE / name).read_text(), object_pairs_hook=P.unique)


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = read('finite.json')['rows']
        cls.observations = read('observations.json')

    def source_audit(self, observations=None):
        value = self.observations if observations is None else observations
        return A.audit({'pins': read('source-pins.json')}, value['ordinary_stdout'],
                       value['debugger_stdout'], value['events'])

    def test_complete_original_and_live_source_audit(self):
        result = self.source_audit()
        self.assertEqual(result['rows'], self.rows)
        self.assertEqual(result['words'], 19008)
        self.assertEqual((result['source_vertices'], result['packed_vertices']), (1584, 2376))
        P.compare(self.rows, self.rows)

    def test_all_eight_vertex_slots_in_every_batch(self):
        for index in range(36):
            for slot in range(8):
                rows = [{**row, 'words': row['words'].copy()} for row in self.rows]
                rows[index]['words'][slot] = f"{int(rows[index]['words'][slot],16) ^ 1:08x}"
                with self.subTest(group=index, slot=slot), self.assertRaises(ValueError):
                    P.compare(rows, self.rows)

    def test_last_words_and_strip_boundaries(self):
        for index, group in enumerate(self.rows):
            positions = {len(group['words']) - 1}
            if group['batch'] >= 2:
                positions.update({47, 48, 95, 96, 719, 720, 767})
            for at in positions:
                rows = [{**row, 'words': row['words'].copy()} for row in self.rows]
                rows[index]['words'][at] = f"{int(rows[index]['words'][at],16) ^ 1:08x}"
                with self.subTest(group=index, at=at), self.assertRaises(ValueError):
                    P.compare(rows, self.rows)

    def test_schema_order_extent_duplicates_and_types(self):
        for kind in ('icon', 'batch', 'order', 'short', 'long', 'word', 'int', 'extra', 'missing'):
            rows = [{**row, 'words': row['words'].copy()} for row in self.rows]
            if kind == 'icon': rows[0]['icon'] = 65
            if kind == 'batch': rows[0]['batch'] = True
            if kind == 'order': rows[0], rows[1] = rows[1], rows[0]
            if kind == 'short': rows[0]['words'].pop()
            if kind == 'long': rows[0]['words'].append('00000000')
            if kind == 'word': rows[0]['words'][0] = '3F800000'
            if kind == 'int': rows[0]['words'][0] = 0
            if kind == 'extra': rows[0]['extra'] = 1
            if kind == 'missing': del rows[0]['words']
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                P.compare(rows, self.rows)
        text = 'HELD_ITEM_GEOMETRY ' + json.dumps(self.rows[0])[:-1] + ',"icon":64}'
        with self.assertRaises(ValueError): P.rows(text)

    def test_actual_source_entries_flags_callsite_and_code(self):
        for kind in ('sourcepc', 'flags', 'count', 'normal', 'return', 'alias', 'pin'):
            data = copy.deepcopy(self.observations)
            draw = next(e for e in data['events'] if e['event'] == 'nw-draw')
            if kind == 'sourcepc': draw['source-frames'][0]['pc'] += 1
            if kind == 'flags': draw['state']['l'] = True
            if kind == 'count': draw['state']['h'] = 4
            if kind == 'normal': draw['state']['n'] = False
            if kind == 'return': data['events'].pop()
            if kind == 'alias': draw['receiver'] += 1
            if kind == 'pin': next(e for e in data['events'] if e['event'] == 'live-code')['sha256'] = '0' * 64
            with self.subTest(kind=kind), self.assertRaises(AssertionError): self.source_audit(data)

    def test_actual_stale_initial_is_preserved_and_inactive_slots_known(self):
        initial = self.observations['initial']
        self.assertFalse(initial['all_zero'])
        self.assertEqual(sum(v != '00000000' for v in initial['words']), 18)
        self.assertTrue(all(word == '00000000' for row in self.rows
                            for index, word in enumerate(row['words']) if index % 8 in (5, 7)))
        data = copy.deepcopy(self.observations)
        next(e for e in data['events'] if e['event'] == 'nw-draw')['words'][5] = 'ffffffff'
        with self.assertRaises(AssertionError): self.source_audit(data)

    def test_distinct_original_sword_and_bow_uvs(self):
        groups = {icon: [r for r in self.rows if r['icon'] == icon] for icon in P.ICONS}
        for icon, rows in groups.items():
            self.assertEqual(len(rows), 6)
            # Original first face begins at maxU/maxV. Keep f32
            # arithmetic, including original15.99f, independently explicit.
            def f32(x): return struct.unpack('>f', struct.pack('>f', x))[0]
            max_u = f32(f32((icon % 16) * 16 + f32(15.99)) / 256.)
            max_v = f32(f32((icon // 16) * 16 + f32(15.99)) / 256.)
            self.assertEqual(rows[0]['words'][3], struct.pack('>f', max_u).hex())
            self.assertEqual(rows[0]['words'][4], struct.pack('>f', max_v).hex())

    def test_source_and_fixture_byte_receipts(self):
        provenance = read('provenance.json')
        for section in ('source_files', 'fixtures'):
            for name, value in provenance[section].items():
                self.assertEqual(hashlib.sha256((HERE / name).read_bytes()).hexdigest(), value)
        self.assertEqual(provenance['status'], 'CAPTURED_ORIGINAL')
        self.assertTrue(provenance['public_sources_pinned_before_capture'])
        self.assertEqual(provenance['raw_allocations'], 0)
        self.assertFalse(provenance['source_nw_backing_cleared'])


if __name__ == '__main__':
    unittest.main()
