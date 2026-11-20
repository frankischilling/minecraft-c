"""Run Dispenser inventory controls with authored Registry resources."""
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
    output = Path(tempfile.mkdtemp(prefix='dispenser-', dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('dispenser_resources', support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / 'resources'
    support.resources_at(resources)
    command = [str(args.executable.resolve(strict=True)), str(resources)]
    child = subprocess.run(command, capture_output=True, timeout=220)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    print(child.stdout.decode('utf-8'), end='')
    print(child.stderr.decode('utf-8'), end='', file=sys.stderr)
    if child.returncode or child.stderr or b'Dispenser Container CPU controls passed:' not in child.stdout:
        raise RuntimeError('Dispenser Container controls failed: ' + str(output))


if __name__ == '__main__':
    main()
