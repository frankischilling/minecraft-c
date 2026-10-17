#!/usr/bin/env python3
"""Compare protected room/path helpers with original calls on owned block patterns."""
import argparse
from dataclasses import asdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import sys
import types
import uuid

TABLE_SHA = '87a67a39fff59636acb04edcda4ef622831037291f11fb61586c57596771dac2'
JAR_HASHES = {'client': 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c',
              'server': '033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d'}
REQUEST_SHA = 'be792fc398e8a1e22702b08f7ff6bf96b007bb950e687e61e5550713e60c88b2'
MAX_STDOUT, MAX_STDERR, MAX_LINE = 32 * 1024 * 1024, 4 * 1024 * 1024, 512 * 1024
CONTROL = 'CONTROL_GEOMETRY table_exact registry_exact range_exact random_identity suffix_exact shared_rng_continuation_match'
C_CONTROL = 'CONTROL_C_GEOMETRY table_exact range_exact random_identity suffix_exact shared_rng_continuation_match'
CLASS_NAMES = {'CaveGeometryObservation.class', 'CaveGeometryObservation$Request.class', 'CaveGeometryObservation$Model.class'}
KEYS = {'schema', 'case', 'request', 'input_blocks', 'before_continuation', 'output_blocks',
        'after_continuation', 'shared_random_consumption', 'changed_prefix_bytes',
        'math_table_sha256_f32be', 'suffix_unchanged', 'retained_random', 'range'}

class Failure(Exception):
    pass

class Unavailable(Exception):
    pass

def require(condition, message):
    if not condition:
        raise Failure(message)

def record(path, maximum=None):
    path = Path(path).resolve(strict=True)
    before = path.stat()
    require(stat.S_ISREG(before.st_mode), 'not a regular input/artifact: ' + str(path))
    bound = before.st_size if maximum is None else maximum
    require(before.st_size <= bound, 'file size exceeds hash bound: ' + str(path))
    digest = hashlib.sha256()
    remaining = bound
    with path.open('rb') as stream:
        while remaining:
            block = stream.read(min(remaining, 1024 * 1024))
            if not block:
                break
            remaining -= len(block)
            digest.update(block)
        require(not stream.read(1), 'file grew beyond hashing bound: ' + str(path))
    after = path.stat()
    require((before.st_size, before.st_mtime_ns, before.st_dev, before.st_ino) ==
            (after.st_size, after.st_mtime_ns, after.st_dev, after.st_ino), 'file changed while hashing')
    return {'path': str(path), 'bytes': after.st_size, 'sha256': digest.hexdigest()}

def source_module(path, name):
    path = Path(path).resolve(strict=True)
    require(path.stat().st_size <= 2 * 1024 * 1024, 'source exceeds execution size bound')
    with path.open('rb') as stream:
        data = stream.read(2 * 1024 * 1024 + 1)
    require(len(data) <= 2 * 1024 * 1024, 'source grew beyond execution size bound')
    attributed = record(path, 2 * 1024 * 1024)
    require(len(data) == attributed['bytes'] and hashlib.sha256(data).hexdigest() == attributed['sha256'],
            'source bytes changed before execution')
    module = types.ModuleType(name)
    module.__file__ = str(path)
    old = sys.modules.get(name)
    sys.modules[name] = module
    try:
        exec(compile(data, str(path), 'exec'), module.__dict__)
    finally:
        if old is None:
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = old
    return module, attributed

def load_shared(path):
    # Compile exact source bytes, including its fixture dependency. Cached .pyc
    # files never participate in the executed utility/fixture attribution.
    fixture, fixture_record = source_module(path.with_name('fixtures.py'), '_caves_geometry_natural_fixtures')
    old = sys.modules.get('fixtures')
    sys.modules['fixtures'] = fixture
    try:
        helper, helper_record = source_module(path, '_caves_geometry_natural_helper')
    finally:
        if old is None:
            sys.modules.pop('fixtures', None)
        else:
            sys.modules['fixtures'] = old
    require(helper.TABLE_SHA == TABLE_SHA, 'shared helper accepted math table differs')
    return helper, [helper_record, fixture_record]

def fixture_module():
    return source_module(Path(__file__).with_name('geometry_fixtures.py'), '_caves_geometry_fixtures')

