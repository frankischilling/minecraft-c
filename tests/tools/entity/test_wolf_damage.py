"""Execute actual-owner Wolf hurt, group and failure controls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='wolf-', dir=args.work_dir)).resolve()
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
    result = subprocess.run([str(executable), str(resources), str(world)],
                            capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    match = re.search(rb'PASS: ([1-9][0-9]*) Wolf damage source-order/ownership checks', result.stdout)
    after = hashlib.sha256(executable.read_bytes()).hexdigest()
    report = {'returncode': result.returncode, 'executable_before': before,
              'executable_after': after, 'checks': int(match[1]) if match else 0}
    report['status'] = ('PASS' if result.returncode == 0 and not result.stderr and match and
                        before == after else 'FAIL')
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(result.stdout.decode('utf-8'), end='')
    print(result.stderr.decode('utf-8'), end='')
    if report['status'] != 'PASS':
        raise RuntimeError('Native Wolf damage control failed: ' + str(output))
    print('Native Wolf damage PASS: ' + str(output))


if __name__ == '__main__':
    main()
