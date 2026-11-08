"""Retain a four-way fixed-work FD.g diagnostic CPU comparison on one native core.

Legacy diagnostics, World-tail only, Mob workspace only, and both changes use
one compiled genuine workload and identical ordered core archives. This is a
native component CPU scope; no gameplay-frame, FPS or original timing claim.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import shlex
import statistics
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
VARIANTS = ('legacy', 'tail', 'workspace', 'both')
CELLS = (('empty', 'empty', 0), ('pig1', 'pig', 1), ('pig8', 'pig', 8),
         ('pig64', 'pig', 64), ('zombie1', 'zombie', 1),
         ('zombie8', 'zombie', 8), ('zombie64', 'zombie', 64))
SIZES = ('world_error', 'mob_error', 'effect_error', 'environment_error',
         'action_error', 'world_scope', 'mob_scope', 'mob_child_workspace')
DIGESTS = ('semantic_before', 'semantic_after', 'raw_before', 'raw_after',
           'rng_before', 'rng_after', 'chunks_before', 'chunks_after',
           'ownership_before', 'ownership_after', 'constructor_before', 'constructor_after')
INVARIANTS = ('uniquifier_unchanged', 'world_rng_unchanged', 'player_unchanged')
PAIRED = ('semantic_before', 'semantic_after', 'rng_before', 'rng_after',
          'chunks_before', 'chunks_after', 'ownership_before', 'ownership_after')
HEADER_KEYS = {'event', 'schema', 'variant', 'clock', 'frequency', 'ticks_per_interval',
               'warmups', 'samples', 'fixture_chunks', 'digest_policy',
               'entropy_exclusions', 'sizeof', 'world_finalization', 'mob_dispatch'}
ROW_KEYS = {'event', 'cell', 'kind', 'count', 'sample', 'warmup', 'ticks', 'returns',
            'start', 'end', 'status', 'no_hurt', 'hurt_reached', 'hurt_returned', 'hurt_true',
            'actor_ticks_before', 'actor_ticks_after', 'checks', 'constructor_unchanged',
            'dispatch_calls', 'workspace_calls',
            *DIGESTS, *INVARIANTS}
FOOTER_KEYS = {'event', 'complete', 'rows', 'cleanup', 'checks'}
MAX_RAW_BYTES = 8 * 1024 * 1024
PROCESS_SECONDS = 600
ROLE_ALGORITHMS = {'legacy': ('legacy', 'legacy'), 'tail': ('candidate', 'legacy'),
                   'workspace': ('legacy', 'candidate'), 'both': ('candidate', 'candidate')}
ALGORITHM_SCHEMA = 'MOB_DIAGNOSTIC_CAUSAL_ADAPTERS_V1'
LEGACY_COMMIT = '2d9ea870c8be4566d02328e46813cc020743f78e'
LEGACY_FRAGMENTS = {
    'world_tail_sha256': 'b46f5f5376125b862cffe321b00916e6cb4a2a969dec6d6365707f1fa6a02bb7',
    'dispatch_call_sha256': '54371371dcc5b5d841fa0edb66e2c2fa8383891d07ee150a57ae57ecd9edd1d6'}
PRODUCTION_INPUTS = ('src/world/runtime/entity_tick.c', 'src/world/runtime/entity_tick_dispatch.c')
ADAPTER_INPUTS = tuple('tests/benchmarks/mob_tick/' + role + '_' + algorithm + '.c'
                       for role in ('world', 'dispatch') for algorithm in ('legacy', 'candidate'))
ALGORITHM_SCOPE = ('Legacy diagnostic algorithms on identical final common source/interfaces; not historical whole-product timing. '
                   'Four crossed World-tail/dispatch object combinations, one identical workload object, identical ordered archives. '
                   'Production source calls, ownership, native fallback entry and workspace cleanup remain common.')
OBSERVER_SCOPE = ('Both Mob dispatch algorithms call the identical single-workload passive reservation counter after the actual Mob invocation; '
                  'no source getter, allocation or gameplay decision is added.')
REGION_NAMES = frozenset(('region/r.-1.-1.mcr', 'region/r.-1.0.mcr',
                          'region/r.0.-1.mcr', 'region/r.0.0.mcr'))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, minimum=0, maximum=(1 << 64) - 1):
    return type(value) is int and minimum <= value <= maximum


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1048576), b''):
            value.update(block)
    return value.hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, 'Actual support module unavailable')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def hardware_info():
    cpu = platform.processor()
    if sys.platform == 'win32':
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            r'HARDWARE\DESCRIPTION\System\CentralProcessor\0') as key:
            cpu = winreg.QueryValueEx(key, 'ProcessorNameString')[0].strip()
    elif sys.platform.startswith('linux'):
        for line in Path('/proc/cpuinfo').read_text(encoding='ascii').splitlines():
            if line.startswith('model name'):
                cpu = line.split(':', 1)[1].strip()
                break
    return dict(cpu=cpu, logical_processors=os.cpu_count(), machine=platform.machine())


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        require(key not in value, 'Duplicate JSON key: ' + key)
        value[key] = item
    return value


def reject_constant(value):
    raise ValueError('Nonfinite JSON constant: ' + value)


def validate_algorithms(path, root=ROOT):
    require(path.stat().st_size <= 1048576, 'Bounded reviewed causal adapter manifest')
    raw = path.read_bytes()
    value = json.loads(raw, object_pairs_hook=unique_object, parse_constant=reject_constant)
    require(type(value) is dict and set(value) == {'schema', 'legacy_algorithm_source_commit',
                'scope', 'production_inputs_at_generation', 'legacy_algorithms', 'common_observer', 'adapters'} and
            value['schema'] == ALGORITHM_SCHEMA and value['legacy_algorithm_source_commit'] == LEGACY_COMMIT and
            value['legacy_algorithms'] == LEGACY_FRAGMENTS and value['scope'] == ALGORITHM_SCOPE and
            value['common_observer'] == OBSERVER_SCOPE,
            'Exact reviewed role/legacy-fragment provenance and causal observation scope')
    production = value['production_inputs_at_generation']
    adapters = value['adapters']
    require(type(production) is dict and set(production) == set(PRODUCTION_INPUTS) and
            type(adapters) is dict and set(adapters) == set(ADAPTER_INPUTS),
            'Exact two production inputs and four reviewed runtime adapter roles')
    files = [path]
    for name in PRODUCTION_INPUTS:
        pin = production[name]
        source = root / name
        require(type(pin) is str and re.fullmatch('[0-9a-f]{64}', pin) and source.is_file() and
                digest(source) == pin, 'Stale production source pin: ' + name)
        files.append(source)
    for name in ADAPTER_INPUTS:
        pin = adapters[name]
        source = root / name
        require(type(pin) is dict and set(pin) == {'bytes', 'sha256'} and
                integer(pin['bytes'], 1, 1048576) and type(pin['sha256']) is str and
                re.fullmatch('[0-9a-f]{64}', pin['sha256']) and source.is_file() and
                source.stat().st_size == pin['bytes'] and digest(source) == pin['sha256'],
                'Stale formatted runtime adapter pin: ' + name)
        files.append(source)
    return dict(file=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest(),
                manifest=value), files


def read_rows(path, variant):
    require(variant in VARIANTS and path.stat().st_size <= MAX_RAW_BYTES,
            'Known native variant and bounded complete timing transport')
    raw = path.read_bytes()
    require(raw.endswith(b'\n'), 'Terminal newline proves complete final native row')
    lines = raw.decode('ascii').splitlines()
    require(len(lines) == 163 and all(lines), 'Header, all161 native intervals, terminal footer')
    values = [json.loads(line, object_pairs_hook=unique_object,
                         parse_constant=reject_constant) for line in lines]
    header, footer = values[0], values[-1]
    require(type(header) is dict and set(header) == HEADER_KEYS and header['event'] == 'header' and
            header['schema'] == 1 and type(header['schema']) is int and header['variant'] == variant,
            'Exact actual native schema/adapter identity')
    require((header['world_finalization'], header['mob_dispatch']) == ROLE_ALGORITHMS[variant],
            'Actual two native role exports match the independent factorial variant')
    require(header['clock'] in ('qpc', 'monotonic') and
            integer(header['frequency'], 1, (1 << 63) - 1) and
            (header['clock'] != 'monotonic' or header['frequency'] == 1000000000),
            'Actual positive native integer clock/frequency')
    require(all(type(header[name]) is int and header[name] == expected for name, expected in
                (('ticks_per_interval', 16), ('warmups', 2), ('samples', 21), ('fixture_chunks', 25))) and
            header['digest_policy'] == 'typed-fnv1a64-le-v1' and
            header['entropy_exclusions'] == ['living.F', 'living.G', 'living.aj'] and
            type(header['sizeof']) is dict and set(header['sizeof']) == set(SIZES) and
            all(integer(value, 1, 16 * 1024 * 1024) for value in header['sizeof'].values()),
            'Literal work, typed hashing and explicit independent-constructor entropy scope')
    rows, previous_end, previous_checks = [], None, 0
    for index, row in enumerate(values[1:-1]):
        cell, kind, count = CELLS[index // 23]
        sample = index % 23
        require(type(row) is dict and set(row) == ROW_KEYS and row['event'] == 'interval' and
                (row['cell'], row['kind'], row['count'], row['sample']) == (cell, kind, count, sample) and
                type(row['count']) is int and type(row['sample']) is int and
                row['warmup'] is (sample < 2), 'Every native workload/sample in exact execution order')
        require(type(row['ticks']) is int and type(row['returns']) is int and
                row['ticks'] == row['returns'] == 16 and row['status'] == 'OK' and row['no_hurt'] is True and
                all(type(row[name]) is int and row[name] == 0 for name in
                    ('hurt_reached', 'hurt_returned', 'hurt_true')),
                'All16 real FD.g returns and zero actual hurt caller/return receipts')
        require(type(row['dispatch_calls']) is int and row['dispatch_calls'] == count * 16 and
                type(row['workspace_calls']) is int and row['workspace_calls'] ==
                (count * 16 if variant in ('workspace', 'both') else 0),
                'Every actual Mob dispatch completed using the declared diagnostic allocation path')
        require(integer(row['start'], 0, (1 << 63) - 1) and
                integer(row['end'], 1, (1 << 63) - 1) and row['end'] > row['start'] and
                (previous_end is None or row['start'] >= previous_end),
                'Strict positive integer intervals, nonoverlapping within the actual process')
        previous_end = row['end']
        for name in ('actor_ticks_before', 'actor_ticks_after'):
            require(type(row[name]) is list and len(row[name]) == count and
                    all(integer(value, -(1 << 31), (1 << 31) - 1) for value in row[name]),
                    'Every genuine actor has an actual signed source tick word')
        require(all(after == ((before + 16 + (1 << 31)) % (1 << 32)) - (1 << 31)
                    for before, after in zip(row['actor_ticks_before'], row['actor_ticks_after'])),
                'No actor tick or scheduler work skipped; Java-int wrap remains literal')
        require(all(type(row[name]) is str and re.fullmatch('[0-9a-f]{16}', row[name]) for name in DIGESTS) and
                all(row[name] is True for name in INVARIANTS) and
                row['chunks_before'] == row['chunks_after'] and
                type(row['constructor_unchanged']) is bool and
                row['constructor_unchanged'] == (row['constructor_before'] == row['constructor_after']),
                'Complete typed source/RNG/Chunk/ownership hashes and actual within-call invariants')
        require(integer(row['checks'], 1) and row['checks'] > previous_checks,
                'Actual cumulative native checks advance for every interval')
        previous_checks = row['checks']
        rows.append({**row, 'clock_ticks': row['end'] - row['start'],
                     'frequency': header['frequency'],
                     'microseconds': (row['end'] - row['start']) * 1e6 / header['frequency']})
    require(type(footer) is dict and set(footer) == FOOTER_KEYS and footer['event'] == 'footer' and
            footer['complete'] is True and footer['cleanup'] is True and
            type(footer['rows']) is int and footer['rows'] == 161 and
            integer(footer['checks'], previous_checks), 'Complete checked native terminal cleanup')
    return dict(header=header, rows=rows, footer=footer,
                raw_sha256=hashlib.sha256(raw).hexdigest(), raw_bytes=len(raw))


def compare_semantics(reference, actual):
    require(reference['header']['sizeof'] == actual['header']['sizeof'] and
            reference['header']['clock'] == actual['header']['clock'] and
            reference['header']['frequency'] == actual['header']['frequency'],
            'Same exact native layouts and host clock for every adapter')
    for before, after in zip(reference['rows'], actual['rows']):
        require(all(before[name] == after[name] for name in
                    (*PAIRED, 'cell', 'kind', 'count', 'sample', 'actor_ticks_before', 'actor_ticks_after')),
                'Equal declared inputs/source outcomes/RNG/ownership: ' + before['cell'] +
                ' sample' + str(before['sample']))
    require(len(reference['rows']) == len(actual['rows']) == 161, 'No comparison subset')


def compare_completed_process(processes):
    require(processes and 'parsed' in processes[0] and 'parsed' in processes[-1],
            'Complete first/current native process required before semantic admission')
    current = processes[-1]
    current['semantic_comparison'] = dict(reference_process=0, status='FAIL')
    compare_semantics(processes[0]['parsed'], current['parsed'])
    current['semantic_comparison']['status'] = 'PASS'


def schedule():
    # Each adapter occupies each order position once; successive rounds reverse
    # direction to retain both execution-order strata for the factorial cohort.
    return [(round_index, VARIANTS[(round_index + (offset if round_index % 2 == 0 else -offset)) % 4])
            for round_index in range(4) for offset in range(4)]


def ninja_words(text):
    result, word, index = [], '', 0
    while index < len(text):
        char = text[index]
        if char == '$':
            index += 1
            require(index < len(text) and text[index] in (' ', ':', '$'), 'Concrete Ninja path escape')
            word += text[index]
        elif char.isspace():
            if word:
                result.append(word); word = ''
        else:
            word += char
        index += 1
    if word:
        result.append(word)
    return result


def native_path(directory, name):
    path = Path(name.replace('\\', '/'))
    return (path if path.is_absolute() else directory / path).resolve()


def compile_identity(entry):
    tokens = entry.get('arguments')
    if tokens is None:
        tokens = shlex.split(entry['command'], posix=os.name != 'nt')
    require(type(tokens) is list and tokens and all(type(value) is str for value in tokens),
            'Actual compiler argument vector')
    tokens = [value[1:-1] if value.startswith('"') and value.endswith('"') else value for value in tokens]
    directory = Path(entry['directory'])
    source = native_path(directory, entry['file'])
    result, index = [], 0
    while index < len(tokens):
        token = tokens[index]
        if token in ('-o', '-MF', '-MT', '-MQ'):
            require(index + 1 < len(tokens), 'Concrete compiler output argument')
            index += 2
            continue
        if token.startswith(('/Fo', '/Fd')):
            index += 1
            if token in ('/Fo', '/Fd'):
                require(index < len(tokens), 'Concrete compiler output path')
                index += 1
            continue
        if native_path(directory, token) == source:
            index += 1
            continue
        result.append(token)
        index += 1
    return result


def link_identity(properties, key, executable):
    value = properties.get(key)
    if key != 'LINK_FLAGS':
        return value
    require(type(value) is str, 'Actual target link flags required')
    expected = executable.with_suffix('.map')
    spellings = sorted({str(expected), str(expected).replace('\\', '/')}, key=len, reverse=True)
    pattern = r'(/MAP:|-Wl,-Map,)("?)(?:' + '|'.join(re.escape(name) for name in spellings) + r')\2'
    matches = list(re.finditer(pattern, value))
    require(len(matches) == 1, 'Exactly the known per-target map output argument')
    match = matches[0]
    return value[:match.start()] + match.group(1) + '<target-map>' + value[match.end():]


def binding_from_metadata(executables, lines, compilation):
    directory = next(iter(executables.values())).parent
    require(all(path.parent == directory for path in executables.values()), 'One actual compiled build')
    edges, objects, libraries, properties = {}, {}, {}, {}
    for variant in VARIANTS:
        executable = executables[variant]
        selected = [index for index, line in enumerate(lines)
                    if line.startswith('build ' + executable.name + ': ')]
        require(len(selected) == 1, 'Exactly one executable link edge: ' + variant)
        index = selected[0]
        edge = lines[index]; words = ninja_words(edge.split(': ', 1)[1])
        require('|' in words and words.index('|') == 4,
                'Exactly workload + World finalization + Mob dispatch explicit objects')
        end = words.index('||') if '||' in words else len(words)
        objects[variant] = [native_path(directory, word) for word in words[1:4]]
        libraries[variant] = [native_path(directory, word) for word in words[5:end]]
        require(libraries[variant] and all(path.suffix in ('.lib', '.a') for path in libraries[variant]),
                'Concrete ordered core archive tail')
        props = {}
        for line in lines[index + 1:]:
            if not line.startswith('  '):
                break
            key, separator, value = line.strip().partition(' = ')
            if separator:
                require(key not in props, 'Duplicate link property')
                props[key] = value
        edges[variant], properties[variant] = edge, props
    require(all(libraries[name] == libraries['legacy'] for name in VARIANTS),
            'Exact identical ordered core archive graph')
    for key in ('FLAGS', 'LINK_FLAGS', 'LINK_LIBRARIES'):
        expected = link_identity(properties['legacy'], key, executables['legacy'])
        require(all(link_identity(properties[name], key, executables[name]) == expected for name in VARIANTS),
                'Identical actual link property: ' + key)
    shared = set.intersection(*(set(objects[name]) for name in VARIANTS))
    require(len(shared) == 1, 'One identical compiled workload object in every product')
    commands = {}
    for entry in compilation:
        require(type(entry) is dict, 'Actual compilation metadata object')
        if 'output' in entry:
            key = native_path(Path(entry.get('directory', directory)), entry['output'])
            require(key not in commands, 'Unique actual compilation output')
            commands[key] = entry
    roles, selected_commands = {}, {}
    for variant in VARIANTS:
        roles[variant] = {}
        for obj in objects[variant]:
            require(obj in commands, 'Concrete compile command for every linked workload/adapter object')
            entry = commands[obj]
            source = native_path(Path(entry.get('directory', directory)), entry['file'])
            if obj in shared:
                role = 'workload'
                require(source == ROOT / 'tests/benchmarks/mob_tick_diagnostics.c',
                        'Actual genuine Mob workload source')
            else:
                if source.name.startswith('dispatch_'):
                    role = 'mob_dispatch'
                else:
                    require(source.name.startswith('world_'), 'Explicit World finalization adapter source role')
                    role = 'world_finalization'
                algorithm = ROLE_ALGORITHMS[variant][int(role == 'mob_dispatch')]
                expected = ROOT / 'tests/benchmarks/mob_tick' / (
                    ('dispatch_' if role == 'mob_dispatch' else 'world_') + algorithm + '.c')
                require(source == expected, 'Exact reviewed source for the actual runtime adapter role')
            require(role not in roles[variant], 'One exact object per native role')
            roles[variant][role] = str(obj)
            selected_commands[str(obj)] = entry
        require(set(roles[variant]) == {'workload', 'world_finalization', 'mob_dispatch'},
                'Exact two runtime adapters and one shared workload')
    for role, pairs in (('world_finalization', (('legacy', 'workspace'), ('tail', 'both'))),
                        ('mob_dispatch', (('legacy', 'tail'), ('workspace', 'both')))):
        require(all(roles[a][role] == roles[b][role] for a, b in pairs) and
                roles[pairs[0][0]][role] != roles[pairs[1][0]][role],
                'Exact independent 2x2 compiled-object factorial: ' + role)
        first, second = (selected_commands[roles[pair[0]][role]] for pair in pairs)
        require(compile_identity(first) == compile_identity(second),
                'Identical actual compiler/options for paired algorithm objects: ' + role)
    products = sorted(set(libraries['legacy'] + [path for row in objects.values() for path in row]))
    return dict(shared_workload=str(next(iter(shared))), roles=roles, link_edges=edges,
                link_properties=properties, compilation=selected_commands), products


def validate_link_map(text, roles):
    require(all(symbol in text for symbol in ('mc_world_runtime_tick_entities', 'wt_tick',
                'mc_benchmark_mob_tick_legacy_tail', 'mc_benchmark_mob_tick_legacy_dispatch')) and
            all(Path(obj).name in text for obj in roles.values()),
            'Actual link map contains each selected object and role export')
    require(re.search(r'mc_simulation(?:\.lib|\.a)?[:(][^\n]*\bentity_tick(?:_dispatch)?\.c\.(?:obj|o)\b',
                      text, re.IGNORECASE) is None,
            'Production World/dispatch archive objects were not extracted behind adapters')


def command_words(value):
    return [word[1:-1] if word.startswith('"') and word.endswith('"') else word
            for word in shlex.split(value, posix=os.name != 'nt')]


def ninja_rule(lines, name):
    starts = [index for index, line in enumerate(lines) if line == 'rule ' + name]
    require(len(starts) == 1, 'One actual Ninja rule: ' + name)
    result = {}
    for line in lines[starts[0] + 1:]:
        if not line.startswith('  '):
            break
        key, separator, value = line.strip().partition(' = ')
        if separator:
            require(key not in result, 'Unique Ninja rule property')
            result[key] = value
    return result


def cmake_configuration(directory, lines, rules, source_root=ROOT):
    ninja = directory / 'build.ninja'
    cache = directory / 'CMakeCache.txt'
    require(ninja.is_file() and cache.is_file(), 'Actual configured Ninja/CMake owners')
    values = {}
    for line in cache.read_text(encoding='utf-8').splitlines():
        if not line or line.startswith(('#', '//')):
            continue
        match = re.fullmatch(r'([^:=]+):([^=]+)=(.*)', line)
        if match:
            require(match[1] not in values, 'Unique CMake configuration variable')
            values[match[1]] = match[3]
    exact = {'CMAKE_BUILD_TYPE': 'Release', 'CMAKE_GENERATOR': 'Ninja',
             'MC_BUILD_BENCHMARKS': 'ON', 'BUILD_TESTING': 'ON'}
    require(all(values.get(key) == value for key, value in exact.items()),
            'Current Release Ninja configuration enables actual benchmark and test targets')
    for key, expected in (('CMAKE_HOME_DIRECTORY', source_root), ('CMAKE_CACHEFILE_DIR', directory)):
        require(key in values and native_path(directory, values[key]) == expected.resolve(),
                'Actual CMake configuration directory: ' + key)
    for key in ('CMAKE_COMMAND', 'CMAKE_MAKE_PROGRAM'):
        require(key in values and native_path(directory, values[key]).is_file(),
                'Actual configured tool binary: ' + key)
    edges = []
    globs = []
    for line in lines:
        if not line.startswith('build ') or ': ' not in line:
            continue
        left, right = line.split(': ', 1)
        words = ninja_words(right)
        if words and words[0] == 'RERUN_CMAKE':
            edges.append((left, words))
        elif words and words[0] == 'VERIFY_GLOBS':
            globs.append((left, words))
    require(len(edges) == 1, 'One actual CMake regeneration edge')
    left, words = edges[0]
    outputs = [native_path(directory, name) for name in ninja_words(left[6:])]
    require(ninja.resolve() in outputs and '||' not in words,
            'Actual build.ninja regeneration outputs and concrete configuration inputs')
    dependencies = [native_path(directory, name) for name in words[1:] if name != '|']
    require(dependencies and cache.resolve() in dependencies,
            'CMake regeneration binds the actual configuration cache')
    graph_mtime = ninja.stat().st_mtime_ns
    inputs = {}
    for path in dict.fromkeys(dependencies):
        require(path.is_file(), 'Missing CMake regeneration input: ' + str(path))
        require(path.stat().st_mtime_ns <= graph_mtime,
                'Stale CMake configuration input newer than actual build.ninja: ' + str(path))
        inputs[str(path)] = dict(mtime_ns=path.stat().st_mtime_ns, bytes=path.stat().st_size,
                                 sha256=digest(path))
    regeneration = ninja_rule(rules, 'RERUN_CMAKE')
    command = command_words(regeneration.get('command', ''))
    require(regeneration.get('generator') == '1' and len(command) == 4 and
            native_path(directory, command[0]) == native_path(directory, values['CMAKE_COMMAND']) and
            command[1] == '--regenerate-during-build' and command[2].startswith('-S') and
            native_path(directory, command[2][2:]) == source_root.resolve() and
            command[3].startswith('-B') and native_path(directory, command[3][2:]) == directory.resolve(),
            'CMake regeneration rule uses the actual configured source, build and binary')
    result = dict(configuration={key: values[key] for key in (*exact, 'CMAKE_HOME_DIRECTORY',
                    'CMAKE_CACHEFILE_DIR', 'CMAKE_COMMAND', 'CMAKE_MAKE_PROGRAM')},
                  regeneration_rule=regeneration, inputs=inputs, graph_mtime_ns=graph_mtime,
                  glob_check=None)
    require(len(globs) <= 1, 'At most one generated CMake glob verification edge')
    if globs:
        script = directory / 'CMakeFiles/VerifyGlobs.cmake'
        stamp = directory / 'CMakeFiles/cmake.verify_globs'
        glob_outputs = [native_path(directory, name) for name in ninja_words(globs[0][0][6:])]
        require(glob_outputs == [stamp.resolve()] and script.resolve() in dependencies and
                stamp.resolve() in dependencies, 'Generated glob script/stamp are actual configuration inputs')
        rule = ninja_rule(rules, 'VERIFY_GLOBS')
        command = command_words(rule.get('command', ''))
        require(rule.get('generator') == '1' and len(command) == 3 and command[1] == '-P' and
                native_path(directory, command[0]) == native_path(directory, values['CMAKE_COMMAND']) and
                native_path(directory, command[2]) == script.resolve(),
                'Exact generated VerifyGlobs.cmake command; no configure/build substitution')
        require(script.read_bytes().startswith(b'# CMAKE generated file: DO NOT EDIT!'),
                'Actual CMake-generated glob verifier')
        result['glob_check'] = dict(rule=rule, command=command, script=str(script), stamp=str(stamp))
    return result


def readiness_result(result, glob=False):
    require(result.returncode == 0 and result.stderr == b'' and
            (result.stdout == b'' if glob else
             result.stdout.decode('ascii').strip() == 'ninja: no work to do.'),
            'Generated glob mismatch/output' if glob else 'Actual compile/link work remains pending')


def verify_readiness(directory, executables, artifacts):
    target = artifacts / 'readiness'
    target.mkdir()
    receipt = dict(status='FAIL', commands=[])
    original, copied = directory / 'build.ninja', target / 'actual-build.ninja'
    paths = [original, copied, target / 'readiness.json']

    def query(command, stem):
        record = dict(command=command, directory=str(directory), timeout_seconds=60)
        receipt['commands'].append(record)
        try:
            result = subprocess.run(command, cwd=directory, stdin=subprocess.DEVNULL,
                                    capture_output=True, timeout=60)
            stdout, stderr = result.stdout, result.stderr
            record.update(returncode=result.returncode, timed_out=False)
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b'', error.stderr or b''
            record.update(returncode=None, timed_out=True)
            raise
        finally:
            if 'stdout' in locals():
                for name, data in (('stdout', stdout), ('stderr', stderr)):
                    path = target / (stem + '.' + name)
                    path.write_bytes(data)
                    record[name] = dict(file=str(path.relative_to(artifacts)), bytes=len(data),
                                        sha256=hashlib.sha256(data).hexdigest())
                    paths.append(path)
        return result

    try:
        require(original.is_file(), 'Actual untouched Ninja graph required')
        receipt['graph_before'] = dict(file=str(original), bytes=original.stat().st_size,
                                       sha256=digest(original), mtime_ns=original.stat().st_mtime_ns)
        shutil.copy2(original, copied)
        require(copied.stat().st_size == original.stat().st_size and digest(copied) == digest(original),
                'Exact byte-identical retained graph copy; no edge removed or rewritten')
        receipt['copied_graph'] = dict(file=str(copied.relative_to(artifacts)),
                                       bytes=copied.stat().st_size, sha256=digest(copied),
                                       mtime_ns=copied.stat().st_mtime_ns)
        rules = directory / 'CMakeFiles/rules.ninja'
        configuration = cmake_configuration(directory, original.read_text(encoding='utf-8').splitlines(),
                                             rules.read_text(encoding='utf-8').splitlines())
        receipt['cmake'] = configuration
        paths.extend(Path(path) for path in configuration['inputs'])
        paths.extend(native_path(directory, configuration['configuration'][key])
                     for key in ('CMAKE_COMMAND', 'CMAKE_MAKE_PROGRAM'))
        if configuration['glob_check'] is not None:
            before = configuration['inputs'][configuration['glob_check']['stamp']]
            result = query(configuration['glob_check']['command'], 'verify-globs')
            readiness_result(result, glob=True)
            stamp = Path(configuration['glob_check']['stamp'])
            require(stamp.stat().st_mtime_ns == before['mtime_ns'] and digest(stamp) == before['sha256'],
                    'Generated glob verification did not invalidate its actual configuration stamp')
        ninja = str(native_path(directory, configuration['configuration']['CMAKE_MAKE_PROGRAM']))
        command = [ninja, '-f', str(copied), '-n', *[executables[name].name for name in VARIANTS]]
        readiness_result(query(command, 'ninja-dry-run'))
        require(original.stat().st_mtime_ns == receipt['graph_before']['mtime_ns'] and
                digest(original) == receipt['graph_before']['sha256'] == digest(copied),
                'Actual and copied graph stayed unchanged through readiness verification')
        for name, before in configuration['inputs'].items():
            path = Path(name)
            require(path.stat().st_mtime_ns == before['mtime_ns'] and digest(path) == before['sha256'],
                    'CMake configuration inputs remained fixed through readiness verification')
        receipt['status'] = 'PASS'
    except BaseException as error:
        receipt['error'] = dict(type=type(error).__name__, message=str(error))
        raise
    finally:
        (target / 'readiness.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    return receipt, sorted(set(paths))


def build_binding(executables, artifacts):
    directory = next(iter(executables.values())).parent
    algorithms, algorithm_files = validate_algorithms(ROOT / 'tests/benchmarks/mob_tick/algorithms.json')
    ninja, rules = directory / 'build.ninja', directory / 'CMakeFiles/rules.ninja'
    require(ninja.is_file() and rules.is_file(), 'Actual complete Ninja graph/compiler rules')
    readiness, readiness_files = verify_readiness(directory, executables, artifacts)
    ninja_binary = str(native_path(directory, readiness['cmake']['configuration']['CMAKE_MAKE_PROGRAM']))
    child = subprocess.run([ninja_binary, '-t', 'compdb'], cwd=directory, stdin=subprocess.DEVNULL,
                           capture_output=True, timeout=60)
    require(child.returncode == 0 and child.stderr == b'', 'Read-only actual compilation inspection')
    binding, products = binding_from_metadata(executables, ninja.read_text(encoding='utf-8').splitlines(),
                                             json.loads(child.stdout))
    binding['compilation_database_sha256'] = hashlib.sha256(child.stdout).hexdigest()
    binding['algorithms'] = algorithms
    binding['build_readiness'] = readiness
    binding['compilation_inspection'] = dict(command=[ninja_binary, '-t', 'compdb'], directory=str(directory))
    metadata = [ninja, rules, directory / 'CMakeCache.txt', *algorithm_files, *readiness_files]
    binding['link_maps'] = {}
    for variant, executable in executables.items():
        maps = list(executable.parent.glob(executable.stem + '*.map'))
        require(maps == [executable.with_suffix('.map')],
                'Exactly the actual declared target map, with no stale matching maps')
        path = maps[0]
        require(256 <= path.stat().st_size <= 32 * 1024 * 1024, 'Bounded complete actual linker map')
        text = path.read_text(encoding='utf-8', errors='strict')
        validate_link_map(text, binding['roles'][variant])
        binding['link_maps'][variant] = dict(file=str(path), sha256=digest(path))
        metadata.extend(maps)
    metadata.extend(directory.glob('CMakeFiles/*/CMakeCCompiler.cmake'))
    compilers = set()
    for entry in binding['compilation'].values():
        name = compile_identity(entry)[0]
        resolved = shutil.which(name)
        if resolved is None:
            path = native_path(Path(entry['directory']), name)
            require(path.is_file(), 'Actual compiler binary identity available')
        else:
            path = Path(resolved).resolve()
        compilers.add(path)
    require(len(compilers) == 1, 'One actual C compiler binary for all five native objects')
    metadata.extend(compilers)
    products = sorted(set(products + metadata))
    binding['product_sha256'] = {str(path): digest(path) for path in products}
    require(validate_algorithms(ROOT / 'tests/benchmarks/mob_tick/algorithms.json')[0] == algorithms,
            'Reviewed source and adapter manifest stayed fixed through build admission')
    return binding, products


def capture_products(paths, output):
    destination = output / 'native-products'; destination.mkdir()
    result = []
    for path in sorted(set(paths)):
        before = dict(bytes=path.stat().st_size, sha256=digest(path))
        target = destination / before['sha256']
        if not target.exists():
            shutil.copyfile(path, target)
        require(target.stat().st_size == before['bytes'] and digest(target) == before['sha256'] and
                path.stat().st_size == before['bytes'] and digest(path) == before['sha256'],
                'Exact independent retained product copy')
        result.append(dict(source=str(path), artifact=str(target.relative_to(output)), **before))
    return result


def input_paths(executables, products, resources, games):
    paths = {Path(__file__).resolve(), Path(__file__).with_name('test_benchmark_mob_tick_diagnostics.py'),
             ROOT / 'CMakeLists.txt', ROOT / '.clang-format', *executables.values(), *products}
    for folder in ('src', 'include', 'cmake', 'tests/support'):
        paths.update(path for path in (ROOT / folder).rglob('*') if path.is_file())
    for folder in ('tests/benchmarks', 'tests/client/statistics', 'tests/reference/client/player_statistics',
                   'tests/reference/client/statistics'):
        paths.update(path for path in (ROOT / folder).rglob('*') if path.is_file())
    paths.update((ROOT / 'tests/tools/entity').glob('test_mob_*.py'))
    paths.update(path for path in (ROOT / 'tests').rglob('*') if path.is_file() and
                 path.suffix in ('.c', '.h', '.inc', '.py', '.java', '.tsv'))
    paths.update(path for path in resources.rglob('*') if path.is_file())
    for game in games:
        paths.update(game.rglob('*.mcr'))
    for executable in executables.values():
        paths.update(executable.parent.glob('*.dll'))
    return sorted(paths)


def fingerprint(paths):
    return {str(path): dict(bytes=path.stat().st_size, sha256=digest(path)) for path in paths}


def terrain_region_hashes(world):
    paths = sorted(world.rglob('*.mcr'))
    require({path.relative_to(world).as_posix() for path in paths} == REGION_NAMES,
            'Exactly four canonical saved region files for the25-Chunk terrain')
    result = {}
    for path in paths:
        require(path.is_file() and not path.is_symlink() and path.stat().st_nlink == 1,
                'Every saved region is an independent regular file, never a hardlink')
        result[path.relative_to(world).as_posix()] = dict(bytes=path.stat().st_size, sha256=digest(path))
    return result


def author_terrain_template(terrain, output):
    producer = Path(terrain.__file__).resolve(strict=True)
    before = dict(file=str(producer), bytes=producer.stat().st_size, sha256=digest(producer))
    template = output / 'terrain-template'
    template.mkdir()
    terrain.authored_world(template)
    require(producer.stat().st_size == before['bytes'] and digest(producer) == before['sha256'],
            'Actual canonical terrain producer stayed fixed through its one authoring call')
    return template, dict(producer=before, authoring_calls=1, regions=terrain_region_hashes(template))


def validate_terrain_copies(template, worlds, expected):
    require(terrain_region_hashes(template) == expected, 'Canonical terrain template is missing or changed')
    for world in worlds:
        require(terrain_region_hashes(world) == expected,
                'Independent saved terrain copy is missing or changed: ' + str(world))
    return dict(independent_scenes=len(worlds), region_files=len(worlds) * len(expected),
                copied_bytes=len(worlds) * sum(item['bytes'] for item in expected.values()),
                all_copies_match_template=True, hardlinks=False)


def copy_terrain_regions(template, worlds, expected):
    require(terrain_region_hashes(template) == expected, 'Canonical template changed before copying')
    for world in worlds:
        (world / 'region').mkdir(parents=True)
        for name in sorted(expected):
            source, destination = template / name, world / name
            require(not destination.exists(), 'Fresh independent scene region destination')
            shutil.copyfile(source, destination)
    return validate_terrain_copies(template, worlds, expected)


def prepare_terrain(terrain, games, output):
    receipt = dict(status='FAIL', template_directory='terrain-template')
    try:
        template, authored = author_terrain_template(terrain, output)
        receipt.update(authored)
        worlds = [game / 'scenes' / cell / ('sample-' + str(sample)) / 'world'
                  for game in games for cell, _, _ in CELLS for sample in range(23)]
        receipt['copies'] = copy_terrain_regions(template, worlds, receipt['regions'])
        receipt['status'] = 'PASS'
    except BaseException as error:
        receipt['error'] = dict(type=type(error).__name__, message=str(error))
        raise
    finally:
        (output / 'terrain-preparation.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    return template, receipt


def verify_stdout(stdout, stderr, returncode, footer):
    require(returncode == 0 and stderr == b'', 'Actual native workload/source/cleanup failure')
    lines = stdout.splitlines()
    require(lines[:-1] == [b'16 achievements', b'151 recipes'] * 161 and
            lines[-1:] == [('Mob tick diagnostics: ' + str(footer['checks']) +
                            ' checks; 161 intervals; 2576 FD.g returns').encode('ascii')],
            'Every genuine native fixture and literal completed-work footer')


def summarize(processes):
    require([(item['round'], item['variant']) for item in processes] == schedule(),
            'Complete16processes in retained balanced execution order')
    reference = processes[0]['parsed']
    for item in processes:
        compare_semantics(reference, item['parsed'])
    result = []
    for cell, kind, count in CELLS:
        row = dict(cell=cell, kind=kind, count=count, variants={})
        for variant in VARIANTS:
            values = [item['microseconds'] for process in processes if process['variant'] == variant
                      for item in process['parsed']['rows'] if item['cell'] == cell and not item['warmup']]
            require(len(values) == 84, 'All four rounds and21 complete measured intervals/cell')
            ordered = sorted(values)
            row['variants'][variant] = dict(samples=84, median_us=statistics.median(values),
                                            minimum_us=min(values), maximum_us=max(values), p95_us=ordered[79])
        baseline = row['variants']['legacy']['median_us']
        row['median_reduction_percent'] = {variant: 100 * (baseline - row['variants'][variant]['median_us']) / baseline
                                            for variant in VARIANTS[1:]}
        result.append(row)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='VARIANT=PATH')
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    executables = {}
    for value in args.executable:
        variant, separator, path = value.partition('=')
        require(separator and variant in VARIANTS and variant not in executables, 'Four exact unique adapters')
        executables[variant] = Path(path).resolve(strict=True)
    require(set(executables) == set(VARIANTS), 'All four causal native variants required')
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='mob-tick-diagnostics-', dir=args.artifacts.resolve()))
    report = dict(status='FAIL', scope=__doc__, host=platform.platform(), python=sys.version,
                  hardware=hardware_info(), processes=[], rounds=4, original_rows=0)
    report['workload_inputs'] = dict(
        chunks='25 normal stored Stone-floor Chunks at x/z -2..2; floor y63; no saved Entities',
        published_players=0, time=13000, sky_darkness=4, difficulty=2, world_g=17, world_Q=6001,
        world_I='1234567890000 + sample',
        actor_pose='x=5+(i%8), y=64, z=5+(i/8 integer); yaw=0, pitch=0',
        unpublished_player_pose='8.5,64,8.5; yaw=0, pitch=0',
        actor_random_seed='0x1234abc000 + sample*65537 + count*257 + native_kind_enum*17 + i',
        world_random_seed='0x334455000 + sample', player_random_seed='0x434455000 + sample',
        shared_math_seed='0x534455000 + sample',
        constructor_entropy='Genuine constructors finish before independent literal workload inputs; no constructor tape or captured-seed replay')
    before, paths = None, None
    try:
        report['readiness_report'] = 'readiness/readiness.json'
        binding, products = build_binding(executables, output)
        report['build_binding'] = binding
        binaries = [*executables.values(), *products]
        for executable in executables.values():
            binaries.extend(executable.parent.glob('*.dll'))
        report['native_products'] = capture_products(binaries, output)
        support = module('mob_tick_statistics_support', ROOT / 'tests/reference/client/player_statistics/support.py')
        terrain = module('mob_tick_world_inputs', ROOT / 'tests/tools/entity/test_mob_tick.py')
        resources = output / 'resources'; support.resources_at(resources)
        games = []
        for index, (_, variant) in enumerate(schedule()):
            game = output / f'game-{index}-{variant}'; game.mkdir(); games.append(game)
        report['terrain_preparation_file'] = 'terrain-preparation.json'
        template, report['terrain_preparation'] = prepare_terrain(terrain, games, output)
        guard_games = [template, *games]
        paths = input_paths(executables, products, resources, guard_games)
        before = fingerprint(paths); report['inputs_before'] = before
        environment = os.environ.copy(); environment['ALSOFT_DRIVERS'] = 'null'
        for index, (round_index, variant) in enumerate(schedule()):
            raw = output / f'intervals-{index}-{variant}.jsonl'
            record = dict(round=round_index, variant=variant, raw_file=raw.name)
            report['processes'].append(record)
            try:
                child = support.run([str(executables[variant]), str(resources), str(games[index]), str(raw)],
                                    output, f'native-{index}-{variant}', timeout=PROCESS_SECONDS, env=environment)
                record['capture'] = child.capture_report
                require(child.returncode == 0 and child.stderr == b'',
                        'Actual native source/workload/cleanup failed; retain this opportunity')
                record['parsed'] = read_rows(raw, variant)
                verify_stdout(child.stdout, child.stderr, child.returncode, record['parsed']['footer'])
                compare_completed_process(report['processes'])
                require(fingerprint(input_paths(executables, products, resources, guard_games)) == before,
                        'Source/product/resource/saved-Chunk guard changed')
            except BaseException as error:
                if hasattr(error, 'capture_report'):
                    record['capture'] = error.capture_report
                record['error'] = dict(type=type(error).__name__, message=str(error))
                raise
            finally:
                if raw.is_file():
                    record['raw_retained'] = dict(bytes=raw.stat().st_size, sha256=digest(raw))
        report['summary'] = summarize(report['processes'])
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = dict(type=type(error).__name__, message=str(error))
        raise
    finally:
        if before is not None:
            try:
                report['inputs_after'] = fingerprint(input_paths(executables, products, resources, guard_games))
                report['inputs_unchanged'] = before == report['inputs_after']
                if not report['inputs_unchanged']:
                    report['status'] = 'FAIL'
            except BaseException as error:
                report['status'] = 'FAIL'
                report['guard_error'] = dict(type=type(error).__name__, message=str(error))
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Mob diagnostic CPU attempt retained: ' + str(output), flush=True)
    require(report['status'] == 'PASS', 'Complete fixed-work CPU comparison did not qualify')
    print(json.dumps(report['summary']))


if __name__ == '__main__':
    main()
