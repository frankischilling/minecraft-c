"""Run actual retained Mob death and execution-owned drop controls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('math-missing', 'profiles', 'remote', 'callback-1',
                                          'callback-2', 'callback-3', 'callback-4', 'native-stop',
                                          'admission'), required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='mob-death-owned-', dir=args.work_dir)).resolve()
    resources, world = output / 'resources', output / 'world'
    world.mkdir()
    for name, data in {
        'lang/en_US.lang': (b'stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n'
                            b'stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n'),
        'lang/stats_US.lang': b'# Authored statistics resource\n',
        'achievement/map.txt': b'',
    }.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    result = subprocess.run([str(executable), args.mode, str(resources), str(world)],
                            capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    stdout = result.stdout.decode('utf-8', errors='strict')
    lines = stdout.splitlines()
    footer = (re.fullmatch(r'PASS: ([1-9][0-9]*) execution-owned Mob death checks;mode=' +
                           re.escape(args.mode), lines[-1]) if lines else None)
    report = {'mode': args.mode, 'returncode': result.returncode, 'stderr_empty': not result.stderr,
              'executable_unchanged': hashlib.sha256(executable.read_bytes()).hexdigest() == before,
              'scope': 'Authored actual-owner controls; no original runtime comparison'}
    report['passed'] = bool(result.returncode == 0 and not result.stderr and footer and
                            report['executable_unchanged'])
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(stdout, end='')
    if result.stderr:
        print(result.stderr.decode('utf-8', errors='strict'), end='')
    if not report['passed']:
        raise RuntimeError('Native owned Mob death control failed: ' + str(output))


if __name__ == '__main__':
    main()
