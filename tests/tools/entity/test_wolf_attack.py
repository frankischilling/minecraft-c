"""Run the actual protected Wolf attack body with authored component inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from test_mob_construction import RESOURCES


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='wolf-attack-', dir=args.work_dir.resolve()))
    resources, worlds = work / 'resources', work / 'worlds'
    worlds.mkdir()
    for name, content in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    root = Path(__file__).resolve().parents[3]
    paths = [executable, Path(__file__).resolve(), Path(__file__).with_name('test_mob_construction.py'),
             root / 'tests/entity/test_wolf_attack.c', root / 'tests/support/java_random.h',
             root / 'tests/world/runtime/test_world_random_tick.c',
             root / 'src/entity/mob/tick/wolf_attack.c', root / 'src/entity/mob/tick/tick.c',
             root / 'src/entity/mob/tick/internal.h']
    paths += sorted(path for path in resources.rglob('*') if path.is_file())
    before = {str(path): digest(path) for path in paths}
    result = subprocess.run([str(executable), str(resources), str(worlds)],
                            capture_output=True, timeout=150, check=False)
    (work / 'stdout.txt').write_bytes(result.stdout)
    (work / 'stderr.txt').write_bytes(result.stderr)
    after = {str(path): digest(path) for path in paths}
    text = result.stdout.decode('utf-8')
    match = re.search(r'PASS: ([1-9][0-9]*) Wolf attack source-component checks\r?\n?$', text)
    passed = result.returncode == 0 and not result.stderr and bool(match) and before == after
    report = {'status': 'PASS' if passed else 'FAIL', 'returncode': result.returncode,
              'inputs_before': before, 'inputs_after': after,
              'checks': int(match[1]) if match else None,
              'scope': 'Authored native Wolf component; original execution is separate'}
    (work / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(text, end='')
    print(result.stderr.decode('utf-8'), end='', file=sys.stderr)
    if not passed:
        raise RuntimeError('Wolf attack control failed: ' + str(work))


if __name__ == '__main__':
    main()
