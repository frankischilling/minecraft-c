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


def compare_attached_original(mode, native, drop, publication, output, supports=None):
    kind = 'torch' if mode.startswith('owned-wake-torch') else 'ladder'
    directory = Path(__file__).resolve().parents[3] / ('tests/reference/entity/' + kind + '_wake')
    spec = importlib.util.spec_from_file_location(kind + '_wake_comparison', directory / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    packet.check()
    finite = directory / 'finite.json'
    rows = packet.P.load(finite)['rows']
    row = rows[0 if mode == 'owned-wake-' + kind else 1]
    keys = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle', 'world_sleeping', 'head')
    added = next(event for event in row['events'] if event['kind'] == 'added')
    expected = {'result': row['result'], 'exception': row['exception'],
                'after': {**{key: row['after'][key] for key in keys}, kind: row['after'][kind]},
                'world_seed': row['after']['world_random'][0],
                'player_seed': row['after']['player_random'][0],
                'item': {key: row['after']['items'][0][key] for key in drop['item']},
                'callbacks': [{key: event[key] for key in ('xyz', 'head', kind, 'sleeping')}
                              for event in row['events'] if event['kind'] == 'cell'],
                'publication': {**{key: added[key] for key in ('head', kind, 'sleeping')},
                    'prior_cells': sum(event['kind'] == 'cell'
                                       for event in row['events'][:row['events'].index(added)])}}
    actual = {'result': native['result'], 'exception': native['exception'],
              'after': {**{key: native['after'][key] for key in keys}, kind: native['after']['orphan']},
              **drop, 'publication': publication,
              'callbacks': [{**{key: event[key] for key in ('xyz', 'head', 'sleeping')},
                             kind: event['orphan']} for event in native['callbacks']]}
    if kind == 'torch':
        expected['supports'] = {key: row['after'][key] for key in ('torch_support', 'torch_floor')}
        actual['supports'] = supports
    report = {'mode': mode, 'finite_sha256': packet.P.digest(finite),
              'matched': packet.P.exact(actual, expected), 'compared_scalars': packet.P.scalars(expected),
              'expected': expected, 'observed': actual}
    (output / (kind + '-original-comparison.json')).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not report['matched']:
        raise RuntimeError('Original ' + kind + ' wake/drop state differs: ' + str(output))
    return report['compared_scalars']


def flower_environment(mode, value):
    keys = {'flower_floor', 'light', 'sky_subtraction', 'chunk_light_seen', 'world_seed', 'player_seed'}
    supported = mode in ('owned-wake-flower-supported', 'owned-wake-flower-sky-supported')
    dark = mode == 'owned-wake-flower-dark'
    low = dark or mode == 'owned-wake-flower-sky-supported'
    if (type(value) is not dict or set(value) != keys or
            type(value['flower_floor']) is not list or len(value['flower_floor']) != 2 or
            type(value['light']) is not list or len(value['light']) != 3 or
            any(type(n) is not int for n in value['flower_floor'] + value['light']) or
            value['flower_floor'] != [2 if supported or dark else 0, 0] or
            value['light'] != [0 if low and not dark else 15, 0, 64] or
            type(value['sky_subtraction']) is not int or value['sky_subtraction'] != 5 or
            type(value['chunk_light_seen']) is not bool or value['chunk_light_seen'] != (not low) or
            any(type(value[key]) is not str or re.fullmatch(r'[0-9a-f]{16}', value[key]) is None
                for key in ('world_seed', 'player_seed'))):
        raise RuntimeError('Exact actual Flower floor/light/Random observation required')
    return value


def compare_flower_original(mode, native, drop, publication, environment, output):
    directory = Path(__file__).resolve().parents[3] / 'tests/reference/entity/flower_wake'
    spec = importlib.util.spec_from_file_location('flower_wake_comparison', directory / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    finite = packet.read_reference()
    profile = {'owned-wake-flower-supported': 'supported', 'owned-wake-flower': 'unsupported',
               'owned-wake-flower-source': 'publication-error'}[mode]
    row = next(r for r in finite['rows'] if r['mode'] == profile)
    keys = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle', 'world_sleeping', 'head')
    item_keys = ('position', 'delay', 'stack', 'inserted', 'world_same')
    light = row['after']['light_inputs']
    expected = {'result': row['result'], 'exception': row['exception'],
                'after': {**{key: row['after'][key] for key in keys}, 'flower': row['after']['flower']},
                'environment': {'flower_floor': row['after']['flower_floor'],
                    'light': [light['sky_nibble'], light['block_light_nibble'], light['height']],
                    'sky_subtraction': light['world_f_sky_subtraction'],
                    'chunk_light_seen': light['chunk_sky_seen_static'],
                    'world_seed': row['after']['world_random'][0],
                    'player_seed': row['after']['player_random'][0]},
                'items': [{key: item[key] for key in item_keys} for item in row['after']['items']],
                'callbacks': [{key: event[key] for key in ('xyz', 'head', 'flower', 'sleeping')}
                              for event in row['events'] if event['kind'] == 'cell']}
    actual = {'result': native['result'], 'exception': native['exception'],
              'after': {**{key: native['after'][key] for key in keys}, 'flower': native['after']['orphan']},
              'environment': environment, 'items': [] if drop is None else [drop['item']],
              'callbacks': [{**{key: event[key] for key in ('xyz', 'head', 'sleeping')},
                             'flower': event['orphan']} for event in native['callbacks']]}
    if drop is not None:
        added = next(event for event in row['events'] if event['kind'] == 'added')
        expected['publication'] = {**{key: added[key] for key in ('head', 'flower', 'sleeping')},
            'prior_cells': sum(e['kind'] == 'cell' for e in row['events'][:row['events'].index(added)])}
        actual['publication'] = publication
        if drop['world_seed'] != environment['world_seed'] or drop['player_seed'] != environment['player_seed']:
            raise RuntimeError('Flower publication/final Random receipts differ')
    report = {'mode': mode, 'profile': profile, 'finite_sha256': packet.P.digest(directory / 'finite.json'),
              'matched': packet.P.exact(actual, expected), 'compared_scalars': packet.P.scalars(expected),
              'expected': expected, 'observed': actual}
    (output / 'flower-original-comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not report['matched']:
        raise RuntimeError('Original Flower wake state differs: ' + str(output))
    return report['compared_scalars']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('damage-wake', 'owned-block-drops', 'owned-wake-recursion',
                                           'owned-wake-null-exit', 'owned-wake-observer-error',
                                           'owned-wake-scope-drift', 'owned-wake-null-chunk-world',
                                           'owned-wake-foot', 'owned-wake-foot-source',
                                           'owned-wake-foot-scope', 'owned-wake-foot-math-missing',
                                           'owned-wake-ladder', 'owned-wake-ladder-source',
                                           'owned-wake-ladder-scope', 'owned-wake-ladder-math-missing',
                                           'owned-wake-ladder-removal-source', 'owned-wake-ladder-supported',
                                           'owned-wake-torch', 'owned-wake-torch-source',
                                           'owned-wake-torch-scope', 'owned-wake-torch-math-missing',
                                           'owned-wake-torch-removal-source', 'owned-wake-torch-supported',
                                           'owned-wake-torch-early', 'owned-wake-torch-early-source',
                                           'owned-wake-torch-early-removal-source',
                                           'owned-wake-flower', 'owned-wake-flower-source',
                                           'owned-wake-flower-scope', 'owned-wake-flower-math-missing',
                                           'owned-wake-flower-removal-source', 'owned-wake-flower-supported',
                                           'owned-wake-flower-sky-supported', 'owned-wake-flower-dark'),
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
                type(native['callbacks']) is not list or len(native['callbacks']) != (1 if args.mode in (
                    'owned-wake-null-chunk-world', 'owned-wake-ladder-source',
                    'owned-wake-ladder-scope', 'owned-wake-ladder-math-missing',
                    'owned-wake-ladder-supported', 'owned-wake-torch-source', 'owned-wake-torch-scope',
                    'owned-wake-torch-math-missing', 'owned-wake-torch-supported',
                    'owned-wake-torch-early-source', 'owned-wake-flower-source',
                    'owned-wake-flower-scope', 'owned-wake-flower-math-missing',
                    'owned-wake-flower-supported', 'owned-wake-flower-sky-supported') else 2) or
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
        supports = None
        support_lines = [line for line in lines if line.startswith(b'TORCH_WAKE_SUPPORT ')]
        if args.mode.startswith('owned-wake-torch'):
            if len(support_lines) != 1:
                raise RuntimeError('Expected actual Torch wall and floor state')
            supports = json.loads(support_lines[0].partition(b' ')[2])
            if (type(supports) is not dict or set(supports) != {'torch_support', 'torch_floor'} or
                    any(type(value) is not list or len(value) != 2 or
                        any(type(v) is not int for v in value) for value in supports.values())):
                raise RuntimeError('Exact Torch support cell fields required')
            lines.remove(support_lines[0])
        elif support_lines:
            raise RuntimeError('Unexpected Torch support observation')
        environment = None
        environment_lines = [line for line in lines if line.startswith(b'FLOWER_WAKE_ENV ')]
        if args.mode.startswith('owned-wake-flower'):
            if len(environment_lines) != 1:
                raise RuntimeError('One actual Flower environment observation required')
            environment = flower_environment(args.mode, json.loads(environment_lines[0].partition(b' ')[2]))
            lines.remove(environment_lines[0])
        elif environment_lines:
            raise RuntimeError('Unexpected Flower environment observation')
        range_lines = [line for line in lines if line.startswith(b'FLOWER_WAKE_RANGE ')]
        if args.mode == 'owned-wake-flower-dark':
            if range_lines != [b'FLOWER_WAKE_RANGE min=7,64,8 max=7,65,8 prior-cells=1 Item=1 height-before-store=65']:
                raise RuntimeError('Exact height-change notification before height store required')
            lines.remove(range_lines[0])
        elif range_lines:
            raise RuntimeError('Unexpected Flower height-change notification')
        if args.mode in ('owned-wake-ladder', 'owned-wake-ladder-source', 'owned-wake-ladder-scope',
                         'owned-wake-ladder-removal-source', 'owned-wake-torch',
                         'owned-wake-torch-source', 'owned-wake-torch-scope',
                         'owned-wake-torch-removal-source', 'owned-wake-torch-early',
                         'owned-wake-torch-early-source', 'owned-wake-torch-early-removal-source',
                         'owned-wake-flower', 'owned-wake-flower-source', 'owned-wake-flower-scope',
                         'owned-wake-flower-removal-source', 'owned-wake-flower-dark'):
            kind = ('flower' if args.mode.startswith('owned-wake-flower') else
                    'torch' if args.mode.startswith('owned-wake-torch') else 'ladder')
            neighbor_drops = [line for line in lines if line.startswith(kind.upper().encode('ascii') + b'_WAKE_DROP ')]
            if len(neighbor_drops) != 1:
                raise RuntimeError('Expected one actual ' + kind.title() + ' Item publication')
            lines.remove(neighbor_drops[0])
            publication_lines = [line for line in lines if line.startswith(kind.upper().encode('ascii') + b'_WAKE_PUBLICATION ')]
            if len(publication_lines) != 1:
                raise RuntimeError('Expected actual ' + kind.title() + ' cell state at publication')
            publication = json.loads(publication_lines[0].partition(b' ')[2])
            if (type(publication) is not dict or
                    publication != {'head': [26, 8], kind: ({'flower': [37, 0], 'torch': [50, 1], 'ladder': [65, 2]}[kind]), 'sleeping': True, 'prior_cells': 1} or
                    publication['sleeping'] is not True or type(publication['prior_cells']) is not int or
                    any(type(publication[key]) is not list or any(type(v) is not int for v in publication[key])
                        for key in ('head', kind))):
                raise RuntimeError(kind.title() + ' publication prefix differs')
            drop = json.loads(neighbor_drops[0].partition(b' ')[2])
            if (type(drop) is not dict or set(drop) != {'world_seed', 'player_seed', 'item'} or
                    type(drop['item']) is not dict or
                    set(drop['item']) != {'position', 'delay', 'stack', 'inserted', 'world_same'}):
                raise RuntimeError('Exact ' + kind.title() + ' Item observation required')
            if args.mode in ('owned-wake-ladder', 'owned-wake-ladder-source', 'owned-wake-torch', 'owned-wake-torch-source'):
                compared = compare_attached_original(args.mode, native, drop, publication, output, supports)
            if args.mode in ('owned-wake-flower', 'owned-wake-flower-source'):
                compared = compare_flower_original(args.mode, native, drop, publication, environment, output)
            lines.remove(publication_lines[0])
        if args.mode == 'owned-wake-flower-supported':
            compared = compare_flower_original(args.mode, native, None, None, environment, output)
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
