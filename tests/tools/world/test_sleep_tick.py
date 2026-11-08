"""Authored native World tick wake, captured iterator and cleanup controls."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    helper = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('sleep_tick_resources', helper)
    if spec is None or spec.loader is None:
        raise RuntimeError('Player statistics resource helper unavailable')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='world-sleep-tick-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    report = {'scope': 'authored native caller controls; no additional original VM rows',
              'status': 'FAIL'}
    try:
        child = support.run([args.executable.resolve(strict=True), 'sleep-tick',
                             str(game).encode('utf-8').hex(),
                             str(resources).encode('utf-8').hex()], output, 'native', timeout=180)
        report['native'] = child.capture_report
        support.require(child.returncode == 0 and child.stderr == b'',
                        'World sleep tick native control failure')
        lines = child.stdout.splitlines()
        ranges = []
        constructor_lines = []
        for line in lines[:-1]:
            if line.startswith(b'SLEEP_RANGE '):
                matched = re.fullmatch(rb'SLEEP_RANGE ([0-8]) ([0-9]+) ([0-9]+) ([0-9]+)', line)
                support.require(matched is not None, 'Malformed World sleep range observation')
                ranges.append([int(matched[i]) for i in range(1, 5)])
            else:
                constructor_lines.append(line)
        support.require(len(lines) == 28 and
                        constructor_lines == [b'16 achievements', b'151 recipes'] * 9,
                        'Unexpected World sleep tick constructor output')
        support.require(len(ranges) == 9 and [row[0] for row in ranges] == list(range(9)),
                        'Missing actual World observer count records')
        support.require([row[3] for row in ranges] == [2, 0, 1, 1, 1, 2, 1, 2, 2] and
                        all(row[2] >= row[3] for row in ranges),
                        'Complete cell/Bed-target observer counts')
        support.require(re.fullmatch(rb'World sleep tick: [1-9][0-9]* checks', lines[-1]) is not None,
                        'Missing World sleep tick completion')
        report['status'] = 'PASS'
        report['ranges'] = ranges
        report['range_scope'] = ('Passive actual invocation counts for stored-air inputs may be zero; '
                                 'no positive range-callback coverage is claimed')
        report['input_chunks'] = {'count': 361, 'x': [-9, 9], 'z': [-9, 9],
                                  'scope': 'authored stored air, no terrain generation or elapsed sleep ticks'}
        report['completion'] = lines[-1].decode('ascii')
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(report['completion'] + '; capture=' + str(output))


if __name__ == '__main__':
    main()
