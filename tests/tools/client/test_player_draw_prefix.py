"""Run genuine TH/DS/Player prefix controls without a Display."""
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
    output = Path(tempfile.mkdtemp(prefix='player-draw-prefix-', dir=args.work_dir.resolve()))
    root = Path(__file__).resolve().parents[3]
    spec = importlib.util.spec_from_file_location('player_draw_resources',
        root / 'tests/reference/client/player_statistics/support.py')
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
    if child.returncode or b'Player draw prefix controls; no Display or whole-preview claim' not in child.stdout:
        raise RuntimeError(f'Native Player draw prefix controls failed: {output}')


if __name__ == '__main__':
    main()
