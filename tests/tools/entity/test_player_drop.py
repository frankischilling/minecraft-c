"""Run actual Entity action owners against declared component inputs, without original assets."""
import argparse
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--action', choices=('drop', 'mount'), default='drop')
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    label = 'Player drop' if args.action == 'drop' else 'Entity mount'
    output = Path(tempfile.mkdtemp(prefix=args.action + '-', dir=args.work_dir.resolve()))
    source = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('drop_statistics_resources', source)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / 'resources'
    support.resources_at(resources)
    game = output / 'game'
    game.mkdir()
    before = digest(executable)
    result = subprocess.run([str(executable), str(resources), str(game)],
                            capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    match = re.search(r'PASS: ([1-9][0-9]*) ' + re.escape(label) + r' source-order/ownership checks',
                      result.stdout.decode('utf-8', errors='strict'))
    report = {'returncode': result.returncode, 'executable_before': before,
              'executable_after': digest(executable),
              'scope': 'Authored actual-owner controls; no original runtime comparison'}
    report['status'] = ('PASS' if result.returncode == 0 and match and
                        before == report['executable_after'] else 'FAIL')
    if match:
        report['checks'] = int(match[1])
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(result.stdout.decode('utf-8'), end='')
    if result.stderr:
        print(result.stderr.decode('utf-8'), end='', file=sys.stderr)
    if report['status'] != 'PASS':
        raise RuntimeError('Native ' + label + ' control failed: ' + str(output))
    print('Native ' + label + ' PASS: ' + str(output))


if __name__ == '__main__':
    main()
