"""Run actual Player damage/wake controls without display or original assets."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile


AFTER_FIELDS = ('sleeping', 'timer', 'health', 'idle', 'head', 'orphan',
                'world_sleeping', 'world_same', 'position', 'dimensions')
CALLBACK_FIELDS = ('xyz', 'head', 'orphan', 'sleeping')
ORIGINAL_MODES = {'owned-wake-recursion': 'success',
                  'owned-wake-null-exit': 'null-exit',
                  'owned-wake-observer-error': 'observer-error'}


def compare_original(mode, native, output):
    directory = (Path(__file__).resolve().parents[3] /
                 'tests/reference/entity/player_recursive_wake')
    spec = importlib.util.spec_from_file_location('player_recursive_wake_packet',
                                                  directory / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    finite_path = directory / 'finite.json'
    finite = packet.P.load(finite_path)
    packet.validate(finite['rows'])
    digest = packet.P.digest(finite_path)
    proof = packet.P.load(directory / 'provenance.json')
    if digest != proof['finite_sha256']:
        raise RuntimeError('Original recursive wake digest differs from its capture')
    selected = next(row for row in finite['rows'] if row['mode'] == ORIGINAL_MODES[mode])
    expected = {'result': selected['result'], 'exception': selected['exception'],
                'after': {key: selected['after'][key] for key in AFTER_FIELDS},
                'callbacks': [{key: row[key] for key in CALLBACK_FIELDS}
                              for row in selected['callbacks']]}
    observed = {key: native[key] for key in expected}
    matched = packet.P.exact(observed, expected)
    report = {'mode': mode, 'finite_sha256': digest, 'matched': matched,
              'compared_scalars': packet.P.scalars(expected),
              'expected': expected, 'observed': observed}
    (output / 'original-comparison.json').write_text(json.dumps(report, indent=2) + '\n',
                                                      encoding='utf-8')
    if not matched:
        raise RuntimeError('Original recursive wake mismatch in ' + str(output))
    return report['compared_scalars']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('damage-wake', 'owned-block-drops', 'owned-wake-recursion',
                                           'owned-wake-null-exit', 'owned-wake-observer-error',
                                           'owned-wake-scope-drift', 'owned-wake-null-chunk-world',
                                           'owned-wake-foot', 'owned-wake-foot-source',
                                           'owned-wake-foot-scope', 'owned-wake-foot-math-missing'),
                        default='damage-wake')
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='damage-wake-', dir=args.work_dir.resolve()))
    child = subprocess.run([args.executable.resolve(strict=True), args.mode,
                            str(output).encode('utf-8').hex()], capture_output=True, timeout=120)
    (output / 'native.stdout').write_bytes(child.stdout)
    (output / 'native.stderr').write_bytes(child.stderr)
    if child.returncode != 0 or child.stderr:
        raise RuntimeError('Player damage wake failed in ' + str(output) + '\n' +
                           child.stderr.decode('utf-8', 'replace'))
    lines = child.stdout.splitlines()
    compared = 0
    if args.mode == 'owned-block-drops':
        expected = rb'PASS: [1-9][0-9]* owned Block drop checks'
    elif args.mode != 'damage-wake':
        expected = rb'PASS: [1-9][0-9]* owned Player wake recursion checks'
        diagnostics = [line for line in lines if line.startswith(b'OWNED_WAKE_RECURSION ')]
        cleanup = b'OWNED_WAKE_RECURSION_CLEANUP complete=1'
        if len(diagnostics) != 1 or lines.count(cleanup) != 1:
            raise RuntimeError('Expected one owned wake observation and checked cleanup in ' + str(output))
        native = json.loads(diagnostics[0].partition(b' ')[2])
        if (type(native) is not dict or
                set(native) != {'continued', 'raw', 'pc', 'wake_entered', 'pending',
                                'result', 'exception', 'after', 'callbacks'} or
                type(native['after']) is not dict or set(native['after']) != set(AFTER_FIELDS) or
                type(native['callbacks']) is not list or len(native['callbacks']) != (1 if args.mode == 'owned-wake-null-chunk-world' else 2) or
                any(type(row) is not dict or set(row) != set(CALLBACK_FIELDS)
                    for row in native['callbacks'])):
            raise RuntimeError('Invalid owned wake observation in ' + str(output))
        if args.mode in ORIGINAL_MODES:
            compared = compare_original(args.mode, native, output)
        drop_lines = [line for line in lines if line.startswith(b'FOOT_WAKE_DROP ')]
        foot_modes = ('owned-wake-foot', 'owned-wake-foot-source', 'owned-wake-foot-scope')
        if args.mode in foot_modes:
            if len(drop_lines) != 1:
                raise RuntimeError('One actual foot Item observation required')
            drop = json.loads(drop_lines[0].partition(b' ')[2])
            if (type(drop) is not dict or set(drop) != {'world_seed', 'player_seed', 'item'} or
                    type(drop['item']) is not dict or
                    set(drop['item']) != {'position', 'delay', 'stack', 'inserted', 'world_same'}):
                raise RuntimeError('Exact foot Item observation fields required')
            if args.mode != 'owned-wake-foot-scope':
                directory = Path(__file__).resolve().parents[3] / 'tests/reference/entity/bed_foot_wake'
                spec = importlib.util.spec_from_file_location('bed_foot_comparison', directory / 'packet.py')
                packet = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(packet)
                packet.check()
                rows = packet.P.load(directory / 'finite.json')['rows']
                row = rows[0 if args.mode == 'owned-wake-foot' else 1]
                keys = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle',
                        'world_sleeping', 'head', 'orphan')
                selected = {'result': row['result'], 'exception': row['exception'],
                            'after': {key: row['after'][key] for key in keys},
                            'callbacks': [{key: event[key] for key in CALLBACK_FIELDS}
                                          for event in row['events'][:2]],
                            'world_seed': row['after']['world_random'][0],
                            'player_seed': row['after']['player_random'][0],
                            'item': {key: row['after']['items'][0][key] for key in drop['item']}}
                actual = {'result': native['result'], 'exception': native['exception'],
                          'after': {key: native['after'][key] for key in keys},
                          'callbacks': native['callbacks'], **drop}
                report = {'matched': packet.P.exact(actual, selected), 'expected': selected,
                          'observed': actual, 'compared_scalars': packet.P.scalars(selected)}
                (output / 'foot-original-comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                if not report['matched']:
                    raise RuntimeError('Original foot wake/drop state differs: ' + str(output))
                compared = report['compared_scalars']
            lines.remove(drop_lines[0])
        elif drop_lines:
            raise RuntimeError('Unexpected foot Item observation')
        lines.remove(diagnostics[0])
        lines.remove(cleanup)
    else:
        expected = rb'PASS: [1-9][0-9]* Player damage wake checks'
    if (not lines or re.fullmatch(expected, lines[-1]) is None or
            any(line not in (b'16 achievements', b'151 recipes') for line in lines[:-1])):
        raise RuntimeError('Unexpected Player damage wake output')
    print(lines[-1].decode('ascii') + '; capture=' + str(output))
    if compared:
        print(f'Original wake matches {compared} selected values')


if __name__ == '__main__':
    main()
