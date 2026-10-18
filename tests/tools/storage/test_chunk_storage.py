"""Java-free chunk-storage fixture, protocol and bounded execution guards."""
from __future__ import annotations
import argparse
import base64
import contextlib
import copy
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
sys.path.insert(0, str(ROOT/'scripts/reference/storage'))
import compare_chunk_storage as chunks
import export_chunk_storage as export
sys.path.pop(0)
FIXTURE = ROOT/'tests/reference/storage/chunk_storage_fixtures.json'


def observation(context):
    return chunks.strict_json(export.unpacked(context['record']))


def artifacts(context):
    return {name: export.unpacked(record, chunks.MAX_NBT) for name, record in context['artifacts'].items()}


def replace_record(context, value):
    context['record'] = export.packed((json.dumps(value, ensure_ascii=True, separators=(',', ':'))+'\n').encode('ascii'))


def replace_artifact(context, name, raw):
    previous = context['artifacts'][name]['bytes']
    context['artifacts'][name] = export.packed(raw)
    inventory = context['artifact_inventory']
    inventory['bytes'] += len(raw)-previous
    next(row for row in inventory['entries'] if row['path'] == name).update(chunks.fp(raw))
    value = observation(context)
    def walk(node):
        if type(node) is dict:
            if node.get('artifact') == name:
                node.update(chunks.fp(raw))
            for item in node.values():
                walk(item)
        elif type(node) is list:
            for item in node:
                walk(item)
    walk(value)
    replace_record(context, value)


class ChunkStorageGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = export.load_fixture(FIXTURE)

    def reject(self, value):
        with self.assertRaises((chunks.Failure, ValueError, KeyError, TypeError)):
            export.validate_fixture(value)

    def mutated(self, callback):
        value = copy.deepcopy(self.fixture)
        context = value['contexts'][0]
        record = observation(context)
        callback(record)
        replace_record(context, record)
        return value

    def test_all_original_cases_artifacts_controls_and_replay_profiles_present(self):
        self.assertEqual(len(self.fixture['preparations']), 2)
        plans = []
        for context in self.fixture['contexts']:
            value, files = observation(context), artifacts(context)
            self.assertEqual(len(value['cases']), 28)
            self.assertEqual(len(files), 88)
            plans += chunks.replay_plans(value, files)
        self.assertEqual(len(plans), 268)
        counts = {kind: sum(p['kind'] == kind for p in plans) for kind in {p['kind'] for p in plans}}
        self.assertEqual(counts, {'raw-decode-or-explicit-departure': 112, 'explicit-frozen-normalization': 36,
                                 'supported-empty-opaque-fresh-serialization': 48, 'C-only-opaque-preservation-policy': 12,
                                 'C-only-opaque-edit-refusal': 48, 'C-only-opaque-fresh-refusal': 12})

    def test_complete_packed_streams_hashes_bounds_and_no_concatenation(self):
        raw = b'original chunk bytes\x00\xff'
        value = export.packed(raw)
        self.assertEqual(export.unpacked(value), raw)
        for changes in ({'bytes': True}, {'bytes': float(len(raw))}, {'bytes': len(raw)+1}, {'sha256': '0'*64}, {'zlib_base64': '!'}):
            with self.assertRaises(chunks.Failure):
                export.unpacked(value | changes)
        for compressed in (zlib.compress(raw)[:-1], zlib.compress(raw)+b'extra', zlib.compress(raw)+zlib.compress(b'next')):
            with self.assertRaises(chunks.Failure):
                export.unpacked(value | {'zlib_base64': base64.b64encode(compressed).decode('ascii')})
        with self.assertRaises(chunks.Failure):
            export.unpacked(value, len(raw)-1)

    def test_complete_contexts_preparations_and_exact_integer_repeats_required(self):
        for mode in ('omit', 'duplicate', 'order', 'float', 'preparation'):
            value = copy.deepcopy(self.fixture)
            if mode == 'omit':
                value['contexts'].pop()
            elif mode == 'duplicate':
                value['contexts'][3] = copy.deepcopy(value['contexts'][2])
            elif mode == 'order':
                value['contexts'].reverse()
            elif mode == 'float':
                value['contexts'][1]['repeat'] = 1.0
            else:
                value['preparations'].pop('server')
            self.reject(value)

    def test_original_only_source_class_tool_and_binding_provenance_required(self):
        for mutate in (lambda x: x['provenance'].update(mode='live-original-C'),
                       lambda x: x['provenance'].update(tested_commit='short'),
                       lambda x: x['provenance']['source_fingerprints'].pop('src/world/storage/chunk.c'),
                       lambda x: x['provenance']['helper_classes'].pop('ChunkStorageObservation.class'),
                       lambda x: x['provenance']['helper_classes'].pop('ChunkStorageObservation$BoundedOutput.class'),
                       lambda x: x['provenance']['executed_source_modules']['scripts/reference/storage/compare_level.py'].update(bytes=1.0),
                       lambda x: x['provenance']['helper_sources']['ChunkStorageObservation.java'].update(bytes=True),
                       lambda x: x['provenance']['tools']['java'].update(major=17.0),
                       lambda x: x['provenance']['compilation']['process']['command'].__setitem__(0, 'tool:other-compiler')):
            value = copy.deepcopy(self.fixture); mutate(value); self.reject(value)
        value = copy.deepcopy(self.fixture)
        value['contexts'][0]['bindings']['methods']['schema_decode']['sha256'] = '0'*64
        self.reject(value)

    def test_origin_loader_and_exact_method_return_field_bindings(self):
        for mutate in (lambda x: x['class_origins']['to'].update(loader='system'),
                       lambda x: x['class_origins']['to'].update(origin='supplied:server'),
                       lambda x: x['bindings'].pop('actual_writer'),
                       lambda x: x['bindings']['schema_decode'].update(signature='(Lfd;Lnu;)V'),
                       lambda x: x['bindings']['actual_loader'].update(**{'class': 'to'}),
                       lambda x: x['fields']['vi.a'].update(modifiers=9)):
            self.reject(self.mutated(mutate))

    def test_no_omitted_or_reordered_case_or_stage(self):
        for mutate in (lambda x: x['cases'].pop(), lambda x: x['cases'].reverse(),
                       lambda x: x['cases'][0]['static'].update(stage='decode'),
                       lambda x: x['cases'][0]['region'].pop('chunk_after_write'),
                       lambda x: x['cases'][0].pop('context_after')):
            self.reject(self.mutated(mutate))

    def test_full_arrays_coordinates_minimum_and_transient_flags_are_checked(self):
        for mutate in (lambda x: x['cases'][0]['static']['chunk_before_write']['coordinates'].__setitem__(0, 2),
                       lambda x: x['cases'][0]['static']['chunk_before_write']['data'].update(minimum_height=1),
                       lambda x: x['cases'][0]['static']['chunk_before_write']['data']['metadata'].update(length=16383),
                       lambda x: x['cases'][0]['static']['chunk_before_write']['flags'].update(o=True),
                       lambda x: x['cases'][0]['static']['chunk_before_write'].update(last_save_time='00000000000004d2')):
            self.reject(self.mutated(mutate))
        value = self.mutated(lambda x: x['cases'][0]['static']['chunk_before_write']['data']['blocks'].update(
            values='00'+x['cases'][0]['static']['chunk_before_write']['data']['blocks']['values'][2:]))
        self.reject(value)

    def test_all_owner_identities_and_per_case_global_context_are_retained(self):
        for mutate in (lambda x: x['cases'][0]['static']['chunk_before_write']['world'].update(ref='r99999'),
                       lambda x: x['cases'][0]['static']['chunk_before_write']['array_references'][1].update(ref=x['cases'][0]['static']['chunk_before_write']['array_references'][0]['ref']),
                       lambda x: x['cases'][0]['static']['chunk_after_write']['array_references'][0].update(ref='r99998'),
                       lambda x: x['cases'][0]['context_after']['world']['cache'].update(ref='r99997'),
                       lambda x: x['after']['world']['queue']['entries'].append({})):
            self.reject(self.mutated(mutate))
        def coherent_wrong_world(record):
            for context in [record['setup'], record['after'], *[row['context_after'] for row in record['cases']]]:
                context['world']['cache']['class'] = 'jb'
        self.reject(self.mutated(coherent_wrong_world))
        def reordered_queries(record):
            for context in [record['setup'], record['after'], *[row['context_after'] for row in record['cases']]]:
                context['world']['presence'].reverse()
        self.reject(self.mutated(reordered_queries))

    def test_wrong_location_and_ignored_last_update_are_not_repaired(self):
        self.reject(self.mutated(lambda x: next(r for r in x['cases'] if r['name'] == 'wrong_location')['region']['chunk_before_write'].update(coordinates=[1, 0])))
        self.reject(self.mutated(lambda x: next(r for r in x['cases'] if r['name'] == 'ignored_wrong_last_update')['static']['chunk_before_write'].update(last_save_time='00000000000004d2')))

    def test_missing_and_short_arrays_remain_original_observations(self):
        for name in ('missing_Data', 'short_all_arrays', 'missing_level'):
            def mutate(value):
                row = next(r for r in value['cases'] if r['name'] == name)
                row['static']['chunk_before_write']['data']['metadata'] = {'length': 16384, 'values': '00'*16384}
            self.reject(self.mutated(mutate))
        self.reject(self.mutated(lambda x: next(r for r in x['cases'] if r['name'] == 'missing_Blocks')['region'].update(status='exception')))

    def test_registry_table_classes_and_explicit_normalizer_are_exact(self):
        for mutate in (lambda x: x['registry_before']['map'].update(values='00'*256),
                       lambda x: x['registry_after']['registry_identity'].update(ref='r9000'),
                       lambda x: x['registry_before']['registered_classes'].__setitem__(1, None)):
            self.reject(self.mutated(mutate))
        def replace_registry(value):
            for key in ('registry_before', 'registry_after'):
                value[key]['registered_classes'][1] = None
                raw = bytearray.fromhex(value[key]['map']['values']); raw[1] = 0
                value[key]['map']['values'] = raw.hex()
        self.reject(self.mutated(replace_registry))

    def test_original_factory_discard_q_reset_and_real_boat_sign_are_retained(self):
        for mutate in (lambda x: next(r for r in x['cases'] if r['name'] == 'unknown_entity')['static']['chunk_before_write']['flags'].update(q=False),
                       lambda x: next(r for r in x['cases'] if r['name'] == 'unknown_entity')['static']['chunk_after_write']['flags'].update(q=True),
                       lambda x: x['cases'][-1]['static']['chunk_before_write']['tile_map'].update(entries=[]),
                       lambda x: x['cases'][-1]['static']['chunk_before_write']['tile_map']['entries'][0]['key_fields'].update(a=16)):
            self.reject(self.mutated(mutate))

    def test_every_original_input_and_output_artifact_is_hashed_and_consumed(self):
        value = copy.deepcopy(self.fixture); value['contexts'][0]['artifacts'].pop(next(iter(value['contexts'][0]['artifacts']))); self.reject(value)
        value = copy.deepcopy(self.fixture); value['contexts'][0]['artifacts']['extra.nbt'] = export.packed(bytes.fromhex('0a000000')); self.reject(value)
        value = copy.deepcopy(self.fixture); context = value['contexts'][0]
        original = observation(context); name = original['cases'][0]['static']['output_level']['artifact']
        tree = chunks.parse_raw(export.unpacked(context['artifacts'][name])); chunks.child(tree, 'xPos')['value'] = 42
        replace_artifact(context, name, chunks.level.encode(tree)); self.reject(value)

    def test_unknown_input_fields_do_not_appear_in_fresh_original_outputs(self):
        value = copy.deepcopy(self.fixture); context = value['contexts'][0]; original = observation(context)
        name = original['cases'][0]['region']['output_root']['artifact']; tree = chunks.parse_raw(export.unpacked(context['artifacts'][name]))
        chunks.put(tree, chunks.level.string('OpaqueRoot', 'retained input only'))
        replace_artifact(context, name, chunks.level.encode(tree)); self.reject(value)

    def test_source_inputs_are_fixed_even_with_coherently_rehashed_descriptors(self):
        value = copy.deepcopy(self.fixture); context = value['contexts'][0]; original = observation(context)
        name = original['cases'][0]['input']['artifact']; tree = chunks.parse_raw(export.unpacked(context['artifacts'][name]))
        chunks.put(chunks.child(tree, 'Level'), chunks.level.integer('xPos', 42))
        replace_artifact(context, name, chunks.level.encode(tree)); self.reject(value)

    def test_full_c_state_and_refusal_json_use_strict_types(self):
        context = self.fixture['contexts'][0]; plans = chunks.replay_plans(observation(context), artifacts(context))
        for name in ('valid_saved-raw', 'unknown_tile-opaque-refuse-q'):
            plan = next(p for p in plans if p['name'] == name)
            chunks.compare_c(copy.deepcopy(plan['expected']), plan)
            for mutate in (lambda x: x['chunk']['flags'].update(c=0), lambda x: x['chunk']['data'].update(minimum_height=0.0),
                           lambda x: x.update(consumed=float(x['consumed'])), lambda x: x['chunk']['coordinates'].__setitem__(1, 0.0)):
                value = copy.deepcopy(plan['expected']); mutate(value)
                with self.assertRaises(chunks.Failure):
                    chunks.compare_c(value, plan)

    def test_serialization_only_ignores_compound_order(self):
        context = self.fixture['contexts'][0]
        plan = next(p for p in chunks.replay_plans(observation(context), artifacts(context)) if p['name'] == 'valid_saved-static-fresh')
        value = copy.deepcopy(plan['expected']); tree = copy.deepcopy(plan['nbt']); chunks.child(tree, 'Level')['value'].reverse()
        value['nbt_hex'] = chunks.level.encode(tree).hex(); chunks.compare_c(value, plan)
        chunks.child(chunks.child(tree, 'Level'), 'Entities')['value']['element_type'] = 10
        value['nbt_hex'] = chunks.level.encode(tree).hex()
        with self.assertRaises(chunks.Failure):
            chunks.compare_c(value, plan)

    def test_original_frames_are_recomputed_even_when_flag_false_on_main(self):
        for main in (True, False):
            value = copy.deepcopy(self.fixture); context = value['contexts'][0]
            threads = chunks.strict_json(export.unpacked(context['threads_before']))
            row = next(row for row in threads if (row['name'] == 'main') is main)
            row['frames'].append('to.a(SourceFile:1)'); row['original_frame'] = False
            context['threads_before'] = export.packed(json.dumps(threads).encode()); self.reject(value)
        value = copy.deepcopy(self.fixture); context = value['contexts'][0]
        threads = chunks.strict_json(export.unpacked(context['threads_after']))
        next(row for row in threads if row['name'] != 'main')['daemon'] = False
        context['threads_after'] = export.packed(json.dumps(threads).encode()); self.reject(value)

    def test_complete_world_bytes_inventories_and_only_genuine_writes(self):
        for mutate in (lambda x: x['contexts'][0]['world_before'].pop('world/level.dat'),
                       lambda x: x['contexts'][0]['world_after_inventory'].update(extra=True),
                       lambda x: x['contexts'][0]['world_before_inventory'].update(files=4.0),
                       lambda x: x['contexts'][0]['world_after_inventory'].update(bytes=True),
                       lambda x: x['preparations']['client']['files'].pop('world/region/r.0.0.mcr')):
            value = copy.deepcopy(self.fixture); mutate(value); self.reject(value)

    def test_complete_commands_streams_and_control_bytes_are_required(self):
        for mutate in (lambda x: x['contexts'][0]['process']['command'].__setitem__(6, 'OtherHelper'),
                       lambda x: x['contexts'][0]['process'].update(returncode=77),
                       lambda x: x['contexts'][0]['process'].update(stdout_observed=False),
                       lambda x: x['contexts'][0]['process'].update(stderr_retained=0)):
            value = copy.deepcopy(self.fixture); mutate(value); self.reject(value)
        self.reject(self.mutated(lambda x: x['cases'][0]['region'].update(extra=True)))

    def test_strict_json_and_renamed_source_guards(self):
        for raw in (b'{"x":0,"x":1}', b'{"x":NaN}'):
            with self.assertRaises(chunks.Failure):
                chunks.strict_json(raw)
        with patch.object(chunks, '__file__', str(ROOT/'scripts/reference/storage/renamed.py')):
            with self.assertRaises(chunks.Failure):
                chunks.helpers()
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(chunks.main(['--client', 'none', '--server', 'none', '--work-dir', '.local/not-created-bad-chunk-label', '--capture-only', '--tested-commit', 'short']), 1)


class ChunkExporterStreamGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='mc-chunk-export-streams-'); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.common, _, _ = chunks.helpers()

    def process(self, prefix, stdout=b'', stderr=b''):
        record = {'command': ['synthetic-tool'], 'returncode': 0, 'seconds': 0.01, 'timed_out': False, 'stop_reason': None,
                  'stdout_observed': len(stdout), 'stdout_retained': len(stdout), 'stderr_observed': len(stderr),
                  'stderr_retained': len(stderr), 'inventory_polls': 0}
        for name, suffix, raw in (('stdout', 'out', stdout), ('stderr', 'err', stderr)):
            path = self.root/(prefix+'-'+suffix+'.txt'); path.write_bytes(raw)
            record[name] = self.common.file_record(path, byte_limit=chunks.MAX_RECORD)
        return record

    def normalize(self, record, prefix):
        return export.normalized_process(record, ['logical-tool'], self.root, self.common, prefix=prefix)

    def test_compile_prepare_and_original_stream_bytes_bind_to_canonical_files(self):
        for prefix in ('compile', 'prepare', 'original'):
            for stdout, stderr in ((b'', b''), (b'actual stdout\x00\xff', b'actual stderr\r\n')):
                with self.subTest(prefix=prefix, empty=not stdout):
                    normalized = self.normalize(self.process(prefix, stdout, stderr), prefix)
                    self.assertEqual(normalized['process']['command'], ['logical-tool'])
                    for name, raw in (('stdout', stdout), ('stderr', stderr)):
                        self.assertEqual(export.unpacked(normalized[name]), raw)
                        self.assertEqual(normalized['process'][name], chunks.fp(raw))

    def test_identical_or_external_stream_rebinding_cannot_mask_missing_canonical_file(self):
        for prefix in ('compile', 'prepare', 'original'):
            record = self.process(prefix)
            changed = copy.deepcopy(record); changed['stdout'] = copy.deepcopy(changed['stderr'])
            with self.assertRaises(chunks.Failure):
                self.normalize(changed, prefix)
            (self.root/(prefix+'-out.txt')).unlink()
            with self.assertRaises(chunks.Failure):
                self.normalize(changed, prefix)
            with self.assertRaises((chunks.Failure, OSError)):
                self.normalize(record, prefix)
            external = self.root/'other.txt'; external.write_bytes(b'')
            changed['stdout'] = self.common.file_record(external)
            with self.assertRaises(chunks.Failure):
                self.normalize(changed, prefix)

    def test_coherently_changed_native_counters_and_fingerprint_shapes_are_rejected(self):
        for prefix in ('compile', 'prepare', 'original'):
            for raw in (b'', b'complete bytes'):
                record = self.process(prefix, raw, raw)
                for stream in ('stdout', 'stderr'):
                    for count in (len(raw)+1, float(len(raw)), bool(raw)):
                        changed = copy.deepcopy(record)
                        changed[stream+'_observed'] = changed[stream+'_retained'] = count
                        with self.subTest(prefix=prefix, stream=stream, count=count), self.assertRaises(chunks.Failure):
                            self.normalize(changed, prefix)
                    for mutate in (lambda x: x[stream].update(extra=True),
                                   lambda x: x[stream].pop('path'),
                                   lambda x: x[stream].update(bytes=float(len(raw)))):
                        changed = copy.deepcopy(record); mutate(changed)
                        with self.assertRaises(chunks.Failure):
                            self.normalize(changed, prefix)
                changed = copy.deepcopy(record); changed['extra'] = True
                with self.assertRaises(chunks.Failure):
                    self.normalize(changed, prefix)

    def version(self, name):
        tool = self.root/(name+'.synthetic'); tool.write_bytes(b'synthetic tool bytes')
        record = self.process('unused')
        record.pop('stdout'); record.pop('stderr')
        record.update(command=[str(tool), '-version'], major=17)
        streams = {'stdout': b'', 'stderr': (name+' version 17.0.1\r\n').encode('ascii')}
        for stream, raw in streams.items():
            (self.root/(name+'-version-'+stream+'.txt')).write_bytes(raw)
            record[stream+'_observed'] = record[stream+'_retained'] = len(raw)
        record['output'] = (streams['stdout']+b'\n'+streams['stderr']).decode('ascii')
        return record, {str(tool): self.common.file_record(tool)}

    def test_all_version_stream_files_and_counters_are_bound_before_redaction(self):
        for name in ('java', 'javac', 'javap'):
            record, before = self.version(name)
            normalized = export.normalized_version(record, name, self.root, before, self.common)
            self.assertEqual(normalized, {'executable': chunks.fp(b'synthetic tool bytes'), 'major': 17, 'output': record['output']})
            for stream in ('stdout', 'stderr'):
                for count in (record[stream+'_observed']+1, float(record[stream+'_observed']), False):
                    changed = copy.deepcopy(record)
                    changed[stream+'_observed'] = changed[stream+'_retained'] = count
                    with self.assertRaises(chunks.Failure):
                        export.normalized_version(changed, name, self.root, before, self.common)
            for mutate in (lambda x: x.update(major=17.0), lambda x: x.update(output=x['output']+'extra'),
                           lambda x: x.update(stdout={'path': str(self.root/'other.txt'), **chunks.fp(b'')})):
                changed = copy.deepcopy(record); mutate(changed)
                with self.assertRaises(chunks.Failure):
                    export.normalized_version(changed, name, self.root, before, self.common)
            path = self.root/(name+'-version-stdout.txt'); path.unlink()
            with self.assertRaises((chunks.Failure, OSError)):
                export.normalized_version(record, name, self.root, before, self.common)


class ChunkExecutionGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='mc-chunk-storage-preflight-'); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); self.common, self.old, self.executed = chunks.helpers()
        self.tools = {}
        for name in ('java', 'javac', 'javap'):
            path = self.root/'jdk/bin'/name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'synthetic tool'); self.tools[name] = path
        for name, relative, raw in (('jdk_release', 'release', b'JAVA_VERSION="17.0.1"\n'), ('jdk_modules', 'lib/modules', b'modules'), ('jdk_ct_sym', 'lib/ct.sym', b'symbols')):
            path = self.root/'jdk'/relative; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(raw); self.tools[name] = path
        self.jars = {}
        for side in chunks.level.PIN:
            path = self.root/(side+'.jar'); path.write_bytes(b'synthetic archive'); self.jars[side] = path
        self.argv = ['--client', str(self.jars['client']), '--server', str(self.jars['server']), '--work-dir', str(self.root/'captures'), '--capture-only']

    def record(self, path, **kwargs):
        value = self.original_record(path, **kwargs)
        for side, jar in self.jars.items():
            if Path(path).resolve() == jar.resolve():
                value['sha256'] = chunks.level.PIN[side]
        return value

    def mocked(self, versions, runner=None):
        self.original_record = self.common.file_record
        with patch.dict(os.environ, {name: '' for name in chunks.level.JAVA_ENVIRONMENT}), \
             patch.object(chunks, 'helpers', return_value=(self.common, self.old, self.executed)), \
             patch.object(self.common, 'runtime_tools', return_value=self.tools), \
             patch.object(self.common, 'file_record', side_effect=self.record), \
             patch.object(self.common, 'tool_versions', side_effect=versions), \
             patch.object(self.common, 'run_live', side_effect=runner), \
             contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            return chunks.main(self.argv)

    def test_missing_preflight_is_only_unavailable_case(self):
        with patch.object(chunks, 'helpers', return_value=(self.common, self.old, self.executed)), \
             patch.object(self.common, 'runtime_tools', side_effect=self.common.Unavailable('missing synthetic JDK')), \
             contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(chunks.main(self.argv), 77)

    def output_preflight(self, work, *, missing=False, overrides=False):
        self.original_record = self.common.file_record
        args = self.argv.copy(); args[args.index('--work-dir')+1] = str(work)
        if overrides:
            for name in ('java', 'javac', 'javap'):
                args += ['--'+name, str(self.tools[name])]
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.dict(os.environ, {name: '' for name in chunks.level.JAVA_ENVIRONMENT}), \
             patch.object(chunks, 'helpers', return_value=(self.common, self.old, self.executed)), \
             patch.object(self.common, 'runtime_tools', side_effect=self.common.Unavailable('missing synthetic JDK') if missing else None, return_value=self.tools), \
             patch.object(self.common, 'file_record', side_effect=self.record), \
             patch.object(self.common, 'run_live') as child, \
             patch.object(self.common, 'tool_versions') as versions, \
             contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = chunks.main(args)
        child.assert_not_called(); versions.assert_not_called()
        self.assertEqual(stdout.getvalue(), '')
        return code, chunks.strict_json(stderr.getvalue().encode('utf8'))

    def test_public_new_descendant_is_refused_before_output_creation(self):
        project = self.root/'project'; work = project/'src/new-reference-output'
        with patch.object(chunks, 'ROOT', project), patch.object(chunks.level, 'ROOT', project):
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

    def test_measured_unavailable_and_child77_are_failures(self):
        def unavailable(*_):
            raise self.common.Unavailable('synthetic executed unavailable')
        self.assertEqual(self.mocked(unavailable), 1)
        def versions(tools, output, report):
            report['versions'] = {name: {'major': 17} for name in ('java', 'javac', 'javap')}
        def child(command, out, err, cwd, **kwargs):
            out.write_bytes(b''); err.write_bytes(b'UNAVAILABLE synthetic child\n')
            return self.common.ProcessResult(list(map(str, command)), 77, 0.01, False, None, 0, 28, 0, 28, 0)
        self.assertEqual(self.mocked(versions, child), 1)

    def test_wrong_jar_and_changed_input_cannot_be_unavailable(self):
        with patch.object(chunks, 'helpers', return_value=(self.common, self.old, self.executed)), \
             patch.object(self.common, 'runtime_tools', return_value=self.tools), \
             contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(chunks.main(self.argv), 1)
        def mutate(*_):
            self.tools['java'].write_bytes(b'changed synthetic tool')
            raise self.common.Unavailable('changed after attribution')
        self.assertEqual(self.mocked(mutate), 1)
        reports = [json.loads(path.read_bytes()) for path in (self.root/'captures').glob('capture-*/manifest.json')]
        self.assertTrue(any(any('attributed input changed' in error for error in report['errors']) for report in reports))


