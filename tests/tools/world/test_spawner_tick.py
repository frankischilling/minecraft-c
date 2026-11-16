"""Run finite genuine World.g Spawner controls; no original entropy comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
CASES = ('inactive', 'countdown', 'unknown', 'nonliving', 'rejected', 'cap6',
         'four-success', 'growth-cap', 'publication-fail', 'smoke-fail', 'flame-fail',
         'explode-fail', 'null-output', 'factory-limit', 'later-factory-limit',
         'predicate-math-fail')
EXPECTED = ((0, 0, 0, 0, 0, 0, 0), (0, 0, 0, 0, 0, 0, 0),
            (1, 1, 0, 0, 0, 1, 0), (1, 1, 0, 0, 0, 0, 1),
            (4, 4, 0, 0, 0, 0, 0), (1, 1, 0, 0, 1, 0, 0),
            (4, 4, 4, 4, 0, 0, 0), (3, 3, 2, 2, 1, 0, 0),
            (1, 1, 1, 0, 0, 0, 1), (1, 1, 1, 1, 0, 0, 1),
            (1, 1, 1, 1, 0, 0, 1), (1, 1, 1, 1, 0, 0, 1),
            (0, 0, 0, 0, 0, 0, 1), (1, 1, 0, 0, 0, 0, 1),
            (2, 2, 1, 1, 0, 0, 1), (1, 1, 0, 0, 0, 0, 1))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def input_files():
    repo = Path(__file__).resolve().parents[3]
    selected = {repo / path for path in (
        'CMakeLists.txt', 'tests/tools/world/test_spawner_tick.py',
        'tests/world/runtime/spawner/test_spawner_tick.c',
        'tests/world/runtime/spawner/fixture.inc',
        'tests/reference/world/spawner_tick/native-invariants.h',
        'tests/world/runtime/collision/test_boxes.c',
        'cmake/client_spawner.cmake', 'cmake/entity_living_view.cmake',
        'include/mc/world/runtime/entity_tick.h',
        'include/mc/world/runtime/spawner_observation.h',
        'include/mc/world/runtime/tile_entities/spawner_tick.h',
        'include/mc/entity/living/spawn_effect.h',
        'src/world/runtime/entity_tick.c', 'src/world/runtime/entity_tick_spawner.c',
        'src/world/runtime/entity_tick_tiles.c', 'src/world/runtime/entity_tick_internal.h',
        'src/world/runtime/entity_tick_math_internal.h',
        'src/world/runtime/tile_entities/tile_entities.c',
        'src/world/runtime/tile_entities/tick_internal.h',
        'src/world/runtime/tile_entities/tile_entities_internal.h',
        'src/world/runtime/collision/class_entities.c',
        'src/world/runtime/collision/entities.c',
        'src/world/runtime/entity_publication.c', 'src/world/runtime/presentation.c',
        'src/world/runtime/world_access.c', 'src/entity/factory/factory.c',
        'src/entity/living/spawn_effect.c', 'src/entity/mob/spawning.c',
        'src/entity/mob/spawn_failure_internal.h', 'src/entity/bounds/bounds.c',
        'src/entity/player/construction.c', 'src/entity/construction_internal.h')}
    return {str(path.relative_to(repo)).replace('\\', '/'): digest(path)
            for path in sorted(selected)}


def receipts(stdout, stderr):
    require(type(stdout) is str and stderr == '', 'Clean actual native Spawner transport')
    lines = stdout.splitlines()
    selected = [line for line in lines if line.startswith('SPAWNER_')]
    require(len(selected) == len(CASES), 'Exactly the declared native Spawner cases')
    rows = []
    for index, line in enumerate(selected):
        match = re.fullmatch(r'SPAWNER_TICK case=([a-z0-9-]+) attempts=([0-4]) factory=([0-4]) '
                             r'accepted=([0-4]) inserted=([0-4]) caps=([01]) nulls=([01]) failed=([01]) cleanup=1', line)
        require(match is not None and match[1] == CASES[index], 'Ordered complete native case receipt')
        values = tuple(map(int, match.groups()[1:]))
        require(values == EXPECTED[index], 'Exact actual native branch and checked cleanup')
        rows.append({'case': match[1], 'values': list(values)})
    markers = [line for line in lines if line.startswith('PASS: Spawner tick')]
    require(len(markers) == 1, 'Unique actual Spawner success marker')
    marker = re.fullmatch(r'PASS: Spawner tick; cases=([1-9][0-9]*); checks=([1-9][0-9]{0,9}); cleanup=1; '
                          r'source-field-repair=0; original-entropy=0', markers[0])
    require(marker is not None and int(marker[1]) == len(CASES) and
            int(marker[2]) <= 4294967295 and lines[-1] == markers[0],
            'Complete final native uint32 check count and ownership result')
    require(lines.index(selected[-1]) < lines.index(markers[0]), 'All cases precede native completion')
    banners = [line for line in lines if line not in selected and line != markers[0]]
    require(banners == ['16 achievements', '151 recipes'] * len(CASES),
            'Exact ordered Registry banners; no unknown or blank stdout records')
    return {'cases': rows, 'checks': int(marker[2])}


def controls():
    rows = []
    for name, values in zip(CASES, EXPECTED):
        rows.append(('SPAWNER_TICK case=%s attempts=%d factory=%d accepted=%d inserted=%d '
                     'caps=%d nulls=%d failed=%d cleanup=1') % (name, *values))
    valid = '\n'.join(['16 achievements', '151 recipes'] * len(CASES) + rows + [
        'PASS: Spawner tick; cases=%d; checks=390; cleanup=1; source-field-repair=0; original-entropy=0' % len(CASES)]) + '\n'
    require(receipts(valid, '')['checks'] == 390, 'Valid synthetic transport control')
    variants = [valid + 'unexpected\n', 'unexpected\n' + valid, valid + '\n',
                valid.replace(rows[0] + '\n', ''), valid.replace(rows[0], rows[1]),
                valid.replace(rows[0], rows[0] + '\n' + rows[0]),
                valid.replace('checks=390', 'checks=0'),
                valid.replace('checks=390', 'checks=4294967296'),
                valid.replace('cleanup=1', 'cleanup=0'),
                valid.replace('source-field-repair=0', 'source-field-repair=1'),
                valid.replace('original-entropy=0', 'original-entropy=1'),
                valid.replace('16 achievements\n', '', 1),
                valid.replace('151 recipes\n', '152 recipes\n', 1),
                valid.replace('attempts=4', 'attempts=3', 1),
                valid.replace(rows[2] + '\n' + rows[3], rows[3] + '\n' + rows[2])]
    for index, text in enumerate(variants):
        require(text != valid, 'Corruption control changes its transport: %d' % index)
        try:
            receipts(text, '')
        except RuntimeError:
            continue
        raise RuntimeError('Corrupt synthetic transport was accepted: %d' % index)
    try:
        receipts(valid, 'unexpected stderr\n')
    except RuntimeError:
        pass
    else:
        raise RuntimeError('Nonempty synthetic stderr was accepted')
    print('Spawner transport controls passed: %d corruptions rejected' % (len(variants) + 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--controls', action='store_true')
    args = parser.parse_args()
    if args.controls:
        require(args.executable is None and args.work_dir is None, 'Controls accept no native inputs')
        return controls()
    require(args.executable is not None and args.work_dir is not None,
            'Native executable and work directory are required')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='spawner-tick-', dir=args.work_dir.resolve()))
    root = evidence / 'worlds'
    command = [str(executable), 'world-spawner-tick', str(root).encode('utf8').hex()]
    before = input_files()
    product = digest(executable)
    report = {'status': 'FAIL', 'command': command,
              'scope': '%d separate genuine GAME World.g calls with authored Region NBT and initial Random.setSeed224; no desktop, original entropy or whole-game parity qualification.' % len(CASES),
              'normal_false_insertion': 'Not dynamically forced: stable Pig Grass lookup has already acquired its Chunk before nonloading publication presence.',
              'selected_inputs': before, 'executable': product}
    stdout = stderr = b''
    try:
        child = subprocess.run(command, capture_output=True, check=False, timeout=220)
        stdout, stderr = child.stdout, child.stderr
        report['exit_code'] = child.returncode
        require(child.returncode == 0, 'Native Spawner controls returned zero')
        report['native'] = receipts(stdout.decode('utf8'), stderr.decode('utf8'))
        report['inputs_unchanged'] = before == input_files() and product == digest(executable)
        require(report['inputs_unchanged'], 'Selected Source and executable stayed unchanged')
        report['status'] = 'PASS'
    except subprocess.TimeoutExpired as error:
        stdout, stderr = error.stdout or b'', error.stderr or b''
        report['timeout'] = True
        report['error'] = 'Native Spawner control timeout'
        raise
    except BaseException as error:
        report['error'] = str(error)
        raise
    finally:
        (evidence / 'stdout.txt').write_bytes(stdout)
        (evidence / 'stderr.txt').write_bytes(stderr)
        report['stdout'] = digest(evidence / 'stdout.txt')
        report['stderr'] = digest(evidence / 'stderr.txt')
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print(stdout.decode('utf8', errors='replace'), end='')
        print(stderr.decode('utf8', errors='replace'), end='', file=sys.stderr)
        print('Spawner tick artifacts: ' + str(evidence))


if __name__ == '__main__':
    main()
