"""Replay the complete retained Windows cohort without native code or assets."""
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
RECEIPT = ROOT / 'tests/benchmarks/client_frame_work_qualification_windows.json'
SPEC = importlib.util.spec_from_file_location(
    'retained_frame_work_parser', ROOT / 'scripts/benchmarks/client_frames_run.py')
FRAMES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FRAMES)
ORDER = [(0, 'parent'), (0, 'current'), (1, 'current'), (1, 'parent'),
         (2, 'parent'), (2, 'current'), (3, 'current'), (3, 'parent')]
STDOUT_IDENTITIES = (
    '11667b60e707ec2c9166bd67c99b20a11a6b878c245afae213d0748c658a43b1',
    '88f380a35cadc3d40c25eb08d5d3d0cdaba147db25a24ffe8f9d222580ab6f08',
    'd3c094424e29b607d6964ec3577790d1cb7ed621fc7778166278fc9f25ecf5c1',
    '62aee294d2a3bfc36bc3d1fec5d3a6ac20f5a5ef94e66de51b93f9fead621a15',
    'cfe94205282b33735afa7cd0aa326549b60c58b5429d68bf4f0c70d33cd09079',
    '2494f0b135594dd03f3586bacd91e040e6dfbaae68d1876e2524ed8902f4598b',
    '39a8efcbb27bc1aa6c670135f63cc03a683eb6809b36120bda9848d907a85309',
    '671ff5e17aee6d748438bbeed203fc07a370dbfeb5c7c527d17e10f9491b7e7a',
)


def require(value, message):
    if not value:
        raise ValueError(message)


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key')
        result[key] = value
    return result


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


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


