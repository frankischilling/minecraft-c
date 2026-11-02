"""Run real Item/Stack combat kernels and their retained owner controls."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='item-combat-', dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('item_combat_resources', support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    child = subprocess.run([str(args.executable.resolve(strict=True)), str(resources), str(game)],
                           capture_output=True, timeout=180)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    print(child.stdout.decode('utf-8'), end='')
    print(child.stderr.decode('utf-8'), end='', file=sys.stderr)
    if child.returncode or b'Item combat source-order/ownership checks' not in child.stdout:
        raise RuntimeError(f'Native Item combat failed: {output}')


if __name__ == '__main__':
    main()
