"""Authored Player death, scatter ordering, failure ownership and K controls."""
import argparse
import importlib.util
from pathlib import Path
import re
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    helper = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('player_death_resources', helper)
    if spec is None or spec.loader is None:
        raise RuntimeError('Player statistics resource helper unavailable')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='player-death-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    child = support.run([args.executable.resolve(strict=True), resources, game], output, 'native')
    support.require(child.returncode == 0 and child.stderr == b'', 'Player death native control failure')
    lines = child.stdout.splitlines()
    support.require(lines[:18] == [b'16 achievements', b'151 recipes'] * 9 and len(lines) == 19,
                    'Unexpected Player death constructor output')
    support.require(re.fullmatch(rb'Player death/scatter: [1-9][0-9]* checks', lines[18]) is not None,
                    'Missing Player death completion')
    print(lines[18].decode('ascii') + '; capture=' + str(output))


if __name__ == '__main__':
    main()
