"""Replay every retained EW frame attempt; failed intervals grant no cohort gain."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
RECEIPT = ROOT / 'tests/benchmarks/chunk_cache_metadata_frames_windows.json'
SPEC = importlib.util.spec_from_file_location(
    'retained_metadata_frames_parser', ROOT / 'scripts/benchmarks/client_frames_run.py')
FRAMES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FRAMES)
ORDER = [(0, 'baseline'), (0, 'ew_failure'), (1, 'ew_failure'), (1, 'baseline'),
         (2, 'baseline'), (2, 'ew_failure'), (3, 'ew_failure'), (3, 'baseline')]
STDOUT_IDENTITIES = (
    '5b4b75e7c253cceba8070fedd2613614dbf318c6f1720c6b18febba6b4bc17f6',
    'b0e7a3d96646a7a65b0839a14202ff2f612ba5601db49be54013d0aad92d3591',
    '12d57d88039bff2bf732ba1f16ad18a8e35da2b6beac0c05222986db24d7ae64',
    'ac1d42b9bf6886f72ca6b91f228cf2eda4760e350b5d09d4613ed480673d8a86',
    '5225c0eaa7b3253184df80949425fe19fabb45d87c99f898a5d8077592854c70',
    '55d50365ea02adfd7aaa32578788fe01a88f51184cda647a67f721a02aecb8f4',
    'cb21f6f987358e0894784441ba9e1108718c85e5bab79048d424e23a9eb03de5',
    'ba8a6603a20cbb15762d64680fb198b1bb6223ff9aab8d0162aee069c5f3c782',
)
# Frames, World coordinator returns, World-time delta, invalid-focus frames.
ACTUAL = [(392, 195, 0, 392), (314, 192, 192, 0), (359, 194, 194, 0),
          (337, 192, 60, 253), (390, 192, 192, 0), (322, 192, 192, 0),
          (286, 194, 129, 117), (307, 192, 192, 0)]
FLAGS = [False, True, True, False, True, True, False, True]


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def canonical(value):
    return digest(json.dumps(value, sort_keys=True, separators=(',', ':')).encode())


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key')
        result[key] = value
    return result


def rows(text):
    return [[int(value) for value in line.split(' ', 1)[1].split(',')]
            for line in text.splitlines() if line.startswith('PERF_ROW ')]


def receipts(text):
    return {name: [line.split(' ', 1)[1] for line in text.splitlines()
                   if line.startswith('PERF_' + name + ' ')]
            for name in ('WORK', 'CACHE', 'POSE', 'CONTEXT', 'FOREGROUND', 'RESULT', 'FINISH')}


def replay_profiles(document):
    result = []
    for attempt in document['attempts']:
        observed = copy.deepcopy(attempt['observation'])
        observed['summary'] = FRAMES.summarize(attempt['stdout_text'])
        result.append({'round': attempt['round'], 'label': attempt['label'],
                       'driver_exit': attempt['driver_exit'], 'observation': observed})
    return result


def audit_build(build):
    require(canonical(build) == '84f6d8ef00326dee4f10acab2260e0d59db03795f40ea41f7cc65cc49def2f14',
            'Frozen complete build, source projection and history identities')
    plan, audit, receipt = build['plan'], build['audit'], build['receipt']
    require(receipt['status'] == 'PASS' and audit['status'] == 'BUILT_AUDITED_NOT_RUN' and
            receipt['all_scoped_inputs_unchanged'] is True, 'Actual successful guarded build')
    require(audit['plan_sha256'] == receipt['plan_sha256'] == build['plan_artifact']['sha256'] and
            receipt['build_audit_sha256'] == build['audit_artifact']['sha256'], 'Plan/audit/product binding')
    require(audit['compiler_executions'] == build['new_compiler_executions'] == 0 and
            audit['new_link_executions'] == build['new_link_executions'] == 1 and
            build['candidate_link_receipt']['exit'] == 0, 'No new compiles; one actual link')
    prior = build['prior_frame_build']
    require(prior['observer']['object_sha256'] ==
            'ed64a853ef7fb1368b4ba2db46c0d182ce445e348a6235668055c6493282866a' and
            prior['observer']['compiled_once'] is True and prior['observer']['shared_by_variants'] is True,
            'Exact single retained observer object')
    require(prior['compile_count'] == 13 and prior['actual_header_count'] == 1104 and
            len(plan['shared_support_objects']) == len(prior['sdk_support_objects']) == 4 and
            len(plan['retained_objects']) == 6 and len(plan['ordered_library_tail']) == 124,
            'Same original compilation/support/tail closure')
    require(prior['executables']['current']['sha256'] ==
            audit['products_sha256']['metadata-frames/baseline/client_frames.exe'] ==
            '2e5a4bced314349c4399c578bf4e9544b9d58ccc1fec293001fa0b74c0a8c00f',
            'Baseline is exact earlier build5 current product')
    require(audit['baseline_PE'] == plan['baseline_pe'] and
            audit['baseline_PE']['executable_sections_sha256']['.text'] ==
            audit['baseline_retained_audit']['code_sections']['.text']['sha256'],
            'Retained baseline PE/code continuity')
    projection = build['source_object_projection']
    require(len(projection['runtime_delta']) == 3 and len(projection['new_objects_reused_compiles']) == 2,
            'Only two implementation objects plus private declaration')
    new_objects = projection['new_objects_reused_compiles']
    require([row['sha256'] for row in new_objects] == [
        '4a05f62ab79e5c610dd9e0f61acd02625db40bfd1602b36af537f9f91aeaa63f',
        '26b7bce4ab7106261ef98d22c94f76daa0780a0a146c609f265d2b1fd4820ce2'],
        'Exact qualified metadata/cache and CV objects')
    for row in new_objects:
        require(all(flag in row['command']['argv'] for flag in ('/O2', '/fp:strict', '-std:c17', '/WX')),
                'Actual strict optimized C17 options')
    comparisons = projection['actual_dependency_comparison']
    require(len(comparisons) == 245, 'Complete actual project-dependency projection')
    changed = [row for row in comparisons if row['historical_sha256'] != row['reused_sha256']]
    require(len(changed) == 2 and all(
        row['declared_prototype_delta'] is True and
        row['relative'] == 'src/world/runtime/chunk_cache_internal.h' for row in changed),
        'Only harmless private declaration differs; no ABI reconstruction')
    require(len(plan['objects']) == 8 and len(audit['selected_application_owners']) == 8,
            'All eight production replacement ownership groups')
    excluded = {(row['archive'], row['member']) for row in plan['objects']}
    require(len(excluded) == 8, 'Eight distinct original archive members')
    loaded = audit['actual_loaded_full_members']
    require({name: len(values) for name, values in loaded.items()} == {
        'mc_client_render_global.lib': 36, 'mc_client_world_load.lib': 1,
        'mc_registry.lib': 40, 'mc_simulation.lib': 237, 'mc_world.lib': 20},
        'Complete affected-archive loaded coverage')
    for archive, values in loaded.items():
        require(len({row['member'] for row in values}) == len(values), 'Unique resolved full members')
        for row in values:
            require((archive, row['member']) not in excluded and row['found'] and
                    row['member'].endswith('/' + row['basename']) and
                    re.fullmatch('[0-9a-f]{64}', row['member_sha256']), 'Actual full-member exclusion proof')
    for obj in plan['objects']:
        owners = audit['selected_application_owners'][obj['source']]
        require(owners and all(row['owner'] == obj['object'].split('/')[-1] for row in owners),
                'Each selected external/data definition binds its explicit object')
    require([row['stage'] for row in build['prior_diagnostic_history']] == [2, 3, 4, 5] and
            all(row['ui_runs'] == 0 and row['preserved'] is True
                for row in build['prior_diagnostic_history']), 'Prior failed strict audits stay separate')


def audit_receipt(document):
    require(type(document['schema']) is int and document['schema'] == 1 and
            document['kind'] == 'chunk_cache_metadata_frames_windows', 'Receipt identity')
    require(document['status'] == document['cohort']['status'] == 'FAIL', 'Failed whole cohort stays failed')
    require(not re.search(r'[A-Za-z]:[\\/]', json.dumps(document)), 'No private absolute paths')
    cohort = document['cohort']
    require(cohort['rounds'] == 4 and cohort['processes'] == len(document['attempts']) == 8 and
            cohort['rows'] == 2707, 'All actual eight profiles/2707rows retained')
    require([(row['round'], row['label']) for row in document['attempts']] == ORDER, 'Actual balanced order')
    require(cohort['aggregate_performance'] is None and cohort['automatic_reruns'] == 0 and
            cohort['all_attempts_retained'] is True and
            cohort['all_original_frozen_inputs_unchanged'] is True, 'No subset gain or sample repair')
    replayed = replay_profiles(document)
    for index, (attempt, replay, expected) in enumerate(zip(document['attempts'], replayed, ACTUAL)):
        text, error = attempt['stdout_text'], attempt['stderr_text']
        raw = text.encode('utf-8')
        require(attempt['ordinal'] == index and attempt['row_columns'] == 19, 'Ordinal/row version')
        require(digest(raw) == attempt['artifacts']['stdout']['sha256'] == STDOUT_IDENTITIES[index] and
                len(raw) == attempt['artifacts']['stdout']['bytes'], 'Exact captured native stdout')
        require(error == '' and attempt['artifacts']['stderr']['bytes'] == 0 and
                digest(error.encode()) == attempt['artifacts']['stderr']['sha256'],
                'Exact empty native stderr')
        actual_rows = rows(text)
        require(actual_rows == attempt['rows'] and all(len(row) == 19 for row in actual_rows),
                'Every actual19-column row and its order')
        require(receipts(text) == attempt['receipts'], 'All scalar work/cache/pose/context/foreground receipts')
        summary = replay['observation']['summary']
        require(summary == attempt['observation']['summary'], 'Full native semantic summary replays')
        measurement, work = summary['measurement'], summary['work_ledger']
        require((len(actual_rows), work['world_coordinator_returns'], work['world_time_delta'],
                 int(measurement['focus-invalid'])) == expected, 'Actual work and focus prefixes')
        qualified = summary['comparison_qualified']
        require(qualified is FLAGS[index] and measurement['healthy'] == '1' and
                summary['full_endpoint_pose_retained'] is True, 'Healthy fixed endpoint is not admission')
        require(attempt['native_exit'] == attempt['observation']['returncode'] ==
                attempt['driver_exit'] == (0 if qualified else 1) and
                attempt['observation']['status'] == ('PASS' if qualified else 'FAIL'),
                'Distinct actual native/driver admission result')
        require(FRAMES.fields('PERF_FINISH ' + attempt['receipts']['FINISH'][0])['cleanup'] == '1',
                'Cleanup completed even after focus refusal')
        continuity = attempt['input_continuity']
        require(continuity['before_equals_after'] is True and
                continuity['assets_before_equals_after'] is True and continuity['assets_count'] == 413 and
                attempt['observation']['inputs_unchanged'] is True and
                attempt['observation']['assets_unchanged'] is True, 'Actual per-process input continuity')
        for artifact in attempt['artifacts'].values():
            require(type(artifact['bytes']) is int and artifact['bytes'] >= 0 and
                    re.fullmatch('[0-9a-f]{64}', artifact['sha256']), 'Complete native/driver/report artifact identities')
    qualification = FRAMES.qualify_cohort(replayed)
    for name in ('status', 'all_intervals_qualified', 'authored_scene_and_camera_matched'):
        require(qualification[name] == cohort[name], 'Whole-cohort admission replays')
    require(cohort['qualified_intervals'] == 5 and cohort['unqualified_intervals'] == 3,
            'Five qualified and three refused remain distinct')
    first = replayed[0]['observation']['summary']
    require(first['measured_fps'] == 39.18046501194969 and
            first['work_ledger']['checkpoint_deltas']['random-samples'] == 0 and
            first['work_ledger']['checkpoint_deltas']['display-random'] == 0 and
            first['work_ledger']['simulation_advanced'] is False and
            first['native_foreground_valid'] is False, 'Raw39FPS paused zero-World work stays unqualified')
    for index in (0, 3, 6):
        summary = replayed[index]['observation']['summary']
        require(summary['work_ledger']['checkpoints'][-1]['paused'] == '1' and
                summary['work_ledger']['simulation_advanced'] is False and
                summary['native_foreground_valid'] is False, 'Actual final-pause/focus failure retained')
    require(document['execution']['status'] == 'FAIL' and document['execution']['exit'] == 1 and
            document['execution']['all_attempts_retained'] is True and
            document['execution']['all_scoped_inputs_and_products_unchanged'] is True,
            'Actual wrapper failure with frozen complete capture')
    audit_build(document['build'])
    return {'qualification': qualification, 'rows': sum(len(row['rows']) for row in document['attempts']),
            'qualified_intervals': 5, 'unqualified_intervals': 3}


def change_work(text, key, value, stage='timed-returned'):
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.startswith('PERF_WORK ') and FRAMES.fields(line)['stage'] == stage:
            lines[index] = re.sub(r'(?<!\S)' + re.escape(key) + r'=\S+', key + '=' + str(value), line)
            return ''.join(lines)
    raise AssertionError('Actual work checkpoint missing')


class ChunkCacheMetadataFramesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(RECEIPT.read_text(encoding='utf-8'), object_pairs_hook=unique)

    def test_all_eight_retained_attempts_replay(self):
        result = audit_receipt(self.document)
        self.assertEqual(result['rows'], 2707)
        self.assertEqual(result['qualification']['status'], 'FAIL')

    def test_no_whole_or_passing_subset_credit(self):
        for change in ('status', 'aggregate', 'subset', 'rerun', 'summary'):
            doc = copy.deepcopy(self.document)
            if change == 'status':
                doc['status'] = doc['cohort']['status'] = 'PASS'
            elif change == 'aggregate':
                doc['cohort']['aggregate_performance'] = {'gain_percent': 1}
            elif change == 'subset':
                doc['attempts'] = [row for row in doc['attempts'] if row['native_exit'] == 0]
            elif change == 'rerun':
                doc['cohort']['automatic_reruns'] = 1
            else:
                doc['attempts'][0]['observation']['summary']['comparison_qualified'] = True
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_receipt(doc)

    def test_forged_statuses_cannot_override_admission(self):
        profiles = replay_profiles(self.document)
        for row in profiles:
            row['driver_exit'] = 0
            row['observation']['status'] = 'PASS'
        self.assertEqual(FRAMES.qualify_cohort(profiles)['status'], 'FAIL')

    def test_zero_world_and_partial_prefixes_preserved(self):
        profiles = replay_profiles(self.document)
        for index, coordinator, world in ((0, 195, 0), (3, 192, 60), (6, 194, 129)):
            with self.subTest(index=index):
                summary = profiles[index]['observation']['summary']
                self.assertEqual(summary['work_ledger']['world_coordinator_returns'], coordinator)
                self.assertEqual(summary['work_ledger']['world_time_delta'], world)
                self.assertFalse(summary['comparison_qualified'])
                self.assertEqual(summary['work_ledger']['checkpoints'][-1]['paused'], '1')

    def test_semantic_work_forgeries_without_hash_gate(self):
        text = self.document['attempts'][1]['stdout_text']
        entry = FRAMES.summarize(text)['work_ledger']['checkpoints'][0]
        for field in ('world-time', 'random-samples', 'display-random'):
            with self.subTest(field=field):
                summary = FRAMES.summarize(change_work(text, field, entry[field]))
                self.assertTrue(summary['valid_focused_Tiny_interval'])
                self.assertFalse(summary['comparison_qualified'])
        with self.assertRaises(ValueError):
            FRAMES.summarize(change_work(text, 'main-ticks', 0, 'timed-entry'))

    def test_paused_zero_world_cannot_gain_credit_from_positive_fps(self):
        profiles = replay_profiles(self.document)
        summary = profiles[0]['observation']['summary']
        self.assertGreater(summary['measured_fps'], 39)
        summary['native_foreground_valid'] = True
        summary['valid_focused_Tiny_interval'] = True
        self.assertEqual(FRAMES.qualify_cohort(profiles)['status'], 'FAIL')

    def test_focus_and_final_pause_are_independent_semantic_gates(self):
        text = self.document['attempts'][1]['stdout_text']
        lines = text.splitlines(keepends=True)
        for index, line in enumerate(lines):
            if line.startswith('PERF_FOREGROUND '):
                lines[index] = re.sub(r'(?<!\S)own-pid=\S+', 'own-pid=0', line)
        summary = FRAMES.summarize(''.join(lines))
        self.assertTrue(summary['work_ledger']['simulation_advanced'])
        self.assertFalse(summary['native_foreground_valid'])
        self.assertFalse(summary['comparison_qualified'])
        paused = FRAMES.summarize(change_work(text, 'paused', 1))
        self.assertTrue(paused['valid_focused_Tiny_interval'])
        self.assertFalse(paused['work_ledger']['simulation_advanced'])
        self.assertFalse(paused['comparison_qualified'])

    def test_complete_raw_rows_and_receipts_required(self):
        for change in ('row', 'order', 'receipt', 'stream'):
            doc = copy.deepcopy(self.document)
            attempt = doc['attempts'][1]
            if change == 'row':
                attempt['rows'].pop()
            elif change == 'order':
                attempt['rows'][0], attempt['rows'][1] = attempt['rows'][1], attempt['rows'][0]
            elif change == 'receipt':
                attempt['receipts']['WORK'].pop()
            else:
                attempt['stdout_text'] = attempt['stdout_text'].replace('healthy=1', 'healthy=0')
                attempt['artifacts']['stdout']['sha256'] = digest(attempt['stdout_text'].encode())
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_receipt(doc)

    def test_provenance_and_exclusion_corruptions(self):
        for change in ('baseline', 'member', 'owner', 'header', 'observer'):
            build = copy.deepcopy(self.document['build'])
            if change == 'baseline':
                build['audit']['products_sha256']['metadata-frames/baseline/client_frames.exe'] = '0' * 64
            elif change == 'member':
                build['audit']['actual_loaded_full_members']['mc_simulation.lib'][0]['member'] = (
                    'CMakeFiles/mc_simulation.dir/src/world/runtime/chunk_cache.c.obj')
            elif change == 'owner':
                build['audit']['selected_application_owners']['src/world/runtime/chunk_cache.c'][0]['owner'] = (
                    'mc_simulation:chunk_cache.c.obj')
            elif change == 'header':
                build['source_object_projection']['actual_dependency_comparison'].pop()
            else:
                build['prior_frame_build']['observer']['object_sha256'] = '0' * 64
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_build(build)


if __name__ == '__main__':
    unittest.main()
