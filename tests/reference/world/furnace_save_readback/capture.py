"""Finite C-save -> unchanged-original readback; no ticks or saves."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import traceback
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
JAR_HASH = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
CORE = ('fd','lm','mx','ei','sk','ow','tc','rw','nu','sp','ij','iz','ix','gm','uu','ln','rh','ok','to','ld','wi','xa','qj','rj')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('readback_bounded_capture', HERE / 'transport.py')
I = module('readback_classfile', HERE / 'classfile.py')
N = module('readback_independent_nbt', HERE / 'decode.py')
LIBRARIES = {'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a', 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad', 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018', 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}


def unique(pairs):
    result = dict(pairs)
    C.ensure(len(result) == len(pairs), 'unique JSON keys')
    return result


def inventory(directory):
    C.ensure(not directory.is_symlink(), 'regular owned directory')
    result = {}
    for path in sorted(directory.rglob('*')):
        C.ensure(not path.is_symlink(), 'no symlinks in source or copy')
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = {'bytes':path.stat().st_size,'sha256':C.digest(path)}
    return result


def value(obj, key, kind):
    C.ensure(key in obj and obj[key][0] == kind, 'typed NBT ' + key)
    return obj[key][1]


def stacks(obj, name, length):
    child, items = value(obj, name, 9)
    C.ensure(child == 10 or not items and 0 <= child <= 10, 'empty or Compound Stack list')
    slots = [None] * length
    typed = []
    for item in items:
        slot = value(item, 'Slot', 1) & 255
        C.ensure(slot < length and slots[slot] is None, 'bounded unique Stack slot')
        slots[slot] = [value(item,'id',2),value(item,'Count',1),value(item,'Damage',2),0]
        typed.append(item)
    return slots, typed


def disk_projection(source):
    alpha = source / 'alpha'
    info = value(N.inflate((alpha/'level.dat').read_bytes(),31), 'Data',10)
    raw = (alpha/'region/r.0.0.mcr').read_bytes()
    C.ensure(len(raw) >= 8192 and len(raw) % 4096 == 0, 'complete actual region sectors')
    entry = struct.unpack_from('>I', raw, 0)[0]
    sector, count = entry >> 8, entry & 255
    C.ensure(sector >= 2 and count > 0 and (sector+count)*4096 <= len(raw), 'selected original region allocation')
    at = sector*4096
    size = struct.unpack_from('>I',raw,at)[0]
    C.ensure(1 < size <= count*4096-4 and raw[at+4] == 2, 'complete selected zlib member')
    chunk = value(N.inflate(raw[at+5:at+4+size],15),'Level',10)
    C.ensure((value(chunk,'xPos',3),value(chunk,'zPos',3)) == (0,0), 'selected Chunk identity')
    child, tiles = value(chunk,'TileEntities',9)
    C.ensure(child == 10 and len(tiles) == 1, 'one serialized real Furnace tile')
    tile = tiles[0]
    C.ensure(value(tile,'id',8) == 'Furnace' and [value(tile,k,3) for k in ('x','y','z')] == [0,64,2], 'selected serialized Furnace')
    block_index = (0<<11)|(2<<7)|64
    data = value(chunk,'Data',7)
    meta = data[block_index//2] >> (4*(block_index&1)) & 15
    player = value(info,'Player',10)
    main, player_typed = stacks(player,'Inventory',104)
    C.ensure(all(v is None for v in main[36:100]), 'no otherwise ignored Player slots')
    tile_slots, tile_typed = stacks(tile,'Items',3)
    projection = {
        'metadata':{'seed':value(info,'RandomSeed',4),'spawn':[value(info,k,3) for k in ('SpawnX','SpawnY','SpawnZ')],
            'time':value(info,'Time',4),'last_played':value(info,'LastPlayed',4),'size_on_disk':value(info,'SizeOnDisk',4),
            'dimension':value(player,'Dimension',3),'version':value(info,'version',3),'name':value(info,'LevelName',8)},
        'block':value(chunk,'Blocks',7)[block_index], 'block_metadata':meta,
        'furnace_xyz':[value(tile,k,3) for k in ('x','y','z')],
        'furnace_fields':[value(tile,'BurnTime',2),1600,value(tile,'CookTime',2)],
        'furnace_slots':tile_slots,'player_main':main[:36],'player_armor':main[100:104],
    }
    C.ensure(projection['metadata']['seed'] == 123 and projection['metadata']['time'] == 211 and
        projection['block'] == 62 and projection['block_metadata'] == 2 and projection['furnace_fields'] == [1397,1600,4] and
        tile_slots == [[15,1,0,0],[263,1,0,0],None] and main[8] == [265,1,0,0] and
        sum(v is not None for v in main) == 1, 'predeclared finite native SDK input values')
    return {'selected_projection':projection,'typed_metadata':info,'typed_furnace':tile,'typed_player_items':player_typed,
        'typed_tile_items':tile_typed,'region_slot':0,'region_record_offset':at,'region_record_bytes':size+4,
        'fuel_duration_note':'1600 is recomputed by original SK.a(NU) from coal263; not a serialized NBT field'}


def expected_warnings(classes):
    uri = classes.as_uri().replace('file:///','file:/')+'/'
    return ['WARNING: A terminally deprecated method in java.lang.System has been called',
        'WARNING: System::setSecurityManager has been called by FurnaceReadback ('+uri+')',
        'WARNING: Please consider reporting this to the maintainers of FurnaceReadback',
        'WARNING: System::setSecurityManager will be removed in a future release']


def transport_record(output, record):
    C.require_success(record)
    C.ensure(type(record['exit']) is int and record['exit'] == 0 and record['timed_out'] is False and
        record['pipe_capture_complete'] is True and record['output_overflow'] is False and record['reader_errors'] == {}, 'complete exact child transport')
    for name in ('stdout','stderr'):
        path = output/record[name+'_file']
        C.ensure(path.parent == output and path.stat().st_size == record[name+'_bytes'] and C.digest(path) == record[name+'_sha256'], 'retained child stream matches receipt')


def read_output(raw, pins):
    lines = raw.decode('utf8').splitlines()
    C.ensure(len(lines) == 5 and lines[:2] == ['16 achievements','151 recipes'], 'exact original preamble and five complete output records')
    records = []
    for line,prefix in zip(lines[2:],('FURNACE_READBACK_ORIGINS ','FURNACE_READBACK_ROW ','FURNACE_READBACK_COMPLETE ')):
        C.ensure(line.startswith(prefix), 'ordered complete original records')
        records.append(json.loads(line[len(prefix):],object_pairs_hook=unique))
    origins,row,footer = records
    C.ensure(origins == {'jar_sha256':JAR_HASH,'classes':{name:pin['class_sha256'] for name,pin in pins.items()}}, 'actual loaded original owners')
    C.ensure(set(footer) == {'rows','network','native','execs','denied_writes','write_checks'} and footer['rows'] == 1 and
        all(type(footer[k]) is int and footer[k] == 0 for k in ('network','native','execs','denied_writes')), 'closed guarded child footer')
    C.ensure(footer['write_checks'] and all(v in ({'operation':'write','role':'existing-world-directory'},
        {'operation':'write','role':'owned-session-lock'},{'operation':'write','role':'existing-copied-region-rw-open'}) for v in footer['write_checks']) and
        any(v['role'] == 'owned-session-lock' for v in footer['write_checks']), 'only contained existing-directory and SessionLock checks')
    return row,footer


def check_row(row, expected):
    C.ensure(set(row) == {'world_class','remote','provider_class','provider_fields','chunk_provider_class','chunk','chunk_loaded',
        'metadata','block','block_metadata','map_before_lookup','lookup_same','furnace','active','pending',
        'player_inventory_owner_class','player_reference_null','player_main','player_armor'}, 'closed original finite projection')
    C.ensure(row['world_class'] == 'fd' and row['remote'] is False and row['provider_class'] == 'rh' and
        row['provider_fields'] == [False,False,False,0] and row['chunk_provider_class'] == 'ok' and row['chunk'] == [0,0] and
        row['chunk_loaded'] is True, 'actual original World/Chunk setup')
    for key in ('metadata','block','block_metadata','player_main','player_armor'):
        C.ensure(row[key] == expected[key], 'original readback equals independently decoded C bytes: '+key)
    C.ensure(row['furnace'] == {'class':'sk','world_same':True,'xyz':expected['furnace_xyz'],'invalid':False,
        'fields':expected['furnace_fields'],'slots':expected['furnace_slots']}, 'actual original SK factory fields/slots')
    C.ensure(row['map_before_lookup'] == [{'class':'sk','xyz':expected['furnace_xyz']}] and row['lookup_same'] is True,
        'saved factory SK exists before lazy lookup and is retained')
    C.ensure(row['active'] == [{'class':'sk','same_saved_furnace':True,'xyz':expected['furnace_xyz']}] and row['pending'] == 0,
        'actual saved Chunk publication lists')
    C.ensure(row['player_inventory_owner_class'] == 'ix' and row['player_reference_null'] is True, 'genuine IX reader with explicit null Player input')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source','output','jar','libraries','java-home'):
        parser.add_argument('--'+name,type=Path,required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    output = args.output.resolve()
    C.ensure(source != output and source not in output.parents, 'output is outside retained source')
    output.mkdir(exist_ok=False)
    jar = args.jar.resolve(strict=True)
    libraries = [(args.libraries/name).resolve(strict=True) for name in LIBRARIES]
    java_home = args.java_home.resolve(strict=True)
    suffix = '.exe' if os.name == 'nt' else ''
    java,javac = [java_home/'bin'/(name+suffix) for name in ('java','javac')]
    jvm = java_home/'bin/server/jvm.dll' if os.name == 'nt' else java_home/'lib/server/libjvm.so'
    classes = output/'classes'
    classes.mkdir()
    report = {'status':'FAIL','scope':__doc__,'source':str(source),'output':str(output),
        'limitations':['One finite C-save -> original load/read boundary only. No original tick, save, gameplay, rendering, or pixel claim.',
            'Player inventory is read by actual IX(null).b(SP) from actual WorldInfo.Player; no concrete original Player construction or World binding.',
            'Archive Code hashes are identities, not an observed runtime trace. No JDI instrumentation in this private capture.'],
        'source_before':inventory(source)}
    try:
        C.ensure(len(report['source_before']) == 13, 'all thirteen files of the source saves root')
        C.ensure(C.digest(jar) == JAR_HASH and all(C.digest(path) == sha for path,sha in zip(libraries,LIBRARIES.values())), 'unchanged original archive and pinned libraries')
        helpers = [HERE/name for name in ('capture.py','FurnaceReadback.java','transport.py','classfile.py','decode.py')]
        inputs = [jar,*libraries,java,javac,jvm,java_home/'lib/modules',*helpers]
        report['inputs_before'] = {str(path):{'bytes':path.stat().st_size,'sha256':C.digest(path)} for path in inputs}
        with zipfile.ZipFile(jar) as archive:
            report['class_pins'] = {name:I.inspect_class(archive.read(name+'.class')) for name in CORE}
        decoded = disk_projection(source)
        (output/'independent-input.json').write_text(json.dumps(decoded,indent=2)+'\n',encoding='utf8')
        report['independent_input'] = {'bytes':(output/'independent-input.json').stat().st_size,'sha256':C.digest(output/'independent-input.json')}
        environment = os.environ.copy()
        report['removed_environment_names'] = [name for name in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS') if environment.pop(name,None) is not None]
        cp = os.pathsep.join(map(str,[classes,jar,*libraries]))
        for stem in ('interpreter','default'):
            work = output/stem
            work.mkdir()
            saves = work/'saves'
            shutil.copytree(source,saves)
            C.ensure(inventory(saves) == report['source_before'], 'byte-identical complete isolated copy before original process')
        report['compile'] = C.capture([str(javac),'-encoding','UTF-8','-implicit:none','-classpath',cp,'-d',str(classes),str(HERE/'FurnaceReadback.java')],output,'compile',60,environment)
        transport_record(output,report['compile'])
        C.ensure(report['compile']['stdout_bytes'] == report['compile']['stderr_bytes'] == 0, 'clean authored observer compile')
        compiled = sorted(classes.rglob('*.class'))
        C.ensure(compiled and all(path.name.startswith('FurnaceReadback') for path in compiled), 'only authored readback observer products')
        report['compiled_before'] = {str(path.relative_to(output)):C.digest(path) for path in compiled}
        for executable,stem in ((java,'java-version'),(javac,'javac-version')):
            report[stem] = C.capture([str(executable),'-version'],output,stem,20,environment)
            transport_record(output,report[stem])
        rows = []
        for stem,flags in (('interpreter',['-Xint']),('default',[])):
            work = output/stem
            env = environment.copy()
            for name in ('APPDATA','TEMP','TMP','USERPROFILE'):
                directory = work/name.lower()
                directory.mkdir()
                env[name] = str(directory)
            report[stem] = C.capture([str(java),*flags,'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true',
                '-Djava.security.manager=allow','-classpath',cp,'FurnaceReadback',str(jar),str(work/'saves'),'alpha','0','64','2',decoded['selected_projection']['metadata']['name']],output,stem,90,env,work)
            transport_record(output,report[stem])
            C.ensure((output/(stem+'.stderr')).read_text(encoding='utf8').splitlines() == expected_warnings(classes), 'exact declared JDK17 guard warnings')
            row,footer = read_output((output/(stem+'.stdout')).read_bytes(),report['class_pins'])
            check_row(row,decoded['selected_projection'])
            report.setdefault('observations',{})[stem] = {'row':row,'footer':footer}
            rows.append(row)
            after = inventory(work/'saves')
            report.setdefault('copies_after',{})[stem] = after
            C.ensure(set(after) == set(report['source_before']) and all(after[name] == pin for name,pin in report['source_before'].items() if name != 'alpha/session.lock') and
                after['alpha/session.lock']['bytes'] == 8, 'only owned alpha SessionLock file may change')
        C.ensure(rows[0] == rows[1], 'two literal independent fresh original VM projections agree')
        # Reparse both retained streams again after execution, not cached observations alone.
        for stem in ('interpreter','default'):
            transport_record(output,report[stem])
            row,footer = read_output((output/(stem+'.stdout')).read_bytes(),report['class_pins'])
            check_row(row,decoded['selected_projection'])
            C.ensure({'row':row,'footer':footer} == report['observations'][stem], 'retained actual raw stream projects exactly to report')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type':type(error).__name__,'message':str(error),'traceback':traceback.format_exc()}
    finally:
        report['all_copies_after'] = {stem:inventory(output/stem/'saves') for stem in ('interpreter','default') if (output/stem/'saves').is_dir()}
        report['source_after'] = inventory(source)
        report['source_unchanged'] = report['source_after'] == report['source_before']
        if not report['source_unchanged']:
            report['status'] = 'FAIL'
        if 'inputs_before' in report:
            report['inputs_after'] = {name:{'bytes':Path(name).stat().st_size,'sha256':C.digest(name)} for name in report['inputs_before']}
            report['inputs_unchanged'] = report['inputs_after'] == report['inputs_before']
            if not report['inputs_unchanged']:
                report['status'] = 'FAIL'
        if 'compiled_before' in report:
            report['compiled_after'] = {str(path.relative_to(output)):C.digest(path) for path in sorted(classes.rglob('*.class'))}
            report['compiled_unchanged'] = report['compiled_after'] == report['compiled_before']
            if not report['compiled_unchanged']:
                report['status'] = 'FAIL'
        (output/'capture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
        print(json.dumps({'status':report['status'],'output':str(output),'source_unchanged':report['source_unchanged'],'error':report.get('error')},indent=2))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
