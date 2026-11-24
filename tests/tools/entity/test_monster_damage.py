"""Run authored actual-owner Monster damage and retaliation, mount and callback controls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def returned_rows(stdout):
    def unique(pairs):
        value = {}
        for key, cell in pairs:
            if key in value:
                raise ValueError('Duplicate Monster observation key')
            value[key] = cell
        return value

    prefix = 'MONSTER_DAMAGE_RETURN '
    rows = [json.loads(line[len(prefix):], object_pairs_hook=unique)
            for line in stdout.splitlines() if line.startswith(prefix)]
    names = ['mob.zombiehurt', 'mob.skeletonhurt', 'mob.creeper', 'mob.spider']
    integers = {'health', 'previous_health', 'resistance', 'last_damage', 'hurt', 'max_hurt'}
    booleans = {'result', 'attacked', 'target_is_source'}
    keys = integers | booleans | {'sound', 'angle', 'velocity'}
    if len(rows) != 4:
        raise ValueError('Four concrete Monster return observations required')
    for row, name in zip(rows, names):
        if (type(row) is not dict or set(row) != keys or row['sound'] != name or
                any(type(row[key]) is not int for key in integers) or
                any(type(row[key]) is not bool for key in booleans) or
                type(row['angle']) is not str or re.fullmatch('[0-9a-f]{8}', row['angle']) is None or
                type(row['velocity']) is not list or len(row['velocity']) != 3 or
                any(type(word) is not str or re.fullmatch('[0-9a-f]{16}', word) is None
                    for word in row['velocity'])):
            raise ValueError('Malformed concrete Monster return observation')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='monster-', dir=args.work_dir)).resolve()
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
    before = digest(executable)
    result = subprocess.run([str(executable), str(resources), str(world)],
                            capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    stdout = result.stdout.decode('utf-8', errors='strict')
    match = re.search(r'PASS: ([1-9][0-9]*) Monster damage source-order/ownership checks', stdout)
    report = {'returncode': result.returncode, 'executable_before': before,
              'executable_after': digest(executable),
              'scope': 'Authored actual-owner controls; original comparison has a separate packet'}
    report['status'] = ('PASS' if result.returncode == 0 and not result.stderr and match and
                        before == report['executable_after'] else 'FAIL')
    if match:
        report['checks'] = int(match[1])
    if report['status'] == 'PASS':
        report['returns'] = returned_rows(stdout)
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(stdout, end='')
    if result.stderr:
        print(result.stderr.decode('utf-8', errors='strict'), end='')
    if report['status'] != 'PASS':
        raise RuntimeError('Native Monster damage control failed: ' + str(output))
    print('Native Monster damage PASS: ' + str(output))


if __name__ == '__main__':
    main()
