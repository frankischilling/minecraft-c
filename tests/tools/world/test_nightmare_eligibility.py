"""Native retained eligibility ownership controls; no original execution claim."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/tools/entity'))
from test_mob_spawning import RESOURCES, authored_world


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='eligibility-', dir=args.work_dir.resolve()))
    resources, world = output / 'resources', output / 'world'
    for name, value in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)
    authored_world(world)
    executable = args.executable.resolve(strict=True)
    inputs = {executable, Path(__file__).resolve(),
              ROOT / 'tests/world/runtime/test_nightmare_eligibility.c',
              ROOT / 'tests/entity/test_mob_spawning.c',
              ROOT / 'tests/tools/entity/test_mob_spawning.py',
              *resources.rglob('*.*'), *world.rglob('*.mcr')}
    for directory in ('src', 'include'):
        inputs.update(path for path in (ROOT / directory).rglob('*')
                      if path.suffix in ('.c', '.h', '.inc'))
    before = {str(path): digest(path) for path in sorted(inputs)}
    child = subprocess.run([str(executable), str(resources), str(world)],
                           stdin=subprocess.DEVNULL, capture_output=True, timeout=120)
    (output / 'native.stdout').write_bytes(child.stdout)
    (output / 'native.stderr').write_bytes(child.stderr)
    lines = child.stdout.splitlines()
    complete = lines and re.fullmatch(rb'Nightmare eligibility: ([1-9][0-9]*) checks', lines[-1])
    after = {str(path): digest(path) for path in sorted(inputs)}
    passed = (child.returncode == 0 and not child.stderr and complete and
              all(line in (b'16 achievements', b'151 recipes') for line in lines[:-1]) and
              before == after)
    report = {
        'status': 'PASS' if passed else 'FAIL', 'checks': int(complete[1]) if complete else 0,
        'input_before_sha256': before, 'input_after_sha256': after,
        'scope': ('Actual Mob/World saved-light failure and post-query Random-null tag controls; '
                  'authored returned-child envelopes for six loader diagnostic projections, '
                  'sixteen real source callback receipt placements, pending retain ceiling/repair, '
                  'native policy versus inactive/mismatched diagnostics and checked retirement. '
                  'These envelopes do not claim complete CQ, loader/activation or original-JAR execution.')}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not passed:
        raise RuntimeError('Eligibility controls failed; inspect ' + str(output))
    print(lines[-1].decode('ascii') + '; capture=' + str(output))


if __name__ == '__main__':
    main()
