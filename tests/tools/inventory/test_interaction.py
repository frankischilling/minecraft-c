"""Run source Item-name and Container controls with authored resources."""
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
    parser.add_argument('--kind', choices=('item-name', 'container'), required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=args.kind + '-', dir=args.work_dir.resolve()))
    path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('inventory_authored_resources', path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    command = [str(args.executable.resolve(strict=True)), str(resources)]
    if args.kind == 'item-name':
        command.append(str(game))
    child = subprocess.run(command, capture_output=True, timeout=180)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    print(child.stdout.decode('utf-8'), end='')
    print(child.stderr.decode('utf-8'), end='', file=sys.stderr)
    marker = b'404 original scalar cases' if args.kind == 'item-name' else b'Container interaction kernel controls passed:'
    if child.returncode or marker not in child.stdout:
        raise RuntimeError('Native ' + args.kind + ' controls failed: ' + str(output))


if __name__ == '__main__':
    main()
