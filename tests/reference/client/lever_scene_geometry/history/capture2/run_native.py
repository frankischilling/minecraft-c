"""Replay the precise scene with AO/flat bases and source translation against original words."""
import argparse
import importlib.util
from pathlib import Path
import re
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
    output = Path(tempfile.mkdtemp(prefix='lever-scene-', dir=args.work_dir.resolve()))
    for name, data in inputs.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs.authored_world(output / 'world')
    result = subprocess.run([str(args.executable.resolve(strict=True)), str(output / 'resources'),
                             str(output / 'world'), str(HERE / 'scene-input.bin')], capture_output=True, timeout=90)
    (output / 'native.stdout').write_bytes(result.stdout)
    (output / 'native.stderr').write_bytes(result.stderr)
    assert result.returncode == 0 and not result.stderr, result.stderr.decode(errors='replace')
    actual = [line for line in result.stdout.decode('ascii').splitlines() if line.startswith('SCENE\t')]
    expected = list(read_packet().values())
    assert len(expected) == 160
    if actual != expected:
        first = next((index for index, pair in enumerate(zip(actual, expected))
                      if pair[0] != pair[1]), min(len(actual), len(expected)))
        detail = {'row': first, 'actual_rows': len(actual), 'expected_rows': len(expected)}
        if first < len(actual) and first < len(expected):
            a, b = actual[first].split('\t'), expected[first].split('\t')
            detail['actual_header'], detail['expected_header'] = a[:-1], b[:-1]
            aw, bw = a[-1].split(','), b[-1].split(',')
            differing = [(index, left, right) for index, (left, right) in enumerate(zip(aw, bw))
                         if left != right]
            detail['first_word_differences'] = differing[:8]
        raise AssertionError('original scene geometry differs: ' + str(detail))
    assert re.fullmatch(r'Lever scene geometry: [1-9][0-9]* checks; 160 original translated AO/flat rows',
                        result.stdout.decode('ascii').splitlines()[-1]), 'complete native cleanup marker'
    print('PASS: 160 exact translated scene geometry rows; ' + str(output))


if __name__ == '__main__':
    main()
