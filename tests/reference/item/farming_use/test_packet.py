import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('farming_use_packet', HERE / 'packet.py')
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


class Packet(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.records = P.read_packet()
        cls.profiles = P.inputs(HERE)
        cls.rows = P.observations(HERE, cls.profiles)
        cls.events = [json.loads(line) for line in (HERE / 'events.jsonl').read_text(encoding='ascii').splitlines()]
        cls.pins = json.loads((HERE / 'source-pins.json').read_text(encoding='utf-8'))
        cls.live = (HERE / 'live-code.tsv').read_text(encoding='ascii').splitlines()

    def reject(self, edit):
        values = [copy.deepcopy(self.profiles), copy.deepcopy(self.rows), copy.deepcopy(self.events),
                  copy.deepcopy(self.pins), copy.deepcopy(self.live)]
        edit(*values)
        with self.assertRaises(ValueError):
            P.validate(*values)

    def test_inventory(self):
        self.assertEqual(len(self.records), 118)
        self.assertEqual(sum(r['profile']['values'][1] == 1 for r in self.records), 57)
        self.assertEqual(sum(r['observed']['exception'] != 'NONE' for r in self.records), 16)
        self.assertEqual(sum(len(r['events']) for r in self.records), 4246)

    def test_detached_return(self):
        a, b = P.read_packet(), P.read_packet()
        a[0]['observed']['stack_after'][0] = 19
        self.assertNotEqual(a, b)

    def test_archive_pin(self):
        self.reject(lambda p, r, e, s, l: s.update(client_jar_sha256='0' * 64))

    def test_live_code_pin(self):
        self.reject(lambda p, r, e, s, l: l.__setitem__(0, l[0][:-1] + ('0' if l[0][-1] != '0' else '1')))

    def test_duplicate_live_pin(self):
        self.reject(lambda p, r, e, s, l: l.append(l[0]))

    def test_missing_profile(self):
        self.reject(lambda p, r, e, s, l: p.pop())

    def test_missing_row(self):
        self.reject(lambda p, r, e, s, l: r.pop())

    def test_false_sequence(self):
        self.reject(lambda p, r, e, s, l: e[0].update(seq=1))

    def test_boolean_sequence(self):
        self.reject(lambda p, r, e, s, l: e[0].update(seq=False))

    def test_out_of_range_profile(self):
        self.reject(lambda p, r, e, s, l: e[0].update(profile=118))

    def test_unknown_event(self):
        self.reject(lambda p, r, e, s, l: e[0].update(event='invented'))

    def test_wrong_source_pc(self):
        self.reject(lambda p, r, e, s, l: e[0].update(at=e[0]['at'].split('@')[0] + '@2147483647'))

    def test_duplicate_call(self):
        def edit(p, r, e, s, l):
            entries = [x for x in e if x['event'] == 'entry']
            entries[1]['call'] = entries[0]['call']
        self.reject(edit)

    def test_wrong_executed_argument(self):
        def edit(p, r, e, s, l):
            entry = next(x for x in e if x['event'] == 'entry' and x['at'].startswith('la.a('))
            entry['args'][-1] += 1
        self.reject(edit)

    def test_wrong_actual_receiver(self):
        def edit(p, r, e, s, l):
            next(x for x in e if x['event'] == 'entry' and x['at'].startswith('la.a('))['receiver'] = None
        self.reject(edit)

    def test_wrong_constructed_stack(self):
        self.reject(lambda p, r, e, s, l: r[0]['stack_before'].__setitem__(0, 91))

    def test_wrong_return(self):
        def edit(p, r, e, s, l):
            root = next(x for x in e if x['event'] == 'entry')
            ret = next(x for x in e if x['event'] == 'return' and x['call'] == root['call'])
            ret['value'] = not ret['value']
        self.reject(edit)

    def test_wrong_exception(self):
        def edit(p, r, e, s, l):
            next(x for x in e if x['event'] == 'exception')['type'] = 'java.lang.Error'
        self.reject(edit)

    def test_wrong_sound_bits(self):
        def edit(p, r, e, s, l):
            next(x for x in r if x['sounds'])['sounds'][0][1] = '0000000000000000'
        self.reject(edit)

    def test_false_setter_witness(self):
        def edit(p, r, e, s, l):
            i = next(x['index'] for x in r if x['name'] == 'seeds_y127_false_setter_stack')
            entry = next(x for x in e if x['profile'] == i and x['event'] == 'entry' and x['caller'] == 'rk.a(Liz;Lgs;Lfd;IIII)Z@59')
            next(x for x in e if x['event'] == 'return' and x['call'] == entry['call'])['value'] = True
        self.reject(edit)

    def test_wrapper_used_statistic(self):
        def edit(p, r, e, s, l):
            next(x for x in r if x['values'][1] == 1 and x['result'] == 'true')['used_delta'] = 0
        self.reject(edit)

    def test_direct_statistic_not_normalized(self):
        self.reject(lambda p, r, e, s, l: r[0].update(used_delta=1))

    def test_each_captured_product_is_pinned(self):
        for name in P.CAPTURED:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as d:
                root = Path(d)
                for source in HERE.iterdir():
                    if source.is_file():
                        (root / source.name).write_bytes(source.read_bytes())
                (root / name).write_bytes((root / name).read_bytes() + b'\n')
                with self.assertRaisesRegex(ValueError, 'product pin'):
                    P.read_packet(root)

    def test_provenance_counts(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            for source in HERE.iterdir():
                if source.is_file():
                    (root / source.name).write_bytes(source.read_bytes())
            value = json.loads((root / 'provenance.json').read_text(encoding='utf-8'))
            value['events'] += 1
            (root / 'provenance.json').write_text(json.dumps(value), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'counts'):
                P.read_packet(root)


if __name__ == '__main__':
    unittest.main()
