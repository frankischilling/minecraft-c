"""Public no-assets corruption controls for the original held Item packet."""
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
    spec = importlib.util.spec_from_file_location('held_packet_' + name, HERE / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


P, A, W = module('compare'), module('audit'), module('cow')


def read(name):
    return json.loads((HERE / name).read_text(), object_pairs_hook=P.unique)


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = read('finite.json')['rows']
        cls.inventory = read('item-inventory.json')
        cls.cow = read('cow-death.json')['observations']

    def test_all422_actual_rows_and_independent_formulas(self):
        result = A.audit(self.rows, self.inventory['items'])
        self.assertEqual((result['runtime_classes'], result['direct_base_ids'],
                          result['durability_rows'], result['break_rows'], result['saddle_rows']),
                         (38, 22, 320, 120, 16))
        P.compare(self.rows, self.rows)

    def test_every_compared_field_corruption(self):
        wanted = ('dirt-ordinary', 'wool-count-3-fatal', 'durable-256-crosses-max',
                  'durable-267-equal-resisted', 'durable-276-wrap-max',
                  'saddle-pig-count0-plain', 'saddle-cow-count1-plain')
        for profile in wanted:
            index = next(i for i, r in enumerate(self.rows) if r['profile'] == profile)
            for key in P.FIELDS:
                changed = copy.deepcopy(self.rows)
                value = changed[index][key]
                changed[index][key] = not value if type(value) is bool else value + 1 if type(value) is int else value + '-bad'
                with self.subTest(profile=profile, key=key), self.assertRaises(ValueError):
                    P.compare(changed, self.rows)

    def test_exact_schema_order_types_and_duplicates(self):
        for change in ('bool', 'missing', 'extra', 'order', 'short', 'duplicate'):
            rows = copy.deepcopy(self.rows)
            if change == 'bool': rows[0]['health'] = True
            if change == 'missing': del rows[0]['health']
            if change == 'extra': rows[0]['extra'] = 1
            if change == 'order': rows[0], rows[1] = rows[1], rows[0]
            if change == 'short': rows.pop()
            if change == 'duplicate': rows[1] = rows[0]
            with self.subTest(change=change), self.assertRaises(ValueError):
                P.compare(rows, self.rows)
        text = 'HELD_ITEM ' + json.dumps(self.rows[0])[:-1] + ',"health":9}'
        with self.assertRaises(ValueError): P.rows(text)

    def test_independent_formula_rejects_boundary_and_saddle_changes(self):
        for profile, key in [('durable-256-ends-at-max', 'damage-after'),
                             ('durable-256-crosses-max', 'broken-after'),
                             ('durable-276-wrap-max', 'count-after'),
                             ('saddle-pig-count-3-plain', 'count-after'),
                             ('saddle-pig-count1-saddled', 'used-after')]:
            rows = copy.deepcopy(self.rows)
            next(r for r in rows if r['profile'] == profile)[key] += 1
            with self.subTest(profile=profile), self.assertRaises(AssertionError):
                A.audit(rows, self.inventory['items'])

    def test_cow_real_entropy_is_not_normalized(self):
        for observation in self.cow:
            W.audit(observation['rows'])
        W.compare(self.cow[0]['rows'], self.cow[1]['rows'])
        a, b = self.cow[0]['rows'][0], self.cow[1]['rows'][0]
        self.assertNotEqual(a['selected']['drops'], b['selected']['drops'])
        self.assertNotEqual(a['cow-before'], b['cow-before'])
        self.assertNotIn('drops', W.projection(self.cow[0]['rows'])[0])
        self.assertEqual(len(W.projection(self.cow[0]['rows'])[0]), 40)

    def test_cow_rng_sound_quantity_and_publication_corruption(self):
        controls = [('cow-before', '0000000000000000'), ('cow-after', '0000000000000000'),
                    ('player-after', '0000000000000000'), ('world-after', '0000000000000000')]
        for key, value in controls:
            rows = copy.deepcopy(self.cow[0]['rows']); rows[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): W.audit(rows)
        for field in ('name', 'volume', 'pitch'):
            rows = copy.deepcopy(self.cow[0]['rows']); rows[0]['sounds'][0][field] = 'bad'
            with self.subTest(field=field), self.assertRaises(ValueError): W.audit(rows)
        for key in ('drops', 'health', 'count-after', 'broken-after', 'used-after'):
            rows = copy.deepcopy(self.cow[0]['rows']); rows[0]['selected'][key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): W.audit(rows)
        for index in range(4):
            rows = copy.deepcopy(self.cow[0]['rows']); rows[0]['items'][0]['stack'][index] += 1
            with self.subTest(index=index), self.assertRaises(ValueError): W.audit(rows)
        for key, value in [('pickup-delay', 11), ('chunk-members', 2), ('world-member', False),
                           ('distinct', False), ('private-random', 'ffffffffffffffff')]:
            rows = copy.deepcopy(self.cow[0]['rows']); rows[0]['items'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): W.audit(rows)
        rows = copy.deepcopy(self.cow[0]['rows']); rows[0]['events'].reverse()
        with self.assertRaises(ValueError): W.audit(rows)

    def test_nextint3_signed_rejection_boundary(self):
        inv = pow(0x5deece66d, -1, 1 << 48)
        seed = ((1 << 31) - 1) << 17
        for _ in range(3): seed = ((seed - 11) * inv) & W.MASK
        self.assertGreaterEqual(W.entropy(f'{seed:016x}')['next_int_attempts'], 2)

    def test_render_owners_and_real_field_vs_constant_bodies(self):
        pins = read('source-pins.json')['classes']
        def method(name, member, descriptor):
            return next(m for m in pins[name]['methods'] if m['name'] == member and m['descriptor'] == descriptor)
        constant = method('dq', 'b', '()Z')['code_sha256']
        for owner, name in [('qd', 'b'), ('la', 'b'), ('cs', 'b'), ('cs', 'c')]:
            self.assertEqual(method(owner, name, '()Z')['code_sha256'], constant)
        self.assertNotEqual(method('gm', 'b', '()Z')['code_sha256'], constant)
        self.assertEqual(len(self.inventory['items']), 202)
        self.assertEqual(len(self.inventory['metadata15']), 26)
        self.assertEqual({r['class'] for r in self.inventory['items']}, {r['item-class'] for r in self.rows})

    def test_preserved_research_and_unarmed_packet(self):
        original = read('research-rows.json')['rows']
        self.assertEqual([{k: r[k] for k in old} for r, old in zip(self.rows, original)], original)
        path = HERE.parent / 'player_attack/finite.json'
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),
                         read('provenance.json')['unarmed_fixture_sha256'])
        self.assertEqual(len(json.loads(path.read_text())['rows']), 5)

    def test_published_receipts_and_authored_source_continuity(self):
        provenance = read('provenance.json')
        for section in ('source_files', 'fixtures'):
            for name, value in provenance[section].items():
                self.assertEqual(hashlib.sha256((HERE / name).read_bytes()).hexdigest(), value)
        self.assertEqual(provenance['ordinary_captures'][0]['status'], 'PASS')
        self.assertEqual(provenance['ordinary_captures'][1]['status'], 'PASS')
        self.assertTrue(provenance['selected422_equal_across_independent_vms'])
        self.assertTrue(provenance['cow_raw_entropy_audited_per_process'])


if __name__ == '__main__':
    unittest.main()
