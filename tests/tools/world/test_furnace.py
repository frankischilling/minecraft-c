"""Run portable actual Furnace controls with authored Registry resources."""
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
    parser.add_argument('--mode', choices=('core', 'inventory', 'tick', 'output', 'lifecycle'), required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='furnace-', dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('furnace_resources', support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / 'resources'
    support.resources_at(resources)
    executable = str(args.executable.resolve(strict=True))
    if args.mode == 'core':
        command = [executable, str(resources), str(output / 'world')]
        marker = b'Furnace core source/ownership checks'
    elif args.mode == 'inventory':
        command = [executable, str(resources)]
        marker = b'Furnace backend controls passed:'
    elif args.mode == 'output':
        work = output / 'work'
        work.mkdir()
        command = [executable, str(resources), str(work)]
        marker = b'Player Furnace output:'
    elif args.mode == 'lifecycle':
        command = [executable, 'furnace-lifecycle-reference', str(output / 'world').encode('utf-8').hex()]
        marker = b'7 original Furnace World post-step projections; native cleanup=1; phase-trace-parity=0'
    else:
        command = [executable, 'world-furnace-tick', str(output / 'world').encode('utf-8').hex()]
        marker = b'actual World Furnace tick, same-Tile and deferred-list checks'
    child = subprocess.run(command, capture_output=True, timeout=220)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    print(child.stdout.decode('utf-8'), end='')
    print(child.stderr.decode('utf-8'), end='', file=sys.stderr)
    if child.returncode or child.stderr or marker not in child.stdout:
        raise RuntimeError('Furnace ' + args.mode + ' controls failed: ' + str(output))


if __name__ == '__main__':
    main()