def audit_receipt(document):
    """Validate capture identity and replay work/admission, never grant subset credit."""
    require(type(document['schema']) is int and document['schema'] == 1, 'Schema')
    require(document['kind'] == 'client_frame_work_qualification_windows', 'Receipt kind')
    require(document['status'] == document['cohort']['status'] == 'FAIL', 'Failed cohort stays failed')
    require(not re.search(r'[A-Za-z]:[\\/]', json.dumps(document)), 'Private absolute path')
    cohort = document['cohort']
    require(cohort['rounds'] == 4 and cohort['processes'] == 8 and len(document['attempts']) == 8,
            'All four rounds/eight processes must remain')
    require([(a['round'], a['label']) for a in document['attempts']] == ORDER, 'Exact capture order')
    require(cohort['aggregate_performance'] is None and cohort['automatic_reruns'] == 0,
            'No aggregate comparison credit or rerun')
    require(cohort['all_attempts_retained'] is True and
            cohort['all_original_frozen_inputs_unchanged'] is True, 'Captured continuity')
    replayed = replay_profiles(document)
    for index, (attempt, replay) in enumerate(zip(document['attempts'], replayed)):
        text = attempt['stdout_text']
        raw = text.encode('utf8')
        require(attempt['ordinal'] == index and attempt['row_columns'] == 19, 'Ordinal/columns')
        require(digest(raw) == attempt['artifacts']['stdout']['sha256'] == STDOUT_IDENTITIES[index],
                'Exact original native stdout bytes')
        require(len(raw) == attempt['artifacts']['stdout']['bytes'], 'Stdout byte count')
        error = attempt['stderr_text'].encode('utf8')
        require(digest(error) == attempt['artifacts']['stderr']['sha256'] and
                len(error) == attempt['artifacts']['stderr']['bytes'], 'Original native stderr')
        actual_rows = rows(text)
        require(actual_rows == attempt['rows'] and all(len(row) == 19 for row in actual_rows),
                'All actual nineteen-column rows/order must remain')
        require(receipts(text) == attempt['receipts'], 'All actual scalar receipts must remain')
        require(replay['observation']['summary'] == attempt['observation']['summary'],
                'Complete captured summary must replay from raw stdout')
        qualified = replay['observation']['summary']['comparison_qualified']
        require(attempt['native_exit'] == attempt['observation']['returncode'] == (0 if qualified else 1),
                'Actual native focus refusal exit retained separately from cleanup')
        require(attempt['driver_exit'] == (0 if qualified else 1) and
                attempt['observation']['status'] == ('PASS' if qualified else 'FAIL'),
                'Actual native and driver admission exits retained')
        continuity = attempt['input_continuity']
        require(continuity['before_equals_after'] is True and
                continuity['assets_before_equals_after'] is True and continuity['assets_count'] == 413,
                'Original per-process continuity')
        require(attempt['observation']['inputs_unchanged'] is True and
                attempt['observation']['assets_unchanged'] is True, 'Original report continuity')
        for artifact in attempt['artifacts'].values():
            require(type(artifact['bytes']) is int and artifact['bytes'] >= 0 and
                    re.fullmatch('[0-9a-f]{64}', artifact['sha256']), 'Complete artifact identity')
    qualification = FRAMES.qualify_cohort(replayed)
    for name in ('status', 'all_intervals_qualified', 'authored_scene_and_camera_matched'):
        require(qualification[name] == cohort[name], 'Whole-cohort qualification must replay')
    flags = [p['observation']['summary']['comparison_qualified'] for p in replayed]
    require(flags == [True, True, False, True, True, True, True, True], 'Seven admitted; one refused')
    require(cohort['qualified_intervals'] == 7 and cohort['unqualified_intervals'] == 1,
            'Retain admission distinction')
    failed = replayed[2]['observation']['summary']
    work = failed['work_ledger']
    require(work['world_coordinator_returns'] == 191 and work['world_time_delta'] == 189 and
            work['checkpoint_deltas']['random-calls'] == 189 and
            work['checkpoints'][-1]['paused'] == '1', 'Do not repair the paused actual work prefix')
    require(failed['native_foreground_valid'] is False and
            failed['valid_focused_Tiny_interval'] is False and work['simulation_advanced'] is False,
            'Foreground/final-pause refusal retained despite positive actual World work')
    build = document['build']
    require(build['production_revisions'] == {
        'parent': '44483ff9ed8f63e250157bd9e0ba41fea48b002c',
        'current': '719089051182e835e7bc16bdbe0fd98bfe57e5b7'}, 'Frozen production identities')
    require(build['compile_count'] == len(build['compiles']) == 13 and
            build['actual_header_count'] == 1104 and len(build['sdk_support_objects']) == 4 and
            build['ordered_library_count'] == 124, 'Shared finite build closure')
    require(build['observer']['compiled_once'] is True and build['observer']['shared_by_variants'] is True,
            'One identical observer')
    require(build['code_sections_identical_to_builds'] == [2, 4, 5], 'Recorded code continuity')
    expected_counts = {'mc_client_world_load.lib': 1, 'mc_registry.lib': 40,
                       'mc_simulation.lib': 238, 'mc_world.lib': 20}
    require(build['catalog_member_counts'] == {'mc_client_world_load.lib': 2, 'mc_registry.lib': 43,
                                              'mc_simulation.lib': 239, 'mc_world.lib': 21},
            'Actual frozen archive catalog counts')
    excluded = {(row['archive'], row['member']) for row in build['excluded_original_members']}
    require(len(excluded) == 6, 'All six replaced member identities')
    for variant in ('parent', 'current'):
        loaded = build['loaded_full_members'][variant]
        require({name: len(values) for name, values in loaded.items()} == expected_counts,
                'Complete captured full-member coverage')
        for archive, values in loaded.items():
            require(len({row['member'] for row in values}) == len(values), 'Unique full loaded members')
            for row in values:
                require((archive, row['member']) not in excluded and row['found'] and
                        row['member'].endswith('/' + row['basename']) and
                        re.fullmatch('[0-9a-f]{64}', row['member_sha256']), 'Actual bound member exclusion')
        require(len(build['selected_definition_proof'][variant]) == 6 and all(
            row['selected_count'] > 0 for row in build['selected_definition_proof'][variant].values()),
            'Six selected-definition ownership groups')
    history = document['diagnostic_history']
    require([row['stage'] for row in history] == [2, 3, 4, 5] and all(
        row['ui_runs'] == 0 and row['preserved'] is True for row in history), 'Separate diagnostic history')
    require(history[1]['status'] == 'PREPARATION_NOT_EXECUTED' and
            not history[1]['executed_commands'], 'Do not invent stage3execution')
    return {'cohort': qualification, 'qualified_intervals': 7, 'unqualified_intervals': 1,
            'rows': sum(len(a['rows']) for a in document['attempts'])}


