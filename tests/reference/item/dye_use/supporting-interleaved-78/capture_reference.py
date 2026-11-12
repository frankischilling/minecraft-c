"""Optionally replay the exact frozen authored driver from a new private task directory."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p / 'CMakeLists.txt').is_file())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.parent != ROOT / '.local' or output.exists():
        raise ValueError('Choose a new direct child of this workspace .local directory')
    output.mkdir()
    for name in ('DyeObservation.java', 'profiles.tsv', 'declared-inputs.json', 'source-pins.json'):
        shutil.copy2(HERE / name, output / name)
    shutil.copy2(HERE / 'original-capture-driver.py', output / 'capture.py')
    return subprocess.run([sys.executable, '-B', str(output / 'capture.py'), '--attempt', '1'], cwd=ROOT).returncode


if __name__ == '__main__':
    raise SystemExit(main())
