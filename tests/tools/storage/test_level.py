"""Java-free level metadata fixture, protocol and execution guard tests."""
from __future__ import annotations

import argparse
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
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[3]
DIRECTORY = ROOT/'scripts/reference/storage'
sys.path.insert(0, str(DIRECTORY))
import compare_level as level
import export_level as export
sys.path.pop(0)
FIXTURE = ROOT/'tests/reference/storage/level_fixtures.json'


def observation(context):
    return level.strict_json(export.unpacked(context['observation']))


def files(context, phase):
    return {name: export.unpacked(value) for name, value in context[phase].items()}


def replace_file(context, name, raw, phase='after'):
    previous = context[phase][name]['bytes']
    context[phase][name] = export.packed(raw)
    inventory = context[phase+'_inventory']
    inventory['bytes'] += len(raw)-previous
    found = next(row for row in inventory['entries'] if row['path'] == name)
    found.update(level.fingerprint(raw))
    if name == 'observations.json':
        context['observation'] = export.packed(raw)


def replace_observation(context, value):
    replace_file(context, 'observations.json', (json.dumps(value, ensure_ascii=True, separators=(',', ':'))+'\n').encode('ascii'))


class LevelGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = export.load_fixture(FIXTURE)

    def reject(self, value):
        with self.assertRaises(level.Failure):
            export.validate_fixture(value)

    def mutate_observation(self, callback, index=0):
        value = copy.deepcopy(self.fixture)
        context = value['contexts'][index]
        record = observation(context)
        callback(record)
        replace_observation(context, record)
        return value

    def test_complete_original_only_corpus_and_replay_plans(self):
        self.assertEqual(len(self.fixture['contexts']), 4)
        plans = []
        for context in self.fixture['contexts']:
            record = observation(context)
            self.assertEqual(len(record['codecs']), 10)
            self.assertEqual(len(record['handlers']), 16)
            self.assertEqual(len(context['bindings']['methods']), 13)
            self.assertEqual(len(context['bindings']['fields']), 16)
            plans += level.replay_plan(record, files(context, 'after'))
        self.assertEqual(len(plans), 80)
        self.assertEqual(sum(x['kind'] == 'decode' for x in plans), 40)
        self.assertEqual(sum(x['kind'] == 'original-fresh-serializer' for x in plans), 32)
        self.assertEqual(sum(x['kind'] == 'original-handler-saved-schema-only' for x in plans), 8)

    def test_packed_stream_is_complete_bounded_and_hashed(self):
        raw = b'original bytes\x00\xff'
        packed = export.packed(raw)
        self.assertEqual(export.unpacked(packed), raw)
        for mutate in (lambda x: x.update(bytes=True), lambda x: x.update(bytes=len(raw)+1),
                       lambda x: x.update(sha256='0'*64), lambda x: x.update(zlib_base64='!'),
                       lambda x: x.update(zlib_base64=x['zlib_base64'][:-4])):
            value = copy.deepcopy(packed)
            mutate(value)
            with self.assertRaises(level.Failure):
                export.unpacked(value)
        for encoded in (zlib.compress(raw)+b'trailing', zlib.compress(raw)+zlib.compress(b'extra')):
            value = copy.deepcopy(packed)
            import base64
            value['zlib_base64'] = base64.b64encode(encoded).decode('ascii')
            with self.assertRaises(level.Failure):
                export.unpacked(value)
        with self.assertRaises(level.Failure):
            export.unpacked(export.packed(raw), len(raw)-1)

    def test_all_sides_and_controls_required_in_fixed_order(self):
        for mode in ('omit', 'duplicate', 'reverse', 'bool-repeat'):
            with self.subTest(mode=mode):
                value = copy.deepcopy(self.fixture)
                if mode == 'omit':
                    value['contexts'].pop()
                elif mode == 'duplicate':
                    value['contexts'][3] = copy.deepcopy(value['contexts'][2])
                elif mode == 'reverse':
                    value['contexts'].reverse()
                else:
                    value['contexts'][1]['repeat'] = True
                self.reject(value)

    def test_provenance_cannot_claim_c_produced_expectations(self):
        for change in ({'mode': 'live-original-C'}, {'tested_commit': 'short'},
                       {'helper_class_version': 61.0}, {'exporter_sha256': '0'}):
            value = copy.deepcopy(self.fixture)
            value['provenance'].update(change)
            self.reject(value)
        value = copy.deepcopy(self.fixture)
        value['provenance']['source_fingerprints'].pop('src/storage/level/level.c')
        self.reject(value)
        value = copy.deepcopy(self.fixture)
        value['provenance']['helper_classes'].pop('LevelObservation$Probe.class')
        self.reject(value)

    def test_process_and_copied_source_fingerprints_have_exact_types(self):
        mutations = [lambda x: x['provenance']['compilation']['process']['stdout'].update(bytes=False),
                     lambda x: x['provenance']['compilation']['process']['stderr'].update(bytes=0.0),
                     lambda x: x['contexts'][0]['process']['stdout'].update(bytes=float(x['contexts'][0]['process']['stdout']['bytes'])),
                     lambda x: x['contexts'][0]['process']['stderr'].update(bytes=float(x['contexts'][0]['process']['stderr']['bytes'])),
                     lambda x: x['provenance']['helper_sources']['LevelObservation.java'].update(bytes=float(x['provenance']['helper_sources']['LevelObservation.java']['bytes'])),
                     lambda x: x['provenance']['executed_source_modules']['scripts/reference/chunk/compare_chunk.py'].update(bytes=float(x['provenance']['executed_source_modules']['scripts/reference/chunk/compare_chunk.py']['bytes']))]
        for index, mutate in enumerate(mutations):
            value = copy.deepcopy(self.fixture)
            mutate(value)
            with self.subTest(index=index):
                self.reject(value)

    def test_public_nested_schemas_reject_unattributed_extras(self):
        mutations = [lambda x: x['contexts'][0]['before_inventory'].update(extra='C:/unattributed/path'),
                     lambda x: x['contexts'][0]['after_inventory'].update(original_class_bytes='cafebabe'),
                     lambda x: x['provenance']['compilation']['process'].update(extra='C:/compiler/path'),
                     lambda x: x['provenance']['helper_sources']['LevelObservation.java'].update(extra='unattributed source'),
                     lambda x: x['provenance']['executed_source_modules']['scripts/reference/chunk/compare_chunk.py'].update(extra='unattributed source')]
        for index, mutate in enumerate(mutations):
            value = copy.deepcopy(self.fixture)
            mutate(value)
            with self.subTest(index=index):
                self.reject(value)

    def test_malformed_nested_objects_raise_deliberate_validation_failure(self):
        mutations = [lambda x: x['contexts'].__setitem__(0, 7),
                     lambda x: x['contexts'][0].update(before_inventory=7),
                     lambda x: x['contexts'][0]['after_inventory']['entries'].__setitem__(0, 7),
                     lambda x: x['provenance']['tools'].__setitem__('java', 7)]
        for index, mutate in enumerate(mutations):
            value = copy.deepcopy(self.fixture)
            mutate(value)
            with self.subTest(index=index):
                self.reject(value)
        for mutate in (lambda x: x['codecs'].__setitem__(0, 7),
                       lambda x: x['handlers'].__setitem__(0, 7),
                       lambda x: x['handlers'][0].update(load=7)):
            self.reject(self.mutate_observation(mutate))

    def test_origin_loader_bindings_and_fields_are_exact(self):
        mutations = [lambda x: x['class_origins']['ei'].update(loader='system'),
                     lambda x: x['class_origins']['ei'].update(origin='supplied:server'),
                     lambda x: x['bindings'].pop('info_retained'),
                     lambda x: x['bindings']['handler_load'].update(signature='()V'),
                     lambda x: x['bindings']['handler_save_retained'].update(**{'class': 'mx'}),
                     lambda x: x['fields']['info.f'].update(descriptor='I')]
        for index, mutate in enumerate(mutations):
            with self.subTest(index=index):
                self.reject(self.mutate_observation(mutate))
        value = copy.deepcopy(self.fixture)
        value['contexts'][0]['bindings']['methods']['info_ctor']['sha256'] = '0'*64
        self.reject(value)

    def test_codec_case_omissions_and_type_coercions_rejected(self):
        for mutate in (lambda x: x['codecs'].pop(), lambda x: x['codecs'].reverse(),
                       lambda x: x['codecs'][0]['constructor']['info'].update(version=0.0),
                       lambda x: x['codecs'][0]['constructor']['info'].update(raining=0),
                       lambda x: x['codecs'][0]['constructor']['info']['spawn'].__setitem__(0, False)):
            self.reject(self.mutate_observation(mutate))

    def test_info_serialization_preserves_loaded_state(self):
        for mutate in (lambda x: x['codecs'][1]['retained'].update(player_identity_retained=False),
                       lambda x: x['codecs'][1]['retained']['info_after'].update(last_played_hex='0000000000000000'),
                       lambda x: x['codecs'][1]['empty-list']['info_after'].update(version=19132)):
            self.reject(self.mutate_observation(mutate))

    def test_every_original_serialized_scalar_is_checked(self):
        for field, wrong in (('RandomSeed', 42), ('SpawnY', 3), ('SizeOnDisk', -1)):
            with self.subTest(field=field):
                value = copy.deepcopy(self.fixture)
                context = value['contexts'][0]
                tree = level.parse_file(level.FileView(files(context, 'after'))/'codec-full/retained.dat')
                level.child(level.child(tree, 'Data'), field)['value'] = wrong
                replace_file(context, 'codec-full/retained.dat', level.gzip(level.encode(tree)))
                self.reject(value)

    def test_opaque_player_nan_empty_list_and_utf16_not_normalized_in_c_comparison(self):
        context = self.fixture['contexts'][0]
        plan = next(p for p in level.replay_plan(observation(context), files(context, 'after')) if p['label'] == 'full-retained')
        original = copy.deepcopy(plan['expected'])
        good = {'ok': True, 'nbt_hex': level.encode(original).hex()}
        level.compare_c(good, plan)
        data = level.child(original, 'Data')
        opaque = level.child(level.child(data, 'Player'), 'Opaque')
        for name, bad in (('FloatNaN', '7fa12345'), ('Text', '0041')):
            tree = copy.deepcopy(original)
            target = level.child(level.child(level.child(level.child(tree, 'Data'), 'Player'), 'Opaque'), name)
            target['value'] = bad
            with self.assertRaises(level.Failure):
                level.compare_c({'ok': True, 'nbt_hex': level.encode(tree).hex()}, plan)
        tree = copy.deepcopy(original)
        level.child(level.child(level.child(level.child(tree, 'Data'), 'Player'), 'Opaque'), 'Empty')['value']['element_type'] = 10
        with self.assertRaises(level.Failure):
            level.compare_c({'ok': True, 'nbt_hex': level.encode(tree).hex()}, plan)
        tree = copy.deepcopy(original)
        level.child(level.child(level.child(level.child(tree, 'Data'), 'Player'), 'Opaque'), 'Numbers')['value']['items'].reverse()
        with self.assertRaises(level.Failure):
            level.compare_c({'ok': True, 'nbt_hex': level.encode(tree).hex()}, plan)
        # Compound order alone is explicitly irrelevant.
        original['value'].reverse()
        data['value'].reverse()
        opaque['value'].reverse()
        level.compare_c({'ok': True, 'nbt_hex': level.encode(original).hex()}, plan)

    def test_strict_c_json_rejects_boolean_integer_float_aliases(self):
        context = self.fixture['contexts'][0]
        plan = next(p for p in level.replay_plan(observation(context), files(context, 'after')) if p['label'] == 'defaults-decode')
        for field, bad in (('ok', 1), ('version', False), ('version', 0.0), ('raining', 0), ('has_player', 0)):
            actual = copy.deepcopy(plan['expected'])
            actual[field] = bad
            with self.subTest(field=field, bad=bad), self.assertRaises(level.Failure):
                level.compare_c(actual, plan)
        with self.assertRaises(level.Failure):
            level.strict_json(b'{"ok":true,"ok":false}')
        with self.assertRaises(level.Failure):
            level.strict_json(b'{"value":NaN}')

    def test_sampled_clock_must_be_in_original_event_and_child_windows(self):
        value = copy.deepcopy(self.fixture)
        context = value['contexts'][0]
        tree = level.parse_file(level.FileView(files(context, 'after'))/'codec-full/retained.dat')
        level.child(level.child(tree, 'Data'), 'LastPlayed')['value'] = -(1 << 63)
        replace_file(context, 'codec-full/retained.dat', level.gzip(level.encode(tree)))
        self.reject(value)
        self.reject(self.mutate_observation(lambda x: x['codecs'][1]['retained'].update(clock_before=1, clock_after=2)))

    def test_missing_data_does_not_fall_back_but_wrong_data_does(self):
        for name in ('missing-data', 'wrong-data'):
            def mutate(record):
                row = next(x for x in record['handlers'] if x['case'] == name)
                row['load'] = {'status': 'null'}
            self.reject(self.mutate_observation(mutate))

    def test_two_save_rotation_and_list_only_version_are_observed(self):
        value = copy.deepcopy(self.fixture)
        context = value['contexts'][0]
        replace_file(context, 'handler-primary/world/level.dat_old', export.unpacked(context['after']['handler-primary/world/level.dat']))
        self.reject(value)
        self.reject(self.mutate_observation(lambda x: x['handlers'][0]['save-list']['info_after'].update(version=1234)))

    def test_short_conflicting_and_trailing_lock_bytes_are_bound(self):
        for name in ('short-lock', 'trailing-lock', 'conflict'):
            value = copy.deepcopy(self.fixture)
            context = value['contexts'][0]
            path = 'handler-'+name+'/world/session.lock'
            raw = export.unpacked(context['after'][path])
            replace_file(context, path, raw+b'\x00')
            self.reject(value)
        self.reject(self.mutate_observation(lambda x: next(r for r in x['handlers'] if r['case'] == 'conflict')['first_after_second'].update(handler_token_hex='0000000000000000')))

    def test_complete_input_output_files_and_directory_inventories_required(self):
        for phase in ('before', 'after'):
            value = copy.deepcopy(self.fixture)
            value['contexts'][0][phase].pop(next(iter(value['contexts'][0][phase])))
            self.reject(value)
        value = copy.deepcopy(self.fixture)
        value['contexts'][0]['after_inventory']['entries'] = [row for row in value['contexts'][0]['after_inventory']['entries'] if row['path'] != 'handler-true-flag/world/players']
        self.reject(value)

    def test_original_commands_and_complete_streams_are_bound(self):
        value = copy.deepcopy(self.fixture)
        value['contexts'][0]['process']['command'][5] = 'DifferentHelper'
        self.reject(value)
        value = copy.deepcopy(self.fixture)
        value['contexts'][0]['process']['stdout_retained'] -= 1
        self.reject(value)
        value = copy.deepcopy(self.fixture)
        value['contexts'][0]['process']['returncode'] = 77
        self.reject(value)

    def test_renamed_driver_and_bad_source_label_fail_before_execution(self):
        with patch.object(level, '__file__', str(ROOT/'scripts/reference/storage/renamed.py')):
            with self.assertRaises(level.Failure):
                level.helpers()
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(level.main(['--client', 'none', '--server', 'none', '--work-dir', '.local/not-created-invalid-source', '--capture-only', '--tested-commit', 'short']), 1)


class LevelExecutionGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='mc-level-preflight-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.common, self.executed = level.helpers()
        self.java = self.root/'jdk/bin/java'
        self.java.parent.mkdir(parents=True)
        self.tools = {}
        for name in ('java', 'javac', 'javap'):
            path = self.java.with_name(name)
            path.write_bytes(b'synthetic tool')
            self.tools[name] = path
        for name, relative, raw in (('jdk_release', 'release', b'JAVA_VERSION="17.0.1"\n'), ('jdk_modules', 'lib/modules', b'synthetic modules'), ('jdk_ct_sym', 'lib/ct.sym', b'synthetic symbols')):
            path = self.root/'jdk'/relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            self.tools[name] = path
        self.jars = {}
        for side in level.PIN:
            path = self.root/(side+'.jar')
            path.write_bytes(b'synthetic original archive')
            self.jars[side] = path
        self.argv = ['--client', str(self.jars['client']), '--server', str(self.jars['server']), '--work-dir', str(self.root/'captures'), '--capture-only']

    def record(self, path, **kwargs):
        result = self.original_record(path, **kwargs)
        for side, jar in self.jars.items():
            if Path(path).resolve() == jar.resolve():
                result['sha256'] = level.PIN[side]
        return result

    def run_main(self, versions, runner=None):
        self.original_record = self.common.file_record
        with patch.dict(os.environ, {name: '' for name in level.JAVA_ENVIRONMENT}), \
             patch.object(level, 'helpers', return_value=(self.common, self.executed)), \
             patch.object(self.common, 'runtime_tools', return_value=self.tools), \
             patch.object(self.common, 'file_record', side_effect=self.record), \
             patch.object(self.common, 'tool_versions', side_effect=versions), \
             patch.object(self.common, 'run_live', side_effect=runner), \
             contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            return level.main(self.argv)

    def test_true_missing_preflight_tool_is_unavailable(self):
        with patch.object(level, 'helpers', return_value=(self.common, self.executed)), \
             patch.object(self.common, 'runtime_tools', side_effect=self.common.Unavailable('missing synthetic JDK')), \
             contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(level.main(self.argv), 77)

    def output_preflight(self, work, *, missing=False, overrides=False):
        self.original_record = self.common.file_record
        args = self.argv.copy(); args[args.index('--work-dir')+1] = str(work)
        if overrides:
            for name in ('java', 'javac', 'javap'):
                args += ['--'+name, str(self.tools[name])]
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.dict(os.environ, {name: '' for name in level.JAVA_ENVIRONMENT}), \
             patch.object(level, 'helpers', return_value=(self.common, self.executed)), \
             patch.object(self.common, 'runtime_tools', side_effect=self.common.Unavailable('missing synthetic JDK') if missing else None, return_value=self.tools), \
             patch.object(self.common, 'file_record', side_effect=self.record), \
             patch.object(self.common, 'run_live') as child, \
             patch.object(self.common, 'tool_versions') as versions, \
             contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = level.main(args)
        child.assert_not_called(); versions.assert_not_called()
        self.assertEqual(stdout.getvalue(), '')
        return code, level.strict_json(stderr.getvalue().encode('utf8'))

    def test_public_new_descendant_is_refused_before_output_creation(self):
        project = self.root/'project'; work = project/'src/new-reference-output'
        with patch.object(level, 'ROOT', project):
            code, report = self.output_preflight(work)
        self.assertEqual((code, report['status']), (1, 'fail'))
        self.assertIn('public source/input layout', report['errors'][0])
        self.assertFalse(project.exists())

    def test_default_and_override_tool_roots_are_protected_before_mkdir(self):
        for overrides in (False, True):
            before = sorted(path.relative_to(self.root/'jdk').as_posix() for path in (self.root/'jdk').rglob('*'))
            code, report = self.output_preflight(self.root/'jdk', overrides=overrides)
            self.assertEqual((code, report['status']), (1, 'fail'))
            self.assertTrue(any('attributed input' in value for value in report['errors']))
            self.assertEqual(before, sorted(path.relative_to(self.root/'jdk').as_posix() for path in (self.root/'jdk').rglob('*')))

    def test_input_file_output_and_missing_prerequisite_emit_json_without_captures(self):
        original = self.jars['client'].read_bytes()
        code, report = self.output_preflight(self.jars['client'])
        self.assertEqual((code, report['status']), (1, 'fail'))
        self.assertEqual(self.jars['client'].read_bytes(), original)
        work = self.root/'missing-captures'
        code, report = self.output_preflight(work, missing=True)
        self.assertEqual((code, report['status']), (77, 'unavailable'))
        self.assertFalse(work.exists())

    def test_static_link_or_junction_output_is_refused(self):
        target, link = self.root/'owned-target', self.root/'linked-output'
        target.mkdir()
        try:
            link.symlink_to(target, target_is_directory=True)
        except OSError:
            if os.name != 'nt':
                self.skipTest('directory links are unavailable')
            import _winapi
            create = getattr(_winapi, 'CreateJunction', None)
            if create is None:
                self.skipTest('directory links and junctions are unavailable')
            try:
                create(str(target), str(link))
            except OSError:
                self.skipTest('directory links and junctions are unavailable')
        code, report = self.output_preflight(link/'nested')
        self.assertEqual((code, report['status']), (1, 'fail'))
        self.assertIn('link/reparse', report['errors'][0])
        self.assertEqual(list(target.iterdir()), [])

    def test_post_start_unavailable_and_child77_are_failures(self):
        def unavailable(*_):
            raise self.common.Unavailable('synthetic measured unavailable')
        self.assertEqual(self.run_main(unavailable), 1)

        def versions(tools, output, report):
            report['versions'] = {name: {'major': 17} for name in ('java', 'javac', 'javap')}
        def child77(command, stdout, stderr, cwd, **kwargs):
            stdout.write_bytes(b'')
            stderr.write_bytes(b'UNAVAILABLE synthetic child\n')
            return self.common.ProcessResult(list(map(str, command)), 77, 0.01, False, None, 0, 28, 0, 28, 0)
        self.assertEqual(self.run_main(versions, child77), 1)

    def test_wrong_jar_is_failure_not_unavailable(self):
        with patch.object(level, 'helpers', return_value=(self.common, self.executed)), \
             patch.object(self.common, 'runtime_tools', return_value=self.tools), \
             contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(level.main(self.argv), 1)

    def test_changed_attributed_tool_is_retained_failure(self):
        def mutate(*_):
            self.java.write_bytes(b'changed synthetic tool')
            raise self.common.Unavailable('cannot mask changed input')
        self.assertEqual(self.run_main(mutate), 1)
        manifests = list((self.root/'captures').glob('capture-*/manifest.json'))
        report = json.loads(manifests[0].read_text())
        self.assertEqual(report['status'], 'fail')
        self.assertTrue(any('attributed input changed' in error for error in report['errors']))

    def test_compiler_and_original_streams_require_canonical_paths(self):
        fixture = export.load_fixture(FIXTURE)
        for prefix, source, payloads in (
                ('compile', fixture['provenance']['compilation']['process'],
                 fixture['provenance']['compilation']),
                ('client-0', fixture['contexts'][0]['process'], fixture['contexts'][0])):
            process = copy.deepcopy(source)
            expected = {}
            for name, suffix in (('stdout', 'out'), ('stderr', 'err')):
                raw = export.unpacked(payloads[name])
                path = self.root/(prefix+'-'+suffix+'.txt')
                path.write_bytes(raw)
                process[name] = {'path': str(path), **level.fingerprint(raw)}
                expected[name] = raw
            self.assertEqual(export.retained_streams(process, self.root, prefix, self.common), expected)
            for name in ('stdout', 'stderr'):
                alias = self.root/(prefix+'-'+name+'-alias.txt')
                alias.write_bytes(expected[name])
                redirected = copy.deepcopy(process)
                redirected[name]['path'] = str(alias)
                # Byte-identical sibling streams cannot substitute for the
                # canonical process output, even if its own file is absent.
                canonical = Path(process[name]['path'])
                canonical.unlink()
                with self.subTest(prefix=prefix, stream=name), self.assertRaisesRegex(level.Failure, 'canonical .* stream path'):
                    export.retained_streams(redirected, self.root, prefix, self.common)
                canonical.write_bytes(expected[name])
            if prefix == 'compile':
                swapped = copy.deepcopy(process)
                swapped['stdout'], swapped['stderr'] = swapped['stderr'], swapped['stdout']
                with self.assertRaisesRegex(level.Failure, 'canonical compile stdout stream path'):
                    export.retained_streams(swapped, self.root, prefix, self.common)

    def test_complete_child_stream_counters_are_checked_before_class_inventory(self):
        def versions(tools, output, report):
            report['versions'] = {name: {'major': 17} for name in ('java', 'javac', 'javap')}
        def mismatched_stream(command, stdout, stderr, cwd, **kwargs):
            stdout.write_bytes(b'x')
            stderr.write_bytes(b'')
            return self.common.ProcessResult(list(map(str, command)), 0, 0.01, False, None,
                                             0, 0, 0, 0, 0)
        self.assertEqual(self.run_main(versions, mismatched_stream), 1)
        reports = list((self.root/'captures').glob('capture-*/manifest.json'))
        report = level.strict_json(reports[0].read_bytes())
        self.assertTrue(any('complete retained process stream byte count' in error for error in report['errors']))
        self.assertTrue((reports[0].parent/'compile-out.txt').read_bytes() == b'x')