def replay(executable, fixture):
    count = 0
    for context in fixture['contexts']:
        for plan in chunks.replay_plans(observation(context), artifacts(context)):
            process = subprocess.run([str(executable.resolve()), *plan['argv']], input=plan['input'].hex().encode('ascii')+b'\n', capture_output=True, timeout=15)
            chunks.require(process.returncode == 0 and process.stderr == b'' and len(process.stdout) <= 2*1024*1024 and
                           process.stdout.endswith(b'\n') and process.stdout.count(b'\n') == 1, 'complete bounded C chunk replay without sanitizer diagnostics')
            chunks.compare_c(chunks.strict_json(process.stdout), plan); count += 1
    chunks.require(count == 268, 'all finite original-derived and separate-policy C calls')
    print('PASS: 196 original-derived chunk codec/normalization comparisons and explicit departures; 72 separate C opaque-policy checks')
    original = fixture['contexts'][0]; raw = next(p['input'] for p in chunks.replay_plans(observation(original), artifacts(original)) if p['name'] == 'valid_saved-raw')
    malformed = [([], b''), (['decode', '-'], b'0\n'), (['decode', '-'], b'GG\n'), (['decode', '-'], raw.hex().encode()+b'\nextra'),
                 (['decode', '0'], raw.hex().encode()+b'\n'), (['build', '0', '-', 'keep', 'none'], raw.hex().encode()+b'\n'),
                 (['build', '00000000000004d2', '-', 'keep', 'unsupported'], raw.hex().encode()+b'\n')]
    for argv, wire in malformed:
        result = subprocess.run([str(executable.resolve()), *argv], input=wire, capture_output=True, timeout=15)
        chunks.require(result.returncode == 2 and result.stdout == b'', 'malformed wire/argv fails without invented JSON')
    print('PASS: 7 malformed wire/argv exit2/no-stdout guards')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('--executable', type=Path); args = parser.parse_args(argv)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__]))
    if not result.wasSuccessful():
        return 1
    if args.executable is not None:
        replay(args.executable, export.load_fixture(FIXTURE))
    return 0


if __name__ == '__main__':
    sys.exit(main())
