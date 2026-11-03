"""Measure one fresh native Windows client in the authored sky-view scenario.

Used by client_frames.py. Imported assets remain local. Full source-frame work,
ticks, autosaves, rendering and SDK swaps run normally inside the interval.
No FPS threshold determines success; focus, scene and cleanup checks do.
"""
import argparse
import gzip
import importlib.util
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]


def module(name, relative):
    path = ROOT / relative
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def fields(line):
    pairs = [word.split('=', 1) for word in line.split()[1:]]
    if any(len(pair) != 2 for pair in pairs) or len({k for k, _ in pairs}) != len(pairs):
        raise ValueError('Malformed or duplicate probe fields')
    return dict(pairs)


def percentile(values, q):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(q * len(ordered)) - 1)]


def distribution(values):
    if not values:
        return None
    return {'count': len(values), 'mean_ms': sum(values) / len(values),
            'p50_ms': percentile(values, .50), 'p95_ms': percentile(values, .95),
            'p99_ms': percentile(values, .99), 'max_ms': max(values)}


def source_clock_span(end, start):
    if not all(-(1 << 63) <= value < (1 << 63) for value in (start, end)):
        raise ValueError('Source clock must be a signed Java long')
    # Source nanoTime subtraction has Java long wrapping, including a sample
    # pair that crosses the signed boundary during an otherwise short frame.
    span = (end - start + (1 << 63)) % (1 << 64) - (1 << 63)
    if span < 0:
        raise ValueError('Source clock interval regressed')
    return span / 1e6


