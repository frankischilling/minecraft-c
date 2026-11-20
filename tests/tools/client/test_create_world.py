"""Authored Create World name/seed controls with real read-only SaveFormat input."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(root):
    return {path.relative_to(root).as_posix(): digest(path)
            for path in root.rglob('*') if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--work-dir', required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).with_name('test_source_world_selection.py')
    spec = importlib.util.spec_from_file_location('create_world_saved_metadata', source)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='model-', dir=args.work_dir.resolve()))
    root = output / 'input'
    (root / 'saves/World').mkdir(parents=True)
    (root / 'saves/World-').mkdir()
    (root / 'saves/empty_directory').mkdir()
    (root / 'font.txt').write_bytes(bytes(range(32, 127)) + b'\n')
    for folder, filename in (('World', 'level.dat'), ('World-', 'level.dat_old')):
        (root / 'saves' / folder / filename).write_bytes(
            gzip.compress(support.metadata(folder, 1304000000000, 19132), mtime=0))
    before, product = snapshot(root), digest(executable)
    result = subprocess.run([str(executable), str(root)], capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    match = re.search(r'PASS: ([1-9][0-9]*) Create World source-name/seed/model checks',
                      result.stdout.decode('utf-8', errors='strict'))
    report = {'returncode': result.returncode, 'checks': int(match[1]) if match else 0,
              'input_before': before, 'input_after': snapshot(root),
              'executable_before': product, 'executable_after': digest(executable),
              'scope': __doc__}
    passed = result.returncode == 0 and match and report['input_after'] == before and digest(executable) == product
    report['status'] = 'PASS' if passed else 'FAIL'
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(result.stdout.decode('utf-8'), end='')
    print(result.stderr.decode('utf-8'), end='', file=sys.stderr)
    if not passed:
        raise RuntimeError('Create World component control failed: ' + str(output))


if __name__ == '__main__':
    main()