def no_links(path):
    for item in reversed((path, *path.parents)):
        try:
            info = item.lstat()
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(info.st_mode) and not (getattr(info, 'st_file_attributes', 0) & 0x400),
                'link/reparse output component: ' + str(item))

def guard_work(work, protected):
    no_links(work)
    for path in protected:
        require(work.resolve() != path and work.resolve() not in path.parents, 'work directory contains a supplied input/source')
    manifest = work / 'manifest.json'
    if os.path.lexists(manifest):
        no_links(manifest)
        require(manifest.is_file() and manifest.stat().st_nlink == 1, 'manifest is not independent regular storage')
        require(all(not os.path.samefile(manifest, path) for path in protected if path.exists()), 'manifest aliases supplied input')

def write_json(path, value):
    temporary = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    data = (json.dumps(value, indent=2, allow_nan=False) + '\n').encode()
    with temporary.open('xb') as stream:
        stream.write(data)
    os.replace(temporary, path)

def source_paths(root):
    names = ('CMakeLists.txt', 'scripts/reference/caves/compare_geometry.py',
             'scripts/reference/caves/geometry_fixtures.py', 'scripts/reference/caves/compare_caves.py',
             'scripts/reference/caves/fixtures.py', 'tests/reference/caves/CaveGeometryObservation.java',
             'tests/world/terrain/caves_geometry_probe.c', 'tests/tools/caves/test_geometry_reference.py',
             'src/world/terrain/caves.c', 'src/world/terrain/caves_internal.h', 'include/mc/world/terrain/caves.h',
             'src/core/java/java.c', 'include/mc/core/java/java.h', 'src/core/math/math.c', 'include/mc/core/math/math.h',
             'tests/world/terrain/test_caves.c', 'tests/world/terrain/caves_fixtures.h',
             'tests/world/terrain/test_caves_geometry.c', 'tests/world/terrain/caves_geometry_fixtures.h',
             'docs/reference/caves.md', 'docs/world/caves.md')
    return [root / name for name in names]

def signed(text, low, high):
    require(type(text) is str and re.fullmatch(r'-?(?:0|[1-9][0-9]*)', text), 'canonical integer required')
    value = int(text)
    require(low <= value <= high, 'request integer outside domain')
    return value

def raw_number(text, width):
    require(type(text) is str and re.fullmatch('[0-9a-f]{' + str(width) + '}', text), 'raw geometry bits required')
    raw = bytes.fromhex(text)
    bits = int(text, 16)
    mask = 0x7ff0000000000000 if width == 16 else 0x7f800000
    require(bits & mask != mask, 'finite geometry required')
    return struct.unpack('>d' if width == 16 else '>f', raw)[0]

def pattern(text):
    require(type(text) is str, 'pattern must be text')
    parts = text.split(':')
    if parts == ['stone']:
        return ('stone',)
    values = [signed(item, 0, 255) for item in parts[1:]]
    if len(parts) == 2 and parts[0] == 'solid':
        require(values[0] in (0, 1, 2, 3, 7, 12, 13, 24, 79, 128, 255), 'unreviewed solid ID')
    elif len(parts) == 5 and parts[0] == 'water':
        require(values[0] in (8, 9) and values[1] <= 15 and values[2] <= 127 and values[3] <= 15, 'water pattern domain')
    elif len(parts) == 3 and parts[0] == 'water-plane':
        require(values[0] in (8, 9) and values[1] <= 127, 'water plane domain')
    elif len(parts) == 2 and parts[0] == 'strata':
        require(values[0] <= 127, 'strata domain')
    else:
        raise Failure('unreviewed pattern recipe')
    return (parts[0], *values)

def validate_request(text):
    require(type(text) is str and len(text) <= 256 and text.isascii(), 'bounded ASCII request required')
    fields = text.split('\t')
    require((len(fields) == 8 and fields[0] == 'room') or (len(fields) == 13 and fields[0] == 'path'), 'room/path field count')
    signed(fields[1], -(1 << 63), (1 << 63) - 1)
    require(signed(fields[2], 0, 0) == 0 and signed(fields[3], 0, 0) == 0, 'protected target is origin only')
    x, y, z = (raw_number(value, 16) for value in fields[4:7])
    require(0 <= x <= 15 and 0 <= y <= 127 and 0 <= z <= 15, 'ordinary protected center required')
    pattern(fields[7])
    if fields[0] == 'path':
        width, yaw, pitch = (raw_number(value, 8) for value in fields[8:11])
        end = signed(fields[12], 85, 112)
        step = signed(fields[11], 0, end - 1)
        require(step < end and 0 <= width <= 3 and 0 <= yaw <= struct.unpack('>f', bytes.fromhex('40c90fdb'))[0] and
                -0.125 <= pitch <= 0.125, 'bounded protected path required')
    return fields

