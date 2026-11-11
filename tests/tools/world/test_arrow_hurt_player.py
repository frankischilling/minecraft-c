"""Actual Arrow parent with real Player, application, Inventory and statistic owners."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('live', 'fatal', 'fatal-source', 'wake',
                                          'wake-source-null-list', 'wake-bed-foot',
                                          'wake-bed-foot-source'), required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]

    def load(name, relative):
        spec = importlib.util.spec_from_file_location(name, repo / relative)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    support = load('arrow_player_capture', 'tests/reference/client/statistics/support.py')
    resources = load('arrow_player_resources', 'tests/reference/client/player_statistics/support.py')
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='arrow-player-', dir=args.work_dir)).resolve()
    resource_root, game = out / 'resources', out / 'game'
    resources.resources_at(resource_root)
    game.mkdir()
    child = support.run_bounded(
        [str(executable), args.mode, str(resource_root), str(game)], out, 'native',
        timeout=240, limit=1024**2, env=os.environ.copy())
    lines = child.stdout.decode('utf-8', errors='strict').splitlines()
    footer = (re.fullmatch(r'PASS: ([1-9][0-9]*) actual Arrow Player hurt checks;mode=' +
                           re.escape(args.mode), lines[-1]) if lines else None)
    report = {'mode': args.mode, 'exit': child.returncode, 'capture': child.capture_report,
              'stderr_empty': child.stderr == b'',
              'executable_unchanged': hashlib.sha256(executable.read_bytes()).hexdigest() == before,
              'scope': 'Actual Arrow finite Player hit, fatal work and wake controls'}
    report['passed'] = bool(child.returncode == 0 and footer and not child.stderr and
                            report['executable_unchanged'])
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(child.stdout.decode('utf-8', errors='strict'), end='')
    if child.stderr:
        print(child.stderr.decode('utf-8', errors='strict'), end='')
    if not report['passed']:
        raise RuntimeError('Arrow Player hurt control failed: ' + str(out))


if __name__ == '__main__':
    main()
