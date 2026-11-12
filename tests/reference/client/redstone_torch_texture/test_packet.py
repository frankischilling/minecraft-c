"""Original texture packet admission and semantic corruption controls."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('db_texture_packet', HERE / 'packet.py')
a = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(a)


class AuthoredTests(unittest.TestCase):
    def test_exact_authored_operands_and_code(self):
        self.assertEqual(len(a.authored(HERE)), 160)

    def test_unsealed_packet_fails(self):
        prior = a.MANIFEST_SHA256
        try:
            a.MANIFEST_SHA256 = 'UNFROZEN'
            with self.assertRaisesRegex(ValueError, 'UNFROZEN'):
                a.read_packet(HERE)
        finally:
            a.MANIFEST_SHA256 = prior


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.provenance = a.load(HERE / 'provenance.json')
        cls.profiles = a.authored(HERE)
        cls.payloads = [{key: (HERE / ('ordinary-' + str(i)) / name).read_bytes()
                         for key, name in (('stdout', 'original.stdout'), ('stderr', 'original.stderr'), ('selected', 'selected.tsv'))}
                        for i in range(2)]
        cls.rows = a.validate_streams(cls.payloads[0], cls.profiles, cls.provenance['capture_root'])

    def reject_rows(self, mutate, message):
        rows = copy.deepcopy(self.rows)
        mutate(rows)
        with self.assertRaisesRegex(ValueError, message):
            a.validate_rows(rows, self.profiles)

    def test_actual_sealed_two_vm_packet(self):
        value = a.validate(HERE)
        self.assertEqual(value['ordinary_vms'], 2)
        self.assertEqual(len(value['rows']), 160)
        self.assertEqual(a.read_packet(HERE), value['rows'])
        for payload in self.payloads:
            self.assertEqual(a.validate_streams(payload, self.profiles, self.provenance['capture_root']), self.rows)

    def test_actual_sm_declaring_owner_not_inherited_uu(self):
        for field, replacement in ((15, 'uu'), (16, 'uu'), (17, 'uu'), (18, '50'), (19, '0'), (20, '0')):
            with self.subTest(field=field):
                self.reject_rows(lambda rows: rows[0].__setitem__(field, replacement), 'declaring classes')

    def test_source_branch_result_and_hex_join(self):
        target = next(i for i, row in enumerate(self.rows) if row[4] == '1')
        def wrong_branch(rows):
            value = int(rows[target][11])
            rows[target][13], rows[target][14] = str(value), format(value & 0xffffffff, '08x')
        self.reject_rows(wrong_branch, 'source-reachable')
        self.reject_rows(lambda rows: rows[0].__setitem__(14, 'ffffffff'), 'signed/hex join')

    def test_declared_input_store_and_restoration_conditioning(self):
        target = next(i for i, row in enumerate(self.rows) if row[6] == '1')
        self.reject_rows(lambda rows: rows[target].__setitem__(11, rows[target][9]), 'input/restoration')
        self.reject_rows(lambda rows: rows[1].__setitem__(9, '42'), 'input/restoration')

    def test_signed_operands_and_closed_order(self):
        for field, value in ((3, True), (4, '+0'), (5, '2147483648'), (6, '00')):
            with self.subTest(field=field, value=value):
                self.reject_rows(lambda rows: rows[0].__setitem__(field, value), 'declaring classes|canonical signed|signed scalar range')
        self.reject_rows(lambda rows: rows.pop(), 'complete scalar')
        self.reject_rows(lambda rows: rows.append(rows[0]), 'complete scalar')
        self.reject_rows(lambda rows: rows.__setitem__(slice(0, 2), [rows[1], rows[0]]), 'row identity')

    def test_no_ignored_throwable_daemon_or_warning_stderr(self):
        for suffix in (b'Exception in thread "main" java.lang.AssertionError\n',
                       b'Exception in thread "Thread-0" java.lang.NullPointerException\n',
                       b'java.lang.LinkageError\n', b'WARNING: extra\n'):
            payload = dict(self.payloads[0])
            payload['stderr'] += suffix
            with self.subTest(suffix=suffix), self.assertRaisesRegex(ValueError, 'SecurityManager-only'):
                a.validate_streams(payload, self.profiles, self.provenance['capture_root'])
        payload = dict(self.payloads[0]); payload['stderr'] = payload['stderr'].replace(b'\n', b'\r\n')
        with self.assertRaisesRegex(ValueError, 'SecurityManager-only'):
            a.validate_streams(payload, self.profiles, self.provenance['capture_root'])

    def test_closed_stdout_guard_and_projection(self):
        for key, mutate, message in (
                ('stdout', lambda b: b.replace(b'151 recipes\r\n', b'151 recipes\r\nignored\r\n', 1), 'scalar line count'),
                ('stdout', lambda b: b.replace(b'network=2', b'network=0'), 'guards and completion'),
                ('stdout', lambda b: b + b'ignored\r\n', 'scalar line count'),
                ('selected', lambda b: b.replace(b'DB_TEXTURE', b'OTHER', 1), 'raw/selected'),
                ('stdout', lambda b: b.replace(b'\r\n', b'\n'), 'CRLF')):
            payload = dict(self.payloads[0]); payload[key] = mutate(payload[key])
            with self.subTest(key=key, message=message), self.assertRaisesRegex(ValueError, message):
                a.validate_streams(payload, self.profiles, self.provenance['capture_root'])

    def test_process_health_and_raw_byte_records(self):
        receipt = a.load(HERE / 'ordinary-0/original.capture.json')
        for change in ({'exit': False}, {'exit': 1}, {'timeout_seconds': 91}, {'timed_out': True},
                       {'pipe_capture_complete': False}, {'reader_errors': {'stderr': 'read failed'}},
                       {'output_overflow': True}, {'stderr_bytes': True}, {'stdout_sha256': '0' * 64}, {'extra': 1}):
            altered = copy.deepcopy(receipt); altered.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                a.validate_receipt(altered, self.payloads[0], 'original', 90)

    def reject_evidence(self, mutate, message):
        with tempfile.TemporaryDirectory(prefix='db-texture-corruption-') as name:
            folder = Path(name)
            for file in a.frozen_files():
                target = folder / file
                target.parent.mkdir(exist_ok=True)
                target.write_bytes((HERE / file).read_bytes())
            report = a.load(folder / 'capture.json')
            mutate(report)
            (folder / 'capture.json').write_text(json.dumps(report) + '\n', encoding='ascii', newline='\n')
            provenance = a.load(folder / 'provenance.json')
            provenance['capture_sha256'] = a.sha(folder / 'capture.json')
            (folder / 'provenance.json').write_text(json.dumps(provenance) + '\n', encoding='ascii', newline='\n')
            with self.assertRaisesRegex(ValueError, message):
                a.validate_evidence(folder)

    def test_helper_and_independent_vm_closure(self):
        self.reject_evidence(lambda r: [r[k].update({'Injected.class': '0' * 64}) for k in ('helpers_before', 'helpers_after')],
                             'two-class compiled')
        self.reject_evidence(lambda r: r['runs'].pop(), 'two independent ordered')
        self.reject_evidence(lambda r: r['runs'][1].update(repeat=False), 'two independent ordered')

    def test_raw_json_rejects_duplicate_keys_nonfinite_and_bool_int_substitution(self):
        for value in ('{"exit":0,"exit":0}', '{"v":NaN}', '{"v":Infinity}', '{"v":-Infinity}'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                a.loads(value)
        self.assertFalse(a.same({'exit': False}, {'exit': 0}))
        self.assertFalse(a.same([True], [1]))


if __name__ == '__main__':
    unittest.main()