def request_data(module):
    cases = module.fixtures()
    require(len(cases) == 44 and module.REQUEST_SHA256 == REQUEST_SHA, 'protected request cardinality/hash metadata')
    for item in cases:
        validate_request(item.request)
    data = ''.join(item.request + '\n' for item in cases).encode('ascii')
    require(len(data) <= 16384 and hashlib.sha256(data).hexdigest() == REQUEST_SHA, 'reviewed protected whitelist differs')
    return cases, data

def owned_input(fields):
    values = bytearray(b'\x01' * 32768 + bytes.fromhex('8a8b8c'))
    recipe = pattern(fields[7])
    if recipe[0] == 'solid':
        values[:32768] = bytes([recipe[1]]) * 32768
    elif recipe[0] == 'water':
        _, block, x, y, z = recipe
        values[(x * 16 + z) * 128 + y] = block
    elif recipe[0] == 'water-plane':
        _, block, y = recipe
        for x in range(16):
            for z in range(16):
                values[(x * 16 + z) * 128 + y] = block
    elif recipe[0] == 'strata':
        top = recipe[1]
        column = bytes(3 if y < top else 2 if y == top else 0 for y in range(128))
        values[:32768] = column * 256
    return bytes(values)

def validate_record(value, index, fields, common):
    common.object_keys(value, KEYS, 'protected record')
    require(type(value['schema']) is int and value['schema'] == 2, 'protected schema version')
    require(type(value['case']) is int and value['case'] == index, 'protected case index')
    require(type(value['request']) is list and all(type(item) is str for item in value['request']) and value['request'] == fields,
            'raw protected request echo differs')
    before = common.block_descriptor(value['input_blocks'], 'protected input')
    after = common.block_descriptor(value['output_blocks'], 'protected output')
    require(before == owned_input(fields), 'reported input differs from owned pattern recipe')
    common.bit_list(value['before_continuation'], 4, 'before shared continuation')
    common.bit_list(value['after_continuation'], 4, 'after shared continuation')
    require(value['math_table_sha256_f32be'] == TABLE_SHA, 'protected math table digest differs')
    for name in ('suffix_unchanged', 'retained_random'):
        require(value[name] is True, name + ' control not true')
    require(before[32768:] == after[32768:], 'protected suffix bytes differ')
    require(type(value['range']) is int and value['range'] == 8, 'protected range differs')
    count = sum(a != b for a, b in zip(before[:32768], after[:32768]))
    require(type(value['changed_prefix_bytes']) is int and value['changed_prefix_bytes'] == count, 'observed changed count differs from bytes')
    control = value['shared_random_consumption']
    common.object_keys(control, ('basis', 'next_float_calls', 'next_long_calls', 'fork_inferred', 'candidates_matched'), 'shared consumption')
    room = fields[0] == 'room'
    require(control['basis'] == 'continuation_match' and type(control['next_float_calls']) is int and
            control['next_float_calls'] == (1 if room else 0), 'shared float control differs')
    require(type(control['next_long_calls']) is int and control['next_long_calls'] in ((1,) if room else (1, 3)), 'shared long candidate domain')
    require(type(control['fork_inferred']) is bool and control['fork_inferred'] == (not room and control['next_long_calls'] == 3), 'fork inference differs')
    require(type(control['candidates_matched']) is int and control['candidates_matched'] == 1, 'shared continuation candidate is not unique')
    return {'case': index, 'changed_prefix_bytes': count, 'shared_random_consumption': control}

