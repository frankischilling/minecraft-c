"""Compare fixed actual ally-alert retirement work on one compiled native core.

Authored saved Chunks and genuine duplicate/unique Wolf occurrences; no original
runtime, gameplay frame or FPS qualification. Independent constructor entropy
is checked for preservation within each real call, never equated across VMs.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import statistics
import struct
import subprocess
import sys
import tempfile
import zlib


ROOT = Path(__file__).resolve().parents[3]
VARIANTS = ('quadratic', 'candidate', 'fallback')
ADAPTERS = {'quadratic': 'quadratic', 'candidate': 'pointer-count', 'fallback': 'fallback'}
COUNTS = (1, 8, 64, 256, 1024)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def statistics_support():
    path = ROOT / 'tests/reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('ally_retire_statistics', path)
    require(spec is not None and spec.loader is not None, 'Actual statistics support unavailable')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def tag(kind, name, payload):
    name = name.encode('ascii')
    return bytes([kind]) + struct.pack('>H', len(name)) + name + payload


def authored_region(path):
    fields = [tag(3, 'xPos', bytes(4)), tag(3, 'zPos', bytes(4)),
              tag(4, 'LastUpdate', bytes(8))]
    for name, length in (('Blocks', 32768), ('Data', 16384), ('SkyLight', 16384),
                         ('BlockLight', 16384), ('HeightMap', 256)):
        fields.append(tag(7, name, struct.pack('>I', length) + bytes(length)))
    fields += [tag(1, 'TerrainPopulated', b'\1'), tag(9, 'Entities', b'\x0a' + bytes(4)),
               tag(9, 'TileEntities', b'\x0a' + bytes(4))]
    nbt = tag(10, '', tag(10, 'Level', b''.join(fields) + b'\0') + b'\0')
    payload = b'\2' + zlib.compress(nbt)
    record = struct.pack('>I', len(payload)) + payload
    require(len(record) <= 4096, 'One bounded actual saved-Chunk record')
    header = bytearray(8192)
    struct.pack_into('>I', header, 0, (2 << 8) | 1)
    path.parent.mkdir(parents=True)
    path.write_bytes(header + record + bytes(4096 - len(record)))


def build_binding(executables):
    directories = {path.parent for path in executables.values()}
    require(len(directories) == 1, 'All adapters must share one compiled core/build')
    directory = directories.pop()
    ninja = directory / 'build.ninja'
    require(ninja.is_file(), 'Actual Ninja link metadata required')
    lines = ninja.read_text(encoding='utf-8').splitlines()
    edges, objects, libraries = {}, {}, {}
    for variant, executable in executables.items():
        selected = [line for line in lines if line.startswith('build ' + executable.name + ': ')]
        require(len(selected) == 1, 'One actual executable link edge: ' + variant)
        edges[variant] = selected[0]
        explicit, implicit = selected[0].split(' | ', 1)
        objects[variant] = [directory / value.replace('\\', '/')
                            for value in explicit.split(': ', 1)[1].split()[1:]]
        libraries[variant] = [directory / value.replace('\\', '/')
                              for value in implicit.split(' || ', 1)[0].split()]
    require(all(libraries[name] == libraries['quadratic'] for name in VARIANTS),
            'Exact identical ordered core archive graph')
    shared = set.intersection(*(set(objects[name]) for name in VARIANTS))
    require(len(shared) == 1 and next(iter(shared)).name in
            ('player_ally_retire.c.obj', 'player_ally_retire.c.o') and
            all(len(objects[name]) == 2 for name in VARIANTS),
            'One same compiled observer plus one adapter per executable')
    commands = directory / 'compile_commands.json'
    metadata = [ninja]
    if commands.is_file():
        raw = commands.read_bytes()
        metadata.append(commands)
    else:
        rules = directory / 'CMakeFiles/rules.ninja'
        require(rules.is_file(), 'Actual Ninja compiler rules required')
        child = subprocess.run(['ninja', '-t', 'compdb'], cwd=directory,
                               stdin=subprocess.DEVNULL, capture_output=True, timeout=60)
        require(child.returncode == 0 and not child.stderr, 'Read-only compilation inspection')
        raw = child.stdout
        metadata.append(rules)
    names = {'ally_alert.c', 'ally_alert_occurrences.c', 'player_ally_retire.c',
             *(f'player_ally_retire_{variant}.c' for variant in VARIANTS)}
    compilation = [entry for entry in json.loads(raw) if Path(entry['file']).name in names]
    require(len(compilation) == 6, 'Exactly six production/observer/adapter compilation commands')
    products = sorted(set(metadata + libraries['quadratic'] +
                          [path for values in objects.values() for path in values]))
    return {'shared_observer': str(next(iter(shared))), 'link_edges': edges,
            'compilation': compilation, 'compilation_sha256': hashlib.sha256(raw).hexdigest(),
            'product_sha256': {str(path): digest(path) for path in products}}, products


def read_rows(path, variant):
    lines = path.read_text(encoding='ascii').splitlines()
    header = lines[0].split('\t') if lines else []
    require(len(lines) == 231 and len(header) == 5 and header[:2] ==
            ['ALLY_RETIRE_V2', ADAPTERS[variant]] and header[3:] == ['2', '21'],
            'Exact adapter and complete fixed-work native timing protocol')
    require(re.fullmatch('[1-9][0-9]*', header[2]) is not None, 'Positive actual clock frequency')
    frequency = int(header[2])
    rows = []
    for index, line in enumerate(lines[1:]):
        values = line.split('\t')
        mode = 'unique' if index < 115 else 'duplicate'
        count, sample = COUNTS[(index // 23) % 5], index % 23
        require(len(values) == 9 and values[:4] == ['RETIRE', mode, str(count), str(sample)] and
                all(re.fullmatch('[0-9]+', value) is not None for value in values[4:]),
                'Every native occurrence/interval in source order')
        timed, ticks, distinct, quadratic_gets, failures = map(int, values[4:])
        require(timed == int(sample >= 2) and distinct == (count if mode == 'unique' else 1) and
                quadratic_gets == count * (count + 1) and
                failures == int(variant == 'fallback' and count > 8),
                'Complete source work, multiplicity and actual optional-allocation fault')
        rows.append({'mode': mode, 'occurrences': count, 'sample': sample, 'timed': bool(timed),
                     'ticks': ticks, 'frequency': frequency, 'microseconds': ticks * 1e6 / frequency,
                     'distinct': distinct, 'derived_quadratic_gets': quadratic_gets,
                     'forced_failed_allocations': failures})
    return rows


def inputs(executables, products, resources, games):
    paths = {Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format',
             ROOT / 'tests/client/statistics/test_player_dispatch.c',
             ROOT / 'tests/reference/client/player_statistics/support.py',
             ROOT / 'tests/reference/client/statistics/support.py', *executables.values(), *products}
    for folder in ('src', 'include', 'cmake'):
        paths.update(path for path in (ROOT / folder).rglob('*') if path.is_file())
    paths.update((ROOT / 'tests/benchmarks').glob('player_ally_retire*'))
    paths.update(path for path in resources.rglob('*') if path.is_file())
    for game in games:
        paths.update(game.rglob('*.mcr'))
    for executable in executables.values():
        paths.update(executable.parent.glob('*.dll'))
    return {str(path): digest(path) for path in sorted(paths)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='VARIANT=PATH')
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    executables = {}
    for value in args.executable:
        variant, path = value.split('=', 1)
        require(variant in VARIANTS and variant not in executables, 'Three unique exact adapters')
        executables[variant] = Path(path).resolve(strict=True)
    require(set(executables) == set(VARIANTS), 'All three actual retirement adapters required')
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ally-retire-', dir=args.artifacts.resolve()))
    report = {'status': 'FAIL', 'scope': __doc__, 'original_rows': 0, 'processes': [],
              'host': platform.platform(), 'python': sys.version,
              'hardware': {'machine': platform.machine(), 'processor': platform.processor(),
                           'logical_processors': os.cpu_count()}}
    before = None
    try:
        binding, products = build_binding(executables)
        report['build_binding'] = binding
        support = statistics_support()
        resources = output / 'resources'
        support.resources_at(resources)
        schedule = [(round_index, VARIANTS[(offset + round_index) % 3])
                    for round_index in range(4) for offset in range(3)]
        games = []
        for index, (_, variant) in enumerate(schedule):
            game = output / f'game-{index}-{variant}'
            game.mkdir()
            games.append(game)
            for mode in ('unique', 'duplicate'):
                for count in COUNTS:
                    authored_region(game / f'{mode}-{count}/world/region/r.0.0.mcr')
        before = inputs(executables, products, resources, games)
        report['inputs_before_sha256'] = before
        environment = os.environ.copy()
        environment['ALSOFT_DRIVERS'] = 'null'
        all_rows = []
        for index, (round_index, variant) in enumerate(schedule):
            raw = output / f'timing-{index}-{variant}.tsv'
            record = {'round': round_index, 'variant': variant, 'raw_timing_file': raw.name}
            report['processes'].append(record)
            try:
                child = support.run([str(executables[variant]), str(resources), str(games[index]),
                                     str(raw)], output, f'native-{index}-{variant}',
                                    timeout=180, env=environment)
            except BaseException as error:
                if hasattr(error, 'capture_report'):
                    record['capture'] = error.capture_report
                raise
            record['capture'] = child.capture_report
            require(child.returncode == 0 and not child.stderr,
                    'Actual source/retirement fixture failed; preserve this opportunity')
            lines = child.stdout.splitlines()
            require(lines[:-1] == [b'16 achievements', b'151 recipes'] * 10 and
                    re.fullmatch(rb'Ally retire: [1-9][0-9]* checks; 10 actual Wolf workloads; '
                                 rb'210 measured retirements', lines[-1]) is not None,
                    'Every genuine World and checked-cleanup workload completed')
            record['actual_footer'] = lines[-1].decode('ascii')
            rows = read_rows(raw, variant)
            record['rows'] = rows
            all_rows.extend(dict(row, variant=variant, round=round_index) for row in rows)
        summaries = []
        for mode in ('unique', 'duplicate'):
            for count in COUNTS:
                row = {'mode': mode, 'occurrences': count}
                for variant in VARIANTS:
                    values = [v['microseconds'] for v in all_rows if v['timed'] and
                              (v['variant'], v['mode'], v['occurrences']) == (variant, mode, count)]
                    require(len(values) == 84, 'All four native fixed-work rounds')
                    row[variant] = {'samples': len(values), 'median_us': statistics.median(values),
                                    'min_us': min(values), 'max_us': max(values)}
                baseline = row['quadratic']['median_us']
                row['median_reduction_percent'] = (100 * (baseline - row['candidate']['median_us']) /
                                                   baseline) if baseline else None
                summaries.append(row)
        report['summary'] = summaries
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        if before is not None:
            try:
                after = inputs(executables, products, resources, games)
                report['inputs_after_sha256'] = after
                report['inputs_unchanged'] = before == after
                if before != after:
                    report['status'] = 'FAIL'
            except BaseException as error:
                report['status'] = 'FAIL'
                report['guard_error'] = {'type': type(error).__name__, 'message': str(error)}
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Ally retirement attempt retained: ' + str(output))
    require(report['status'] == 'PASS', 'Native benchmark inputs changed')
    print(json.dumps(report['summary']))


if __name__ == '__main__':
    main()
