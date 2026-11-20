"""Run checked Chest texture bodies over genuine authored World/Chunk owners."""
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
    root = Path(__file__).resolve().parents[3]
    spec = importlib.util.spec_from_file_location('chest_texture_inputs', root / 'tests/tools/client/test_picking.py')
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='chest-texture-', dir=args.work_dir.resolve()))
    for name, data in inputs.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs.authored_world(output / 'world')
    child = subprocess.run([str(args.executable.resolve(strict=True)), str(output / 'resources'),
                            str(output / 'world')], capture_output=True, timeout=120)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    print(child.stdout.decode('utf-8'), end='')
    print(child.stderr.decode('utf-8'), end='', file=sys.stderr)
    if child.returncode or b'Chest texture controls passed:' not in child.stdout:
        raise RuntimeError('Chest texture controls failed: ' + str(output))


if __name__ == '__main__':
    main()