def replay(executable, fixture):
    count = 0
    for context in fixture['contexts']:
        for plan in level.replay_plan(observation(context), files(context, 'after')):
            completed = subprocess.run([str(executable.resolve()), *plan['argv']], input=plan['input'].hex().encode('ascii')+b'\n',
                                       capture_output=True, timeout=15)
            level.require(completed.returncode == 0 and completed.stderr == b'' and len(completed.stdout) <= level.MAX_RECORD and
                          completed.stdout.endswith(b'\n') and completed.stdout.count(b'\n') == 1, 'successful bounded C fixture replay')
            level.compare_c(level.strict_json(completed.stdout), plan)
            count += 1
    level.require(count == 80, 'all original-only C codec fixture comparisons')
    print('PASS: 80 complete original-only metadata codec comparisons')
    # These are explicitly C safety-policy checks, separate from original
    # serializers, which always construct fresh unnamed root/Data compounds.
    context = fixture['contexts'][0]
    captured = files(context, 'after')
    source = level.parse_file(level.FileView(captured)/'codec-full/input.dat')
    source['name_utf16'] = level.u16('caller\x00\ud800root')
    source_raw = level.encode(source)
    safety = []
    for mode, keep in (('retained', 'keep'), ('empty-list', 'drop')):
        expected = level.parse_file(level.FileView(captured)/'codec-full'/(mode+'.dat'))
        expected['name_utf16'] = source['name_utf16']
        data = level.child(expected, 'Data')
        known = {x['name_utf16'] for x in data['value']} | {level.u16('Player')}
        data['value'] += [copy.deepcopy(x) for x in level.child(source, 'Data')['value'] if x['name_utf16'] not in known]
        expected['value'] += [copy.deepcopy(x) for x in source['value'] if x['name_utf16'] != level.u16('Data')]
        saved_at = level.child(data, 'LastPlayed')['value']
        safety.append({'kind': 'C-only-preservation-policy', 'label': 'preserve-'+keep,
                       'input': source_raw, 'argv': ['build', f'{saved_at & ((1 << 64)-1):016x}', keep, 'preserve'], 'expected': expected})
    raw = level.encode(level.parse_file(level.FileView(captured)/'codec-defaults/input.dat'))
    for saved_at in (-(1 << 63), -1, 0, (1 << 63)-1):
        expected = level.parse_file(level.FileView(captured)/'codec-defaults/retained.dat')
        level.child(level.child(expected, 'Data'), 'LastPlayed')['value'] = saved_at
        safety.append({'kind': 'C-only-clock-input', 'label': 'clock-'+str(saved_at), 'input': raw,
                       'argv': ['build', f'{saved_at & ((1 << 64)-1):016x}', 'keep', 'fresh'], 'expected': expected})
    for plan in safety:
        completed = subprocess.run([str(executable.resolve()), *plan['argv']], input=plan['input'].hex().encode('ascii')+b'\n', capture_output=True, timeout=15)
        level.require(completed.returncode == 0 and completed.stderr == b'' and len(completed.stdout) <= level.MAX_RECORD and
                      completed.stdout.endswith(b'\n') and completed.stdout.count(b'\n') == 1, 'successful separate C safety-policy replay')
        level.compare_c(level.strict_json(completed.stdout), plan)
    print('PASS: 6 separate C preservation/drop and explicit-clock safety checks')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    args = parser.parse_args(argv)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__]))
    if not result.wasSuccessful():
        return 1
    if args.executable is not None:
        replay(args.executable, export.load_fixture(FIXTURE))
    return 0


if __name__ == '__main__':
    sys.exit(main())
