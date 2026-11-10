"""Run actual client Wolf/Slime/Squid/Ghast hurt calls and record their returns."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def exact(left, right):
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(exact(left[k], right[k]) for k in left)
    if type(left) is list:
        return len(left) == len(right) and all(exact(a, b) for a, b in zip(left, right))
    return left == right


def unique(pairs):
    value = {}
    for key, cell in pairs:
        if key in value:
            raise ValueError('Duplicate remaining-damage record key')
        value[key] = cell
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='hurt-', dir=args.work_dir)).resolve()
    resources, world = output / 'resources', output / 'world'
    world.mkdir()
    for name, data in {
        'lang/en_US.lang': (b'stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n'
                            b'stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n'),
        'lang/stats_US.lang': b'# Authored statistics resource\n', 'achievement/map.txt': b'',
    }.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    result = subprocess.run([str(executable), str(resources), str(world)], capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    print(result.stdout.decode('utf-8'), end='')
    print(result.stderr.decode('utf-8'), end='')
    match = re.search(rb'PASS: ([1-9][0-9]*) Remaining damage checks', result.stdout)
    if result.returncode or result.stderr or not match or before != hashlib.sha256(executable.read_bytes()).hexdigest():
        raise RuntimeError('Remaining Mob damage failed: ' + str(output))
    rows = [json.loads(line[len('REMAINING_DAMAGE '):], object_pairs_hook=unique)
            for line in result.stdout.decode('utf-8', errors='strict').splitlines()
            if line.startswith('REMAINING_DAMAGE ')]
    expected = Path(__file__).resolve().parents[2] / 'entity/mob/remaining_damage_expected.json'
    if not exact(rows, json.loads(expected.read_text(encoding='utf-8'), object_pairs_hook=unique)):
        raise RuntimeError('Exact ten-record remaining damage comparison failed')
    report = {'status': 'PASS', 'checks': int(match[1]), 'rows': rows, 'executable_sha256': before}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('Remaining Mob damage PASS: ' + str(output))


if __name__ == '__main__':
    main()
