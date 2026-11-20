"""Execute the authored canonical FallingSand tick and ownership controls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='falling-tick-', dir=args.work_dir.resolve()))
    root = Path(__file__).resolve().parents[3]
    inputs = [executable, Path(__file__).resolve(),
              root / 'tests/entity/falling_sand/test_tick.c',
              root / 'src/entity/falling_sand/tick.c',
              root / 'src/entity/falling_sand/tick_internal.h',
              root / 'include/mc/entity/falling_sand/tick.h']
    before = {str(path): digest(path) for path in inputs}
    result = subprocess.run([str(executable), 'falling-tick',
                             str(output / 'worlds').encode('utf-8').hex()],
                            capture_output=True, timeout=150, check=False)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    after = {str(path): digest(path) for path in inputs}
    text = result.stdout.decode('utf-8')
    match = re.search(r'PASS: ([1-9][0-9]*) FallingSand tick checks\r?\n?$', text)
    passed = result.returncode == 0 and not result.stderr and bool(match) and before == after
    report = {'status': 'PASS' if passed else 'FAIL', 'returncode': result.returncode,
              'inputs_before': before, 'inputs_after': after,
              'checks': int(match[1]) if match else None,
              'scope': 'Authored native actual-owner controls; no executed-original comparison'}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(text, end='')
    print(result.stderr.decode('utf-8'), end='', file=sys.stderr)
    if not passed:
        raise RuntimeError('FallingSand tick control failed: ' + str(output))


if __name__ == '__main__':
    main()
