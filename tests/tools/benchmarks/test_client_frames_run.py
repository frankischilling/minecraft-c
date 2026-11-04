"""Check frame-observation parsing without a display or imported resources."""
import importlib.util
import copy
import hashlib
import json
from pathlib import Path
import statistics
import unittest


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    'client_frames_run', ROOT / 'scripts/benchmarks/client_frames_run.py')
FRAMES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FRAMES)

# Include the retained native cohort in the configured frame-observation suite.
WORK_SPEC = importlib.util.spec_from_file_location(
    'client_frame_work_qualification', Path(__file__).with_name('test_client_frame_work_qualification.py'))
WORK = importlib.util.module_from_spec(WORK_SPEC)
WORK_SPEC.loader.exec_module(WORK)
ClientFrameWorkQualificationTests = WORK.ClientFrameWorkQualificationTests

METADATA_FRAME_SPEC = importlib.util.spec_from_file_location(
    'chunk_cache_metadata_frames', Path(__file__).with_name('test_chunk_cache_metadata_frames.py'))
METADATA_FRAMES = importlib.util.module_from_spec(METADATA_FRAME_SPEC)
METADATA_FRAME_SPEC.loader.exec_module(METADATA_FRAMES)
ChunkCacheMetadataFramesTests = METADATA_FRAMES.ChunkCacheMetadataFramesTests


def receipt(version=2):
    rows = [
        [7, 1000, 1100, 10, 1000010, 4000010, 1, 1, 7, 0, 1, -1, 1, 2, 2, 2,
         3000010, 1, 1],
        [8, 1101, 1200, 4000010, 5000010, 8000010, 0, 0, 7, 0, 1, -1, 1, 2, 2, 3,
         7000010, 1, 1],
    ]
    wire = [
        f'PERF_RESULT row-version={version} qpc-hz=1000 start=999 end=1200 frames=2 '
        'world-ticks=1 healthy=1 capacity=0 focus-invalid=0',
        'PERF_CONTEXT major=3 minor=3 profile=1 swap-zero=1 width=854 height=480 '
        'view-distance=3 performance-mode=1',
        'PERF_GPU_VENDOR test vendor',
        'PERF_GPU_RENDERER test renderer',
        'PERF_GPU_VERSION test version',
        'PERF_FINISH cleanup=1',
        'PERF_CACHE stage=before-selection present=0',
    ]
    for stage in ('first-loaded', 'setup-returned', 'warmup-returned', 'timed-returned'):
        wire.append(f'PERF_CACHE stage={stage} present=1 owned=1 raw-exports=0 mutation-depth=0')
    for stage in ('pre-warmup', 'timed-entry', 'timed-returned'):
        wire.append(f'PERF_POSE stage={stage} x=3fe0000000000000 y=405067ae147ae148 '
                    'z=3fe0000000000000 yaw=43340000 pitch=c1f00000')
    wire.append('PERF_FOREGROUND frame=7 hwnd=42 pid=100 own-pid=100')
    for row in rows:
        wire.append('PERF_ROW ' + ','.join(map(str, row[:16] if version == 1 else row)))
    return '\n'.join(wire)


def edit_rows(wire, change):
    lines = wire.splitlines()
    indices = [i for i, line in enumerate(lines) if line.startswith('PERF_ROW ')]
    rows = [[int(v) for v in lines[i].split(' ', 1)[1].split(',')] for i in indices]
    change(rows)
    for index, row in zip(indices, rows):
        lines[index] = 'PERF_ROW ' + ','.join(map(str, row))
    return '\n'.join(lines)