def summarize(stdout):
    result = [fields(s) for s in stdout.splitlines() if s.startswith('PERF_RESULT ')]
    context = [fields(s) for s in stdout.splitlines() if s.startswith('PERF_CONTEXT ')]
    finish = [fields(s) for s in stdout.splitlines() if s.startswith('PERF_FINISH ')]
    if len(result) != 1 or len(context) != 1 or len(finish) != 1:
        raise ValueError('Missing unique measurement/context/cleanup receipt')
    result, context, finish = result[0], context[0], finish[0]
    raw = [[int(v) for v in s.split(' ', 1)[1].split(',')]
           for s in stdout.splitlines() if s.startswith('PERF_ROW ')]
    row_version = int(result.get('row-version', '1'))
    if row_version not in (1, 2):
        raise ValueError('Unknown frame observation version')
    columns = 16 if row_version == 1 else 19
    if not raw or any(len(row) != columns for row in raw) or len(raw) != int(result['frames']):
        raise ValueError('Incomplete typed raw frame rows')
    hz = int(result['qpc-hz'])
    start, end = int(result['start']), int(result['end'])
    if (hz <= 0 or end <= start or any(not start <= row[1] <= row[2] <= end for row in raw) or
            raw[-1][2] != end or any(row[1] < prior[2] for prior, row in zip(raw, raw[1:]))):
        raise ValueError('Invalid raw QPC interval')
    if any(row[0] != raw[0][0] + i for i, row in enumerate(raw)):
        raise ValueError('Nonconsecutive returned source frame indices')
    total = [(r[2] - r[1]) * 1000 / hz for r in raw]
    tick = [source_clock_span(r[4], r[3]) for r in raw if r[8] & 3 == 3]
    render_prefix = [source_clock_span(r[5], r[4]) for r in raw if r[8] & 6 == 6]
    split_rows = []
    if row_version == 2:
        if any(r[17] not in (0, 1) or r[18] not in (0, 1) for r in raw):
            raise ValueError('Invalid render clock/return observations')
        split_rows = [r for r in raw if r[8] & 6 == 6 and r[17] == 1 and r[18] == 1]
    pre_hud = [source_clock_span(r[16], r[4]) for r in split_rows]
    hud_and_active = [source_clock_span(r[5], r[16]) for r in split_rows]
    source_fps = []
    for line in stdout.splitlines():
        if line.startswith('PERF_SOURCE_FPS '):
            row = fields(line)
            words = row.pop('words')
            if len(words) % 4:
                raise ValueError('Invalid UTF16 Source FPS words')
            row['source_text'] = ''.join(chr(int(words[i:i+4], 16)) for i in range(0, len(words), 4))
            source_fps.append(row)
    cache = [fields(s) for s in stdout.splitlines() if s.startswith('PERF_CACHE ')]
    stages = ['before-selection', 'first-loaded', 'setup-returned', 'warmup-returned', 'timed-returned']
    if [r.get('stage') for r in cache] != stages or cache[0].get('present') != '0':
        raise ValueError('Missing ordered fresh-process cache receipts')
    if any(r.get('present') != '1' or r.get('owned') != '1' or r.get('raw-exports') != '0' or
           r.get('mutation-depth') != '0' for r in cache[1:]):
        raise ValueError('Setup or interval exported/disabled the ordinary owned cache')
    poses = [fields(s) for s in stdout.splitlines() if s.startswith('PERF_POSE ')]
    if [row.get('stage') for row in poses] != ['pre-warmup', 'timed-entry', 'timed-returned']:
        raise ValueError('Missing passive scalar camera receipts')
    foreground = [fields(s) for s in stdout.splitlines() if s.startswith('PERF_FOREGROUND ')]
    native_foreground_valid = bool(foreground) and all(
        int(row['hwnd']) != 0 and int(row['pid']) == int(row['own-pid']) > 0
        for row in foreground)
    actual_rotation_valid = all(row['yaw'] == '43340000' and row['pitch'] == 'c1f00000'
                                for row in poses[1:])
    valid = (result['healthy'] == '1' and result['capacity'] == '0' and
             result['focus-invalid'] == '0' and finish['cleanup'] == '1' and
             int(context['view-distance']) == 3 and all(r[12] == 1 for r in raw) and
             all(r[11] == -1 and r[10] == 1 for r in raw) and native_foreground_valid and
             actual_rotation_valid and (row_version == 1 or len(split_rows) == len(raw)))
    return {'cache_receipts': cache, 'no_raw_cache_exports': True,
            'camera_receipts': poses, 'native_foreground_receipts': foreground,
            'native_foreground_valid': native_foreground_valid,
            'authored_rotation_retained': actual_rotation_valid,
            'valid_focused_Tiny_interval': valid, 'context': context, 'measurement': result,
            'observation_row_version': row_version,
            'measured_fps': len(raw) * hz / (end - start),
            'gpu': {key: next((s.split(' ', 1)[1] for s in stdout.splitlines()
                              if s.startswith('PERF_GPU_' + key.upper() + ' ')), None)
                    for key in ('vendor', 'renderer', 'version')},
            'host_frame': distribution(total),
            'tick_bearing_host_frame': distribution([x for x, r in zip(total, raw) if r[6] > 0]),
            'zero_tick_host_frame': distribution([x for x, r in zip(total, raw) if r[6] == 0]),
            'source_tick_batch': distribution(tick),
            'source_render_and_update_prefix_not_GPU_time': distribution(render_prefix),
            'source_pre_HUD_prefix_not_GPU_time': distribution(pre_hud),
            'source_HUD_and_active_check_not_GPU_time': distribution(hud_and_active),
            'source_render_split_rows': len(split_rows),
            'source_render_split_scope': (
                'Existing PX.b450/I453 clock only; no additional source clock reads. '
                'Pre-HUD prefix includes sound/light work, SDK swap, World draw and source pacing. '
                'The remaining prefix includes HUD and display-active handling. Neither is GPU time.'
                if row_version == 2 else 'Legacy rows contain no pre-HUD clock observation.'),
            'frames_over_16_667_ms': sum(x > 1000 / 60 for x in total),
            'frames_over_33_333_ms': sum(x > 1000 / 30 for x in total),
            'observed_source_FPS_strings': source_fps,
            'scope': 'Fresh no-export setup; 2s natural warmup + 10s actual direct normal frames; includes normal Source pacing, SDK swap, tick/services/World/render work. No unrestricted/Far/GPU-only FPS claim.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--executable', type=Path, required=True)
    p.add_argument('--assets', type=Path, required=True)
    p.add_argument('--artifacts', type=Path, required=True)
    args = p.parse_args()
    if os.name != 'nt':
        raise RuntimeError('This raw-QPC/native-SDK observer declares Windows only')
    A = module('perf_auto_inputs', 'tests/tools/client/test_workbench_auto_close_loop.py')
    mining = module('perf_mining_inputs', 'tests/tools/client/test_mining_loop.py')
    selected = module('perf_selected_inputs', 'tests/tools/client/test_block_selection.py')
    world = module('perf_world_inputs', 'tests/tools/client/test_world_selection_loop.py')
    exe, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='release-sky-play-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {exe, Path(__file__).resolve(), ROOT / 'tests/benchmarks/client_frames.c',
              ROOT / 'scripts/benchmarks/client_frames.py',
              ROOT / 'scripts/benchmarks/client_frames_build.py'}
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*')
                      if path.is_file() and path.suffix in ('.c', '.h', '.inc', '.cmake', '.py'))
    inputs.update(exe.parent.glob('*.dll'))
    before = {str(path): mining.digest(path) for path in sorted(inputs)}
    asset_before = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before,
              'assets_before': asset_before, 'read_only_source_clock_observations': True,
              'audio_backend': 'existing SDK fixture OpenAL null driver; Source sound work unchanged',
              'per_frame_test_screenshots_disk_checks_assertion_helpers': False,
              'authored_rotation_only_change': {'before': [0.0, 30.0], 'after': [180.0, -30.0],
                                                'live_pose_repair': False}}
    try:
        # Keep the authored home/save as private evidence. Runtime autosaves are
        # allowed; pre/post file snapshots are deliberately outside subprocess.
        home = evidence / 'home'
        game = home / '.minecraft'
        game.mkdir(parents=True)
        report['statistics_input'] = A.B.statistics_input(game, assets)
        (game / 'options.txt').write_bytes(b'viewDistance:3\n')
        saves = game / 'saves'
        saves.mkdir()
        for folder, name, played in [('older', 'Old world', 1303000000000),
                                     ('bravo', '', 1304000000000),
                                     ('alpha', 'Release steady play', 1304000000000)]:
            directory = saves / folder
            directory.mkdir()
            wire = A.metadata(mining, selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
            if folder == 'alpha':
                original = world.tag(9, 'Rotation', b'\x05' + struct.pack('>i2f', 2, 0.0, 30.0))
                changed = world.tag(9, 'Rotation', b'\x05' + struct.pack('>i2f', 2, 180.0, -30.0))
                if wire.count(original) != 1 or wire.count(changed) != 0:
                    raise ValueError('Exactly one declared pre-load saved Rotation')
                before_rotation_change = wire
                wire = wire.replace(original, changed, 1)
                if wire.replace(changed, original, 1) != before_rotation_change:
                    raise ValueError('Only authored Rotation payload may change')
                report['authored_rotation_only_change'].update(
                    original_wire_sha256=mining.hashlib.sha256(before_rotation_change).hexdigest(),
                    sky_wire_sha256=mining.hashlib.sha256(wire).hexdigest(),
                    changed_bytes=sum(a != b for a, b in zip(before_rotation_change, wire)),
                    equal_wire_length=len(before_rotation_change) == len(wire))
            (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
            (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
        region = saves / 'alpha/region'
        region.mkdir()
        A.saved_regions(selected, world, region)
        (region / 'r.9.9.mcr').write_bytes(bytes(8192))
        report['save_before'] = world.snapshot(saves)
        environment = os.environ.copy()
        environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
            ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
            MC_WORLD_SAVE_TITLE_SAVES=str(saves),
            MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
            MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
        command = [str(exe), str(assets), str(evidence)]
        report['command'] = command
        report['started_ns'] = time.time_ns()
        # No polling, screenshots, disk hashing or assertions during the child.
        try:
            result = subprocess.run(command, cwd=evidence, env=environment,
                                    capture_output=True, text=False, timeout=240, check=False)
        except subprocess.TimeoutExpired as error:
            (evidence / 'stdout.txt').write_bytes(error.stdout or b'')
            (evidence / 'stderr.txt').write_bytes(error.stderr or b'')
            raise
        (evidence / 'stdout.txt').write_bytes(result.stdout)
        (evidence / 'stderr.txt').write_bytes(result.stderr)
        report['ended_ns'] = time.time_ns()
        report['returncode'] = result.returncode
        report['summary'] = summarize(result.stdout.decode('utf8', errors='strict'))
        report['save_after'] = world.snapshot(saves)
        report['save_changed_by_normal_source_execution'] = report['save_before'] != report['save_after']
        if result.returncode != 0 or not report['summary']['valid_focused_Tiny_interval']:
            raise RuntimeError('Probe did not return a complete focused Tiny steady-play interval')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): mining.digest(path) for path in sorted(inputs)}
        report['assets_after'] = world.snapshot(assets)
        report['inputs_unchanged'] = report['inputs_after'] == before
        report['assets_unchanged'] = report['assets_after'] == asset_before
        if not report['inputs_unchanged'] or not report['assets_unchanged']:
            report['status'] = 'FAIL'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print(evidence)
    if report['status'] != 'PASS':
        raise RuntimeError('Selected inputs changed')
    print(json.dumps(report['summary'], indent=2))


if __name__ == '__main__':
    main()
