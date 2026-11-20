"""Compare actual damage/helper calls over genuine native owners to the original packet."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import tempfile
import zlib

import packet as P
from observation import MASK, MULTIPLIER, f32

ROOT = P.HERE.parents[3]
PLANES = {'blocks': 32768, 'metadata': 16384, 'sky': 16384, 'block_light': 16384, 'height': 256}
TIMEOUT = 480
STREAM_LIMIT = 2 * 1024 * 1024


def completed_native(child):
    capture = child.capture_report
    P.need(type(child.returncode) is int and child.returncode == 0 and
           type(capture.get('exit')) is int and capture['exit'] == child.returncode and
           capture.get('timed_out') is False and capture.get('output_overflow') is False and
           capture.get('pipe_capture_complete') is True and capture.get('reaped') is True and
           capture.get('reader_errors') == {} and capture.get('timeout_seconds') == TIMEOUT and
           capture.get('limit') == STREAM_LIMIT and capture.get('stdout_bytes') == len(child.stdout) and
           capture.get('stderr_bytes') == len(child.stderr), 'complete bounded native capture')
    P.need(child.stderr == b'' and child.stdout.endswith(b'\n'), 'quiet complete native transport')
    lines = child.stdout.splitlines()
    declared = []
    for index in range(40):
        declared += [b'16 achievements', b'151 recipes', ('PLAYER_ALLY_ALERT_NATIVE case=' + str(index)).encode('ascii')]
    P.need(len(lines) == len(declared) + 1 and lines[:-1] == declared and
           re.fullmatch(rb'Player ally alert: ([1-9][0-9]*) checks;40 genuine profiles', lines[-1]) is not None,
           'exact ordered constructors/cases/checked-cleanup footer')
    return int(re.fullmatch(rb'Player ally alert: ([1-9][0-9]*) checks;40 genuine profiles', lines[-1]).group(1))


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def tag(kind, name, payload):
    encoded = name.encode('ascii')
    return bytes([kind]) + struct.pack('>H', len(encoded)) + encoded + payload


def authored_world(path):
    region = path / 'region'
    region.mkdir(parents=True)
    groups = {}
    for x in range(-2, 3):
        for z in range(-2, 3):
            groups.setdefault((x // 32, z // 32), []).append((x, z))
    for (rx, rz), positions in groups.items():
        header = bytearray(8192)
        records = []
        for x, z in positions:
            fields = [tag(3, 'xPos', struct.pack('>i', x)), tag(3, 'zPos', struct.pack('>i', z)), tag(4, 'LastUpdate', bytes(8))]
            for name, length in (('Blocks', 32768), ('Data', 16384), ('SkyLight', 16384), ('BlockLight', 16384), ('HeightMap', 256)):
                value = bytearray(length)
                if name == 'Blocks':
                    for column in range(256):
                        value[column * 128 + 63] = 1
                if name == 'HeightMap':
                    value[:] = bytes([64]) * length
                fields.append(tag(7, name, struct.pack('>i', length) + value))
            fields += [tag(1, 'TerrainPopulated', b'\1'), tag(9, 'Entities', b'\x0a\0\0\0\0'), tag(9, 'TileEntities', b'\x0a\0\0\0\0')]
            payload = b'\2' + zlib.compress(tag(10, '', tag(10, 'Level', b''.join(fields) + b'\0') + b'\0'))
            record = struct.pack('>I', len(payload)) + payload
            P.need(len(record) <= 4096, 'bounded authored normal Chunk sector')
            struct.pack_into('>I', header, 4 * ((x & 31) + 32 * (z & 31)), ((2 + len(records)) << 8) | 1)
            records.append(record + bytes(4096 - len(record)))
        (region / f'r.{rx}.{rz}.mcr').write_bytes(header + b''.join(records))


def plane(value, size):
    P.need(type(value) is dict and set(value) == {'length', 'runs'} and value['length'] == size, 'complete native plane extent')
    result = bytearray(size)
    end, previous = 0, 0
    for run in value['runs']:
        P.need(type(run) is list and len(run) == 3 and all(type(word) is int for word in run), 'typed native plane run')
        start, count, byte = run
        P.need(0 <= start < size and count > 0 and start >= end and start + count <= size and
               1 <= byte <= 255 and not (start == end and byte == previous), 'ordered maximal native plane runs')
        result[start:start + count] = bytes([byte]) * count
        end, previous = start + count, byte
    return result


def read_state(path):
    value = P.load(path)
    P.need(type(value) is dict and set(value) == {'actor_name', 'actor_world_null', 'actor_app_null',
           'actor_inventory_null', 'world_entities', 'world_players', 'chunks', 'statistics',
           'box_pool_used', 'events', 'entities', 'world_random'}, 'closed bounded native state')
    P.need([(row['x'], row['z']) for row in value['chunks']] == [(x, z) for x in range(-2, 3) for z in range(-2, 3)], 'actual 25 Chunk inventory')
    for chunk in value['chunks']:
        P.need(set(chunk) == {'x', 'z', 'slices', 'raw_planes'} and set(chunk['raw_planes']) == set(PLANES), 'closed native Chunk readback')
        data = hashlib.sha256()
        for name, size in PLANES.items():
            data.update(plane(chunk['raw_planes'][name], size))
        del chunk['raw_planes']
        chunk['planes'] = data.hexdigest()
    return value


def private_streams(before, after):
    P.need(before['world_random'] == after['world_random'], 'native call preserves actual World stream')
    sounds = after['events']
    P.need(len(sounds) <= 1 and before['events'] == [], 'native actual sound inventory')
    for entry, exit in zip(before['entities'], after['entities']):
        P.need(entry['id'] == exit['id'], 'native retained Entity inventory')
        state = int(entry['random'][0], 16)
        P.need(0 <= state <= MASK and entry['random'][1:] == [False, '0000000000000000'], 'native actual private stream input')
        draws = []
        for _ in range(2 if entry['id'] == 0 and sounds else 0):
            state = (state * MULTIPLIER + 11) & MASK
            draws.append((state >> 24) / 16777216.0)
        P.need(exit['random'] == [f'{state:016x}', False, '0000000000000000'], 'native exact private consumption')
        if draws:
            pitch = struct.pack('>f', f32(f32(f32(draws[0] - draws[1]) * f32(.2)) + f32(1))).hex()
            P.need(sounds[0]['pitch'] == pitch, 'native sound uses this run actual private stream')


def selected(value):
    value = copy.deepcopy(value)
    del value['world_random']
    for entity in value['entities']:
        del entity['random']
    for event in value['events']:
        del event['pitch']
    return value


def compare_native(expected, directory):
    actual = []
    compared = 0
    unpaired = 0
    for original in expected:
        index = original['case']
        row = P.load(directory / f'case{index}.json')
        P.need(type(row) is dict and set(row) == {'case', 'input', 'returned', 'result', 'exception',
               'native_status', 'source_pc', 'examined', 'alerted'}, 'closed native outcome metadata')
        P.need(row['case'] == index and P.exact(row['input'], original['input']) and
               row['returned'] is (original['exception'] == 'NONE'), 'actual completed native outcome')
        before, after = [read_state(directory / f'case{index}-{phase}.json') for phase in ('before', 'after')]
        private_streams(before, after)
        value = {'case': row['case'], 'input': row['input'], 'result': row['result'], 'exception': row['exception'],
                 'before': selected(before), 'after': selected(after)}
        reference = {name: original[name] for name in value}
        P.need(P.exact(reference, value), 'original/native selected disagreement in case ' + str(index))
        compared += P.scalars(reference)
        unpaired += P.scalars(original['exception_message']) + P.scalars(original['exception_frames'])
        actual.append({**row, 'before': before, 'after': after})
    return actual, compared, unpaired


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    expected, _ = P.read()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ally-alert-comparison-', dir=args.work_dir.resolve()))
    resources, game, native = output / 'resources', output / 'game', output / 'native'
    game.mkdir(); native.mkdir()
    support_path = ROOT / 'tests/reference/client/player_statistics/support.py'
    support = module('ally_actual_statistics_resources', support_path)
    support.resources_at(resources)
    for index in range(40):
        authored_world(game / f'case{index}' / 'world')
    inputs = {executable, Path(__file__).resolve(), ROOT / 'tests/entity/player/test_ally_alert_original.c',
              ROOT / 'tests/client/statistics/test_player_dispatch.c', support_path,
              ROOT / 'tests/reference/client/statistics/support.py', *P.HERE.glob('*.*'),
              *resources.rglob('*.*'), *game.rglob('*.mcr'), *executable.parent.glob('*.dll')}
    inputs = {path for path in inputs if path.is_file()}
    for directory in ('src', 'include', 'cmake'):
        inputs.update(path for path in (ROOT / directory).rglob('*') if path.is_file())
    before = {str(path): digest(path) for path in sorted(inputs)}
    report = {'schema': 'PLAYER_ALLY_ALERT_NATIVE_COMPARISON_V1', 'status': 'FAIL', 'input_before_sha256': before,
              'scope': P.SCOPE, 'timeout_seconds': TIMEOUT, 'stream_limit_each': STREAM_LIMIT,
              'snapshot_file_limit_each': 8 * 1024 * 1024}
    first_failure = None
    try:
        command = [str(executable), str(resources), str(game), str(P.HERE / 'profiles.tsv'), str(native)]
        report['command'] = command
        child = support.run(command, output, 'native', timeout=TIMEOUT)
        report['returncode'] = child.returncode
        report['capture'] = child.capture_report
        report['fixture_checks'] = completed_native(child)
        actual, count, unpaired = compare_native(expected['rows'], native)
        (output / 'actual.json').write_text(json.dumps(actual, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', compared_scalars=count, original_message_frame_scalars_unpaired=unpaired,
                      calls=40, normal_returns=sum(row['returned'] for row in actual), actual_sha256=digest(output / 'actual.json'))
    except BaseException as failure:
        first_failure = failure
        report['exception'] = {'type': type(failure).__name__, 'message': str(failure)}
        if getattr(failure, 'capture_report', None) is not None:
            report['capture'] = failure.capture_report
        raise
    finally:
        unchanged = False
        try:
            report['input_after_sha256'] = {str(path): digest(path) for path in sorted(inputs)}
            unchanged = before == report['input_after_sha256']
        except BaseException as failure:
            report['recheck_exception'] = {'type': type(failure).__name__, 'message': str(failure)}
        if not unchanged:
            report['status'] = 'FAIL'
        for name in ('stdout', 'stderr'):
            path = output / ('native.' + name)
            if path.exists():
                report[name] = {'bytes': path.stat().st_size, 'sha256': digest(path)}
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        if first_failure is None:
            P.need(unchanged, 'native source/product/inputs changed during comparison')
    print(f'Player ally alert original comparison: {count} shared scalars; {output / "run.json"}')


if __name__ == '__main__':
    main()
