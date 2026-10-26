"""Real App/Writer dispatch through travel, jump, fall and damage callsites."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    helper = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('player_callsite_support', helper)
    if spec is None or spec.loader is None:
        raise RuntimeError('Player statistic fixture support unavailable')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='player-callsites-', dir=args.work_dir.resolve()))
    resources, game = out / 'resources', out / 'game'
    support.resources_at(resources)
    game.mkdir()
    result = support.run([args.executable.resolve(strict=True), resources, game], out, 'native')
    support.require(result.returncode == 0 and result.stderr == b'', 'Actual Player callsite failure')
    lines = result.stdout.splitlines()
    support.require(lines[:2] == [b'16 achievements', b'151 recipes'] and len(lines) == 3,
                    'Unexpected callsite profile output')
    checks = re.fullmatch(rb'Player statistic callsites: ([1-9][0-9]*) checks', lines[2])
    support.require(checks is not None, 'Missing completed callsite checks')
    report = dict(status='PASS', checks=int(checks[1]), capture=result.capture_report,
                  scope='Native genuine App/Writer jump, movement/travel/fall completion and exact hurt/null prefixes')
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Player statistic callsites: ' + str(report['checks']) + ' checks; report=' + str(out / 'report.json'))


if __name__ == '__main__':
    main()
