"""Check armor aggregation, aliased wear, failure prefixes and signed wrap."""
import argparse
import importlib.util
import re
from pathlib import Path
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    support_path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('armor_resources', support_path)
    if spec is None or spec.loader is None:
        raise RuntimeError('Player statistic fixture support unavailable')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='armor-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    child = support.run([args.executable.resolve(strict=True), resources, game], output, 'native')
    support.require(child.returncode == 0 and child.stderr == b'', 'Armor native control failure')
    lines = child.stdout.splitlines()
    support.require(len(lines) == 3 and lines[:2] == [b'16 achievements', b'151 recipes'],
                    'Unexpected armor constructor output')
    support.require(re.fullmatch(rb'Armor ownership/arithmetic: [1-9][0-9]* checks', lines[2]) is not None,
                    'Missing completed armor controls')
    print(lines[2].decode('ascii') + '; capture=' + str(output))


if __name__ == '__main__':
    main()
