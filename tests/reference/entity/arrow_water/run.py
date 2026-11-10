"""Compare finite genuine original and native Arrow water World calls."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
LIMIT = 8 * 1024 ** 2
MODES=('water-first','water-multi','dry-to-water','water-to-air','null-callback')
def need(v,m):
    if not v: raise ValueError(m)
def pin(p):
    raw=Path(p).read_bytes(); return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
def load(p,n):
    spec=importlib.util.spec_from_file_location(n,p); value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
def files(p): return {str(x.relative_to(p)).replace('\\','/'):pin(x) for x in sorted(p.rglob('*')) if x.is_file()}
def returned(c,command,timeout,limit=LIMIT):
    need(type(c) is subprocess.CompletedProcess and c.args==command,'actual returned child')
    e=c.capture_report
    for k,v in {'command':command,'exit':c.returncode,'timed_out':False,'timeout_seconds':timeout,'pipe_capture_complete':True,'reader_errors':{},'output_overflow':False,'stdout_bytes':len(c.stdout),'stderr_bytes':len(c.stderr),'limit':limit,'reaped':True}.items():
        need(type(e[k]) is type(v) and e[k]==v,'complete capture '+k)
    for name,raw in [('stdout',c.stdout),('stderr',c.stderr)]: need((Path(c.cwd)/(c.prefix+'.'+name)).read_bytes()==raw,'retained '+name)
def observe(c,mode,classes):
    rows=c.stdout.decode('utf-8').splitlines(); need(rows[-1].startswith('ARROW_WATER_ORIGINAL_COMPLETE\t'+mode+'\t'),'complete EOF')
    need(rows.count('16 achievements')==1 and rows.count('151 recipes')==1,'original bootstrap output')
    widths={'ARROW_WATER_ORIGINAL_BEGIN':3,'ORIGIN':2,'SETUP':5,'SNAPSHOT':6,'FIELD':7,'RANDOM':7,'LIST':5,'LIST_ITEM':5,'CHUNK':7,'PREPARE_QUIET_SET':7,'CALL':4,'OUTCOME':None,'WATER_EVENT':18,'WATER_CALLBACKS':2,'ARROW_WATER_ORIGINAL_SOURCE_COMPLETE':4,'ARROW_WATER_ORIGINAL_REGION_CLOSED':2,'GUARD':5,'ARROW_WATER_ORIGINAL_COMPLETE':5}
    parsed=[]
    for row in rows:
        if row in ('16 achievements','151 recipes'): continue
        cols=row.split('\t'); need(cols[0] in widths,'unconsumed row '+row[:90]); need(len(cols)==widths[cols[0]] if widths[cols[0]] is not None else len(cols) in (3,4),'row width '+cols[0]); parsed.append(cols)
    need(parsed[0]==['ARROW_WATER_ORIGINAL_BEGIN','1',mode] and int(parsed[-1][-1])==len(parsed),'complete row count')
    need(parsed[-2]==['GUARD','0','0','0','0'],'zero denied side effects')
    need(parsed[-3]==['ARROW_WATER_ORIGINAL_REGION_CLOSED',mode],'real region close')
    events=[r for r in parsed if r[0]=='WATER_EVENT']; need(len(events)=={'water-first':4,'water-multi':None,'dry-to-water':27,'water-to-air':4,'null-callback':1}[mode] if mode!='water-multi' else len(events)<=64,'event count')
    need([int(e[2]) for e in events]==list(range(1,len(events)+1)),'ordered callback indices')
    for event in events:
        need(event[3] in ('true','false') and event[14] in ('true','false') and event[16] in ('true','false'),'typed callback flags')
        need(all(re.fullmatch('[0-9a-f]{16}',x) for x in event[5:11]+event[17:18]) and all(re.fullmatch('[0-9a-f]{8}',x) for x in event[11:13]),'raw float callback words')
    if mode=='null-callback': need([r for r in parsed if r[0]=='OUTCOME'][-1][2:]==['threw','java.lang.NullPointerException'],'actual null source exception')
    origin=classes.as_uri().replace('file:///','file:/',1)+'/'
    need(c.stderr.decode().splitlines()==['WARNING: A terminally deprecated method in java.lang.System has been called','WARNING: System::setSecurityManager has been called by ArrowWaterOriginalObservation ('+origin+')','WARNING: Please consider reporting this to the maintainers of ArrowWaterOriginalObservation','WARNING: System::setSecurityManager will be removed in a future release'],'exact JDK warning')
    return {'rows':len(parsed),'events':len(events),'calls':int(parsed[-1][2]),'snapshots':int(parsed[-1][3]),'observations':parsed}


STATE_MAPPING = {
    'x': 'sn.aM', 'y': 'sn.aN', 'z': 'sn.aO', 'velocity_x': 'sn.aP',
    'velocity_y': 'sn.aQ', 'velocity_z': 'sn.aR', 'yaw': 'sn.aS', 'pitch': 'sn.aT',
    'previous_yaw': 'sn.aU', 'previous_pitch': 'sn.aV', 'min_x': 'eq.a', 'min_y': 'eq.b',
    'min_z': 'eq.c', 'max_x': 'eq.d', 'max_y': 'eq.e', 'max_z': 'eq.f',
    'age': 'sn.bt', 'flight': 'sl.k', 'embedded': 'sl.i', 'shake': 'sl.b',
    'wet': 'sn.bx', 'dead': 'sn.be', 'first': 'sn.c',
}
EVENT_FIELDS = ('age', 'ordinal', 'sound', 'name', 'x', 'y', 'z', 'velocity_x', 'velocity_y',
                'velocity_z', 'volume', 'pitch', 'wet', 'random_state', 'random_cached',
                'random_gaussian')


def compare(native, original):
    rows = original['observations']
    snapshots = {int(row[1]): row[2:] for row in rows if row[0] == 'SNAPSHOT'}
    fields = {index: {} for index in snapshots}
    randoms = {}
    for row in rows:
        if row[0] == 'FIELD':
            fields[int(row[1])][row[3] + '.' + row[4]] = row[6]
        if row[0] == 'RANDOM' and row[2] == 'arrow.bs':
            randoms[int(row[1])] = dict(zip(
                ('random_state', 'random_cached', 'random_gaussian'), row[4:7]))
    result = {'state_pairs': [], 'event_pairs': [], 'state_exact_cells': 0,
              'event_exact_cells': 0, 'state_differences': [], 'event_differences': []}
    for state in native['states']:
        candidates = [index for index, value in fields.items()
                      if snapshots[index][0] in ('returned', 'failed') and
                      value.get('sn.bt') == state['age']]
        need(len(candidates) == 1, 'One whole actual original snapshot for each native tick')
        index = candidates[0]
        source = {name: fields[index][field] for name, field in STATE_MAPPING.items()}
        source.update(randoms[index])
        pairs = []
        for name, value in state.items():
            same = value == source[name]
            pair = {'field': name, 'native': value, 'original': source[name], 'exact': same}
            pairs.append(pair)
            if same:
                result['state_exact_cells'] += 1
            else:
                result['state_differences'].append({'age': state['age'], **pair})
                need(name in ('random_state', 'random_gaussian'),
                     'Deterministic actual state mismatch: ' + name)
        result['state_pairs'].append({'age': state['age'], 'fields': pairs})
    events = [row for row in rows if row[0] == 'WATER_EVENT']
    need(len(events) == len(native['events']), 'Same complete actual callback extent')
    for target, source in zip(native['events'], events):
        need(source[1] == source[13], 'Original callback age equals entered World call count')
        values = source[1:13] + source[14:18]
        # Represent the identical 48-bit integer with the native raw-word spelling.
        values[-3] = f'{int(values[-3]):016x}'
        source = dict(zip(EVENT_FIELDS, values))
        pairs = []
        for name, value in source.items():
            same = target[name] == value
            pair = {'field': name, 'native': target[name], 'original': value, 'exact': same}
            pairs.append(pair)
            if same:
                result['event_exact_cells'] += 1
            else:
                result['event_differences'].append({'ordinal': target['ordinal'], **pair})
                stochastic = target['parent_pc'] == '1' and (
                    target['sound'] == 'true' and name == 'pitch' or
                    target['sound'] == 'false' and (name in ('x', 'z') or
                        target['name'] == 'bubble' and name == 'velocity_y'))
                need(name in ('random_state', 'random_gaussian') or stochastic,
                     'Deterministic actual callback mismatch: ' + name)
        result['event_pairs'].append({'ordinal': target['ordinal'], 'fields': pairs})
    return result


def validate_inventory(original, inventories):
    rows = original['observations']
    need([row[1] for row in rows if row[0] == 'ORIGIN'] ==
         ['fd', 'mx', 'fm', 'wt', 'xa', 'rh', 'ok', 'ld', 'lm', 'li', 'sn', 'sl',
          'eq', 'bt', 'vf', 'in', 'uu', 'to', 'as', 'nu', 'rj', 'qj'],
         'Every declared genuine original class origin')
    snapshots = [row for row in rows if row[0] == 'SNAPSHOT']
    need([row[1] for row in snapshots] == [str(i) for i in range(len(snapshots))],
         'Complete snapshot ordinal inventory')
    descriptors = {'B': 'byte', 'S': 'short', 'I': 'int', 'J': 'long',
                   'F': 'float', 'D': 'double', 'Z': 'boolean', 'C': 'char'}
    expected = []
    for owner in ('sn', 'sl', 'eq', 'fd'):
        for field in sorted(inventories[owner]['fields'], key=lambda value: value['name']):
            if field['static']:
                continue
            descriptor = field['descriptor']
            kind = descriptors.get(descriptor, descriptor[1:-1].replace('/', '.')
                                   if descriptor.startswith('L') else descriptor.replace('/', '.'))
            expected.append([owner, field['name'], kind])
    coordinates = [('0', '0')] + [(str(x), str(z)) for x in range(-2, 3) for z in range(-2, 3)
                                  if x or z]
    for snapshot in snapshots:
        index = snapshot[1]
        actual = [row[3:6] for row in rows if row[0] == 'FIELD' and row[1] == index]
        need(actual == expected, 'All declared original instance fields, in exact order')
        chunks = [row for row in rows if row[0] == 'CHUNK' and row[1] == index]
        need(len(chunks) == 25 and [tuple(row[4:6]) for row in chunks] == coordinates,
             'Complete ordered actual 25-Chunk snapshot')
        lists = [row for row in rows if row[0] == 'LIST' and row[1] == index]
        need(len(lists) == 204 and len({row[2] for row in lists}) == 204,
             'Complete World and Chunk slice list inventory')
        for listing in lists:
            items = [row for row in rows if row[0] == 'LIST_ITEM' and
                     row[1:3] == listing[1:3]]
            need([row[3] for row in items] == [str(i) for i in range(int(listing[4]))],
                 'Every ordered actual list item')


import gzip
import struct
import zlib

def stored_air_chunk(random_tick, x, z):
    # Normal named-tag serialization BEFORE startup, following the actual
    # canonical chunk() body. Only LastUpdate differs: genuine tick_seed=-123.
    tag = random_tick.tag
    arrays = (("Blocks", random_tick.block_bytes(1)), ("Data", bytes(16384)),
              ("SkyLight", bytes(16384)), ("BlockLight", bytes(16384)),
              ("HeightMap", random_tick.height_bytes(1)))
    fields = [tag(3, "xPos", struct.pack(">i", x)),
              tag(3, "zPos", struct.pack(">i", z)),
              tag(4, "LastUpdate", struct.pack(">q", -123))]
    fields += [tag(7, name, struct.pack(">i", len(data)) + data) for name, data in arrays]
    fields += [tag(1, "TerrainPopulated", b"\1"),
               tag(9, "Entities", b"\x0a\0\0\0\0"),
               tag(9, "TileEntities", b"\x0a\0\0\0\0")]
    return tag(10, "", tag(10, "Level", b"".join(fields) + b"\0") + b"\0")


def prepared(mode, construction, random_tick):
    if mode != "air-first":
        raise ValueError("Declared independent original Arrow mode")
    tag, text = construction.tag, construction.string
    # Same explicit LevelInfo arguments as tick_setup_configured's ordinary
    # World fixture: seed23, name Tick, spawn(0,64,0), time0, version19132.
    # LastPlayed123 is an authored file input, not a sampled Source clock.
    metadata = b"".join([
        tag(8, "LevelName", text("Tick")),
        tag(4, "RandomSeed", struct.pack(">q", 23)),
        tag(4, "Time", struct.pack(">q", 0)),
        tag(4, "LastPlayed", struct.pack(">q", 123)),
        tag(4, "SizeOnDisk", struct.pack(">q", 0)),
        tag(3, "SpawnX", struct.pack(">i", 0)),
        tag(3, "SpawnY", struct.pack(">i", 64)),
        tag(3, "SpawnZ", struct.pack(">i", 0)),
        tag(3, "version", struct.pack(">i", 19132)),
    ])
    level = b"\x0a\0\0" + tag(10, "Data", metadata + b"\0") + b"\0"
    result = {
        "world0/level.dat": gzip.compress(level, mtime=0),
        "world0/data/idcounts.dat": construction.raw_ids(),
    }
    groups = {}
    for x in range(-2, 3):
        for z in range(-2, 3):
            groups.setdefault((x // 32, z // 32), []).append((x, z))
    for (rx, rz), coordinates in sorted(groups.items()):
        header = bytearray(8192)
        sectors = []
        for x, z in coordinates:
            # Actual normal stored-air record with authored LastUpdate=-123;
            # no serialized byte patch, loaded object or Source state is edited.
            payload = b"\2" + zlib.compress(stored_air_chunk(random_tick, x, z))
            record = struct.pack(">I", len(payload)) + payload
            if len(record) > 4096:
                raise ValueError("Each actual saved Chunk must fit one declared sector")
            slot = (x & 31) + 32 * (z & 31)
            struct.pack_into(">I", header, 4 * slot, ((2 + len(sectors)) << 8) | 1)
            struct.pack_into(">i", header, 4096 + 4 * slot, 123)
            sectors.append(record + bytes(4096 - len(record)))
        result[f"world0/region/r.{rx}.{rz}.mcr"] = bytes(header) + b"".join(sectors)
    if len(result) != 6 or any(type(value) is not bytes for value in result.values()):
        raise ValueError("Exactly six literal files before bootstrap")
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-dir', type=Path, required=True)
    parser.add_argument('--java', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='arrow-water-reference-', dir=args.work_dir.resolve()))
    report = {'status': 'RUNNING', 'runs': {}, 'first_failure': None, 'secondary_failures': [],
              'scope': 'Finite actual World/Arrow water states and callbacks; independent entropy retained'}
    safe, before = True, None

    def fail(error):
        value = {'type': type(error).__name__, 'message': str(error)}
        if report['first_failure'] is None:
            report['first_failure'] = value
        else:
            report['secondary_failures'].append(value)
        report['status'] = 'FAIL'

    try:
        support_path = ROOT / 'tests/reference/client/statistics/support.py'
        common_path = ROOT / 'tests/tools/world/test_world_query_controls.py'
        native_driver_path = ROOT / 'tests/tools/world/test_arrow_water_flight.py'
        construction_path = ROOT / 'tests/tools/world/test_source_world_construction.py'
        random_tick_path = ROOT / 'tests/tools/world/test_random_tick.py'
        reference_path = ROOT / 'tests/reference/world/entity_tick/run.py'
        classfile_path = ROOT / 'tests/reference/client/resource_lookup_common/offline_classfile.py'
        common = load(common_path, 'water_reference_files')
        support = load(support_path, 'water_reference_capture')
        native_driver = load(native_driver_path, 'water_reference_native_protocol')
        reference = load(reference_path, 'water_reference_libraries')
        offline = load(classfile_path, 'water_reference_classfile')
        jar, native, java = (path.resolve(strict=True) for path in
                             (args.client_jar, args.native, args.java))
        javac = java.with_name('javac.exe' if java.suffix.lower() == '.exe' else 'javac')
        release = java.parent.parent / 'release'
        need(re.search(r'^JAVA_VERSION="17[.]', release.read_text(), re.MULTILINE), 'Same-bin JDK17 required')
        need(pin(jar)['sha256'] == reference.JAR, 'Unchanged original client archive')
        need(native.stem == native_driver.TARGET, 'Actual native water fixture executable')
        libraries = []
        for name, digest in reference.LIBRARIES.items():
            candidates = [path for path in args.library_dir.rglob(name)
                          if path.is_file() and pin(path)['sha256'] == digest]
            need(len(candidates) == 1, 'One exact verified reference library: ' + name)
            libraries.append(candidates[0].resolve(strict=True))
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS', 'JDK_JAVAC_OPTIONS', 'CLASSPATH'):
            need(not os.environ.get(key), 'Unadmitted JVM/compiler environment: ' + key)
        selected = [Path(__file__).resolve(), HERE / 'ArrowWaterOriginalObservation.java',
                    support_path, common_path, native_driver_path, construction_path,
                    random_tick_path, reference_path, classfile_path, jar, native, java, javac, release]
        selected += libraries + list(native.parent.glob('*.dll'))

        def guarded():
            for path in selected:
                common.literal_metadata(path)
            return {str(path): pin(path) for path in selected}

        before = guarded()
        report['selected_inputs_before'] = before
        with zipfile.ZipFile(jar) as archive:
            inventories = {owner: offline.read_class(archive.read(owner + '.class'))
                           for owner in ('sn', 'sl', 'eq', 'fd')}
        arrow = next(method for method in inventories['sl']['methods']
                     if method['name'] == 'w_' and method['descriptor'] == '()V')
        need(arrow['code_sha256'] == 'f48dde96f4efaaa629c8b097f793f6ed6a2c561293110f9e2b5bd4e2f4e65f32',
             'Actual original Arrow method Code identity')
        report['original_method'] = {key: arrow[key] for key in ('code_bytes', 'code_sha256')}
        construction = load(construction_path, 'water_reference_metadata')
        random_tick = load(random_tick_path, 'water_reference_chunks')
        values = prepared('air-first', construction, random_tick)
        pristine = output / 'pristine'
        pristine.mkdir()
        for relative, raw in values.items():
            path = pristine / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
        pristine_files = files(pristine)
        report['pristine'] = pristine_files
        classes = output / 'classes'
        classes.mkdir()
        classpath = os.pathsep.join(str(path) for path in [jar] + libraries)
        command = [str(javac), '-proc:none', '-encoding', 'UTF-8', '-classpath', classpath,
                   '-d', str(classes), str(HERE / 'ArrowWaterOriginalObservation.java')]

        def run(command, directory, prefix, timeout, limit):
            nonlocal safe
            safe = False
            child = support.run_bounded(command, directory, prefix, timeout=timeout,
                                        limit=limit, env=os.environ.copy())
            child.cwd, child.prefix = directory, prefix
            returned(child, command, timeout, limit)
            safe = True
            common.verify_retained(child, {'native.' + suffix:
                (directory / (prefix + '.' + suffix)).read_bytes()
                for suffix in ('stdout', 'stderr', 'capture.json')})
            need(child.returncode == 0, 'Actual child exit ' + str(child.returncode))
            return child

        child = run(command, output, 'compile', 120, LIMIT)
        report['compile'] = child.capture_report
        need(child.stdout == b'' and child.stderr == b'', 'Quiet successful helper compilation')
        helpers = files(classes)
        need(set(helpers) == {'ArrowWaterOriginalObservation.class',
                             'ArrowWaterOriginalObservation$Guard.class',
                             'ArrowWaterOriginalObservation$Observer.class'}, 'Exact helper class inventory')
        report['helpers'] = helpers
        for mode in MODES:
            directory = output / mode
            directory.mkdir()
            original_root = directory / 'original-worlds'
            shutil.copytree(pristine, original_root)
            root_identity = (original_root.stat().st_dev, original_root.stat().st_ino)
            native_root = directory / 'native-worlds'
            native_root.mkdir()
            command = [str(native), mode, str(native_root).encode('utf-8').hex()]
            native_child = run(command, directory, 'native', 240, native_driver.LIMIT)
            native_observation = native_driver.completed(native_child, mode, command)
            command = [str(java), '--add-opens', 'java.base/java.util=ALL-UNNAMED', '-Xint',
                       '-Xverify:all', '-Xmx512m', '-Djava.awt.headless=true',
                       '-Djava.security.manager=allow', '-classpath', str(classes) + os.pathsep + classpath,
                       'ArrowWaterOriginalObservation', str(jar), mode, str(original_root)]
            original_child = run(command, directory, 'original', 240, LIMIT)
            original = observe(original_child, mode, classes)
            validate_inventory(original, inventories)
            run_report = {'native_capture': native_child.capture_report,
                          'original_capture': original_child.capture_report,
                          'native': native_observation, 'original': original}
            report['runs'][mode] = run_report
            run_report['comparison'] = compare(native_observation, original)
            final = files(original_root)
            need((original_root.stat().st_dev, original_root.stat().st_ino) == root_identity,
                 'Original fixture root identity preserved')
            need(all(final[key] == value for key, value in pristine_files.items()),
                 'All six original stored inputs preserved byte for byte')
            need(set(final) - set(pristine_files) == {'world0/session.lock'} and
                 final['world0/session.lock']['bytes'] == 8, 'Only actual eight-byte session lock added')
            run_report['original_files_after'] = final
            run_report['native_files_after'] = files(native_root)
            need(guarded() == before and files(classes) == helpers and files(pristine) == pristine_files,
                 'Source/products/helpers/pristine unchanged during cohort')
        report['status'] = 'PASS_FINITE_DETERMINISTIC_OBSERVATIONS_ENTROPY_DIFFERENCES_RETAINED'
    except BaseException as error:
        fail(error)
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
    finally:
        report['safe_terminal_boundary'] = safe
        if safe and before is not None:
            try:
                report['selected_inputs_after'] = guarded()
                need(report['selected_inputs_after'] == before, 'Terminal source/product guard')
            except BaseException as error:
                fail(error)
        if not safe:
            report['terminal_guard'] = 'DEFERRED_UNPROVED_NATIVE_RETURN'
        with (output / 'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': ' + str(output / 'report.json'), flush=True)
    return 0 if report['status'].startswith('PASS_') else 1


if __name__ == '__main__':
    raise SystemExit(main())
