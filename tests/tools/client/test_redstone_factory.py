"""Run Redstone particle publication or CPU draw/atlas-failure controls without GL."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--mode', choices=('factory', 'draw'), default='factory')
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='redstone-', dir=args.work_dir)).resolve()
    resources = output / 'resources'
    for name, data in {
        'lang/en_US.lang': (b'stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n'
                            b'stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n'),
        'lang/stats_US.lang': b'# Authored statistics resource\n',
        'achievement/map.txt': b'',
    }.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    world = output / 'game'
    world.mkdir()
    before = digest(executable)
    result = subprocess.run([str(executable), str(resources), 'game'], cwd=output,
                            capture_output=True, timeout=60)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    text = result.stdout.decode('utf-8', errors='strict')
    suffix = ('factory publication' if args.mode == 'factory' else
              'draw CPU prefix and atlas-failure')
    match = re.search(r'PASS: ([1-9][0-9]*) Redstone ' + suffix + r' checks\n',
                      text.replace('\r\n', '\n'))
    passed = (result.returncode == 0 and not result.stderr and match is not None and
              digest(executable) == before)
    report = {'status': 'PASS' if passed else 'FAIL', 'returncode': result.returncode,
              'mode': args.mode, 'executable_sha256': before,
              'checks': int(match[1]) if match else None}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(text, end='')
    print(result.stderr.decode('utf-8', errors='strict'), end='')
    print('Redstone factory capture: ' + str(output))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
