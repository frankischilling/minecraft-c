"""Execute actual-owner fatal Monster controls without original assets."""
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


def expected_rows():
    """Finite deterministic observations; constructor and Item entropy stay independent."""
    result = []

    def row(kind, mode, source, remote, seed, random, ids, sounds):
        result.append({
            'profile': f'{kind}-{mode}-{source}-{"remote" if remote else "local"}-{seed}',
            'kind': kind, 'mode': mode, 'source': source, 'remote': remote, 'seed': seed,
            'result': True if mode == 'fatal' else None, 'dying': True, 'dead': False,
            'health': 0 if mode == 'fatal' else 4, 'random': random,
            'items': [[item, 1, 0, 10, True, 1] for item in ids], 'sounds': sounds, 'player': None})

    ordinary = ['0000bb20b4600a74', '0000bb2c7239d74e', '0000bb26934cf0e1']
    skeleton = ['0000d4d95138ab6f', '00004b1ab9f8c041', '00008ffa0598b5d8']
    for kind, item in [('uz', 288), ('cn', 287), ('fr', 262), ('gb', 289)]:
        for index, seed in enumerate((0, 2, 3)):
            ids = [[352], [262], [262, 262, 352, 352]][index] if kind == 'fr' else [item] * index
            row(kind, 'direct', 'null', False, seed,
                (skeleton if kind == 'fr' else ordinary)[index], ids, [])
    for kind, item, sound in [('uz', 288, 'mob.zombiedeath'), ('cn', 287, 'mob.spiderdeath'),
                               ('fr', 262, 'mob.skeletonhurt'), ('gb', 289, 'mob.creeperdeath')]:
        row(kind, 'fatal', 'cow', False, 38,
            '0000d7f39cd9ecff' if kind == 'fr' else '0000014ab00c9844',
            [262, 352] if kind == 'fr' else [item], [sound])
    for remote in (False, True):
        for index, seed in enumerate((0, 2)):
            row('gb', 'direct', 'skeleton', remote, seed,
                (ordinary if remote else skeleton)[index],
                [2257] if remote or seed == 0 else [289, 2256], [])
    return result


def returned_rows(stdout):
    def unique(pairs):
        value = {}
        for key, cell in pairs:
            if key in value:
                raise ValueError('Duplicate Monster death observation key')
            value[key] = cell
        return value

    prefix = 'MONSTER_DEATH '
    rows = [json.loads(line[len(prefix):], object_pairs_hook=unique)
            for line in stdout.splitlines() if line.startswith(prefix)]
    if not exact(rows, expected_rows()):
        raise ValueError('Exact twenty-row Monster death return/state/Item comparison failed')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
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
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    result = subprocess.run([str(executable), str(resources), str(world)],
                            capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    print(result.stdout.decode('utf-8'), end='')
    print(result.stderr.decode('utf-8'), end='')
    match = re.search(rb'PASS: ([1-9][0-9]*) Monster death source-order/ownership checks', result.stdout)
    after = hashlib.sha256(executable.read_bytes()).hexdigest()
    if result.returncode or result.stderr or before != after or not match:
        raise RuntimeError('Native Monster death control failed: ' + str(output))
    rows = returned_rows(result.stdout.decode('utf-8', errors='strict'))
    report = {'status': 'PASS', 'checks': int(match[1]), 'returncode': result.returncode,
              'executable_before': before, 'executable_after': after, 'returns': rows,
              'scope': 'Twenty finite deterministic records and authored failure/cleanup controls; '
                       'independent Item and constructor entropy excluded.'}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('Native Monster death PASS: ' + str(output))


if __name__ == '__main__':
    main()