def change_work(text, key, value, stage='timed-returned'):
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.startswith('PERF_WORK ') and FRAMES.fields(line)['stage'] == stage:
            lines[index] = re.sub(r'(?<!\S)' + re.escape(key) + r'=\S+', key + '=' + str(value), line)
            return ''.join(lines)
    raise AssertionError('Actual checkpoint missing')


class ClientFrameWorkQualificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(RECEIPT.read_text(encoding='utf8'), object_pairs_hook=unique)

    def test_complete_retained_cohort_replays(self):
        result = audit_receipt(self.document)
        self.assertEqual(result['rows'], 2217)
        self.assertEqual(result['cohort']['status'], 'FAIL')

    def test_derived_labels_cannot_grant_cohort_credit(self):
        for path in ('status', 'aggregate_performance', 'failed_summary'):
            doc = copy.deepcopy(self.document)
            if path == 'status':
                doc['status'] = doc['cohort']['status'] = 'PASS'
            elif path == 'aggregate_performance':
                doc['cohort']['aggregate_performance'] = {'gain_percent': 1}
            else:
                doc['attempts'][2]['observation']['summary']['comparison_qualified'] = True
            with self.subTest(path=path), self.assertRaises(ValueError):
                audit_receipt(doc)

    def test_passing_subset_cannot_replace_eight_attempts(self):
        doc = copy.deepcopy(self.document)
        del doc['attempts'][2]
        doc['cohort'].update(processes=7, qualified_intervals=7, unqualified_intervals=0, status='PASS')
        with self.assertRaises(ValueError):
            audit_receipt(doc)

    def test_forged_statuses_do_not_override_raw_admission(self):
        profiles = replay_profiles(self.document)
        for row in profiles:
            row['driver_exit'] = 0
            row['observation']['status'] = 'PASS'
        self.assertEqual(FRAMES.qualify_cohort(profiles)['status'], 'FAIL')

    def test_actual_paused_world_delta_stays_distinct(self):
        summary = FRAMES.summarize(self.document['attempts'][2]['stdout_text'])
        self.assertEqual(summary['work_ledger']['world_time_delta'], 189)
        self.assertEqual(summary['work_ledger']['world_coordinator_returns'], 191)
        doc = copy.deepcopy(self.document)
        doc['attempts'][2]['observation']['summary']['work_ledger']['world_time_delta'] = 191
        with self.assertRaises(ValueError):
            audit_receipt(doc)

    def test_work_forgery_is_rejected_semantically_without_hash_checks(self):
        text = self.document['attempts'][0]['stdout_text']
        entry = FRAMES.summarize(text)['work_ledger']['checkpoints'][0]
        for field in ('world-time', 'random-samples', 'display-random'):
            with self.subTest(field=field):
                summary = FRAMES.summarize(change_work(text, field, entry[field]))
                self.assertTrue(summary['valid_focused_Tiny_interval'])
                self.assertFalse(summary['comparison_qualified'])
                self.assertFalse(summary['work_ledger']['simulation_advanced'])
        with self.assertRaises(ValueError):
            FRAMES.summarize(change_work(text, 'main-ticks', 0, 'timed-entry'))

    def test_clearing_pause_does_not_erase_actual_foreground_failure(self):
        text = change_work(self.document['attempts'][2]['stdout_text'], 'paused', 0)
        summary = FRAMES.summarize(text)
        self.assertFalse(summary['native_foreground_valid'])
        self.assertFalse(summary['comparison_qualified'])

    def test_hash_repair_does_not_rebind_a_capture(self):
        doc = copy.deepcopy(self.document)
        attempt = doc['attempts'][2]
        attempt['stdout_text'] = change_work(attempt['stdout_text'], 'paused', 0)
        attempt['artifacts']['stdout']['sha256'] = digest(attempt['stdout_text'].encode())
        with self.assertRaises(ValueError):
            audit_receipt(doc)

    def test_rows_and_member_proofs_cannot_be_dropped_or_relabelled(self):
        for change in ('row', 'member'):
            doc = copy.deepcopy(self.document)
            if change == 'row':
                doc['attempts'][0]['rows'].pop()
            else:
                target = doc['build']['excluded_original_members'][0]
                entry = doc['build']['loaded_full_members']['parent'][target['archive']][0]
                entry['member'] = target['member']
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_receipt(doc)


if __name__ == '__main__':
    unittest.main()