def read_records(path, cases, common, artifact_root=None):
    with path.open('rb') as stream:
        digest = None
        if artifact_root is not None:
            header = stream.readline(65536 * 8 + 9)
            require(header.startswith(b'TABLE\t') and header.endswith(b'\n'), 'C measured TABLE header missing')
            payload = header[6:-1]
            if payload.endswith(b'\r'):
                payload = payload[:-1]
            require(len(payload) == 65536 * 8 and re.fullmatch(b'[0-9a-f]+', payload), 'C TABLE raw bits/cardinality')
            raw = bytes.fromhex(payload.decode('ascii'))
            digest = hashlib.sha256(raw).hexdigest()
            require(digest == TABLE_SHA, 'measured C table differs from accepted original table')
            with (artifact_root / 'c-table.f32be').open('xb') as output:
                output.write(raw)
        index = 0
        while True:
            line = stream.readline(MAX_LINE + 1)
            if not line:
                break
            require(len(line) <= MAX_LINE and line.endswith(b'\n') and line.startswith(b'OK\t'), 'bounded complete geometry record required')
            require(index < len(cases), 'extra protected response')
            value = common.strict_json(line[3:].decode('utf-8', errors='strict'))
            if digest is not None:
                common.object_keys(value, (KEYS - {'math_table_sha256_f32be'}) | {'table_unchanged'}, 'C protected record')
                require(value.pop('table_unchanged') is True, 'C table preservation marker not true')
                value['math_table_sha256_f32be'] = digest
            fields = cases[index].request.split('\t')
            observation = validate_record(value, index + 1, fields, common)
            yield value, observation
            index += 1
        if artifact_root is not None:
            require(index == len(cases), 'C protected response cardinality differs')

def lifecycle(path, count):
    require(path.stat().st_size <= MAX_STDERR, 'geometry stderr exceeds bound')
    phase, completed, controls = 'class', 0, 0
    unavailable = []
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith('UNAVAILABLE:'):
            unavailable.append({'line': line, 'phase': phase})
        elif line == 'GEOMETRY_READY':
            require(phase == 'class', 'GEOMETRY_READY out of order')
            phase = 'between'
        elif line.startswith('GEOMETRY_BEGIN'):
            require(phase == 'between' and completed < count and line == f'GEOMETRY_BEGIN case={completed + 1}', 'GEOMETRY_BEGIN differs/order')
            phase = 'measure'
        elif line.startswith('CONTROL_GEOMETRY'):
            require(phase == 'measure' and controls == completed and line == CONTROL, 'protected control differs/order')
            controls += 1
        elif line.startswith('GEOMETRY_DONE'):
            require(phase == 'measure' and controls == completed + 1 and line == f'GEOMETRY_DONE case={completed + 1}', 'GEOMETRY_DONE differs/order')
            completed += 1
            phase = 'between'
        elif line.startswith(('GEOMETRY_', 'CONTROL_')):
            raise Failure('unknown protected lifecycle/control marker')
    return {'phase': phase, 'completed': completed, 'controls': controls, 'unavailable': unavailable}

def unavailable_setup(process, markers, records):
    return (not process.stop_reason and not process.timed_out and process.returncode == 77 and records == 0 and markers['phase'] == 'class' and
            markers['completed'] == 0 and markers['controls'] == 0 and markers['unavailable'] ==
            [{'line': 'UNAVAILABLE: protected cave class setup failed', 'phase': 'class'}])

def c_controls(path, count):
    require(path.stat().st_size <= MAX_STDERR, 'C geometry stderr exceeds bound')
    expected = [line for index in range(1, count + 1) for line in (C_CONTROL, f'GEOMETRY_DONE case={index}')]
    require(path.read_text(encoding='utf-8').splitlines() == expected, 'C geometry controls/cardinality differ')
    return {'controls': count, 'completed': count}

def classes_inventory(directory):
    require({item.name for item in directory.iterdir()} == CLASS_NAMES, 'fresh protected helper class set differs')
    result = {}
    for name in sorted(CLASS_NAMES):
        path = directory / name
        no_links(path)
        require(path.stat().st_nlink == 1, 'protected helper class is not independent storage')
        with path.open('rb') as stream:
            header = stream.read(8)
        require(len(header) == 8 and header[:4] == b'\xca\xfe\xba\xbe' and header[6:] == b'\x00\x37', 'protected class must be major55')
        result[name] = record(path, 4 * 1024 * 1024)
    return result

