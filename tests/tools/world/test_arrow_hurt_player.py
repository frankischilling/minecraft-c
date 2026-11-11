"""Actual Arrow parent with real Player, application, Inventory and statistic owners."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import tempfile


def flower_receipts(lines, mode):
    def fields(line, prefix):
        result = {}
        for word in line.removeprefix(prefix).split():
            if '=' not in word:
                raise ValueError('Malformed Flower receipt field')
            key, value = word.split('=', 1)
            if key in result:
                raise ValueError('Duplicate Flower receipt field')
            result[key] = value
        return result
    rows = [line for line in lines if line.startswith('ARROW_FLOWER_INPUT ')]
    speed = struct.unpack('>f', struct.pack('>f', .6))[0]
    expected = {'flower': '37:0', 'floor': '0:0', 'sky': '15', 'block': '0', 'height': '64',
                'world-f': '5', 'sleeping': '1', 'timer': '37',
                'arrow-x': struct.pack('>d', 9.4).hex(), 'arrow-y': struct.pack('>d', 64.85).hex(),
                'arrow-vx': struct.pack('>d', -speed).hex()}
    inputs = fields(rows[0], 'ARROW_FLOWER_INPUT ') if len(rows) == 1 else None
    if inputs != expected:
        raise ValueError('Exact real Flower/light/sleep/Arrow inputs required')
    rows = [fields(line, 'ARROW_FLOWER_PREFIX ') for line in lines if line.startswith('ARROW_FLOWER_PREFIX ')]
    normal = mode == 'wake-bed-flower'
    phases = ['returned'] if normal else ['diagnostic', 'capture', 'hurt-close', 'hurt-cleanup', 'quiescent']
    if len(rows) != len(phases) or [row.get('phase') for row in rows] != phases:
        raise ValueError('Exact five checked native unwind phases or normal return required')
    removed = normal or mode == 'wake-bed-flower-removal-source'
    for index, row in enumerate(rows):
        expected = {'phase': phases[index], 'cells': '2' if removed else '1', 'additions': '1',
                    'flower': '0' if removed else '37', 'metadata': '0', 'sleeping': '0' if normal else '1',
                    'timer': '0' if normal else '37', 'health': '16' if normal else '20',
                    'J': '0' if normal else '1', 'pending': '1' if not normal and index < 3 else '0'}
        random_keys = {stream + '-' + name for stream in ('world', 'player', 'arrow')
                       for name in ('seed', 'cache', 'gaussian')}
        if set(row) != set(expected) | random_keys or any(row[key] != value for key, value in expected.items()):
            raise ValueError('Committed Flower/Player/Item prefix changed during unwind')
        for stream in ('world', 'player', 'arrow'):
            if (not re.fullmatch('[0-9a-f]{16}', row[stream + '-seed']) or
                    int(row[stream + '-seed'], 16) >= 1 << 48 or row[stream + '-cache'] not in ('0', '1') or
                    not re.fullmatch('[0-9a-f]{16}', row[stream + '-gaussian'])):
                raise ValueError('Complete actual Random fields required')
        if index and any(row[key] != rows[0][key] for key in row if key not in ('phase', 'pending')):
            raise ValueError('Source callback, prefix or Random replay during native unwind')
    return {'inputs': inputs,
            'prefixes': rows, 'original_comparison': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('live', 'fatal', 'fatal-source', 'wake',
                                          'wake-source-null-list', 'wake-bed-foot',
                                          'wake-bed-foot-source', 'wake-bed-ladder',
                                          'wake-bed-ladder-source', 'wake-bed-ladder-removal-source', 'wake-bed-torch', 'wake-bed-torch-source', 'wake-bed-torch-removal-source', 'wake-bed-flower',
                                          'wake-bed-flower-source', 'wake-bed-flower-removal-source'), required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]

    def load(name, relative):
        spec = importlib.util.spec_from_file_location(name, repo / relative)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    support = load('arrow_player_capture', 'tests/reference/client/statistics/support.py')
    resources = load('arrow_player_resources', 'tests/reference/client/player_statistics/support.py')
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='arrow-player-', dir=args.work_dir)).resolve()
    resource_root, game = out / 'resources', out / 'game'
    resources.resources_at(resource_root)
    game.mkdir()
    child = support.run_bounded(
        [str(executable), args.mode, str(resource_root), str(game)], out, 'native',
        timeout=240, limit=1024**2, env=os.environ.copy())
    lines = child.stdout.decode('utf-8', errors='strict').splitlines()
    footer = (re.fullmatch(r'PASS: ([1-9][0-9]*) actual Arrow Player hurt checks;mode=' +
                           re.escape(args.mode), lines[-1]) if lines else None)
    report = {'mode': args.mode, 'exit': child.returncode, 'capture': child.capture_report,
              'stderr_empty': child.stderr == b'',
              'executable_unchanged': hashlib.sha256(executable.read_bytes()).hexdigest() == before,
              'scope': 'Actual Arrow finite Player hit, fatal work and wake controls'}
    report['passed'] = bool(child.returncode == 0 and footer and not child.stderr and
                            report['executable_unchanged'])
    if args.mode.startswith('wake-bed-flower'):
        try:
            report['flower'] = flower_receipts(lines, args.mode)
        except ValueError as failure:
            report['passed'] = False
            report['flower_failure'] = str(failure)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(child.stdout.decode('utf-8', errors='strict'), end='')
    if child.stderr:
        print(child.stderr.decode('utf-8', errors='strict'), end='')
    if not report['passed']:
        raise RuntimeError('Arrow Player hurt control failed: ' + str(out))


if __name__ == '__main__':
    main()
