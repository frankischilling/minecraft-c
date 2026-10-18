#!/usr/bin/env python3
"""Export lossless original-only chunk records and finite public replay inputs.

Only host class origins, inventory roots and process argv paths become logical
names. Raw NBT, copied world files and thread inventories retain every byte.
Original method bodies remain private; public bindings retain pinned hashes.
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

import compare_chunk_storage as chunks

MAX_FIXTURE = 8*1024*1024
require = chunks.require


def fingerprint(value, maximum=512*1024*1024):
    chunks.keys(value, ('bytes', 'sha256'), 'plain typed public fingerprint')
    chunks.level.integer_value(value['bytes'], 0, maximum, 'fingerprint byte count')
    chunks.level.hex_value(value['sha256'], 32, 'fingerprint SHA256')
    return value


def file_fp(value):
    require(type(value) is dict, 'retained artifact record')
    return fingerprint({key: value[key] for key in ('bytes', 'sha256')})


def checked(path, common, expected=None, maximum=chunks.MAX_RECORD):
    common.no_link_components(path)
    record = common.file_record(path, byte_limit=maximum)
    with path.open('rb') as stream:
        raw = stream.read(maximum+1)
    require(len(raw) <= maximum and chunks.fp(raw) == file_fp(record) and
            (expected is None or chunks.fp(raw) == file_fp(expected)), 'bounded stable retained artifact: '+str(path))
    return raw


def packed(raw):
    require(type(raw) is bytes and len(raw) <= chunks.MAX_RECORD, 'bounded lossless original stream')
    return {**chunks.fp(raw), 'zlib_base64': base64.b64encode(zlib.compress(raw, 9)).decode('ascii')}


def unpacked(value, maximum=chunks.MAX_RECORD):
    chunks.keys(value, ('bytes', 'sha256', 'zlib_base64'), 'complete packed stream')
    expected = fingerprint({key: value[key] for key in ('bytes', 'sha256')}, maximum)
    require(type(value['zlib_base64']) is str and len(value['zlib_base64']) <= 4*((maximum+maximum//1000+4096+2)//3), 'bounded encoded stream')
    try:
        raw = base64.b64decode(value['zlib_base64'], validate=True)
        decoder = zlib.decompressobj()
        result = decoder.decompress(raw, expected['bytes']+1)
        require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and chunks.fp(result) == expected, 'one complete hashed lossless stream')
        return result
    except (ValueError, zlib.error) as error:
        raise chunks.Failure('invalid packed original stream') from error


def source_name(path):
    path = path.replace('\\', '/')
    root = str(chunks.ROOT.resolve()).replace('\\', '/')
    if path.lower().startswith(root.lower()+'/'):
        name = path[len(root)+1:]
        if name == 'CMakeLists.txt' or re.fullmatch(r'(?:src|include|scripts|tests)/[A-Za-z0-9_./-]+\.(?:c|h|py|java)', name):
            return name
    return None


def success(value, complete=False):
    require(type(value) is dict and type(value.get('returncode')) is int and value['returncode'] == 0 and value.get('timed_out') is False and
            value.get('stop_reason') is None and type(value.get('seconds')) in (int, float) and 0 <= value['seconds'] <= 120,
            'complete successful bounded process; no setup77 accepted')
    for key in ('stdout_observed', 'stderr_observed', 'stdout_retained', 'stderr_retained', 'inventory_polls'):
        chunks.level.integer_value(value[key], 0, chunks.MAX_RECORD, 'typed complete process counter')
    require(value['stdout_observed'] == value['stdout_retained'] and value['stderr_observed'] == value['stderr_retained'] and
            type(value['command']) is list and 1 <= len(value['command']) <= 64 and all(type(x) is str and 0 < len(x) <= 4096 for x in value['command']), 'complete bounded argv and streams')
    if complete:
        chunks.keys(value, ('command', 'returncode', 'seconds', 'timed_out', 'stop_reason', 'stdout_observed', 'stderr_observed',
                            'stdout_retained', 'stderr_retained', 'inventory_polls', 'stdout', 'stderr'), 'complete public process')
        fingerprint(value['stdout']); fingerprint(value['stderr'])


def logical_compile():
    return ['tool:javac', '-J-Xmx512m', '--release', '17', '-proc:none', '-implicit:none', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
            '-classpath', 'helper:empty', '-sourcepath', 'helper:empty', '--module-path', 'helper:empty', '-d', 'helper:classes',
            *['helper:source/'+Path(name).name for name in chunks.SOURCES]]


def logical_original(side, repeat):
    label = side+'-'+str(repeat)
    return ['tool:java', '-Xint', '-Xmx512m', '-Djava.awt.headless=true', '-cp', 'helper:classes', 'ChunkStorageObservation',
            'supplied:'+side, side, 'case:'+label+'/owned', 'case:'+label+'/snapshots.json']


def logical_prepare(side):
    return ['tool:java', '-Xint', '-Xmx512m', '-cp', 'helper:classes', 'CacheCallbacksObservation', 'supplied:'+side,
            side, 'prepare', '0000000000000017', 'prepare:'+side+'/owned', 'prepare:'+side+'/snapshots.json']


def public_record(value, side, prepare=False):
    result = copy.deepcopy(value)
    if prepare:
        result['class_origins'] = {name: 'supplied:'+side for name in result['class_origins']}
    else:
        for row in result['class_origins'].values():
            row['origin'] = 'supplied:'+side
    raw = (json.dumps(result, separators=(',', ':'), ensure_ascii=True, allow_nan=False)+'\n').encode('ascii')
    require(re.search(rb'[A-Za-z]:[\\/]', raw) is None and b'file:/' not in raw and b'bytecode_hex' not in raw, 'no host paths or original bytecode in public record')
    return raw


def public_inventory(value, root):
    result = copy.deepcopy(value); result['root'] = root
    return result


def validate_inventory(value, files, root, directories):
    chunks.keys(value, ('root', 'exists', 'entries', 'files', 'directories', 'bytes', 'complete', 'error'), 'complete public inventory')
    require(value['root'] == root and value['exists'] is True and value['complete'] is True and value['error'] is None and
            type(value['entries']) is list and len(value['entries']) <= 256, 'complete actual public inventory')
    for key in ('files', 'directories', 'bytes'):
        chunks.level.integer_value(value[key], 0, chunks.MAX_RECORD, 'typed inventory count')
    expected = [{'path': name, 'kind': 'file', **chunks.fp(raw)} for name, raw in files.items()]
    expected += [{'path': name, 'kind': 'directory'} for name in directories]
    expected.sort(key=lambda row: row['path'])
    require(chunks.same_json(value['entries'], expected) and value['files'] == len(files) and value['directories'] == len(directories) and
            value['bytes'] == sum(map(len, files.values())), 'every actual inventory entry and byte hash')


def file_map(value, maximum=chunks.MAX_RECORD):
    require(type(value) is dict and len(value) <= 128, 'bounded complete packed file inventory')
    result = {}
    for name, record in value.items():
        require(type(name) is str and re.fullmatch(r'[A-Za-z0-9_./-]{1,160}', name) and all(x not in ('', '.', '..') for x in name.split('/')), 'safe relative original artifact path')
        result[name] = unpacked(record, maximum)
    require(sum(map(len, result.values())) <= chunks.MAX_RECORD, 'complete unpacked context byte budget')
    return result


def validate_provenance(value):
    chunks.keys(value, ('mode', 'manifest', 'tested_commit', 'source_fingerprints', 'executed_source_modules', 'helper_sources',
                        'helper_classes', 'jar_fingerprints', 'tools', 'compilation', 'exporter_sha256', 'redaction'), 'complete original producer provenance')
    require(value['mode'] == 'original-only' and (value['tested_commit'] is None or type(value['tested_commit']) is str and re.fullmatch(r'[0-9a-f]{40}', value['tested_commit'])), 'original-only explicit source label')
    fingerprint(value['manifest']); chunks.level.hex_value(value['exporter_sha256'], 32, 'exporter source SHA256')
    sources = value['source_fingerprints']
    require(type(sources) is dict and 10 <= len(sources) <= 256 and
            {'CMakeLists.txt', 'scripts/reference/storage/compare_chunk_storage.py', 'scripts/reference/storage/compare_level.py',
             'tests/world/storage/chunk_probe.c', 'include/mc/world/storage/chunk.h', 'src/world/storage/chunk.c', *chunks.SOURCES} <= set(sources), 'complete attributed chunk-codec source closure')
    for name, row in sources.items():
        require(type(name) is str and (name == 'CMakeLists.txt' or re.fullmatch(r'(?:src|include|tests|scripts)/[A-Za-z0-9_./-]+\.(?:c|h|py|java)', name)) and
                all(part not in ('', '.', '..') for part in name.split('/')), 'relative source attribution only')
        fingerprint(row)
    expected_modules = {'scripts/reference/storage/compare_level.py', 'scripts/reference/chunk/compare_chunk.py', 'scripts/reference/caves/compare_caves.py', 'scripts/reference/caves/fixtures.py'}
    require(type(value['executed_source_modules']) is dict and set(value['executed_source_modules']) == expected_modules, 'every actually loaded Python helper')
    for name, row in value['executed_source_modules'].items():
        fingerprint(row); require(chunks.same_json(row, sources[name]), 'exact executed helper fingerprint')
    expected_sources = {Path(name).name: sources[name] for name in chunks.SOURCES}
    expected_sources['compare_chunk_storage.py'] = sources['scripts/reference/storage/compare_chunk_storage.py']
    require(type(value['helper_sources']) is dict and set(value['helper_sources']) == set(expected_sources), 'complete copied source inventory')
    for name, row in value['helper_sources'].items():
        fingerprint(row); require(chunks.same_json(row, expected_sources[name]), 'copied source attribution')
    classes = value['helper_classes']
    require(type(classes) is dict and set(classes) == chunks.CLASS_NAMES, 'all ten exact project-owned helper classes')
    for row in classes.values():
        fingerprint(row, 2*1024*1024)
    require(type(value['jar_fingerprints']) is dict and set(value['jar_fingerprints']) == set(chunks.level.PIN), 'both pinned original archives')
    for side, size in (('client', 1465375), ('server', 503100)):
        row = fingerprint(value['jar_fingerprints'][side])
        require(row == {'bytes': size, 'sha256': chunks.level.PIN[side]}, 'exact original archive bytes')
    tools = value['tools']
    require(type(tools) is dict and set(tools) == {'java', 'javac', 'javap', 'python', 'jdk_release', 'jdk_modules', 'jdk_ct_sym'}, 'all JDK and Python runtime files')
    for name, row in tools.items():
        if name in ('java', 'javac', 'javap'):
            chunks.keys(row, ('executable', 'major', 'output'), 'actual JDK version')
            fingerprint(row['executable']); chunks.level.integer_value(row['major'], 17, 1000, 'JDK17 or newer')
            require(type(row['output']) is str and 0 < len(row['output']) <= 131073, 'complete actual version output')
        else:
            fingerprint(row)
    compilation = value['compilation']
    chunks.keys(compilation, ('process', 'stdout', 'stderr'), 'complete isolated compilation')
    success(compilation['process'], True)
    require(compilation['process']['command'] == logical_compile(), 'exact isolated Java17 compilation argv')
    for stream in ('stdout', 'stderr'):
        raw = unpacked(compilation[stream]); require(compilation['process'][stream] == chunks.fp(raw) and compilation['process'][stream+'_observed'] == len(raw), 'all compile stream bytes')
    require(value['redaction'] == 'Only host class-origin URLs, inventory roots and process argv paths use logical names. Snapshot JSON is re-encoded solely for origin replacement; raw-record hashes remain separate. Every NBT/world/thread/stream byte is retained. Original method code is represented by pinned class/method fingerprints only.', 'explicit exact public artifact policy')


def validate_fixture(value):
    chunks.keys(value, ('schema', 'scope', 'provenance', 'preparations', 'contexts'), 'complete public chunk fixture')
    require(type(value['schema']) is int and value['schema'] == 1 and value['scope'] ==
            'Finite chunk codec and explicit normalization replay; original direct-region/factory observations retained. Fixed-shape rejection and opaque/unknown retention are C safety policies. No native world-filesystem, entity/tile attachment or full persistence claim.', 'finite public scope')
    validate_provenance(value['provenance'])
    common, old, _ = chunks.helpers()
    require(type(value['preparations']) is dict and set(value['preparations']) == set(chunks.level.PIN), 'both complete genuine saved-world preparations')
    baseline_files = {}
    for side, row in value['preparations'].items():
        chunks.keys(row, ('record', 'raw_record', 'files', 'inventory', 'process', 'stdout', 'stderr'), 'complete public original preparation')
        fingerprint(row['raw_record'])
        files = file_map(row['files'])
        validate_inventory(row['inventory'], files, 'prepare:'+side, {'world', 'world/data', 'world/region'})
        original = chunks.strict_json(unpacked(row['record']))
        chunks.validate_preparation(original, side, files, 'supplied:'+side, old)
        success(row['process'], True)
        require(row['process']['command'] == logical_prepare(side), 'exact logical genuine creator argv')
        for stream in ('stdout', 'stderr'):
            raw = unpacked(row[stream]); require(row['process'][stream] == chunks.fp(raw) and row['process'][stream+'_observed'] == len(raw), 'all actual preparation streams')
        baseline_files[side] = files
    contexts = value['contexts']
    require(type(contexts) is list and [(row.get('side'), row.get('repeat')) for row in contexts] ==
            [('client', 0), ('client', 1), ('server', 0), ('server', 1)] and all(type(row['repeat']) is int for row in contexts), 'all four ordered side/control contexts')
    signatures, controls = {}, {}
    for row in contexts:
        chunks.keys(row, ('side', 'repeat', 'record', 'raw_record', 'artifacts', 'artifact_inventory', 'world_before', 'world_after',
                         'world_before_inventory', 'world_after_inventory', 'world_changed', 'threads_before', 'threads_after',
                         'process', 'stdout', 'stderr', 'bindings'), 'complete public original context')
        side, repeat = row['side'], row['repeat']; label = side+'-'+str(repeat)
        fingerprint(row['raw_record'])
        artifacts = file_map(row['artifacts'], chunks.MAX_NBT)
        validate_inventory(row['artifact_inventory'], artifacts, 'case:'+label+'/artifacts', set())
        before, after = file_map(row['world_before']), file_map(row['world_after'])
        for phase, files in (('before', before), ('after', after)):
            validate_inventory(row['world_'+phase+'_inventory'], files, 'case:'+label+'/owned', {'world', 'world/data', 'world/region'})
        require(before == baseline_files[side] | {'.chunk-storage-owned': chunks.MARKER}, 'byte-exact complete fresh genuine baseline copy')
        require(row['world_changed'] == chunks.world_changes(before, after), 'all actual copied-world changes')
        raw = unpacked(row['record']); original = chunks.strict_json(raw)
        before_threads = chunks.strict_json(unpacked(row['threads_before'], 1024*1024))
        after_threads = chunks.strict_json(unpacked(row['threads_after'], 1024*1024))
        chunks.validate_original(original, side, artifacts, before_threads, after_threads, 'supplied:'+side, old)
        require(chunks.same_json(chunks.level.order_only(chunks.parse_raw(chunks.artifact(original['saved_baseline'], artifacts))),
                                 chunks.level.order_only(chunks.baseline_record(baseline_files[side]))), 'whole saved baseline NBT agrees with genuine creator bytes')
        require(chunks.same_json(row['bindings'], chunks.BINDING_PINS[side]), 'all pinned original method/class fingerprints; no raw original bytecode')
        success(row['process'], True)
        require(row['process']['command'] == logical_original(side, repeat), 'exact logical untouched original argv')
        for stream in ('stdout', 'stderr'):
            data = unpacked(row[stream]); require(row['process'][stream] == chunks.fp(data) and row['process'][stream+'_observed'] == len(data), 'all actual original stream bytes')
        require(unpacked(row['stdout']) == b'', 'original diagnostic output retained on stderr only')
        signature = chunks.original_signature(original, artifacts)
        if side not in signatures:
            signatures[side] = signature; controls[side] = (raw, artifacts)
        else:
            require(controls[side] == (raw, artifacts), 'entire original/control public record and raw NBT byte equality')
    require(chunks.same_json(signatures['client'], signatures['server']), 'all paired original full projections and typed NBT, compound order only')
    return value


def load_fixture(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= MAX_FIXTURE, 'bounded public chunk fixture')
    with path.open('rb') as stream:
        raw = stream.read(MAX_FIXTURE+1)
    require(len(raw) <= MAX_FIXTURE, 'complete bounded fixture bytes')
    return validate_fixture(chunks.strict_json(raw))


def normalized_process(record, command, directory, common, *, prefix):
    chunks.keys(record, ('command', 'returncode', 'seconds', 'timed_out', 'stop_reason', 'stdout_observed', 'stderr_observed',
                        'stdout_retained', 'stderr_retained', 'inventory_polls', 'stdout', 'stderr'), 'complete actual native process')
    success(record)
    require(prefix in ('compile', 'prepare', 'original'), 'known actual process stream prefix')
    result = copy.deepcopy(record); result['command'] = command
    streams = {}
    for name, suffix in (('stdout', 'out'), ('stderr', 'err')):
        path = directory/(prefix+'-'+suffix+'.txt')
        chunks.keys(record[name], ('path', 'bytes', 'sha256'), 'complete actual stream fingerprint')
        require(record[name]['path'] == str(path), 'canonical '+prefix+' '+name+' stream path')
        raw = checked(path, common, record[name], maximum=chunks.MAX_RECORD if name == 'stdout' else 4*1024*1024)
        require(record[name+'_observed'] == len(raw) and record[name+'_retained'] == len(raw),
                'complete '+prefix+' '+name+' stream counters match actual bytes')
        streams[name] = packed(raw); result[name] = chunks.fp(raw)
    return {'process': result, **streams}


def normalized_version(record, name, directory, before, common):
    chunks.keys(record, ('command', 'returncode', 'seconds', 'timed_out', 'stop_reason', 'stdout_observed', 'stderr_observed',
                        'stdout_retained', 'stderr_retained', 'inventory_polls', 'output', 'major'), 'complete actual tool version process')
    success(record)
    require(name in ('java', 'javac', 'javap'), 'known attributed JDK tool')
    command = record['command']
    require(command == [command[0], '-version'] and command[0] in before, 'exact attributed JDK version command')
    chunks.level.integer_value(record['major'], 17, 1000, 'actual JDK17 or newer')
    streams = {}
    for stream in ('stdout', 'stderr'):
        streams[stream] = checked(directory/(name+'-version-'+stream+'.txt'), common, maximum=65536)
        require(record[stream+'_observed'] == len(streams[stream]) and record[stream+'_retained'] == len(streams[stream]),
                'complete actual '+name+' version '+stream+' counters match retained bytes')
    require((streams['stdout']+b'\n'+streams['stderr']).decode('utf-8') == record['output'], 'whole actual tool version output')
    return {'executable': file_fp(before[command[0]]), 'major': record['major'], 'output': record['output']}


def export_capture(directory):
    common, old, executed = chunks.helpers()
    directory = common.no_link_components(directory).resolve(strict=True)
    manifest_raw = checked(directory/'manifest.json', common, maximum=8*1024*1024)
    report = chunks.strict_json(manifest_raw)
    require(type(report.get('schema')) is int and report['schema'] == 1 and report.get('status') == 'pass' and report.get('scope') == 'chunk-storage-codec' and
            report.get('mode') == 'original-only' and report.get('errors') == [] and report.get('c_records') == [] and report.get('artifact_root') == str(directory), 'complete successful original-only chunk producer without C expectations')
    require(report['comparisons'] == {'client/control': 'byte-equal-complete-records-and-NBT', 'server/control': 'byte-equal-complete-records-and-NBT', 'client/server': 'complete-supported-projection-and-typed-NBT'}, 'all complete paired/control comparisons')
    before = report['inputs_before']
    require(type(before) is dict and before and chunks.same_json(before, report['inputs_after']), 'all attributed source/tool/original inputs preserved')
    sources, jars = {}, {}
    for name, row in before.items():
        require(row.get('path') == name, 'exact retained input path')
        common.no_link_components(Path(name))
        require(file_fp(common.file_record(Path(name), byte_limit=512*1024*1024)) == file_fp(row), 'attributed producer bytes changed: '+name)
        source = source_name(name)
        if source is not None:
            require(source not in sources, 'unique attributed source'); sources[source] = file_fp(row)
        for side, pin in chunks.level.PIN.items():
            if row['sha256'] == pin:
                require(side not in jars, 'unique selected original archive'); jars[side] = Path(name)
    require(set(jars) == set(chunks.level.PIN) and sources['scripts/reference/storage/compare_chunk_storage.py'] == chunks.fp(Path(chunks.__file__).read_bytes()), 'both originals and exact executing driver interpretation')
    loaded = {source_name(row['path']): file_fp(row) for row in report['executed_source_modules']}
    require(chunks.same_json(loaded, {source_name(row['path']): file_fp(row) for row in executed}), 'all exact executing dependency sources')
    copied_sources = {Path(name).name: sources[name] for name in chunks.SOURCES}
    copied_sources['compare_chunk_storage.py'] = sources['scripts/reference/storage/compare_chunk_storage.py']
    require({path.name for path in (directory/'source').iterdir()} == set(copied_sources), 'all copied source files')
    for name, row in copied_sources.items():
        checked(directory/'source'/name, common, row, maximum=2*1024*1024)
    require(chunks.same_json(report['classes_before'], report['classes_after']) and
            {path.name for path in (directory/'classes').iterdir()} == set(report['classes_before']), 'all compiled class names/bytes preserved')
    classes = {}
    for name, row in report['classes_before'].items():
        raw = checked(directory/'classes'/name, common, row, maximum=2*1024*1024)
        require(raw[:8] == b'\xca\xfe\xba\xbe\x00\x00\x00\x3d', 'actual Java17 helper bytecode')
        classes[name] = file_fp(row)
    tools = {}
    for name in ('java', 'javac', 'javap'):
        tools[name] = normalized_version(report['versions'][name], name, directory, before, common)
    java, javac = report['versions']['java']['command'][0], report['versions']['javac']['command'][0]
    python = [row for name, row in before.items() if Path(name).resolve() == Path(sys.executable).resolve()]
    require(len(python) == 1, 'attributed executing Python'); tools['python'] = file_fp(python[0])
    java_home = Path(java).parent.parent
    for name, relative in (('jdk_release', 'release'), ('jdk_modules', 'lib/modules'), ('jdk_ct_sym', 'lib/ct.sym')):
        tools[name] = file_fp(before[str((java_home/relative).resolve())])
    require(report['compile']['command'] == [javac, '-J-Xmx512m', '--release', '17', '-proc:none', '-implicit:none', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
            '-classpath', str(directory/'empty'), '-sourcepath', str(directory/'empty'), '--module-path', str(directory/'empty'), '-d', str(directory/'classes'),
            *[str(directory/'source'/Path(name).name) for name in chunks.SOURCES]], 'actual isolated Java17 compilation argv')
    result = {'schema': 1, 'scope': 'Finite chunk codec and explicit normalization replay; original direct-region/factory observations retained. Fixed-shape rejection and opaque/unknown retention are C safety policies. No native world-filesystem, entity/tile attachment or full persistence claim.',
              'provenance': {'mode': 'original-only', 'manifest': chunks.fp(manifest_raw), 'tested_commit': report['tested_commit'],
                             'source_fingerprints': dict(sorted(sources.items())), 'executed_source_modules': loaded, 'helper_sources': copied_sources,
                             'helper_classes': classes, 'jar_fingerprints': {side: file_fp(before[str(jar)]) for side, jar in jars.items()},
                             'tools': tools, 'compilation': normalized_process(report['compile'], logical_compile(), directory, common, prefix='compile'),
                             'exporter_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                             'redaction': 'Only host class-origin URLs, inventory roots and process argv paths use logical names. Snapshot JSON is re-encoded solely for origin replacement; raw-record hashes remain separate. Every NBT/world/thread/stream byte is retained. Original method code is represented by pinned class/method fingerprints only.'},
              'preparations': {}, 'contexts': []}
    for side, row in report['preparations'].items():
        path = directory/('prepare-'+side); raw = checked(path/'snapshots.json', common, row['snapshot'])
        world, inventory = chunks.files_in(path/'owned', common)
        baseline, baseline_inventory = chunks.files_in(path/'baseline', common)
        require(chunks.same_json(inventory, row['world']) and baseline == world and chunks.same_json(row['baseline_before'], row['baseline_after']) and
                chunks.same_json(baseline_inventory, row['baseline_before']), 'complete genuine saved world and immutable baseline inventories')
        original = chunks.strict_json(raw)
        chunks.validate_preparation(original, side, world, jars[side].as_uri().replace('file:///', 'file:/', 1), old)
        expected = [java, '-Xint', '-Xmx512m', '-cp', str(directory/'classes'), 'CacheCallbacksObservation', str(jars[side]), side,
                    'prepare', '0000000000000017', str(path/'owned'), str(path/'snapshots.json')]
        require(row['process']['command'] == expected, 'exact actual genuine creator argv')
        result['preparations'][side] = {'record': packed(public_record(original, side, True)), 'raw_record': chunks.fp(raw),
                                        'files': {name: packed(data) for name, data in world.items()},
                                        'inventory': public_inventory(inventory, 'prepare:'+side), **normalized_process(row['process'], logical_prepare(side), path, common, prefix='prepare')}
    for row in report['runs']:
        side, repeat = row['side'], row['repeat']; label = side+'-'+str(repeat); path = directory/label
        snapshot = path/'snapshots.json'; raw = checked(snapshot, common, row['snapshot']); original = chunks.strict_json(raw)
        artifacts, artifact_inventory = chunks.files_in(Path(str(snapshot)+'.nbt'), common, chunks.MAX_NBT)
        require(chunks.same_json(artifact_inventory, row['artifacts']), 'complete exact raw NBT artifact inventory')
        before_files, _ = chunks.files_in(path/'before', common); after_files, _ = chunks.files_in(path/'after', common)
        live_files, live_inventory = chunks.files_in(path/'owned', common)
        require(live_files == after_files and chunks.same_json(live_inventory, row['world_after']), 'complete archived and live observed world bytes')
        threads = {phase: checked(Path(str(snapshot)+'.threads-'+phase+'.json'), common, row['threads'][phase], maximum=1024*1024) for phase in ('before', 'after')}
        chunks.validate_original(original, side, artifacts, chunks.strict_json(threads['before']), chunks.strict_json(threads['after']), jars[side].as_uri().replace('file:///', 'file:/', 1), old)
        chunks.verify_bindings(side, jars[side]); require(chunks.same_json(row['bindings'], chunks.BINDING_PINS[side]), 'actual pinned original method/code fingerprints')
        require(row['process']['command'] == [java, '-Xint', '-Xmx512m', '-Djava.awt.headless=true', '-cp', str(directory/'classes'), 'ChunkStorageObservation', str(jars[side]), side,
                                             str(path/'owned'), str(snapshot)], 'exact actual untouched original argv')
        result['contexts'].append({'side': side, 'repeat': repeat, 'record': packed(public_record(original, side)), 'raw_record': chunks.fp(raw),
                                   'artifacts': {name: packed(data) for name, data in artifacts.items()},
                                   'artifact_inventory': public_inventory(artifact_inventory, 'case:'+label+'/artifacts'),
                                   'world_before': {name: packed(data) for name, data in before_files.items()}, 'world_after': {name: packed(data) for name, data in after_files.items()},
                                   'world_before_inventory': public_inventory(row['world_before'], 'case:'+label+'/owned'),
                                   'world_after_inventory': public_inventory(row['world_after'], 'case:'+label+'/owned'), 'world_changed': row['world_changed'],
                                   'threads_before': packed(threads['before']), 'threads_after': packed(threads['after']), 'bindings': row['bindings'],
                                   **normalized_process(row['process'], logical_original(side, repeat), path, common, prefix='original')})
    return validate_fixture(result)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        require(args.manifest.name == 'manifest.json' and Path(__file__).resolve() == (chunks.ROOT/'scripts/reference/storage/export_chunk_storage.py').resolve(), 'canonical exporter and capture manifest')
        common, _, _ = chunks.helpers(); common.no_link_components(args.output)
        common.samefile_or_ancestor(args.output, [args.manifest, Path(chunks.__file__), Path(__file__)])
        value = export_capture(args.manifest.resolve(strict=True).parent)
        raw = (json.dumps(value, indent=2, ensure_ascii=True, allow_nan=False)+'\n').encode('ascii')
        require(len(raw) <= MAX_FIXTURE, 'complete bounded public chunk fixture')
        with args.output.open('xb') as stream:
            stream.write(raw)
        print(json.dumps({'output': str(args.output), **chunks.fp(raw)}))
        return 0
    except Exception as error:
        print('FAILURE: '+str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
