#!/usr/bin/env python3
"""Asset-independent cave helper guards; no C, JVM or original execution."""
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / 'scripts/reference/caves/compare_caves.py'


def load_helper():
    spec = importlib.util.spec_from_file_location('caves_public_guard_subject', HELPER)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.dict(sys.modules):
        sys.modules.pop('fixtures', None)
        sys.modules[spec.name] = module
        sys.path.insert(0, str(HELPER.parent))
        try:
            spec.loader.exec_module(module)
        finally:
            sys.path.pop(0)
    return module


def synthetic_case(helper, index=1):
    def descriptor(length):
        return {'length': length, 'values': ['0000000000000000'] * length}
    noise = {name: [{'offsets': ['0000000000000000'] * 3,
                     'permutation': list(range(256)) * 2} for _ in range(count)]
             for name, count in helper.CHANNELS.items()}
    constructor = {'generator_continuation': ['0000000000000000'] * 4,
                   'cave_continuation': None, 'noise82': noise, 'q425': None,
                   'scratch': {name: None if name in 'defgh' else descriptor(size)
                               for name, size in helper.SCRATCH.items()},
                   'climate': None, 'materials': None,
                   'math_table_sha256_f32be': helper.TABLE_SHA, 'blocks': None}
    raw = bytes(32768) + bytes.fromhex('8a8b8c')
    def block(data):
        return {'length': len(data), 'values': data.hex()}
    prefix = copy.deepcopy(constructor)
    prefix.update(q425=descriptor(425), scratch={name: descriptor(size) for name, size in helper.SCRATCH.items()},
                  climate={'temperature': descriptor(256), 'rainfall': descriptor(256),
                           'auxiliary': descriptor(256), 'biomes': 'a' * 256},
                  materials='0203' * 256, blocks=block(raw))
    first = bytearray(raw)
    first[0] = 1
    changed = bytearray(first)
    changed[1] = 2
    passes = []
    for name, before, after in (('first', raw, first), ('restored', raw, first), ('changed', first, changed)):
        snapshot = copy.deepcopy(prefix)
        snapshot['blocks'] = block(after)
        snapshot['cave_continuation'] = ['0000000000000001'] * 4
        passes.append({'label': name, 'input_blocks': block(before), 'after': snapshot,
                       'changed_prefix_bytes': 1, 'suffix_unchanged': True})
    seed, x, z = helper.CASES[index - 1]
    chunk_seed = (x * 341873128712 + z * 132897987541) & ((1 << 64) - 1)
    return {'case': index, 'seed': seed, 'chunk': [x, z], 'chunk_seed': f'{chunk_seed:016x}',
            'constructor': constructor, 'prefix': prefix, 'passes': passes,
            'restored_output_equal_first': True, 'reused_input_differs_from_prefix': True,
            'changed_output_equal_its_input': False}


class ProtocolGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.helper = load_helper()
        cls.case = synthetic_case(cls.helper)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='caves protocol guards ')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()

    def test_request_whitelist_is_exact_and_bounded(self):
        cases, data = self.helper.request_data()
        self.assertEqual(len(cases), 10)
        self.assertEqual(len(data), 159)
        self.assertEqual(hashlib.sha256(data).hexdigest(), self.helper.REQUEST_SHA)
        with mock.patch.object(self.helper, 'fixtures', return_value=cases[:-1]):
            with self.assertRaises(self.helper.Failure):
                self.helper.request_data()

    def test_valid_changed_input_remains_an_observation(self):
        observation = self.helper.validate_case(copy.deepcopy(self.case), 1, self.helper.CASES[0])
        self.assertFalse(observation['observations']['changed_output_equal_its_input'])
        self.assertEqual(observation['changed_prefix_bytes'], [1, 1, 1])

    def test_schema_raw_bits_and_unobserved_initial_state_are_strict(self):
        mutations = [lambda x: x.update(extra=1), lambda x: x.update(case=True),
                     lambda x: x['constructor'].update(cave_continuation=['0' * 16] * 4),
                     lambda x: x['constructor']['noise82']['k'][0]['permutation'].__setitem__(0, False),
                     lambda x: x['prefix']['q425']['values'].__setitem__(0, '000000000000000A'),
                     lambda x: x['passes'][0].update(changed_prefix_bytes=0),
                     lambda x: x.update(restored_output_equal_first=1),
                     lambda x: x.update(changed_output_equal_its_input=True)]
        for mutate in mutations:
            value = copy.deepcopy(self.case)
            mutate(value)
            with self.subTest(mutation=mutate), self.assertRaises(self.helper.Failure):
                self.helper.validate_case(value, 1, self.helper.CASES[0])
        for text in ('{"a":1,"a":2}', 'NaN', 'Infinity'):
            with self.subTest(text=text), self.assertRaises(self.helper.Failure):
                self.helper.strict_json(text)

    def test_restored_cave_continuation_is_required(self):
        value = copy.deepcopy(self.case)
        value['passes'][1]['after']['cave_continuation'][0] = '0000000000000002'
        with self.assertRaisesRegex(self.helper.Failure, 'restored public entry'):
            self.helper.validate_case(value, 1, self.helper.CASES[0])

    def c_case(self):
        value = copy.deepcopy(self.case)
        for snapshot in [value['constructor'], value['prefix']] + [item['after'] for item in value['passes']]:
            del snapshot['math_table_sha256_f32be']
            snapshot['table_unchanged'] = True
        return value

    def test_only_true_measured_table_marker_adapts_to_measured_digest(self):
        value = self.helper.adapt_c_case(self.c_case(), self.helper.TABLE_SHA)
        self.assertEqual(value, self.case)
        for marker in (False, 1, None):
            value = self.c_case()
            value['prefix']['table_unchanged'] = marker
            with self.subTest(marker=marker), self.assertRaises(self.helper.Failure):
                self.helper.adapt_c_case(value, self.helper.TABLE_SHA)
        value = self.c_case()
        value['prefix']['math_table_sha256_f32be'] = self.helper.TABLE_SHA
        with self.assertRaises(self.helper.Failure):
            self.helper.adapt_c_case(value, self.helper.TABLE_SHA)

    def test_table_header_is_hashed_from_actual_bytes(self):
        # This checks transport hashing with generated zeros, not numerical table
        # generation. Only the expected digest is patched in this isolated test.
        raw = bytes(65536 * 4)
        digest = hashlib.sha256(raw).hexdigest()
        path = self.root / 'synthetic-c.tsv'
        with path.open('wb') as output:
            output.write(b'TABLE\t' + raw.hex().encode() + b'\n')
            for index in range(1, 11):
                value = synthetic_case(self.helper, index)
                for snapshot in [value['constructor'], value['prefix']] + [item['after'] for item in value['passes']]:
                    del snapshot['math_table_sha256_f32be']
                    snapshot['table_unchanged'] = True
                output.write(b'OK\t' + json.dumps(value, separators=(',', ':')).encode() + b'\n')
        artifacts = self.root / 'artifacts'
        artifacts.mkdir()
        with mock.patch.object(self.helper, 'TABLE_SHA', digest):
            records = list(self.helper.read_c_records(path, artifacts))
        self.assertEqual(len(records), 10)
        self.assertEqual((artifacts / 'c-table.f32be').read_bytes(), raw)
        wrong = self.root / 'wrong-table.tsv'
        wrong.write_bytes(b'TABLE\t' + raw.hex().encode() + b'\n')
        with self.assertRaisesRegex(self.helper.Failure, 'table differs'):
            list(self.helper.read_c_records(wrong, self.root))

    def test_completed_mismatch_fails_before_later_setup_unavailable(self):
        reference = self.root / 'reference.json'
        reference.write_text(json.dumps(self.case))
        changed = copy.deepcopy(self.case)
        changed['prefix']['materials'] = '0302' * 256
        for item in changed['passes']:
            item['after']['materials'] = '0302' * 256
        self.helper.validate_case(changed, 1, self.helper.CASES[0])
        with self.assertRaisesRegex(self.helper.Failure, 'server-to-C case 1'):
            self.helper.save_records(iter([(changed, {})]), 'server', self.root, [('server-to-C', [reference])])

    def test_setup_timeout_mapping_requires_valid_unfinished_setup(self):
        owned = self.root / 'owned'
        path = self.root / 'stderr.txt'
        path.write_text(f'SETUP_BEGIN seed=0 root={owned / "case-001"}\n')
        markers = self.helper.lifecycle(path, self.helper.CASES, owned)
        timeout = self.helper.ProcessResult([], 1, 0, True, 'child timeout', 0, 0, 0, 0, 0)
        self.assertTrue(self.helper.permitted_unavailable(timeout, markers, 0, self.helper.CASES))
        path.write_text(path.read_text() + 'SETUP_DONE seed=0\n')
        markers = self.helper.lifecycle(path, self.helper.CASES, owned)
        self.assertFalse(self.helper.permitted_unavailable(timeout, markers, 0, self.helper.CASES))

    def test_c_preservation_markers_are_counted_exactly(self):
        path = self.root / 'c-stderr.txt'
        text = ''.join((self.helper.CONTROL + '\n') * 3 + f'CASE_DONE case={index}\n' for index in range(1, 11))
        path.write_text(text)
        self.assertEqual(self.helper.c_controls(path)['control_count'], 30)
        path.write_text(text.replace(self.helper.CONTROL, self.helper.CONTROL + ' BAD', 1))
        with self.assertRaises(self.helper.Failure):
            self.helper.c_controls(path)

    def test_ordinary_file_inventory_uses_real_windows_stat_fields(self):
        path = self.root / 'single-file.bin'
        data = b'owned inventory regression\n'
        path.write_bytes(data)
        self.assertEqual(os.stat(path, follow_symlinks=False).st_nlink, 1)
        result = self.helper.world_inventory(self.root, hashes=True)
        self.assertTrue(result['complete'], result)
        self.assertIsNone(result['error'])
        self.assertEqual((result['files'], result['bytes']), (1, len(data)))
        self.assertEqual(result['entries'][0]['sha256'], hashlib.sha256(data).hexdigest())

    def test_actual_hardlink_inventory_is_rejected(self):
        path = self.root / 'original.bin'
        path.write_bytes(b'generated')
        try:
            (self.root / 'alias.bin').hardlink_to(path)
        except (OSError, NotImplementedError) as error:
            self.skipTest('filesystem hard links unavailable: ' + str(error))
        self.assertEqual(os.stat(path, follow_symlinks=False).st_nlink, 2)
        result = self.helper.world_inventory(self.root, hashes=True)
        self.assertFalse(result['complete'])
        self.assertIn('hard-linked', result['error'])


