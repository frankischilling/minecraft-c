#!/usr/bin/env python3
"""Asset-independent protected cave reference guards; no C/JVM/original execution."""
import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import py_compile
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / 'scripts/reference/caves/compare_geometry.py'


def load_helper():
    module = types.ModuleType('geometry_public_guard_subject')
    module.__file__ = str(HELPER)
    exec(compile(HELPER.read_bytes(), str(HELPER), 'exec'), module.__dict__)
    return module


def synthetic_record(helper, fields, index=1):
    data = helper.owned_input(fields)
    blocks = {'length': len(data), 'values': data.hex()}
    room = fields[0] == 'room'
    return {'schema': 2, 'case': index, 'request': list(fields), 'input_blocks': blocks,
            'before_continuation': ['0000000000000000'] * 4,
            'output_blocks': copy.deepcopy(blocks), 'after_continuation': ['0000000000000001'] * 4,
            'shared_random_consumption': {'basis': 'continuation_match', 'next_float_calls': 1 if room else 0,
                                          'next_long_calls': 1, 'fork_inferred': False, 'candidates_matched': 1},
            'changed_prefix_bytes': 0, 'math_table_sha256_f32be': helper.TABLE_SHA,
            'suffix_unchanged': True, 'retained_random': True, 'range': 8}


class ProtocolGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.helper = load_helper()
        cls.common, cls.executed = cls.helper.load_shared(HELPER.with_name('compare_caves.py'))
        cls.fixture, _ = cls.helper.fixture_module()
        cls.cases, cls.data = cls.helper.request_data(cls.fixture)
        cls.fields = cls.cases[0].request.split('\t')
        cls.value = synthetic_record(cls.helper, cls.fields)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='protected caves guards ')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.failures = (self.helper.Failure, self.common.Failure)

    def test_exact_whitelist_and_finite_request_domains(self):
        self.assertEqual((len(self.cases), len(self.data)), (44, 3644))
        self.assertEqual(hashlib.sha256(self.data).hexdigest(), self.helper.REQUEST_SHA)
        for replace in ('7ff0000000000000', '7ff8000000000000', 'c020000000000000'):
            fields = self.fields.copy()
            fields[4] = replace
            with self.subTest(bits=replace), self.assertRaises(self.helper.Failure):
                self.helper.validate_request('\t'.join(fields))
        with self.assertRaises(self.helper.Failure):
            self.helper.validate_request(self.cases[1].request.replace('40400000', '7f800000'))
        changed = types.SimpleNamespace(REQUEST_SHA256=self.helper.REQUEST_SHA, fixtures=lambda: self.cases[:-1])
        with self.assertRaises(self.helper.Failure):
            self.helper.request_data(changed)

    def test_all_owned_pattern_recipes_and_byte_axes(self):
        fields = self.fields.copy()
        fields[7] = 'solid:255'
        self.assertEqual(self.helper.owned_input(fields), bytes([255]) * 32768 + bytes.fromhex('8a8b8c'))
        fields[7] = 'water:8:9:33:8'
        raw = self.helper.owned_input(fields)
        self.assertEqual(raw[(9 * 16 + 8) * 128 + 33], 8)
        self.assertEqual(raw[:32768].count(8), 1)
        fields[7] = 'water-plane:9:37'
        raw = self.helper.owned_input(fields)
        self.assertEqual(raw[:32768].count(9), 256)
        self.assertEqual(raw[(3 * 16 + 5) * 128 + 37], 9)
        fields[7] = 'strata:35'
        raw = self.helper.owned_input(fields)
        self.assertEqual(raw[34:37], bytes([3, 2, 0]))
        self.assertEqual(raw[-3:], bytes.fromhex('8a8b8c'))
        for invalid in ('solid:4', 'water:7:0:0:0', 'water:9:16:0:0', 'strata:128', 'water-plane:8:-1'):
            with self.subTest(pattern=invalid), self.assertRaises(self.helper.Failure):
                self.helper.pattern(invalid)

    def test_strict_fields_counts_controls_and_raw_echo(self):
        self.helper.validate_record(self.value, 1, self.fields, self.common)
        mutations = [lambda x: x.update(schema=True), lambda x: x.update(extra=1), lambda x: x.update(range=True),
                     lambda x: x.update(changed_prefix_bytes=1), lambda x: x.update(retained_random=1),
                     lambda x: x['request'].__setitem__(1, '-0'),
                     lambda x: x['before_continuation'].__setitem__(0, '000000000000000A'),
                     lambda x: x['shared_random_consumption'].update(candidates_matched=2),
                     lambda x: x['shared_random_consumption'].update(next_float_calls=True),
                     lambda x: x['shared_random_consumption'].update(next_long_calls=3),
                     lambda x: x['shared_random_consumption'].update(fork_inferred=True)]
        for mutate in mutations:
            value = copy.deepcopy(self.value)
            mutate(value)
            with self.subTest(mutation=mutate), self.assertRaises(self.failures):
                self.helper.validate_record(value, 1, self.fields, self.common)
        for text in ('{"a":1,"a":2}', 'NaN'):
            with self.assertRaises(self.common.Failure):
                self.common.strict_json(text)

    def test_reported_input_must_match_owned_pattern(self):
        value = copy.deepcopy(self.value)
        value['input_blocks']['values'] = '03' + value['input_blocks']['values'][2:]
        value['changed_prefix_bytes'] = 1
        with self.assertRaisesRegex(self.helper.Failure, 'owned pattern'):
            self.helper.validate_record(value, 1, self.fields, self.common)

    def test_negative_zero_request_bits_remain_distinct(self):
        case = self.cases[-1]
        fields = case.request.split('\t')
        value = synthetic_record(self.helper, fields, 44)
        self.helper.validate_record(value, 44, fields, self.common)
        value['request'][8] = '00000000'
        with self.assertRaisesRegex(self.helper.Failure, 'request echo'):
            self.helper.validate_record(value, 44, fields, self.common)

    def test_actual_table_bytes_are_hashed_before_adaptation(self):
        raw = bytes(65536 * 4)
        digest = hashlib.sha256(raw).hexdigest()
        path = self.root / 'C.tsv'
        with path.open('wb') as output:
            output.write(b'TABLE\t' + raw.hex().encode() + b'\n')
            for index, case in enumerate(self.cases, 1):
                value = synthetic_record(self.helper, case.request.split('\t'), index)
                del value['math_table_sha256_f32be']
                value['table_unchanged'] = True
                output.write(b'OK\t' + json.dumps(value, separators=(',', ':')).encode() + b'\n')
        artifacts = self.root / 'artifacts'
        artifacts.mkdir()
        with mock.patch.object(self.helper, 'TABLE_SHA', digest):
            values = list(self.helper.read_records(path, self.cases, self.common, artifacts))
        self.assertEqual(len(values), 44)
        self.assertEqual((artifacts / 'c-table.f32be').read_bytes(), raw)
        with self.assertRaisesRegex(self.helper.Failure, 'table differs'):
            list(self.helper.read_records(path, self.cases, self.common, self.root))

    def test_false_or_ambiguous_c_marker_is_rejected(self):
        raw = bytes(65536 * 4)
        digest = hashlib.sha256(raw).hexdigest()
        for marker in (False, 1):
            value = copy.deepcopy(self.value)
            del value['math_table_sha256_f32be']
            value['table_unchanged'] = marker
            path = self.root / ('marker-' + str(marker) + '.tsv')
            path.write_bytes(b'TABLE\t' + raw.hex().encode() + b'\nOK\t' + json.dumps(value).encode() + b'\n')
            artifacts = self.root / ('parsed-' + str(marker))
            artifacts.mkdir()
            with mock.patch.object(self.helper, 'TABLE_SHA', digest), self.assertRaisesRegex(self.helper.Failure, 'marker'):
                list(self.helper.read_records(path, self.cases, self.common, artifacts))

    def test_complete_mismatch_precedes_later_unavailable(self):
        reference = self.root / 'reference.json'
        reference.write_text(json.dumps(self.value))
        value = copy.deepcopy(self.value)
        value['after_continuation'][0] = '0000000000000002'
        self.helper.validate_record(value, 1, self.fields, self.common)
        with self.assertRaisesRegex(self.helper.Failure, 'client-to-C case 1'):
            self.helper.save_records(iter([(value, {})]), 'client', self.root, self.common, [('client-to-C', [reference])])

    def test_lifecycle_and_setup_unavailable_are_narrow(self):
        path = self.root / 'stderr.txt'
        path.write_text('UNAVAILABLE: protected cave class setup failed\n')
        markers = self.helper.lifecycle(path, 44)
        process = types.SimpleNamespace(stop_reason=None, returncode=77, timed_out=False)
        self.assertTrue(self.helper.unavailable_setup(process, markers, 0))
        process.timed_out = True
        self.assertFalse(self.helper.unavailable_setup(process, markers, 0))
        path.write_text('GEOMETRY_READY\nGEOMETRY_BEGIN case=1\n' + self.helper.CONTROL + '\nGEOMETRY_DONE case=1\n')
        markers = self.helper.lifecycle(path, 44)
        self.assertEqual((markers['completed'], markers['controls']), (1, 1))
        self.assertFalse(self.helper.unavailable_setup(process, markers, 1))
        path.write_text(path.read_text().replace(self.helper.CONTROL, 'CONTROL_GEOMETRY BAD'))
        with self.assertRaises(self.helper.Failure):
            self.helper.lifecycle(path, 44)

    def test_shared_source_execution_ignores_cached_bytecode(self):
        directory = self.root / 'shared'
        directory.mkdir()
        natural = directory / 'compare_caves.py'
        fixture = directory / 'fixtures.py'
        natural.write_bytes(HELPER.with_name('compare_caves.py').read_bytes())
        real = HELPER.with_name('fixtures.py').read_bytes()
        fixture.write_bytes(real)
        stamp = fixture.stat()
        malicious = b"raise AssertionError('cached bytecode executed')\n"
        self.assertGreater(len(real), len(malicious))
        fixture.write_bytes(malicious + b' ' * (len(real) - len(malicious)))
        os.utime(fixture, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        py_compile.compile(str(fixture), doraise=True)
        fixture.write_bytes(real)
        os.utime(fixture, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        common, executed = self.helper.load_shared(natural)
        self.assertEqual(common.TABLE_SHA, self.helper.TABLE_SHA)
        self.assertEqual(executed[1]['sha256'], hashlib.sha256(real).hexdigest())


class MainGuards(unittest.TestCase):
    def setUp(self):
        self.helper = load_helper()
        temporary = tempfile.TemporaryDirectory(prefix='protected cave main guards ')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.helper.__file__ = str(self.root / 'scripts/reference/caves/compare_geometry.py')
        for path in self.helper.source_paths(self.root):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'generated canonical source placeholder\n')
        for name in ('compare_caves.py', 'fixtures.py', 'geometry_fixtures.py'):
            (self.root / 'scripts/reference/caves' / name).write_bytes(HELPER.with_name(name).read_bytes())
        self.inputs = self.root / 'inputs'
        self.inputs.mkdir()
        self.exe, self.client, self.server = (self.inputs / name for name in ('probe.bin', 'client.jar', 'server.jar'))
        for path in (self.exe, self.client, self.server):
            path.write_bytes(('generated ' + path.name).encode())
        self.helper.JAR_HASHES = {name: self.helper.record(path)['sha256'] for name, path in [('client', self.client), ('server', self.server)]}
        self.work = self.root / 'reports'
        self.args = ['--executable', str(self.exe), '--client', str(self.client), '--server', str(self.server),
                     '--work-dir', str(self.work), '--tested-commit', 'a' * 40]

    def invoke(self, args=None):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return self.helper.main(args or self.args)

    def snapshot(self):
        return {str(path): self.helper.record(path) for path in self.inputs.iterdir()}

    def test_missing_shared_source_replaces_stale_pass(self):
        (self.root / 'scripts/reference/caves/compare_caves.py').unlink()
        self.work.mkdir()
        (self.work / 'manifest.json').write_text('{"status":"pass"}')
        before = self.snapshot()
        self.assertEqual(self.invoke(), 77)
        report = json.loads((self.work / 'manifest.json').read_text())
        self.assertEqual(report['status'], 'unavailable')
        self.assertFalse(report['completed'])
        self.assertEqual(self.snapshot(), before)

    def test_missing_jdk_after_exact_source_loading_stays_unavailable(self):
        real = self.helper.load_shared
        def missing(path):
            common, executed = real(path)
            common.runtime_tools = mock.Mock(side_effect=common.Unavailable('injected missing JDK'))
            common.run_live = mock.Mock(side_effect=AssertionError('no child execution allowed'))
            return common, executed
        with mock.patch.object(self.helper, 'load_shared', side_effect=missing):
            self.assertEqual(self.invoke(), 77)
        report = json.loads((self.work / 'manifest.json').read_text())
        self.assertEqual(report['status'], 'unavailable')
        self.assertEqual(len(report['executed_source_modules']), 3)
        self.assertTrue(report['inputs_preserved'])
        self.assertFalse((self.root / '.local/reference/caves-setup').exists())

    def test_wrong_hash_prevents_shared_or_tool_execution(self):
        self.helper.JAR_HASHES['client'] = '0' * 64
        with mock.patch.object(self.helper, 'load_shared', side_effect=AssertionError('must not execute shared source')):
            self.assertEqual(self.invoke(), 1)
        self.assertEqual(json.loads((self.work / 'manifest.json').read_text())['status'], 'fail')

    def test_source_ancestor_is_rejected_before_writes(self):
        before = self.snapshot()
        args = self.args.copy()
        args[args.index('--work-dir') + 1] = str(self.root)
        self.assertEqual(self.invoke(args), 1)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse((self.root / 'manifest.json').exists())

    def test_renamed_helper_is_rejected_before_report_creation(self):
        canonical = Path(self.helper.__file__)
        alias = canonical.with_name('renamed_compare_geometry.py')
        alias.write_bytes(HELPER.read_bytes())
        before = self.snapshot()
        result = subprocess.run([sys.executable, str(alias), *self.args], capture_output=True,
                                text=True, timeout=10, check=False)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('attributed canonical source path', result.stderr)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse(self.work.exists())

    def test_hardlink_manifest_alias_is_rejected(self):
        self.work.mkdir()
        try:
            (self.work / 'manifest.json').hardlink_to(self.client)
        except (OSError, NotImplementedError) as error:
            self.skipTest('filesystem hardlinks unavailable: ' + str(error))
        before = self.snapshot()
        self.assertEqual(self.invoke(), 1)
        self.assertEqual(self.snapshot(), before)

    def test_symlink_manifest_alias_is_rejected(self):
        self.work.mkdir()
        try:
            (self.work / 'manifest.json').symlink_to(self.client)
        except (OSError, NotImplementedError) as error:
            self.skipTest('filesystem symlinks unavailable: ' + str(error))
        before = self.snapshot()
        self.assertEqual(self.invoke(), 1)
        self.assertEqual(self.snapshot(), before)

    def test_unexpected_prerequisite_exception_never_publishes_pass(self):
        with mock.patch.object(self.helper, 'load_shared', side_effect=RuntimeError('injected source failure')):
            self.assertEqual(self.invoke(), 1)
        report = json.loads((self.work / 'manifest.json').read_text())
        self.assertEqual(report['status'], 'fail')
        self.assertFalse(report['completed'])
        self.assertTrue(any('injected source failure' in error for error in report['errors']))


if __name__ == '__main__':
    unittest.main(verbosity=2)