def work_receipt(wire=None):
    wire = receipt() if wire is None else wire
    wire = wire.replace('end=1200', 'end=11000')
    def longer(rows):
        rows[0][2] = 6000
        rows[1][1:3] = [6001, 11000]
    wire = edit_rows(wire, longer)
    rows = [[int(v) for v in line.split(' ', 1)[1].split(',')]
            for line in wire.splitlines() if line.startswith('PERF_ROW ')]
    result = FRAMES.fields(wire.splitlines()[0])
    world = int(result['world-ticks'])
    for stage, main, returned, draws, time, calls in (
            ('timed-entry', rows[0][13] - rows[0][7], rows[-1][14] - world,
             rows[0][15] - 1, 6000, 1),
            ('timed-returned', rows[-1][13], rows[-1][14], rows[-1][15], 6000 + world, 1 + world)):
        wire += (f'\nPERF_WORK stage={stage} present=1 main-ticks={main} world-ticks={returned} '
                 f'world-draws={draws} world-time={time} paused=0 random-calls={calls} '
                 f'random-samples={calls * 28880} display-calls={calls} '
                 f'display-samples={calls * 1000} display-random={calls * 6000}')
    return wire


class FrameWorkObservations(unittest.TestCase):
    def test_independent_work_checkpoints_qualify(self):
        summary = FRAMES.summarize(work_receipt())
        self.assertTrue(summary['comparison_qualified'])
        self.assertTrue(summary['work_ledger']['simulation_advanced'])
        self.assertEqual(summary['work_ledger']['world_time_delta'], 1)
        self.assertEqual(summary['work_ledger']['world_draws_proved'], 2)
        self.assertEqual(summary['work_ledger']['main_ticks'], 1)

    def test_main_and_world_counts_need_not_be_equal(self):
        def unequal(rows):
            rows[0][6:8], rows[1][6:8] = [2, 2], [3, 3]
            rows[0][13:16], rows[1][13:16] = [12, 4, 6], [15, 5, 7]
        wire = edit_rows(receipt().replace('world-ticks=1', 'world-ticks=2'), unequal)
        summary = FRAMES.summarize(work_receipt(wire))
        self.assertTrue(summary['comparison_qualified'])
        self.assertEqual(summary['work_ledger']['main_ticks'], 5)
        self.assertEqual(summary['work_ledger']['world_coordinator_returns'], 2)

    def test_zero_tick_data_retained_without_comparison_credit(self):
        def zero(rows):
            for row in rows:
                row[6:8], row[13:15] = [0, 0], [25, 20]
        wire = edit_rows(receipt().replace('world-ticks=1', 'world-ticks=0'), zero)
        result = FRAMES.summarize(work_receipt(wire))
        self.assertTrue(result['valid_focused_Tiny_interval'])
        self.assertFalse(result['comparison_qualified'])
        self.assertEqual(result['work_ledger']['main_ticks'], 0)
        self.assertEqual(result['work_ledger']['world_entry'], 20)

    def test_world_returns_are_not_world_time_or_random_work(self):
        wire = work_receipt()
        for before, after in (('world-time=6001', 'world-time=6000'),
                              ('paused=0', 'paused=1'),
                              ('random-samples=57760', 'random-samples=28880'),
                              ('display-samples=2000', 'display-samples=1000'),
                              ('PERF_WORK stage=timed-returned present=1',
                               'PERF_WORK stage=timed-returned present=0')):
            with self.subTest(after=after):
                result = FRAMES.summarize(wire.replace(before, after))
                self.assertTrue(result['valid_focused_Tiny_interval'])
                self.assertFalse(result['comparison_qualified'])

    def test_positive_main_ticks_without_world_return(self):
        wire = edit_rows(receipt().replace('world-ticks=1', 'world-ticks=0'),
                         lambda rows: [r.__setitem__(14, 1) for r in rows])
        result = FRAMES.summarize(work_receipt(wire))
        self.assertEqual(result['work_ledger']['main_ticks'], 1)
        self.assertFalse(result['comparison_qualified'])

    def test_completed_counter_corruptions_are_rejected(self):
        wire = work_receipt()
        mutations = [(0, 6, 11), (0, 7, 0), (0, 13, -1), (1, 13, 3),
                     (1, 14, 1), (1, 14, 4), (1, 15, 2), (1, 15, 4),
                     (0, 13, 1 << 64)]
        for row, column, value in mutations:
            with self.subTest(row=row, column=column, value=value), self.assertRaises(ValueError):
                FRAMES.summarize(edit_rows(wire, lambda rows: rows[row].__setitem__(column, value)))
        for value in (0, 4, 5, 1 << 64):
            with self.subTest(world_delta=value), self.assertRaises(ValueError):
                FRAMES.summarize(wire.replace('world-ticks=1 healthy=', f'world-ticks={value} healthy='))

    def test_checkpoint_baseline_and_schema_corruptions(self):
        wire = work_receipt()
        for before, after in (('main-ticks=1', 'main-ticks=0'),
                              ('world-draws=1', 'world-draws=0'),
                              ('world-time=6001', 'world-time=9223372036854775808'),
                              ('display-random=12000', 'display-random=1'),
                              ('PERF_WORK stage=timed-entry present=1',
                               'PERF_WORK stage=timed-entry present=2'),
                              ('stage=timed-entry present=', 'stage=other present=')):
            with self.subTest(after=after), self.assertRaises(ValueError):
                FRAMES.summarize(wire.replace(before, after))
        lines = [line for line in wire.splitlines() if not line.startswith('PERF_WORK stage=timed-entry')]
        with self.assertRaises(ValueError):
            FRAMES.summarize('\n'.join(lines))

    def test_negative_timer_value_with_zero_returns_is_source_valid(self):
        wire = edit_rows(receipt(), lambda rows: rows[1].__setitem__(6, -1))
        result = FRAMES.summarize(work_receipt(wire))
        self.assertTrue(result['comparison_qualified'])

    def test_failed_partial_prefix_keeps_its_distinct_scope(self):
        wire = edit_rows(work_receipt().replace('healthy=1', 'healthy=0'),
                         lambda rows: rows[1].__setitem__(14, 4))
        result = FRAMES.summarize(wire)
        self.assertFalse(result['comparison_qualified'])
        self.assertFalse(result['work_ledger']['completed'])

    def test_legacy_frames_lack_independent_simulation_evidence(self):
        result = FRAMES.summarize(receipt(1))
        self.assertTrue(result['valid_focused_Tiny_interval'])
        self.assertFalse(result['comparison_qualified'])

    def test_duration_and_full_endpoint_pose_are_required(self):
        result = FRAMES.summarize(receipt())
        self.assertFalse(result['measurement_at_least_ten_seconds'])
        wire = work_receipt()
        def shorter(rows):
            rows[0][2], rows[1][1:3] = 1100, [1101, 1200]
        short = edit_rows(wire.replace('end=11000', 'end=1200'), shorter)
        result = FRAMES.summarize(short)
        self.assertTrue(result['work_ledger']['simulation_advanced'])
        self.assertTrue(result['full_endpoint_pose_retained'])
        self.assertFalse(result['comparison_qualified'])
        for axis in ('x', 'y', 'z'):
            lines = wire.splitlines()
            index = next(i for i, line in enumerate(lines) if line.startswith('PERF_POSE stage=timed-returned'))
            values = FRAMES.fields(lines[index])
            lines[index] = lines[index].replace(f"{axis}={values[axis]}", f'{axis}=0000000000000000')
            with self.subTest(axis=axis):
                result = FRAMES.summarize('\n'.join(lines))
                self.assertTrue(result['valid_focused_Tiny_interval'])
                self.assertFalse(result['comparison_qualified'])

    def test_either_unqualified_member_rejects_whole_cohort(self):
        observation = {'status': 'PASS', 'summary': FRAMES.summarize(work_receipt()),
                       'save_before': {'world': 'same'}, 'assets_before': {'assets': 'same'}}
        profiles = [{'driver_exit': 0, 'observation': copy.deepcopy(observation)} for _ in range(2)]
        self.assertEqual(FRAMES.qualify_cohort(profiles)['status'], 'PASS')
        for index in range(2):
            changed = copy.deepcopy(profiles)
            changed[index]['observation']['summary']['comparison_qualified'] = False
            self.assertEqual(FRAMES.qualify_cohort(changed)['status'], 'FAIL')
            self.assertEqual(len(changed), 2)
        for key in ('save_before', 'assets_before'):
            changed = copy.deepcopy(profiles)
            changed[1]['observation'][key]['extra'] = 'changed'
            self.assertEqual(FRAMES.qualify_cohort(changed)['status'], 'FAIL')
        for key in ('context', 'gpu'):
            changed = copy.deepcopy(profiles)
            changed[1]['observation']['summary'][key]['changed'] = True
            self.assertEqual(FRAMES.qualify_cohort(changed)['status'], 'FAIL')

    def test_world_time_uses_java_long_wrapping(self):
        wire = work_receipt().replace('world-time=6000', f'world-time={(1 << 63) - 1}')
        wire = wire.replace('world-time=6001', f'world-time={-(1 << 63)}')
        result = FRAMES.summarize(wire)
        self.assertEqual(result['work_ledger']['world_time_delta'], 1)
        self.assertTrue(result['comparison_qualified'])

    def test_missing_or_duplicate_checkpoint_keeps_no_comparison_credit(self):
        wire = work_receipt()
        legacy = '\n'.join(line for line in wire.splitlines() if not line.startswith('PERF_WORK '))
        result = FRAMES.summarize(legacy)
        self.assertTrue(result['valid_focused_Tiny_interval'])
        self.assertFalse(result['comparison_qualified'])
        duplicate = wire + '\n' + next(line for line in wire.splitlines() if line.startswith('PERF_WORK '))
        with self.assertRaisesRegex(ValueError, 'schema/order'):
            FRAMES.summarize(duplicate)

    def test_fixed_pose_requires_finite_complete_positions(self):
        wire = work_receipt()
        for value in ('7ff8000000000000', '7ff0000000000000', '00000000', 'invalid'):
            with self.subTest(value=value):
                result = FRAMES.summarize(wire.replace('x=3fe0000000000000', f'x={value}'))
                self.assertTrue(result['valid_focused_Tiny_interval'])
                self.assertFalse(result['comparison_qualified'])

    def test_complete_graphics_identity_is_required(self):
        wire = work_receipt()
        for prefix in ('PERF_CONTEXT ', 'PERF_GPU_VENDOR ', 'PERF_GPU_RENDERER ', 'PERF_GPU_VERSION '):
            if prefix == 'PERF_CONTEXT ':
                changed = wire.replace('width=854 ', '')
            else:
                changed = '\n'.join(line for line in wire.splitlines() if not line.startswith(prefix))
            with self.subTest(prefix=prefix):
                result = FRAMES.summarize(changed)
                self.assertTrue(result['valid_focused_Tiny_interval'])
                self.assertFalse(result['comparison_qualified'])
        self.assertEqual(FRAMES.qualify_cohort([])['status'], 'FAIL')