class MainGuards(unittest.TestCase):
    def setUp(self):
        self.helper = load_helper()
        temporary = tempfile.TemporaryDirectory(prefix='caves main guards ')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        canonical = self.root / 'scripts/reference/caves/compare_caves.py'
        self.helper.__file__ = str(canonical)
        for path in self.helper.source_paths(self.root):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'generated canonical source placeholder\n')
        self.inputs = self.root / 'inputs'
        self.inputs.mkdir()
        self.executable = self.inputs / 'probe.bin'
        self.client, self.server = self.inputs / 'client.jar', self.inputs / 'server.jar'
        for path in (self.executable, self.client, self.server):
            path.write_bytes(('generated ' + path.name).encode())
        self.helper.JAR_HASHES = {name: self.helper.sha256(path) for name, path in [('client', self.client), ('server', self.server)]}
        self.work = self.root / 'reports'
        self.args = ['--executable', str(self.executable), '--client', str(self.client), '--server', str(self.server),
                     '--work-dir', str(self.work), '--tested-commit', 'a' * 40]

    def invoke(self, args=None):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()), \
                mock.patch.object(self.helper, 'run_live', side_effect=AssertionError('no child execution allowed')):
            return self.helper.main(args or self.args)

    def snapshot(self):
        return {str(path): self.helper.sha256(path) for path in self.inputs.iterdir()}

    def test_missing_prerequisite_replaces_stale_pass_and_preserves_inputs(self):
        self.work.mkdir()
        (self.work / 'manifest.json').write_text('{"status":"pass","stale":true}')
        before = self.snapshot()
        with mock.patch.object(self.helper, 'runtime_tools', side_effect=self.helper.Unavailable('injected missing JDK')):
            self.assertEqual(self.invoke(), 77)
        report = json.loads((self.work / 'manifest.json').read_text())
        self.assertEqual(report['status'], 'unavailable')
        self.assertFalse(report['completed'])
        self.assertEqual(report['tested_commit'], 'a' * 40)
        self.assertTrue(report['inputs_before'])
        self.assertEqual(self.snapshot(), before)
        self.assertFalse((self.root / '.local/reference/caves-setup').exists())

    def test_wrong_hash_fails_before_tools(self):
        self.helper.JAR_HASHES['client'] = '0' * 64
        with mock.patch.object(self.helper, 'runtime_tools', side_effect=AssertionError('must not reach tools')):
            self.assertEqual(self.invoke(), 1)
        report = json.loads((self.work / 'manifest.json').read_text())
        self.assertEqual(report['status'], 'fail')
        self.assertTrue(any('SHA-256 differs' in item for item in report['errors']))

    def test_source_ancestor_rejected_before_writes(self):
        before = self.snapshot()
        args = self.args.copy()
        args[args.index('--work-dir') + 1] = str(self.root)
        self.assertEqual(self.invoke(args), 1)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse((self.root / 'manifest.json').exists())

    def test_renamed_helper_is_rejected_before_report_creation(self):
        canonical = Path(self.helper.__file__)
        alias = canonical.with_name('renamed_compare_caves.py')
        alias.write_bytes(HELPER.read_bytes())
        canonical.with_name('fixtures.py').write_bytes(HELPER.with_name('fixtures.py').read_bytes())
        before = self.snapshot()
        result = subprocess.run([sys.executable, str(alias), *self.args], capture_output=True,
                                text=True, timeout=10, check=False)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('attributed canonical source path', result.stderr)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse(self.work.exists())

    def test_hardlink_manifest_input_alias_is_rejected(self):
        self.work.mkdir()
        try:
            (self.work / 'manifest.json').hardlink_to(self.client)
        except (OSError, NotImplementedError) as error:
            self.skipTest('filesystem hard links unavailable: ' + str(error))
        before = self.snapshot()
        self.assertEqual(self.invoke(), 1)
        self.assertEqual(self.snapshot(), before)

    def test_symlink_manifest_input_alias_is_rejected(self):
        self.work.mkdir()
        try:
            (self.work / 'manifest.json').symlink_to(self.client)
        except (OSError, NotImplementedError) as error:
            self.skipTest('filesystem symlinks unavailable: ' + str(error))
        before = self.snapshot()
        self.assertEqual(self.invoke(), 1)
        self.assertEqual(self.snapshot(), before)

    def test_unexpected_prerequisite_exception_never_publishes_pass(self):
        with mock.patch.object(self.helper, 'runtime_tools', side_effect=RuntimeError('injected exceptional setup')):
            self.assertEqual(self.invoke(), 1)
        report = json.loads((self.work / 'manifest.json').read_text())
        self.assertEqual(report['status'], 'fail')
        self.assertFalse(report['completed'])
        self.assertTrue(any('injected exceptional setup' in item for item in report['errors']))


if __name__ == '__main__':
    unittest.main(verbosity=2)
