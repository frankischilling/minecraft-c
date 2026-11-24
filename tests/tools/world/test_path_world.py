"""Run real direct-World PathFinder reuse and failure ownership controls."""
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
from test_mob_tick import authored_world
from test_mob_construction import RESOURCES


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1048576), b''):
            result.update(block)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='path-world-', dir=args.work_dir.resolve()))
    resources, worlds = output / 'resources', output / 'worlds'
    for name, value in RESOURCES.items():
        target = resources / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(value)
    for mode in range(4):
        authored_world(worlds / ('case' + str(mode)))
    executable = args.executable.resolve(strict=True)
    inputs = {executable, Path(__file__).resolve(),
              ROOT / 'tests/world/runtime/test_path_world.c',
              ROOT / 'tests/world/runtime/test_path.c',
              ROOT / 'tests/world/runtime/test_world_random_tick.c',
              ROOT / 'tests/tools/entity/test_mob_tick.py',
              ROOT / 'tests/tools/entity/test_mob_construction.py',
              *resources.rglob('*.*'), *worlds.rglob('*.mcr')}
    for name in ('src', 'include'):
        inputs.update(path for path in (ROOT / name).rglob('*') if path.suffix in ('.c', '.h', '.inc'))
    before = {str(path): digest(path) for path in sorted(inputs)}
    child = subprocess.run([str(executable), str(resources), str(worlds)],
                           stdin=subprocess.DEVNULL, capture_output=True, timeout=120)
    (output / 'native.stdout').write_bytes(child.stdout)
    (output / 'native.stderr').write_bytes(child.stderr)
    lines = child.stdout.splitlines()
    complete = lines and re.fullmatch(rb'Direct World paths: ([1-9][0-9]*) checks', lines[-1])
    after = {str(path): digest(path) for path in sorted(inputs)}
    passed = (child.returncode == 0 and not child.stderr and complete and
              all(line in (b'16 achievements', b'151 recipes') for line in lines[:-1]) and before == after)
    report = {'status': 'PASS' if passed else 'FAIL', 'checks': int(complete[1]) if complete else 0,
              'input_before_sha256': before, 'input_after_sha256': after,
              'scope': 'Four authored native World/FW scenarios; same-finder searches, stable retained Path/Point graph, no EW construction, actual direct block/metadata query failures and checked cleanup retry. An authored borrowed-loader failure envelope separately proves capture/retirement at exhausted byte/allocation budgets. No original-comparison credit.'}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not passed:
        raise RuntimeError('Direct World path controls failed; inspect ' + str(output))
    print(lines[-1].decode('ascii') + '; capture=' + str(output))


if __name__ == '__main__':
    main()
