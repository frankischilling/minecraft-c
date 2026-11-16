"""Public finite projection audit; original binaries and saved files stay external."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
def load_module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result
C=load_module('furnace_readback_public_capture',HERE/'capture.py')
def need(value,message):
    if not value: raise AssertionError(message)
def parse(raw): return json.loads(raw,object_pairs_hook=C.unique)
def digest(raw): return hashlib.sha256(raw).hexdigest()
def files(directory):
    entries=list(Path(directory).iterdir())
    need(all(p.is_file() and not p.is_symlink() for p in entries),'only root text files; no nested or linked artifacts')
    return {p.name:p.read_bytes() for p in entries}
def audit(blobs):
    manifest=parse(blobs['publication.json'])
    need(set(blobs)==set(manifest['files'])|{'publication.json'},'complete exact public file set')
    for name,pin in manifest['files'].items():
        need(Path(name).name==name and pin=={'bytes':len(blobs[name]),'sha256':digest(blobs[name])},'public file count/hash')
    pins=parse(blobs['source-pins.json'])
    need(pins['jar_sha256']==C.JAR_HASH and set(pins['classes'])==set(C.CORE),'closed original class/resource inventory')
    expected=parse(blobs['input-projection.json'])
    declaration=parse(blobs['declaration.json'])
    need(declaration['world_folder']=='alpha' and declaration['name_source']=='Data.LevelName' and declaration['operations']==[
        'construct actual FD/MX','load actual saved Chunk(0,0)','inspect already factory-created SK in actual Chunk Map',
        'actual World Tile lookup and identity comparison','read actual WorldInfo.Player through IX(null).b(SP)',
        'read selected fields only'] and declaration['prohibited_operations']==[
        'tick','save','render','postconstruction field repair'],'exact finite read-only declaration scope')
    need(declaration['target']==[0,64,2] and declaration['source_file_count']==13 and declaration['seed']==123 and declaration['world_time']==211 and
        declaration['block']==62 and declaration['block_metadata']==2 and declaration['burn']==1397 and declaration['cook']==4 and
        declaration['tile_items']==[[0,15,1,0],[1,263,1,0]] and declaration['player_items']==[[8,265,1,0]],'finite original input declaration')
    need(expected['metadata']['seed']==123 and expected['metadata']['time']==211 and expected['block']==62 and expected['block_metadata']==2 and
        expected['furnace_xyz']==[0,64,2] and expected['furnace_fields']==[1397,1600,4] and expected['furnace_slots']==[[15,1,0,0],[263,1,0,0],None] and
        expected['player_main'][8]==[265,1,0,0] and sum(v is not None for v in expected['player_main'])==1,'finite independently decoded native input')
    finite=parse(blobs['finite.json'])
    receipts=parse(blobs['capture-receipts.json'])
    need(set(finite)=={'interpreter','default'} and set(receipts)=={'compile','java-version','javac-version','interpreter','default'},'two fresh process cohort and complete tooling receipts')
    for name,receipt in receipts.items():
        need(type(receipt['exit']) is int and receipt['exit']==0 and receipt['timed_out'] is False and receipt['pipe_capture_complete'] is True and
            receipt['output_overflow'] is False and receipt['reader_errors']=={},'completed bounded original/tool process')
        need(receipt['stdout_limit']==4194304 and receipt['stderr_limit']==1048576,'declared pipe bounds')
    for mode in ('interpreter','default'):
        raw=blobs[mode+'.stdout']
        need(len(raw)==receipts[mode]['stdout_bytes'] and digest(raw)==receipts[mode]['stdout_sha256'],'retained original raw stdout transport hash')
        row,footer=C.read_output(raw,pins['classes'])
        C.check_row(row,expected)
        need(type(footer['rows']) is int and footer['rows']==1 and footer['write_checks']==[
            {'operation':'write','role':'owned-session-lock'},{'operation':'write','role':'existing-copied-region-rw-open'}],'exact actual guard sequence')
        need(finite[mode]=={'row':row,'footer':footer},'both raw streams reparse to exact public finite projection')
    need(finite['interpreter']['row']==finite['default']['row'],'fresh VM projections agree')
    manifests=parse(blobs['save-manifests.json'])
    before=manifests['source_before']
    need(len(before)==13 and before==manifests['source_after'] and sum(v['bytes'] for v in before.values())==1750184,'all native source files exact before and after')
    need(set(manifests['copies_after'])=={'interpreter','default'},'both owned copies pinned')
    for inv in manifests['copies_after'].values():
        need(set(inv)==set(before) and inv['alpha/session.lock']['bytes']==8 and
            all(inv[name]==pin for name,pin in before.items() if name!='alpha/session.lock'),'only owned SessionLock may change')
    provenance=parse(blobs['provenance.json'])
    need(provenance['excluded']==['original tick','original save','original gameplay','rendering or pixel parity',
        'concrete original Player entity','JDI/live-Code trace','bidirectional roundtrip','current native source qualification'],
        'exact finite original readback exclusions')
    need(provenance['archive_pins_sha256']==digest(json.dumps(pins['classes'],sort_keys=True,separators=(',',':')).encode('utf8')),'all original archive Code/class pins bind exact captured projection')
    need(provenance['archive_class_count']==len(pins['classes'])==24 and provenance['archive_method_count']==sum(len(v['methods']) for v in pins['classes'].values())==595 and
        provenance['archive_Code_method_count']==sum('code_sha256' in method for v in pins['classes'].values() for method in v['methods'])==591,'exact archive class/declared-method/Code-method summary counts')
    common=['-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow']
    need(provenance['modes']=={'interpreter':['-Xint',*common],'default':common},'exact actual VM mode/options')
    cp='CLASSES;ORIGINAL_CLIENT_JAR;LIBRARIES/lwjgl.jar;LIBRARIES/lwjgl_util.jar;LIBRARIES/jinput.jar;LIBRARIES/jutils.jar'
    expected_commands={'compile':['JDK/bin/javac.exe','-encoding','UTF-8','-implicit:none','-classpath',cp,'-d','CLASSES','EXECUTED_HELPER/FurnaceReadback.java'],
        'java-version':['JDK/bin/java.exe','-version'],'javac-version':['JDK/bin/javac.exe','-version']}
    for mode,flags in provenance['modes'].items():
        expected_commands[mode]=['JDK/bin/java.exe',*flags,'-classpath',cp,'FurnaceReadback','ORIGINAL_CLIENT_JAR','OWNED_COPY/'+mode+'/saves','alpha','0','64','2',expected['metadata']['name']]
    need({name:value['command_roles'] for name,value in receipts.items()}==expected_commands,'all sanitized actual command arguments bind pinned role/order/target/constructor name')
    need(provenance['processes']==2 and len(provenance['failed_preparations'])==2 and all(v['excluded_from_final_two_process_cohort'] is True for v in provenance['failed_preparations']),'failed preparations excluded from successful cohort')
    for value in provenance['executed_inputs'].values(): need(value['before']==value['after'],'all executed inputs stable')
    for value in provenance['compiled_products'].values(): need(value['before_sha256']==value['after_sha256'],'all executed products stable')
    need(provenance['executed_inputs']['FurnaceReadback.java']['before']==manifest['files']['FurnaceReadback.java'],'public Java is exactly executed Java source')
    need(provenance['executed_inputs']['original-client.jar']['before']['sha256']==pins['jar_sha256'],'loaded original archive bound to pinned input')
    need(provenance['native_save_receipt']['status']=='PASS' and provenance['native_save_receipt']['checks']==9242 and provenance['native_save_receipt']['world_time']==211,'requested historical native receipt scope')
    return finite
if __name__=='__main__':
    audit(files(HERE))
    print('PASS: original readback packet finite projection and hash controls')
