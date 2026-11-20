"""Strict offline audit of the finite original expired-Spawner packet."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_spawner_tick_protocol',HERE/'capture.py')
C=importlib.util.module_from_spec(spec);spec.loader.exec_module(C)
MODES=('interpreter','default')
FILES={'SpawnerTickObservation.java','cases.json','profiles.tsv','input-manifest.json','capture.py','packet.py','publish.py','test_packet.py',
    'finite.json','source-pins.json','source-manifests.json','receipts.json','provenance.json','native-invariants.h','README.md'}
EXECUTED_DRIVER='7b45369ff2ce997aa93a6f910e6dbe4fcbc05bdbe0ae47de57d3c9290c8ed4e8'
SCOPE='Twenty direct unchanged CY.n_ calls, ten per ordinary VM, over declared saved inputs with genuine Player activation and passive WorldAccess observations.'
EXCLUDED=['FD.g whole-World tick','AG/GL/rendering/pixels','original save or bidirectional roundtrip','JDI/live-Code/internal-call trace',
    'unpublished candidate object state','exact cross-VM Random/position/spawn-count equality','captured RNG injection into native runtime','complete original/native field parity']

def need(v,m):
    if not v:raise ValueError(m)
def unique(pairs):
    value=dict(pairs);need(len(value)==len(pairs),'duplicate JSON key');return value
def parse(raw):return json.loads(raw,object_pairs_hook=unique,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite JSON '+x)))
def encoded(v):return json.dumps(v,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode('utf8')
def same(a,b):return encoded(a)==encoded(b)
def digest(raw):return hashlib.sha256(raw).hexdigest()
def keys(v,names,m):need(type(v) is dict and set(v)==set(names),'closed '+m)
def fingerprint(v):
    keys(v,('bytes','sha256'),'byte fingerprint');need(type(v['bytes']) is int and v['bytes']>=0 and type(v['sha256']) is str and re.fullmatch('[0-9a-f]{64}',v['sha256']),'typed byte count/hash')
def files(directory):
    entries=list(Path(directory).iterdir());need(all(p.is_file() and not p.is_symlink() for p in entries),'root regular text/hash files only')
    return {p.name:p.read_bytes() for p in entries}

def closed_rows(rows):
    state_keys=('counter','delay','rotation_bits','previous_rotation_bits','name','world_time','world_random','math_random','entities','players','active_tiles','pending_tiles','world_L','chunks','retained_entities','eq_cursor')
    for row in rows:
        for state in (row['before'],row['after']):
            keys(state,state_keys,'actual sampled World/Tile state')
            need(all(type(state[k]) is int for k in ('counter','delay','world_time','eq_cursor')),'typed counters/delay/time/pool cursor')
            for name in ('entities','players'):
                for e in state[name]:keys(e,('label','class','id'),'actual Entity list occurrence');need(type(e['id']) is int,'typed actual source ID')
            for name in ('active_tiles','pending_tiles'):
                for t in state[name]:
                    keys(t,('class','same_selected','world_same','xyz','invalid'),'actual Tile occurrence')
                    need(same(t,{'class':'cy','same_selected':True,'world_same':True,'xyz':[0,64,2],'invalid':False}),'same genuine loaded CY identity')
            need(len(state['active_tiles'])==1 and len(state['chunks'])==4,'actual selected four-Chunk/one-Tile graph')
            for chunk,xy in zip(state['chunks'],([-1,-1],[-1,0],[0,-1],[0,0])):
                keys(chunk,('chunk','entities'),'actual Chunk slices');need(chunk['chunk']==xy and len(chunk['entities'])==8,'actual saved Chunk coordinates/eight slices')
                for group in chunk['entities']:
                    for e in group:keys(e,('label','class','id'),'actual slice occurrence');need(type(e['id']) is int,'typed actual Chunk Entity ID')
        for e in row['events']:
            if e['event']=='entity_added':keys(e,('event','entity','delay','world_random','world_entity_count'),'actual publication callback')
            else:keys(e,('event','name','position_bits','velocity_bits','delay','world_random'),'actual particle callback');need(e['event']=='particle','no invented source callback')
            need(type(e['delay']) is int,'typed callback-sampled delay')
        if row['failure'] is not None:
            keys(row['failure'],('class','message','frames'),'actual escaped original Throwable')
            for frame in row['failure']['frames']:keys(frame,('class','method','file','line'),'actual Throwable frame');need(type(frame['line']) is int,'typed source stack line')

def stdout_transport(observed,pins,line_ending):
    need(line_ending in ('\n','\r\n'),'recorded original line ending')
    compact=lambda v:json.dumps(v,separators=(',',':'),ensure_ascii=True)
    origins={'jar_sha256':C.R.JAR_HASH,'classes':{n:pins[n]['class_sha256'] for n in C.CORE}}
    lines=['16 achievements','151 recipes','SPAWNER_TICK_ORIGINS '+compact(origins)]
    lines += ['SPAWNER_TICK_ROW '+compact(row) for row in observed['rows']]
    lines += ['SPAWNER_TICK_COMPLETE '+compact(observed['footer'])]
    return (line_ending.join(lines)+line_ending).encode('utf8')

def command_roles(mode):
    return ['JDK/bin/java',*(['-Xint'] if mode=='interpreter' else []),'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow',
        '--add-opens=java.base/java.lang=ALL-UNNAMED','--add-opens=java.base/java.util=ALL-UNNAMED','-classpath','CLASSES;CLIENT_JAR;DEPENDENCIES','SpawnerTickObservation',
        'CLIENT_JAR','OWNED_COPY/'+mode+'/saves','DECLARED_PROFILES']

def native_header(observations):
    common=observations['interpreter']['summary']['common_projection']
    effect_counts=[]
    for mode in MODES:
        for row in observations[mode]['rows']:
            starts=[i for i,e in enumerate(row['events']) if e['event']=='entity_added']
            for at,end in zip(starts,starts[1:]+[len(row['events'])]):
                group=row['events'][at+1:end]
                effect_counts.append(tuple(sum(e.get('name')==name for e in group) for name in ('smoke','flame','explode')))
    need(effect_counts and len(set(effect_counts))==1,'one common count projection from actual observed success groups')
    lines=['/* Derived finite original invariants. No whole-field or captured-RNG native parity. */',
        '#ifndef MC_ORIGINAL_SPAWNER_TICK_INVARIANTS_H','#define MC_ORIGINAL_SPAWNER_TICK_INVARIANTS_H',
        '#include <stdbool.h>','#include <stddef.h>','#include <stdint.h>','',
        'typedef struct mc_spawner_tick_reference {','    uint32_t case_index;','    int32_t counter_delta;','    bool returned;',
        '    const char *exception_class;','    uint64_t rotation_bits;','    uint32_t initial_particles;','} mc_spawner_tick_reference;','',
        'static const mc_spawner_tick_reference mc_spawner_tick_references[10] = {']
    for row,original in zip(common,observations['interpreter']['rows']):
        exception='NULL' if row['failure_class'] is None else '"'+row['failure_class']+'"'
        initial=next((i for i,e in enumerate(original['events']) if e['event']=='entity_added'),len(original['events']))
        lines.append('    {%d, %d, %s, %s, UINT64_C(0x%s), %d},'%(row['index'],row['counter_delta'],'true' if row['returned'] else 'false',exception,row['rotation_bits'],initial))
    lines += ['};','','/* Each actually observed success group: smoke, flame, explode. */',
        'static const uint32_t mc_spawner_tick_effect_counts[3] = {%d, %d, %d};'%effect_counts[0],'',
        '/* Positive counts and final random delays differ between original VMs. */','#endif','']
    return '\n'.join(lines).encode('utf8')

def audit(blobs):
    need(set(blobs)==FILES|{'publication.json'},'exact public packet census; no binary/World/raw stream material')
    manifest=parse(blobs['publication.json']);keys(manifest,('meaning','files'),'publication-only manifest')
    need(manifest['meaning']=='Postexecution publication-only fingerprints; not original execution inputs.' and set(manifest['files'])==FILES,'exact manifest scope/census')
    for n,p in manifest['files'].items():need(same(p,{'bytes':len(blobs[n]),'sha256':digest(blobs[n])}),'public byte/hash guard '+n)
    for n,sha in {'SpawnerTickObservation.java':'df26512ee2e93a5c20eda9ed6f70c70a04e7fbf5130188918bf550f10cdd3bf9',
        'cases.json':'0d6598d6077c0e00fa43d22aa019608d98a8d3bc03330783be5ee29c0254be40','profiles.tsv':'9c5b032d27f0e35e39741db775525f03030c1be187f4d080e41f77d5e10a21e3',
        'input-manifest.json':'78875bd89de470853c9bced09956b35331039f121c70216245fc5573d24a1730'}.items():need(digest(blobs[n])==sha,'exact executed authored input '+n)
    declaration=parse(blobs['cases.json']);finite=parse(blobs['finite.json'])
    keys(finite,('schema','observations'),'finite original outputs');need(finite['schema']=='ORIGINAL_CLIENT_EXPIRED_SPAWNER_V1' and set(finite['observations'])==set(MODES),'two exact retained ordinary modes')
    for observed in finite['observations'].values():
        keys(observed,('rows','footer','summary'),'complete original mode projection');closed_rows(observed['rows'])
        need(same(C.check_rows(observed['rows'],declaration),observed['summary']),'all observed branch/ID/publication/effect/delay invariants')
    need(same(finite['observations']['interpreter']['summary']['common_projection'],finite['observations']['default']['summary']['common_projection']),'deterministic common fields only; raw entropy-dependent outcomes retained')
    provenance=parse(blobs['provenance.json'])
    keys(provenance,('scope','excluded','original_processes','ticks_per_mode','modes','native_parity_claimed','internal_trace_claimed','public_java_is_executed_source','packaging_scope',
        'executed_driver_sha256','selected_input_count','executed_inputs','compiled_products','archive_class_count','declared_method_count','code_method_count','finite_sha256',
        'class_pins_sha256','receipts_sha256','retained_capture_json','stdout_line_ending','jvm_option_keys_removed_if_present','keys_present','java_version_lines','failed_preparations','raw_material_scope'),'provenance')
    need(provenance['scope']==SCOPE and provenance['excluded']==EXCLUDED and provenance['original_processes']==2 and provenance['ticks_per_mode']==10 and provenance['modes']==list(MODES),'exact finite execution scope')
    need(provenance['native_parity_claimed'] is False and provenance['internal_trace_claimed'] is False and provenance['public_java_is_executed_source'] is True,'no invented native/internal trace qualification')
    need(provenance['packaging_scope']=='Portable runner, publisher, offline validator, controls, C projection and prose were authored after execution. Exact Java, cases, profiles and input manifest match executed inputs.','postexecution packaging scope')
    need(provenance['raw_material_scope']=='Raw original streams, private commands/receipts, JAR/classes/dependencies/assets and World bytes remain external. Complete finite outputs and raw receipt/stream hashes were physically joined before publication.','exact material/execution boundary')
    need(same(provenance['retained_capture_json'],{'bytes':1066128,'sha256':'4018abc34e39dace48b2d3b31d6e9eb45b10d8ba338a2cc3250f82bda1cfb220'}),'retained successful physical capture identity')
    need(provenance['executed_driver_sha256']==EXECUTED_DRIVER and provenance['selected_input_count']==len(provenance['executed_inputs'])==21 and len(provenance['compiled_products'])==3,'retained actual execution census')
    for p in list(provenance['executed_inputs'].values())+list(provenance['compiled_products'].values()):
        keys(p,('before','after'),'before/after execution pin');fingerprint(p['before']);fingerprint(p['after']);need(same(p['before'],p['after']),'executed input/product unchanged')
    for role,name in [('observer-java','SpawnerTickObservation.java'),('cases','cases.json'),('profiles','profiles.tsv'),('input-manifest','input-manifest.json')]:need(same(provenance['executed_inputs'][role]['before'],manifest['files'][name]),'actual executed/public input byte identity')
    need(provenance['executed_inputs']['executed-capture-driver']['before']['sha256']==EXECUTED_DRIVER and provenance['finite_sha256']==digest(encoded(finite)),'physical execution/finite projection binding')
    need(provenance['jvm_option_keys_removed_if_present']==['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'] and provenance['keys_present']==[],'historical inherited VM-option removal result')
    need(len(provenance['failed_preparations'])==2 and [(x['kind'],x['original_vm_count'],x['completed_tick_rows']) for x in provenance['failed_preparations']]==[('compile-only',0,0),('runtime-preparation',1,0)],'failed preparations retained separately; excluded from successful cohort')
    need([x['reason'] for x in provenance['failed_preparations']]==['Authored observer compilation failed before any original component VM.','Missing authored data directories caused the write guard to stop original storage preparation before Player or CY entry.'],'exact failed-preparation scope')
    for failed in provenance['failed_preparations']:
        keys(failed,('kind','original_vm_count','completed_tick_rows','capture_json','process_receipt','stdout','stderr','reason'),'separate failed preparation')
        for key in ('capture_json','process_receipt','stdout','stderr'):fingerprint(failed[key])
    pins=parse(blobs['source-pins.json']);keys(pins,('jar_sha256','classes','meaning'),'original archive pins')
    need(pins['jar_sha256']==C.R.JAR_HASH and set(pins['classes'])==set(C.CORE) and pins['meaning']=='Archive Class/Code identities and runtime checked original CodeSource/resource hashes; not live-Code or entered-PC evidence.','exact original identity scope')
    need(provenance['class_pins_sha256']==digest(encoded(pins['classes']))=='717bec6e8d32415c0bb436727cde1a9752117e648f919beeda6c6ac42030dd95' and provenance['archive_class_count']==len(pins['classes'])==34 and provenance['declared_method_count']==sum(len(p['methods']) for p in pins['classes'].values())==827 and provenance['code_method_count']==sum('code_sha256' in m for p in pins['classes'].values() for m in p['methods'])==809,'complete original class/Code inventory and exact summaries')
    for cl,n,desc,size,sha in [('cy','n_','()V',616,'28c0e6b8033ae3323385495a0f63a1797044b6154ae512a07ffe45d68337f26d'),('ls','V','()V',159,'58deba69269e4cfbb2e7f2ade4678cef46e156789fd657be3703d0ccae377302')]:
        m=next(m for m in pins['classes'][cl]['methods'] if m['name']==n and m['descriptor']==desc);need(m['code_bytes']==size and m['code_sha256']==sha,'exact original tick/effect Code identity')
    receipts=parse(blobs['receipts.json']);need(set(receipts)=={'compile','java-version','javac-version',*MODES} and provenance['receipts_sha256']==digest(encoded(receipts))=='7a1651fb29186d0ea374f7297301bcb8722d84c120ce344e74c8b4e740cd3654','all retained original/tool stream and receipt projections bound')
    for mode,r in receipts.items():
        keys(r,('exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stderr_bytes','stdout_sha256','stderr_sha256','stdout_limit','stderr_limit','raw_receipt','command_representation','command_roles'),'sanitized complete process receipt')
        need(type(r['exit']) is int and r['exit']==0 and r['timed_out'] is False and r['pipe_capture_complete'] is True and r['output_overflow'] is False and r['reader_errors']=={},'complete bounded successful child')
        need(r['timeout_seconds']==(90 if mode in MODES else 60 if mode=='compile' else 20) and r['stdout_limit']==4194304 and r['stderr_limit']==1048576 and r['command_representation']=='Role projection, not verbatim private command.','exact transport/command scope')
        fingerprint(r['raw_receipt'])
        for stream in ('stdout','stderr'):fingerprint({'bytes':r[stream+'_bytes'],'sha256':r[stream+'_sha256']});need(r[stream+'_bytes']<=r[stream+'_limit'],'bounded actual stream')
        if mode in MODES:
            need(r['command_roles']==command_roles(mode),'exact original runtime flags/argument roles')
            wire=stdout_transport(finite['observations'][mode],pins['classes'],provenance['stdout_line_ending'])
            need(r['stdout_bytes']==len(wire) and r['stdout_sha256']==digest(wire),'all retained structured observations reconstruct actual complete stdout hash/count')
            C.output(wire,pins['classes'],declaration)
        else:
            expected=['JDK/bin/javac','-encoding','UTF-8','-implicit:none','-classpath','CLASSES;CLIENT_JAR;DEPENDENCIES','-d','CLASSES','OBSERVER_JAVA'] if mode=='compile' else ['JDK/bin/'+('java' if mode=='java-version' else 'javac'),'-version']
            need(r['command_roles']==expected,'exact tool command roles')
            if mode=='compile':need(r['stdout_bytes']==r['stderr_bytes']==0 and r['stdout_sha256']==r['stderr_sha256']==digest(b''),'clean authored-only compilation')
    saves=parse(blobs['source-manifests.json'])
    keys(saves,('source_before','source_after','source_directories_before','source_directories_after','base_save_before','base_save_after','copies_after','copy_directories_after'),'source/copy continuity manifests')
    need(saves['source_before']==saves['source_after'] and len(saves['source_before'])==60 and saves['source_directories_before']==saves['source_directories_after'] and len(saves['source_directories_before'])==30 and saves['base_save_before']==saves['base_save_after'] and len(saves['base_save_before'])==9,'all original external source inputs unchanged')
    inputs=parse(blobs['input-manifest.json']);need(inputs['base_source_unchanged'] is True and saves['source_before']==inputs['fixtures'] and saves['source_directories_before']==inputs['fixture_directories'],'exact predeclared file/directory inputs')
    need(set(saves['copies_after'])==set(saves['copy_directories_after'])==set(MODES),'both complete original copies retained')
    for mode,after in saves['copies_after'].items():
        need(set(after)==set(saves['source_before']) and saves['copy_directories_after'][mode]==saves['source_directories_before'] and sorted(n for n,p in after.items() if p!=saves['source_before'][n])==['case%02d/session.lock'%i for i in range(10)] and all(after['case%02d/session.lock'%i]['bytes']==8 for i in range(10)),'only ten owned copied session locks differ')
    for group in [saves['source_before'],saves['source_after'],saves['base_save_before'],saves['base_save_after'],*saves['copies_after'].values()]:
        for value in group.values():fingerprint(value)
    need(blobs['native-invariants.h']==native_header(finite['observations']),'C projection derives only from actual common/count observations')
    return finite

if __name__=='__main__':audit(files(HERE));print('PASS: finite original expired-Spawner packet audit')
