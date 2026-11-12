"""Run fluid geometry controls with the existing authored World fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from test_picking import RESOURCES, authored_world


def inventory(root):
    return {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(root.rglob('*')) if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='fluid-', dir=args.work_dir.resolve()))
    for name, data in RESOURCES.items():
        path = work / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    authored_world(work / 'world')
    before = inventory(work)
    result = subprocess.run([str(executable), str(work / 'resources'), str(work / 'world')],
                            capture_output=True, timeout=120, check=False)
    after = inventory(work)
    (work / 'stdout.txt').write_bytes(result.stdout)
    (work / 'stderr.txt').write_bytes(result.stderr)
    # The source SaveHandler creates its own ordinary session token.
    stable = all(after.get(name) == value for name, value in before.items())
    report = {'returncode': result.returncode, 'inputs_unchanged': stable,
              'before': before, 'after': after}
    (work / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(result.stdout.decode('utf-8'), end='')
    print(result.stderr.decode('utf-8'), end='', file=sys.stderr)
    complete = re.search(rb'Source fluid: [1-9][0-9]* checks; homogeneous water/lava geometry, '
                         rb'hidden faces and pool effects\r?\n?$', result.stdout) is not None
    if result.returncode != 0 or result.stderr or not stable or not complete:
        raise RuntimeError('Fluid geometry control failed: ' + str(work))


if __name__ == '__main__':
    main()
