#!/usr/bin/env python3
"""Export complete original-only level metadata records for Java-free replay.

The fixture retains exact original file bytes and the whole semantic observation.
Only host class-origin URLs, inventory roots and process command paths are
replaced by explicit logical names. Original-byte fingerprints remain separate.
"""
from __future__ import annotations

import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import re
import sys
import zlib

import compare_level as level

MAX_FIXTURE = 2*1024*1024
MAX_PACKED = level.MAX_RECORD
require = level.require


def checked_bytes(path, expected=None, maximum=MAX_PACKED, common=None):
    if common is not None:
        common.no_link_components(path)
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= maximum, 'bounded regular artifact: '+str(path))
    with path.open('rb') as stream:
        raw = stream.read(maximum+1)
    require(len(raw) <= maximum, 'artifact grew past bound')
    if expected is not None:
        require(level.same_json(level.fingerprint(raw), fingerprint(expected)), 'retained artifact fingerprint: '+str(path))
    return raw


def fingerprint(value):
    return level.fingerprint_record(value, path=type(value) is dict and 'path' in value)


def plain_fingerprint(value):
    level.keys(value, ('bytes', 'sha256'), 'public fingerprint without host paths or payloads')
    return fingerprint(value)


def retained_streams(process, directory, prefix, common):
    """Bind each complete native stream to the path the driver actually writes."""
    level.observed_process(process, paths=True)
    result = {}
    for name, suffix in (('stdout', 'out'), ('stderr', 'err')):
        path = directory/(prefix+'-'+suffix+'.txt')
        require(process[name]['path'] == str(path), 'canonical '+prefix+' '+name+' stream path')
        result[name] = checked_bytes(path, process[name], common=common)
    return result


def compile_command():
    return ['tool:javac', '-J-Xmx512m', '--release', '17', '-proc:none', '-implicit:none', '-encoding', 'UTF-8',
            '-Xlint:all', '-Werror', '-classpath', 'helper:empty', '-sourcepath', 'helper:empty', '--module-path',
            'helper:empty', '-d', 'helper:classes', 'helper:source/LevelObservation.java']


def packed(raw):
    require(type(raw) is bytes and len(raw) <= MAX_PACKED, 'bounded packed original bytes')
    return {**level.fingerprint(raw), 'zlib_base64': base64.b64encode(zlib.compress(raw, 9)).decode('ascii')}