def preserve(inputs, classes=None, request=None):
    for name, value in inputs.items():
        require(record(Path(name)) == value, 'attributed input changed: ' + name)
    if classes is not None:
        directory = Path(next(iter(classes.values()))['path']).parent
        require(classes_inventory(directory) == classes, 'compiled protected class bytes changed')
    if request is not None:
        require(record(Path(request['path']), 16384) == request, 'generated protected requests changed')

def save_records(records, side, root, common, comparisons=()):
    paths, observations = [], []
    for index, (value, observation) in enumerate(records):
        output = root / f'{side}-case-{index + 1:03d}.json'
        with output.open('xb') as stream:
            stream.write(json.dumps(value, separators=(',', ':'), allow_nan=False).encode() + b'\n')
        paths.append(output)
        observations.append(observation)
        for label, references in comparisons:
            require(index < len(references) and references[index].stat().st_size <= MAX_LINE, 'comparison prefix/cardinality bound')
            expected = common.strict_json(references[index].read_bytes())
            difference = common.first_difference(value, expected)
            require(difference is None, f'{label} case {index + 1}: {difference}')
    return paths, observations

def main(argv=None):
    helper = Path(__file__).resolve()
    root = helper.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--client', type=Path, default=root / 'b1.7.3.jar')
    parser.add_argument('--server', type=Path, default=root / 'b1.7.3_server.jar')
    for name in ('java', 'javac', 'javap'):
        parser.add_argument('--' + name)
    parser.add_argument('--work-dir', type=Path, default=root / '.local/reference/caves-geometry')
    parser.add_argument('--tested-commit')
    args = parser.parse_args(argv)
    if args.tested_commit and not re.fullmatch('[0-9a-fA-F]{40}', args.tested_commit):
        parser.error('--tested-commit requires 40 hexadecimal characters')
    work = Path(os.path.abspath(args.work_dir))
    executable = args.executable.resolve()
    jars = {'client': args.client.resolve(), 'server': args.server.resolve()}
    sources = source_paths(root)
    protected = {path.resolve() for path in [*sources, executable, *jars.values(), Path(sys.executable)]}
    for name in ('java', 'javac', 'javap'):
        value = getattr(args, name)
        if value and ('/' in value or '\\' in value):
            protected.add(Path(value).resolve())
    try:
        require(helper == (root / 'scripts/reference/caves/compare_geometry.py').resolve(),
                'executing helper is not the attributed canonical source path')
        guard_work(work, protected)
        natural_world_base = (root / '.local/reference/caves-setup').resolve()
        require(work.resolve() != natural_world_base and work.resolve() not in natural_world_base.parents and
                natural_world_base not in work.resolve().parents, 'geometry reports overlap reserved natural world roots')
    except (OSError, Failure) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    work.mkdir(parents=True, exist_ok=True)
    token = uuid.uuid4().hex
    stage = work / ('run-' + token)
    stage.mkdir()
    report = {'schema': 1, 'protocol': 2, 'status': 'running', 'run_id': token,
              'started_utc': datetime.now(timezone.utc).isoformat(), 'workspace': str(root), 'artifact_root': str(stage),
              'tested_commit': args.tested_commit.lower() if args.tested_commit else None,
              'commit_record': 'caller-supplied label; actual sources/executable recorded separately',
              'scope': 'Protected actual room/path entries on explicit owned patterns; no world construction or normal-scheduler coverage claim',
              'comparison_rules': ['Every complete response field, byte, raw request and shared continuation compares independently C/client, C/server and client/server',
                                   'C TABLE raw f32be bits are hashed; only true table_unchanged maps to that computed accepted digest',
                                   'Shared draw/fork control is unique continuation-candidate matching by original JDK Random and existing C primitive API; no geometry oracle',
                                   'Pattern bytes, changed-byte observations, suffix and retention controls are checked against actual raw records'],
              'limits': {'requests': 48, 'fixture_requests': 44, 'request_bytes': 16384, 'request_characters': 256,
                         'child_seconds': 30, 'java_heap_mib': 512, 'stdout_live_bytes': MAX_STDOUT,
                         'stderr_live_bytes': MAX_STDERR, 'record_bytes': MAX_LINE,
                         'process_policy': 'Direct-child timeout/kill and bounded cleanup; no operating-system process-tree or memory quota'},
              'request_sha256': REQUEST_SHA, 'math_table_sha256': TABLE_SHA, 'sides': {}, 'errors': [],
              'matched_records': {'client-to-C': 0, 'server-to-C': 0, 'client-to-server': 0}, 'completed': False}
    inputs, classes, request_before = {}, None, None
    completed, unavailable, common = False, None, None

    def save():
        write_json(stage / 'manifest.json', report)
        write_json(work / 'manifest.json', report)

    save()
    try:
        for path in [*sources, executable, *jars.values(), Path(sys.executable)]:
            if not path.is_file():
                raise Unavailable('required input file is missing: ' + str(path))
            item = record(path)
            inputs[item['path']] = item
            report['inputs_before'] = dict(inputs)
        for side, path in jars.items():
            require(inputs[str(path)]['sha256'] == JAR_HASHES[side], side + ': original JAR SHA-256 differs')
        common, executed = load_shared(root / 'scripts/reference/caves/compare_caves.py')
        fixture, fixture_source = fixture_module()
        for item in [*executed, fixture_source]:
            require(inputs.get(item['path']) == item, 'executed shared/fixture source differs from attributed bytes')
        report['executed_source_modules'] = [*executed, fixture_source]
        cases, data = request_data(fixture)
        tools = common.runtime_tools(args)
        for path in tools.values():
            item = record(path)
            inputs[item['path']] = item
        report['inputs_before'] = dict(inputs)
        guard_work(work, {Path(path) for path in inputs})
        request_path = stage / 'requests.tsv'
        with request_path.open('xb') as stream:
            stream.write(data)
        request_before = record(request_path, 16384)
        report['request_artifact_before'] = request_before
        write_json(stage / 'fixtures.json', [{'label': item.label, 'request': item.request} for item in cases])
        save()
        preserve(inputs, request=request_before)
        common.tool_versions(tools, stage, report)
        save()
        classes_dir = stage / 'classes'
        classes_dir.mkdir()
        empty_cp, empty_sp = stage / 'empty-classpath', stage / 'empty-sourcepath'
        empty_cp.mkdir()
        empty_sp.mkdir()
        command = [str(tools['javac']), '-J-Xmx512m', '--release', '11', '-Xlint:all', '-Werror', '-proc:none',
                   '-implicit:none', '-classpath', str(empty_cp), '-sourcepath', str(empty_sp), '-encoding', 'UTF-8',
                   '-d', str(classes_dir), str(root / 'tests/reference/caves/CaveGeometryObservation.java')]
        process = common.run_live(command, stage / 'compile-stdout.txt', stage / 'compile-stderr.txt', stage,
                                  seconds=30, stdout_limit=MAX_STDERR, stderr_limit=MAX_STDERR)
        report['compile'] = asdict(process)
        require(not process.stop_reason and process.returncode == 0, 'protected helper compilation failed')
        classes = classes_inventory(classes_dir)
        report['classes_before'] = classes
        preserve(inputs, classes, request_before)
        c_out, c_err = stage / 'C-stdout.tsv', stage / 'C-stderr.txt'
        process = common.run_live([str(executable)], c_out, c_err, stage, seconds=30,
                                  stdout_limit=MAX_STDOUT, stderr_limit=MAX_STDERR, stdin_path=request_path)
        report['sides']['C'] = {'status': 'running', 'process': asdict(process), 'records': 0}
        save()
        c_paths, observations = save_records(read_records(c_out, cases, common, stage), 'C', stage, common)
        report['sides']['C'].update(records=len(c_paths), observations=observations, controls=c_controls(c_err, len(cases)),
                                    case_artifacts=[record(path, MAX_LINE) for path in c_paths])
        require(not process.stop_reason and process.returncode == 0, 'protected C probe failed')
        report['sides']['C']['status'] = 'pass'
        report['c_table'] = record(stage / 'c-table.f32be', 65536 * 4)
        save()
        client_paths = None
        for side in ('client', 'server'):
            preserve(inputs, classes, request_before)
            command = [str(tools['java']), '-Xms32m', '-Xmx512m', '-Djava.awt.headless=true', '-cp', str(classes_dir),
                       'CaveGeometryObservation', side, str(jars[side]), str(request_path)]
            state = {'status': 'running', 'command': command, 'records': 0}
            report['sides'][side] = state
            save()
            stdout, stderr = stage / (side + '-stdout.tsv'), stage / (side + '-stderr.txt')
            process = common.run_live(command, stdout, stderr, stage, seconds=30,
                                      stdout_limit=MAX_STDOUT, stderr_limit=MAX_STDERR)
            state['process'] = asdict(process)
            save()
            comparisons = [(side + '-to-C', c_paths)]
            if side == 'server':
                comparisons.append(('client-to-server', client_paths))
            paths, observations = save_records(read_records(stdout, cases, common), side, stage, common, comparisons)
            state.update(records=len(paths), observations=observations,
                         case_artifacts=[record(path, MAX_LINE) for path in paths])
            report['matched_records'][side + '-to-C'] = len(paths)
            if side == 'server':
                report['matched_records']['client-to-server'] = len(paths)
            markers = lifecycle(stderr, len(cases))
            state['lifecycle'] = markers
            save()
            if unavailable_setup(process, markers, len(paths)):
                state['status'] = 'unavailable'
                raise Unavailable(side + ': protected class setup unavailable')
            require(not process.stop_reason and process.returncode == 0, side + ': protected measured process failed')
            require(len(paths) == len(cases) and markers['completed'] == len(cases) and markers['controls'] == len(cases) and
                    markers['phase'] == 'between' and not markers['unavailable'], side + ': protected lifecycle/cardinality differs')
            state['status'] = 'pass'
            save()
            if side == 'client':
                client_paths = paths
        require(all(value == len(cases) for value in report['matched_records'].values()), 'independent protected comparison cardinality')
        preserve(inputs, classes, request_before)
        completed = True
    except KeyboardInterrupt:
        report['errors'].append('protected comparison interrupted')
    except Exception as error:
        if isinstance(error, Unavailable) or (common is not None and isinstance(error, common.Unavailable)):
            unavailable = str(error)
        else:
            report['errors'].append(type(error).__name__ + ': ' + str(error))
    finally:
        after = {}
        for name, item in inputs.items():
            try:
                current = record(Path(name))
                after[name] = current
                require(current == item, 'attributed input changed: ' + name)
            except Exception as error:
                report['errors'].append(str(error))
        report['inputs_after'] = after
        report['inputs_preserved'] = after == inputs
        if classes is not None:
            try:
                current = classes_inventory(stage / 'classes')
                report['classes_after'] = current
                report['classes_preserved'] = current == classes
                require(current == classes, 'compiled protected classes changed')
            except Exception as error:
                report['classes_preserved'] = False
                report['errors'].append(str(error))
        if request_before is not None:
            try:
                current = record(Path(request_before['path']), 16384)
                report['request_artifact_after'] = current
                require(current == request_before, 'generated protected request artifact changed')
            except Exception as error:
                report['errors'].append(str(error))
        for state in report['sides'].values():
            if state['status'] == 'running':
                state['status'] = 'fail'
        report['completed'] = completed
        report['unavailable_reason'] = unavailable
        if not completed and not unavailable and not report['errors']:
            report['errors'].append('protected comparison did not complete all paired observations')
        report['status'] = 'fail' if report['errors'] else ('unavailable' if unavailable else 'pass')
        report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        artifacts = []
        for path in stage.iterdir():
            if path.is_file() and path.name != 'manifest.json':
                try:
                    maximum = MAX_STDOUT if path.name.endswith('-stdout.tsv') else MAX_LINE
                    if path.name.endswith('-stderr.txt'):
                        maximum = MAX_STDERR
                    artifacts.append(record(path, maximum))
                except Exception as error:
                    report['errors'].append(str(error))
                    report['status'] = 'fail'
        report['artifacts'] = sorted(artifacts, key=lambda item: item['path'])
        save()
    print(json.dumps({'status': report['status'], 'manifest': str(work / 'manifest.json'),
                      'manifest_sha256': record(work / 'manifest.json')['sha256'], 'matched_records': report['matched_records'],
                      'errors': report['errors'], 'unavailable_reason': unavailable}))
    return 1 if report['status'] == 'fail' else (77 if report['status'] == 'unavailable' else 0)


if __name__ == '__main__':
    sys.exit(main())