class FrameObservations(unittest.TestCase):
    def test_existing_clock_split_and_legacy_rows(self):
        result = FRAMES.summarize(receipt())
        self.assertTrue(result['valid_focused_Tiny_interval'])
        self.assertEqual(result['source_render_split_rows'], 2)
        self.assertEqual(result['source_tick_batch']['mean_ms'], 1)
        self.assertEqual(result['source_render_and_update_prefix_not_GPU_time']['mean_ms'], 3)
        self.assertEqual(result['source_pre_HUD_prefix_not_GPU_time']['mean_ms'], 2)
        self.assertEqual(result['source_HUD_and_active_check_not_GPU_time']['mean_ms'], 1)
        self.assertAlmostEqual(result['measured_fps'], 2000 / 201)
        legacy = FRAMES.summarize(receipt(1))
        self.assertTrue(legacy['valid_focused_Tiny_interval'])
        self.assertIsNone(legacy['source_pre_HUD_prefix_not_GPU_time'])

    def test_java_long_wrap_and_negative_interval(self):
        self.assertEqual(FRAMES.source_clock_span(-(1 << 63) + 4, (1 << 63) - 6), .000010)
        with self.assertRaisesRegex(ValueError, 'regressed'):
            FRAMES.source_clock_span(0, 1)
        for value in (-(1 << 63) - 1, 1 << 63):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, 'signed Java long'):
                FRAMES.source_clock_span(value, 0)

    def test_missing_or_invalid_split_does_not_qualify(self):
        for suffix in (',3000010,0,1', ',3000010,1,0'):
            with self.subTest(suffix=suffix):
                result = FRAMES.summarize(receipt().replace(',3000010,1,1', suffix))
                self.assertFalse(result['valid_focused_Tiny_interval'])
                self.assertEqual(result['source_render_split_rows'], 1)
        with self.assertRaisesRegex(ValueError, 'clock/return'):
            FRAMES.summarize(receipt().replace(',3000010,1,1', ',3000010,2,1'))
        with self.assertRaisesRegex(ValueError, 'regressed'):
            FRAMES.summarize(receipt().replace(',3000010,1,1', ',5000010,1,1'))

    def test_inconsistent_or_overlapping_qpc_rows_are_rejected(self):
        original = receipt()
        for before, after in (
                ('start=999', 'start=1200'),
                ('end=1200', 'end=1199'),
                ('qpc-hz=1000', 'qpc-hz=0'),
                ('7,1000,1100,', '7,998,1100,'),
                ('8,1101,1200,', '8,1099,1200,'),
                ('8,1101,1200,', '8,1201,1200,'),
                ('8,1101,1200,', '8,1101,1199,')):
            with self.subTest(after=after), self.assertRaisesRegex(ValueError, 'QPC'):
                FRAMES.summarize(original.replace(before, after))

    def test_bad_rows_focus_and_cleanup(self):
        with self.assertRaisesRegex(ValueError, 'version'):
            FRAMES.summarize(receipt().replace('row-version=2', 'row-version=3'))
        with self.assertRaisesRegex(ValueError, 'frame rows'):
            FRAMES.summarize(receipt().replace(',3000010,1,1', ',3000010,1'))
        with self.assertRaisesRegex(ValueError, 'Nonconsecutive'):
            FRAMES.summarize(receipt().replace('PERF_ROW 8,', 'PERF_ROW 9,'))
        for before, after in (('cleanup=1', 'cleanup=0'),
                              ('pid=100 own-pid=100', 'pid=101 own-pid=100'),
                              ('focus-invalid=0', 'focus-invalid=1'),
                              ('yaw=43340000', 'yaw=00000000')):
            with self.subTest(after=after):
                self.assertFalse(FRAMES.summarize(receipt().replace(before, after))[
                    'valid_focused_Tiny_interval'])

    def test_published_phase_rows_and_instrumentation(self):
        self.check_phase_receipt('client_frame_phases_windows.json', nested=False)

    def test_nested_phase_parents_calls_and_instrumentation(self):
        self.check_phase_receipt('client_frame_nested_phases_windows.json', nested=True)

    def test_current_runtime_compilation_and_scheduler_work(self):
        report = json.loads((ROOT / 'tests/benchmarks/client_frame_work_windows.json').read_text())
        self.assertEqual(report['status'], 'PASS')
        self.assertEqual(len(report['captures']), 2)
        for capture in report['captures']:
            rows = capture['rows']
            hz = int(capture['measurement']['qpc-hz'])
            begin, end = int(capture['measurement']['start']), int(capture['measurement']['end'])
            self.assertGreater(hz, 0)
            self.assertGreater(end, begin)
            self.assertEqual(len(rows), int(capture['measurement']['frames']))
            self.assertEqual(rows[-1][0][2], end)
            self.assertEqual(capture['phase_schema'], [entry['name'] for entry in capture['phases']])
            for index, row in enumerate(rows):
                host, clocks, source, ticks, calls, valid, flags, work = row
                self.assertTrue(begin <= host[1] <= host[2] <= end)
                if index:
                    self.assertGreaterEqual(host[1], rows[index - 1][0][2])
                    self.assertEqual(host[0], rows[index - 1][0][0] + 1)
                self.assertEqual(len(ticks), len(capture['phase_schema']))
                self.assertEqual(len(work), len(capture['work_schema']))
                self.assertEqual(calls, valid)
                self.assertEqual(flags, 0)
                self.assertEqual(calls[:5], [1] * 5)
                self.assertTrue(all(value >= 0 for value in ticks + calls + work))
                self.assertLessEqual(sum(ticks[:5]) + ticks[16], host[2] - host[1])
                self.assertLessEqual(sum(ticks[5:11]), ticks[3])
                self.assertLessEqual(sum(ticks[11:16]), ticks[16])
                self.assertLessEqual(ticks[17], ticks[8])
                self.assertLessEqual(sum(ticks[18:21]), ticks[11])
                self.assertEqual(calls[16], source[1])
                self.assertEqual(calls[18:21], [calls[11]] * 3)
                self.assertEqual(source[2], 7)
                self.assertEqual(source[4:7], [1, -1, 1])
                self.assertEqual(source[-2:], [1, 1])
                self.assertEqual(work[2:4], [calls[17]] * 2)
                self.assertEqual(work[5:7], [calls[17]] * 2)
                self.assertEqual(work[8], work[7] * 80)
                self.assertEqual(work[9], calls[20] * 1000)
                for start, finish in ((0, 1), (1, 2), (1, 3), (3, 2)):
                    FRAMES.source_clock_span(clocks[finish], clocks[start])
                if len(ticks) == 26:
                    self.assertLessEqual(sum(ticks[21:26]), ticks[19])
                    self.assertTrue(all(calls[i] == calls[19] for i in (21, 22, 24, 25)))
                    self.assertLessEqual(calls[23], calls[19])
            for index, entry in enumerate(capture['phases']):
                values = [row[3][index] * 1000 / hz for row in rows]
                expected = {'count': len(rows), 'mean_ms': statistics.mean(values),
                            'p50_ms': statistics.median(values),
                            'p95_ms': FRAMES.percentile(values, .95), 'max_ms': max(values)}
                self.assertEqual(entry['summary'], expected)
            for index, name in enumerate(capture['work_schema']):
                self.assertEqual(capture['work_totals'][name], sum(row[7][index] for row in rows))
            self.assertEqual(int(capture['measurement']['world-ticks']), sum(row[4][11] for row in rows))
            for instrument in capture['instrumentation'].values():
                self.assertEqual(hashlib.sha256(instrument['text'].encode()).hexdigest(),
                                 instrument['sha256'])

    def test_rejected_helper_frame_attempts_and_input_control(self):
        report = json.loads((ROOT / 'tests/benchmarks/client_frame_helper_windows.json').read_text())
        self.assertFalse(report['strict_identical_work_speedup_established'])
        self.assertEqual(len(report['profiles']), 6)
        self.assertEqual(len(report['rejected_predecessor']['profiles']), 6)
        self.assertEqual(len(report['original_shared_control']['profiles']), 2)
        groups = (report['profiles'], report['rejected_predecessor']['profiles'],
                  report['original_shared_control']['profiles'])
        for group in groups:
            for profile in group:
                lines = list(profile['native_records'])
                for foreground in profile['native_foreground_projection']:
                    lines.append('PERF_FOREGROUND frame={frame} hwnd={window} pid={pid} own-pid=1'.format(
                        frame=foreground['frame'], window=int(foreground['window_present']),
                        pid=1 if foreground['owned_by_native_process'] else 2))
                try:
                    summary = FRAMES.summarize('\n'.join(lines))
                except ValueError:
                    self.assertEqual(profile['status'], 'FAIL')
                    self.assertIsNotNone(profile['failure_stage'])
                else:
                    if profile['summary'] is not None:
                        self.assertEqual(summary['valid_focused_Tiny_interval'],
                                         profile['summary']['valid_focused_Tiny_interval'])
                    if profile['status'] == 'PASS':
                        self.assertEqual(profile['native_returncode'], 0)
                        self.assertTrue(summary['valid_focused_Tiny_interval'])
                    else:
                        self.assertFalse(summary['valid_focused_Tiny_interval'])
        prelaunch = sum(not row['host_input_idle_before_launch']['unchanged_last_input']
                        for row in report['profiles'])
        changed = sum(row['host_input_before_child']['last_input_tick'] !=
                      row['host_input_after_child']['last_input_tick'] for row in report['profiles'])
        self.assertEqual(report['latest_cohort_input_evidence']['prelaunch_observation_changed_count'], prelaunch)
        self.assertEqual(report['latest_cohort_input_evidence']['whole_child_last_input_changed_count'], changed)
        for profile in report['original_shared_control']['profiles']:
            trace = profile['host_trace']
            changes = sum(a['last_input_tick'] != b['last_input_tick'] for a, b in zip(trace, trace[1:]))
            self.assertEqual(profile['host_last_input_changes'], changes)
            self.assertEqual(profile['F2_or_F3_observed_held'],
                             any(row['F2_down'] or row['F3_down'] for row in trace))

    def check_phase_receipt(self, filename, *, nested):
        report = json.loads((ROOT / 'tests/benchmarks' / filename).read_text())
        raw = report['raw_rows']
        hz = int(report['source_measurement']['qpc-hz'])
        phases = 17 if nested else 5
        self.assertEqual(len(raw), int(report['source_measurement']['frames']))
        self.assertEqual(report['raw_schema']['phase_ticks_calls_valid'],
                         [phase['name'] for phase in report['phases']])
        for row in raw:
            self.assertEqual(row['phase_flags'], 0)
            self.assertEqual(row['phase_calls'][:5], [1] * 5)
            self.assertEqual(row['phase_calls'], row['phase_valid'])
            self.assertEqual(len(row['phase_ticks']), phases)
            self.assertTrue(all(value >= 0 for value in row['phase_ticks']))
            parent_time = sum(row['phase_ticks'][:5])
            if nested:
                self.assertEqual(row['phase_calls'][5:8], [1, 1, 1])
                self.assertGreaterEqual(row['phase_calls'][8], 1)
                self.assertEqual(row['phase_calls'][9:11], [2, 1])
                self.assertEqual(row['phase_calls'][11:], [row['source_work'][1]] * 6)
                self.assertLessEqual(sum(row['phase_ticks'][5:11]), row['phase_ticks'][3])
                self.assertLessEqual(sum(row['phase_ticks'][11:16]), row['phase_ticks'][16])
                parent_time += row['phase_ticks'][16]
            self.assertLessEqual(parent_time, row['host_frame'][2] - row['host_frame'][1])
            self.assertEqual(row['source_work'][2], 7)
            self.assertEqual(row['source_work'][4:7], [1, -1, 1])
            self.assertEqual(row['source_work'][-2:], [1, 1])
            for start, end in zip(row['source_clock'][:2], row['source_clock'][1:3]):
                FRAMES.source_clock_span(end, start)
            FRAMES.source_clock_span(row['source_clock'][3], row['source_clock'][1])
            FRAMES.source_clock_span(row['source_clock'][2], row['source_clock'][3])
        for index, phase in enumerate(report['phases']):
            values = [row['phase_ticks'][index] * 1000 / hz for row in raw]
            self.assertEqual(phase['summary']['count'], len(raw))
            self.assertAlmostEqual(phase['summary']['mean_ms'], statistics.mean(values))
            self.assertEqual(phase['summary']['p50_ms'], statistics.median(values))
            self.assertEqual(phase['summary']['p95_ms'], FRAMES.percentile(values, .95))
            self.assertEqual(phase['summary']['max_ms'], max(values))
        for instrument in report['instrumentation'].values():
            self.assertEqual(hashlib.sha256(instrument['text'].encode()).hexdigest(),
                             instrument['sha256'])
        if nested:
            for field, index in (('World_chunk_update_calls', 8),
                                 ('World_opaque_and_translucent_submit_calls', 9),
                                 ('actual_completed_tick_calls', 16)):
                self.assertEqual(report['qualification'][field],
                                 sum(row['phase_calls'][index] for row in raw))


if __name__ == '__main__':
    unittest.main()