def unpacked(value, maximum=MAX_PACKED):
    level.keys(value, ('bytes', 'sha256', 'zlib_base64'), 'packed original artifact')
    fp = plain_fingerprint({name: value[name] for name in ('bytes', 'sha256')})
    require(fp['bytes'] <= maximum and type(value['zlib_base64']) is str and len(value['zlib_base64']) <= 4*((maximum+1024+2)//3), 'packed byte budget')
    try:
        encoded = base64.b64decode(value['zlib_base64'], validate=True)
        decoder = zlib.decompressobj()
        raw = decoder.decompress(encoded, maximum+1)
        require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and len(raw) <= maximum and
                level.fingerprint(raw) == fp, 'one complete lossless packed stream')
        return raw
    except (ValueError, zlib.error) as error:
        raise level.Failure('invalid packed stream') from error


def relative_source(path):
    path = path.replace('\\', '/')
    if path.startswith('/mnt/') and len(path) > 7 and path[6] == '/':
        path = path[5].upper()+':'+path[6:]
    root = str(level.ROOT.resolve()).replace('\\', '/')
    if path.lower().startswith(root.lower()+'/'):
        name = path[len(root)+1:]
        if name == 'CMakeLists.txt' or name.endswith(('.c', '.h', '.py', '.java')):
            return name
    return None


def public_observation(value, side):
    out = copy.deepcopy(value)
    for record in out['class_origins'].values():
        record['origin'] = 'supplied:'+side
    return out


def public_inventory(value, label, observation_record=None):
    out = copy.deepcopy(value)
    out['root'] = 'case:'+label
    if observation_record is not None:
        found = False
        for row in out['entries']:
            if row['path'] == 'observations.json':
                out['bytes'] += observation_record['bytes']-row['bytes']
                row.update(observation_record)
                found = True
        require(found, 'complete observation file inventory')
    return out


def validate_provenance(value):
    level.keys(value, ('mode', 'manifest', 'tested_commit', 'source_fingerprints', 'executed_source_modules', 'helper_sources',
                       'helper_classes', 'helper_class_version', 'jar_fingerprints', 'tools', 'compilation', 'exporter_sha256', 'redaction'), 'fixture provenance')
    require(value['mode'] == 'original-only' and value['helper_class_version'] == 61 and type(value['helper_class_version']) is int,
            'original-only Java17 producer')
    plain_fingerprint(value['manifest'])
    require(value['tested_commit'] is None or type(value['tested_commit']) is str and re.fullmatch(r'[0-9a-f]{40}', value['tested_commit']), 'explicit complete source label')
    sources = value['source_fingerprints']
    executed_names = {'scripts/reference/chunk/compare_chunk.py', 'scripts/reference/caves/compare_caves.py', 'scripts/reference/caves/fixtures.py'}
    required = executed_names | {'CMakeLists.txt', 'scripts/reference/storage/compare_level.py', 'tests/reference/storage/LevelObservation.java',
                'tests/storage/level/level_probe.c', 'src/storage/level/level.c', 'include/mc/storage/level/level.h',
                'src/storage/nbt/nbt.c', 'include/mc/storage/nbt/nbt.h'}
    require(type(sources) is dict and required <= set(sources) and len(sources) <= 256, 'complete attributed codec source closure')
    for name, record in sources.items():
        require(type(name) is str and (name == 'CMakeLists.txt' or re.fullmatch(r'(?:scripts|tests|src|include)/[A-Za-z0-9_./-]+\.(?:c|h|py|java)', name)) and
                all(part not in ('', '.', '..') for part in name.split('/')), 'relative attributed source name')
        plain_fingerprint(record)
    level.keys(value['executed_source_modules'], executed_names, 'all loaded process helper sources')
    for name, record in value['executed_source_modules'].items():
        require(plain_fingerprint(record) == sources[name], 'loaded helper source fingerprint closure')
    level.keys(value['helper_sources'], ('LevelObservation.java', 'compare_level.py'), 'copied helper sources')
    for name, source in (('LevelObservation.java', 'tests/reference/storage/LevelObservation.java'),
                         ('compare_level.py', 'scripts/reference/storage/compare_level.py')):
        require(plain_fingerprint(value['helper_sources'][name]) == sources[source], 'copied exact helper and driver source fingerprints')
    classes = value['helper_classes']
    require(type(classes) is dict and set(classes) == {'LevelObservation.class', 'LevelObservation$Probe.class', 'LevelObservation$OriginalFailure.class'}, 'all project-owned helper classes')
    for record in classes.values():
        plain_fingerprint(record)
    require(type(value['jar_fingerprints']) is dict and set(value['jar_fingerprints']) == set(level.PIN), 'both original archives')
    for side, expected in (('client', 1465375), ('server', 503100)):
        require(plain_fingerprint(value['jar_fingerprints'][side]) == {'bytes': expected, 'sha256': level.PIN[side]}, 'exact pinned original archive')
    require(type(value['tools']) is dict and set(value['tools']) == {'java', 'javac', 'javap', 'python', 'jdk_release', 'jdk_modules', 'jdk_ct_sym'}, 'all runtime/compiler/Python/JDK inputs')
    for name, record in value['tools'].items():
        if name in ('java', 'javac', 'javap'):
            level.keys(record, ('major', 'output', 'executable'), 'actual tool version')
            plain_fingerprint(record['executable'])
            require(type(record['major']) is int and record['major'] >= 17 and type(record['output']) is str and len(record['output']) <= 131073, 'actual bounded JDK17 tool version')
        else:
            plain_fingerprint(record)
    compilation = value['compilation']
    level.keys(compilation, ('process', 'stdout', 'stderr'), 'complete retained compilation')
    level.observed_process(compilation['process'])
    require(compilation['process']['command'] == compile_command(), 'exact isolated Java17 compile command')
    for name in ('stdout', 'stderr'):
        raw = unpacked(compilation[name])
        require(compilation['process'][name] == level.fingerprint(raw) and compilation['process'][name+'_observed'] == len(raw),
                'complete compiler streams')
    level.hex_value(value['exporter_sha256'], 32, 'exporter source hash')
    require(value['redaction'] == 'Only class-origin URLs, inventory roots and process paths use logical names; all behavior and original file bytes remain, except observation JSON re-encoding for the class-origin URL replacement. Raw observation fingerprints are retained separately.', 'explicit exact public redaction policy')


def load_fixture(path):
    value = level.strict_json(checked_bytes(path, maximum=MAX_FIXTURE))
    validate_fixture(value)
    return value


def validate_fixture(value):
    level.keys(value, ('schema', 'scope', 'provenance', 'contexts'), 'complete public metadata fixture')
    require(type(value['schema']) is int and value['schema'] == 1 and value['scope'] ==
            'Pure metadata codec replay and retained original handler observations; no native handler, filesystem transaction or full persistence claim.', 'finite public schema and scope')
    validate_provenance(value['provenance'])
    contexts = value['contexts']
    require(type(contexts) is list and all(type(row) is dict for row in contexts), 'typed original contexts')
    require([(x.get('side'), x.get('repeat')) for x in contexts] ==
            [('client', 0), ('client', 1), ('server', 0), ('server', 1)] and all(type(x['repeat']) is int for x in contexts), 'complete ordered paired controls')
    validated = []
    for row in contexts:
        level.keys(row, ('side', 'repeat', 'observation_raw', 'observation', 'before', 'after', 'before_inventory',
                         'after_inventory', 'process', 'stdout', 'stderr', 'bindings'), 'complete public original context')
        side, repeat = row['side'], row['repeat']
        label = side+'-'+str(repeat)
        plain_fingerprint(row['observation_raw'])
        files = {}
        for phase in ('before', 'after'):
            require(type(row[phase]) is dict and len(row[phase]) <= 128, 'bounded complete public file map')
            files[phase] = {name: unpacked(record) for name, record in row[phase].items()}
            require(sum(map(len, files[phase].values())) <= 8*1024*1024, 'total public context byte budget')
            level.validate_inventory(row[phase+'_inventory'], files[phase], phase == 'after')
            require(row[phase+'_inventory']['root'] == 'case:'+label, 'logical public inventory root')
        raw = unpacked(row['observation'])
        require(raw == files['after']['observations.json'], 'same complete public observation bytes')
        observed = level.strict_json(raw)
        level.validate_original(observed, side, files['before'], files['after'], 'supplied:'+side)
        level.observed_process(row['process'])
        require(row['process']['command'] == ['tool:java', '-Xint', '-Xmx128m', '-cp', 'helper:classes', 'LevelObservation',
                                             'supplied:'+side, side, 'case:'+label, 'case:'+label+'/observations.json'], 'exact logical original command')
        level.validate_clock_window(observed, row['process'])
        out, err = unpacked(row['stdout']), unpacked(row['stderr'])
        require(row['process']['stdout'] == level.fingerprint(out) and row['process']['stderr'] == level.fingerprint(err) and
                row['process']['stdout_observed'] == len(out) and row['process']['stderr_observed'] == len(err) and
                out in (('PASS '+side+' metadata codec/handler observations\n').encode('ascii'),
                        ('PASS '+side+' metadata codec/handler observations\r\n').encode('ascii')), 'all original process streams retained')
        expected_bindings = {'classes': level.PINNED_CLASSES[side],
                             'methods': {name: {**record, **level.PINNED_CODE[side][name]} for name, record in level.binding_templates(side).items()},
                             'fields': level.field_templates(side)}
        require(level.same_json(row['bindings'], expected_bindings), 'all pinned method code fingerprints and class/field identities')
        validated.append({'side': side, 'repeat': repeat, 'after_files': files['after']})
    level.validate_controls(validated)
    return value


def export_capture(directory):
    common, executed = level.helpers()
    directory = common.no_link_components(directory).resolve(strict=True)
    manifest_raw = checked_bytes(directory/'manifest.json', maximum=4*1024*1024, common=common)
    report = level.strict_json(manifest_raw)
    require(report.get('schema') == 1 and type(report['schema']) is int and report.get('status') == 'pass' and
            report.get('mode') == 'original-only' and report.get('c_records') == [] and report.get('errors') == [] and
            report.get('artifact_root') == str(directory), 'complete successful original-only producer, no C output')
    require(report['comparisons'] == {'client/control': 'pass', 'server/control': 'pass', 'client/server': 'pass'}, 'all paired original/control comparisons')
    before = report['inputs_before']
    require(type(before) is dict and before and level.same_json(before, report['inputs_after']), 'all producer inputs preserved')
    sources, jars = {}, {}
    for path, record in before.items():
        require(record['path'] == path, 'exact attributed path')
        common.no_link_components(Path(path))
        require(fingerprint(common.file_record(Path(path), byte_limit=512*1024*1024)) == fingerprint(record), 'attributed producer bytes changed: '+path)
        name = relative_source(path)
        if name is not None:
            require(name not in sources, 'unique attributed source')
            sources[name] = fingerprint(record)
        for side, pin in level.PIN.items():
            if record['sha256'] == pin:
                require(side not in jars, 'unique original archive attribution')
                jars[side] = Path(path)
    require(set(jars) == set(level.PIN) and sources['scripts/reference/storage/compare_level.py'] == level.fingerprint(Path(level.__file__).read_bytes()), 'both originals and exact executing driver interpretation')
    loaded = {relative_source(row['path']): fingerprint(row) for row in report['executed_source_modules']}
    require(loaded == {relative_source(row['path']): fingerprint(row) for row in executed}, 'exact executed helper source closure')
    require({p.name for p in (directory/'source').iterdir()} == {'LevelObservation.java', 'compare_level.py'}, 'complete copied source inventory')
    helper_sources = {}
    for name, source in (('LevelObservation.java', 'tests/reference/storage/LevelObservation.java'), ('compare_level.py', 'scripts/reference/storage/compare_level.py')):
        checked_bytes(directory/'source'/name, sources[source], common=common)
        helper_sources[name] = sources[source]
    require(level.same_json(report['classes_before'], report['classes_after']), 'compiled helper inventory/bytes preserved')
    class_records = report['classes_before']
    require({p.name for p in (directory/'classes').iterdir()} == set(class_records) ==
            {'LevelObservation.class', 'LevelObservation$Probe.class', 'LevelObservation$OriginalFailure.class'}, 'complete actual class inventory')
    for name, record in class_records.items():
        raw = checked_bytes(directory/'classes'/name, record, common=common)
        require(raw[:8] == b'\xca\xfe\xba\xbe\x00\x00\x00\x3d', 'actual Java17 project-owned class')
    compiler_streams = retained_streams(report['compile'], directory, 'compile', common)
    compiler = report['versions']['javac']['command'][0]
    require(report['compile']['command'] == [compiler, '-J-Xmx512m', '--release', '17', '-proc:none', '-implicit:none', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-classpath', str(directory/'empty'), '-sourcepath', str(directory/'empty'), '--module-path', str(directory/'empty'), '-d', str(directory/'classes'), str(directory/'source/LevelObservation.java')], 'exact bounded isolated compile command')
    compilation_process = copy.deepcopy(report['compile'])
    compilation_process['command'] = compile_command()
    compilation = {'process': compilation_process}
    for name in ('stdout', 'stderr'):
        raw = compiler_streams[name]
        compilation[name] = packed(raw)
        compilation_process[name] = level.fingerprint(raw)
    tools = {}
    for name in ('java', 'javac', 'javap'):
        version = report['versions'][name]
        level.successful(version, ('major', 'output'))
        command = version['command']
        require(command == [command[0], '-version'] and command[0] in before, 'attributed exact tool version command')
        out = checked_bytes(directory/(name+'-version-stdout.txt'), maximum=65536, common=common)
        err = checked_bytes(directory/(name+'-version-stderr.txt'), maximum=65536, common=common)
        require((out+b'\n'+err).decode('utf-8') == version['output'] and type(version['major']) is int and version['major'] >= 17 and
                version['stdout_observed'] == len(out) and version['stderr_observed'] == len(err), 'complete actual JDK17 version output')
        tools[name] = {'major': version['major'], 'output': version['output'], 'executable': fingerprint(before[command[0]])}
    require(compiler == report['versions']['javac']['command'][0], 'actual attributed compiler')
    python_inputs = [record for path, record in before.items() if Path(path).resolve() == Path(sys.executable).resolve()]
    require(len(python_inputs) == 1, 'attributed executing Python')
    tools['python'] = fingerprint(python_inputs[0])
    java_home = Path(report['versions']['java']['command'][0]).parent.parent
    for name, relative in (('jdk_release', 'release'), ('jdk_modules', 'lib/modules'), ('jdk_ct_sym', 'lib/ct.sym')):
        path = str((java_home/relative).resolve())
        require(path in before, 'complete selected JDK files')
        tools[name] = fingerprint(before[path])
    contexts = []
    for row in report['runs']:
        side, repeat = row['side'], row['repeat']
        label = side+'-'+str(repeat)
        before_files = level.read_file_map(directory/(label+'-before'), common)
        after_files = level.read_file_map(directory/(label+'-after'), common)
        require(after_files == level.read_file_map(directory/label, common), 'archived and live observed files agree byte-exact')
        level.validate_inventory(row['before'], before_files)
        level.validate_inventory(row['after'], after_files, True)
        raw = checked_bytes(directory/label/'observations.json', row['observation'], common=common)
        observed = level.strict_json(raw)
        level.validate_original(observed, side, before_files, after_files, jars[side].resolve().as_uri().replace('file:///', 'file:/', 1))
        level.validate_clock_window(observed, row['process'])
        binding = level.original_bindings(observed, side, jars[side])
        require(level.same_json(binding, row['original_bindings']), 'recorded actual original code bindings')
        streams = retained_streams(row['process'], directory, label, common)
        java = report['versions']['java']['command'][0]
        require(row['process']['command'] == [java, '-Xint', '-Xmx128m', '-cp', str(directory/'classes'), 'LevelObservation',
                                             str(jars[side]), side, str(directory/label), str(directory/label/'observations.json')], 'exact genuine original command')
        stdout, stderr = streams['stdout'], streams['stderr']
        public = (json.dumps(public_observation(observed, side), separators=(',', ':'), ensure_ascii=True)+'\n').encode('ascii')
        after_files['observations.json'] = public
        process = copy.deepcopy(row['process'])
        process['command'] = ['tool:java', '-Xint', '-Xmx128m', '-cp', 'helper:classes', 'LevelObservation',
                              'supplied:'+side, side, 'case:'+label, 'case:'+label+'/observations.json']
        process['stdout'], process['stderr'] = level.fingerprint(stdout), level.fingerprint(stderr)
        contexts.append({'side': side, 'repeat': repeat, 'observation_raw': level.fingerprint(raw), 'observation': packed(public),
                         'before': {name: packed(data) for name, data in sorted(before_files.items())},
                         'after': {name: packed(data) for name, data in sorted(after_files.items())},
                         'before_inventory': public_inventory(row['before'], label),
                         'after_inventory': public_inventory(row['after'], label, level.fingerprint(public)),
                         'process': process, 'stdout': packed(stdout), 'stderr': packed(stderr), 'bindings': binding})
    fixture = {'schema': 1, 'scope': 'Pure metadata codec replay and retained original handler observations; no native handler, filesystem transaction or full persistence claim.',
               'provenance': {'mode': 'original-only', 'manifest': level.fingerprint(manifest_raw), 'tested_commit': report['tested_commit'],
                              'source_fingerprints': dict(sorted(sources.items())), 'executed_source_modules': loaded,
                              'helper_sources': helper_sources, 'helper_classes': {name: fingerprint(record) for name, record in class_records.items()},
                              'helper_class_version': 61, 'jar_fingerprints': {side: fingerprint(before[str(jar)]) for side, jar in jars.items()},
                              'tools': tools, 'compilation': compilation, 'exporter_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                              'redaction': 'Only class-origin URLs, inventory roots and process paths use logical names; all behavior and original file bytes remain, except observation JSON re-encoding for the class-origin URL replacement. Raw observation fingerprints are retained separately.'},
               'contexts': contexts}
    validate_fixture(fixture)
    return fixture


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        require(args.manifest.name == 'manifest.json', 'canonical capture manifest name')
        common, _ = level.helpers()
        common.no_link_components(args.output)
        common.samefile_or_ancestor(args.output, [args.manifest, Path(level.__file__), Path(__file__)])
        fixture = export_capture(args.manifest.resolve(strict=True).parent)
        raw = (json.dumps(fixture, indent=2, ensure_ascii=True, allow_nan=False)+'\n').encode('ascii')
        require(len(raw) <= MAX_FIXTURE, 'complete public fixture byte limit')
        with args.output.open('xb') as stream:
            stream.write(raw)
        print(json.dumps({'output': str(args.output), **level.fingerprint(raw)}))
        return 0
    except Exception as error:
        print('FAILURE: '+str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
