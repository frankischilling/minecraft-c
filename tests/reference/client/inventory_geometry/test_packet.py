"""Java-free corruption controls for specialized inventory source observations."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def module(name):
    spec = importlib.util.spec_from_file_location('inventory_packet_' + name, HERE / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


A, P = module('audit'), module('compare')


def read(name):
    return json.loads((HERE / name).read_text(), object_pairs_hook=P.unique)


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.observations = read('observations.json')
        cls.finite = read('finite.json')
        cls.pins = read('source-pins.json')

    def audit(self, value=None):
        data = self.observations if value is None else value
        return A.audit({'pins': self.pins}, data['ordinary_stdout'], data['debugger_stdout'], data['events'])

    def corrupt(self, mutate):
        data = copy.deepcopy(self.observations)
        mutate(data)
        with self.assertRaises((AssertionError, KeyError, ValueError, IndexError, TypeError)):
            self.audit(data)

    def test_complete_source_audit(self):
        result = self.audit()
        self.assertEqual(result['rows'], self.finite['rows'])
        self.assertEqual((result['draws'], result['raw_words'], result['active_words'], result['inactive_word_checks']),
                         (216, 10368, 7596, 2772))
        self.assertEqual(P.compare(result['rows'], self.finite['rows']), {'groups': 216, 'active_words': 7596})

    def test_active_geometry_words_across_all_batches_and_roles(self):
        roles, vertices = set(), set()
        for row_index, row in enumerate(self.finite['rows']):
            index = row['active_indices'][(row_index * 7) % len(row['active_indices'])]
            roles.add(index % 8)
            vertices.add(index // 8)
            # Exercise the complete public comparison for every batch while
            # copying only the mutated row/words. Repeating the same rejection
            # for all7596 individual words adds no distinct assertion path.
            values = self.finite['rows'].copy()
            values[row_index] = {**row, 'words': row['words'].copy()}
            values[row_index]['words'][index] = f'{int(row["words"][index],16)^1:08x}'
            with self.subTest(row=row_index, word=index), self.assertRaises(ValueError):
                P.compare(values, self.finite['rows'])
        self.assertEqual(roles, {0, 1, 2, 3, 4, 5, 6})
        self.assertEqual(vertices, {0, 1, 2, 3, 4, 5})

    def test_each_inactive_word_requires_own_prior(self):
        # Active-only cross-runtime comparison is safe only with this separate
        # full source backing audit. Mutating either slot family must fail it.
        for slot in (5, 6, 7):
            def mutate(data):
                event = next(e for e in data['events'] if e['event'] == 'nw-draw')
                index = 3 * 8 + slot
                event['words'][index] = f'{int(event["words"][index],16)^1:08x}'
            with self.subTest(slot=slot):
                self.corrupt(mutate)

    def test_source_callsite(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'nw-draw')['source-frames'][1].update(pc=0))

    def test_live_code_pin(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'live-code').update(sha256='0'*64))

    def test_complete_event_order(self):
        self.corrupt(lambda d: d['events'].pop())

    def test_actual_singleton_identity(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'nw-draw').update(receiver=-1))

    def test_gl_translate_argument(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'gl-translate')['arguments'].__setitem__(0,'00000000'))

    def test_bounds_setter_argument(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'block-bounds')['arguments'].__setitem__(0,'3f800000'))

    def test_first_failure_prefix(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'source-exception')['site'].update(pc=0))

    def test_stair_source_alias(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'stair-texture').update(**{'source-id':81}))

    def test_ordinary_jdi_agreement(self):
        self.corrupt(lambda d: d.update(ordinary_stdout=d['ordinary_stdout'].replace('"returned":true','"returned":false',1)))

    def test_source_flags(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'nw-draw')['state'].update(n=False))

    def test_exact_schema_and_mask(self):
        rows = copy.deepcopy(self.finite['rows'])
        rows[0]['active_indices'].append(7)
        with self.assertRaises(ValueError):
            P.validate(rows)
        rows = copy.deepcopy(self.finite['rows'])
        rows[0]['profile'] = False
        with self.assertRaises(ValueError):
            P.validate(rows)

    def test_duplicates(self):
        with self.assertRaises(ValueError):
            json.loads('{"profile":0,"profile":0}', object_pairs_hook=P.unique)

    def test_sources_and_fixtures_pinned(self):
        provenance = read('provenance.json')
        self.assertEqual(provenance['status'], 'CAPTURED_ORIGINAL')
        self.assertTrue(provenance['public_sources_pinned_before_capture'])
        for section in ('source_files', 'fixtures'):
            for name, wanted in provenance[section].items():
                self.assertEqual(hashlib.sha256((HERE / name).read_bytes()).hexdigest(), wanted)


if __name__ == '__main__':
    unittest.main()
