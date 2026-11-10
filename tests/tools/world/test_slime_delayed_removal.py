"""Run a genuine twenty-one-tick Slime death against an asset-free cached World."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    spec = importlib.util.spec_from_file_location(
        'slime_delayed_capture', repo / 'tests/reference/client/statistics/support.py')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    executable = args.executable.resolve(strict=True)
    guards = (executable, Path(__file__).resolve(),
              repo / 'tests/world/runtime/test_slime_delayed_removal.c',
              repo / 'src/entity/mob/removal.c', repo / 'src/entity/mob/tick/tick.c',
              repo / 'src/entity/tick/environment.c')
    def pins():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in guards}
    before = pins()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='slime-delayed-', dir=args.work_dir)).resolve()
    world = out / 'worlds'
    world.mkdir()
    child = support.run_bounded([str(executable), str(world).encode('utf-8').hex()],
        out, 'native', timeout=240, limit=1024**2, env=os.environ.copy())
    lines = child.stdout.decode('utf-8', errors='strict').splitlines()
    expected = ['16 achievements', '151 recipes',
        'SLIME_DELAYED ticks=21 children=4 particles=20 valid=1 cleanup=1 passed=1']
    passed = child.returncode == 0 and not child.stderr and lines == expected and pins() == before
    report = {'passed': passed, 'exit': child.returncode, 'capture': child.capture_report,
              'before': before, 'after': pins(),
              'scope': 'Real fatal damage and21 complete public Mob ticks; legacy World calls'}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(child.stdout.decode('utf-8', errors='strict'), end='')
    if child.stderr:
        print(child.stderr.decode('utf-8', errors='strict'), end='')
    if not passed:
        raise RuntimeError('Slime delayed-removal control failed: ' + str(out))


if __name__ == '__main__':
    main()
