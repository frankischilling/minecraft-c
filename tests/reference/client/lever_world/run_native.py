"""Replay all sixteen Lever metadata states and texture override against original words."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import tempfile

from packet import read_packet

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('lever_inputs', ROOT / 'tests/tools/client/test_picking.py')
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='lever-world-', dir=args.work_dir.resolve()))
    for name, data in inputs.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs.authored_world(output / 'world')
    result = subprocess.run([str(args.executable.resolve(strict=True)), str(output / 'resources'),
                             str(output / 'world')], capture_output=True, timeout=90)
    (output / 'native.stdout').write_bytes(result.stdout)
    (output / 'native.stderr').write_bytes(result.stderr)
    assert result.returncode == 0 and not result.stderr, result.stderr.decode(errors='replace')
    actual = [line for line in result.stdout.decode('ascii').splitlines() if line.startswith('LEVER\t')]
    expected = read_packet()
    assert len(expected) == 32 and actual == expected, 'original Lever geometry rows differ'
    print('PASS: 32 exact Lever World geometry rows; ' + str(output))


if __name__ == '__main__':
    main()
