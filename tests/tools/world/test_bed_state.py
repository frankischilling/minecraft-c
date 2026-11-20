"""Authored native controls of static VE occupancy, ordering and retained errors."""
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
    spec = importlib.util.spec_from_file_location('bed_state_resources', helper)
    if spec is None or spec.loader is None:
        raise RuntimeError('Player statistics resource helper unavailable')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='bed-state-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    report = {'scope': 'authored native VE occupancy controls; no new original VM comparison',
              'status': 'FAIL'}
    try:
        child = support.run([args.executable.resolve(strict=True), 'bed-state',
                             str(game).encode('utf-8').hex(),
                             str(resources).encode('utf-8').hex()], output, 'native')
        report['native'] = child.capture_report
        support.require(child.returncode == 0 and child.stderr == b'',
                        'Bed state native control failure')
        lines = child.stdout.splitlines()
        support.require(len(lines) == 5 and
                        lines[:4] == [b'16 achievements', b'151 recipes'] * 2,
                        'Unexpected Bed state constructor output')
        support.require(re.fullmatch(rb'Bed state: [1-9][0-9]* checks', lines[4]) is not None,
                        'Missing Bed state completion')
        report['status'] = 'PASS'
        report['completion'] = lines[4].decode('ascii')
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(report['completion'] + '; capture=' + str(output))


if __name__ == '__main__':
    main()
