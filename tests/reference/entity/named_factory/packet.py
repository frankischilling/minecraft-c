"""Strict finite original named-factory packet audit; no original execution."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('named_factory_public_capture',HERE/'capture.py')
C=importlib.util.module_from_spec(spec);spec.loader.exec_module(C)
MODES=('interpreter','default')
ENTROPY=('ls.aj','ls.G','ls.F','sn.aS','sn.bs.seed')
EXCLUDED=['AG draw','CY tick','server runtime','native parity','JDI/live-Code or call-order proof','original tick or save','whole-game or pixel parity']
FILES={'NamedFactoryObservation.java','cases.json','capture.py','packet.py','publish.py','test_packet.py','run_native.py','finite.json','input-world.json',
    'source-pins.json','source-manifests.json','receipts.json','diagnostics.json','provenance.json','native-observations.h','README.md'}
REGISTRATIONS=[('Arrow','sl',10),('Snowball','by',11),('Item','hl',1),('Painting','qv',9),('Mob','ls',48),('Monster','gz',49),
    ('Creeper','gb',50),('Skeleton','fr',51),('Spider','cn',52),('Giant','nt',53),('Zombie','uz',54),('Slime','uw',55),('Ghast','bp',56),
    ('PigZombie','ya',57),('Pig','wh',90),('Sheep','dl',91),('Cow','bx',92),('Chicken','ww',93),('Squid','xt',94),('Wolf','gi',95),
    ('PrimedTnt','qw',20),('FallingSand','ju',21),('Minecart','yl',40),('Boat','fz',41)]
def need(v,message):
    if not v:raise ValueError(message)
def parse(raw):return json.loads(raw,object_pairs_hook=C.R.unique,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite JSON '+x)))
def encoded(v):return json.dumps(v,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode('utf8')
def digest(raw):return hashlib.sha256(raw).hexdigest()
def same(a,b):return encoded(a)==encoded(b)
def keys(v,names,where):need(type(v) is dict and set(v)==set(names),'closed '+where)
def files(directory):
    entries=list(Path(directory).iterdir())
    need(all(p.is_file() and not p.is_symlink() for p in entries),'root files only; no linked/nested material')
    return {p.name:p.read_bytes() for p in entries}
def declaration(value):
    keys(value,('schema','scope','startup','cases','world_preparation','after_cases','nondeterministic_fields_excluded_only_from_cross_vm_equality','forbidden'),'declaration')
    need(value['schema']=='PRIVATE_NAMED_ENTITY_FACTORY_INPUTS_V1' and value['scope']=='Direct unchanged JC named factory; same NULL World argument as AG48. AG draw and CY tick are not entered.','exact original component declaration scope')
    expected=[{'index':i,'name':name,'world':'W0' if i>=7 else None,'distinct_string':i==6}
        for i,name in enumerate(('__bounded_unknown__',None,'','pig','Mob','Pig','Pig','Pig','Mob','__bounded_unknown__'))]
    need(same(value['cases'],expected),'all ten exact ordered authored call inputs')
    need(value['startup']==['Class.forName uu','Class.forName gm'] and value['world_preparation']=='After case6, load owned copy of earlier Furnace SDK saves root through genuine FD/MX and Chunk(0,0); use serialized LevelName constructor input.' and
        value['after_cases']=='On the actual case5 Pig invoke original SN.a(FD), then inspect World alias and list membership.','exact startup/World/setter preparation')
    need(value['nondeterministic_fields_excluded_only_from_cross_vm_equality']==list(ENTROPY) and
        value['forbidden']==['Map mutation','field writes','Random writes','tick','save','AG draw','CY tick','JDI or live-Code claim'],'exact direct-observer exclusions and entropy declaration')
def expected_world(source):
    keys(source,('metadata','block','block_metadata','furnace_xyz','furnace_fields','furnace_slots','player_main','player_armor'),'independently decoded source projection')
    metadata=source['metadata']
    keys(metadata,('seed','spawn','time','last_played','size_on_disk','dimension','version','name'),'source metadata')
    need(type(metadata['name']) is str and all(type(metadata[k]) is int for k in ('seed','time','last_played','size_on_disk','dimension','version')),'typed original metadata')
    need(metadata['seed']==123 and metadata['time']==211 and metadata['version']==19132 and source['block']==62 and source['block_metadata']==2 and
        source['furnace_xyz']==[0,64,2] and source['furnace_fields']==[1397,1600,4] and source['furnace_slots']==[[15,1,0,0],[263,1,0,0],None],'finite copied source preconditions')
    need(len(source['player_main'])==36 and source['player_main'][8]==[265,1,0,0] and sum(v is not None for v in source['player_main'])==1 and source['player_armor']==[None]*4,'serialized inventory input only')
    return {'class':'fd','remote':False,'time':metadata['time'],'name':metadata['name'],'provider_class':'rh','provider_fields':[False,False,False,0],
        'chunk_provider_class':'ok','chunk':[0,0],'chunk_loaded':True,'block':62,'metadata':2,'entities':[],'players':[],
        'tiles':[{'class':'sk','xyz':[0,64,2]}],'pending_tiles':0}
def entity(value,index,world):
    keys(value,('stable','nondeterministic'),'Entity observation')
    expected={'label':'E'+str(index),'class':'wh','id':index,'removed':False,'world':world,'world_class':None if world is None else 'fd',
        'width':'3f666666','height':'3f666666','offset':'00000000','position':['0000000000000000']*3,'health':10,'texture':'/mob/pig.png',
        'watcher':{'0':{'class':'java.lang.Byte','value':0},'16':{'class':'java.lang.Byte','value':0}}}
    need(same(value['stable'],expected),'exact finite actual Pig identity/ID/World/typed selected fields')
    entropy=value['nondeterministic'];keys(entropy,ENTROPY,'predeclared raw constructor entropy')
    need(all(type(entropy[n]) is str and re.fullmatch('[0-9a-f]{8}',entropy[n]) for n in ENTROPY[:-1]) and type(entropy['sn.bs.seed']) is int and 0<=entropy['sn.bs.seed']<(1<<48),'raw float-bit/Random seed entropy remains typed and present')
def observations(value,cases,source):
    keys(value,('rows','world','rebind','registry','completion'),'original finite observation')
    world=expected_world(source);need(same(value['world'],world),'exact genuine original World projection')
    need(type(value['rows']) is list and len(value['rows'])==10,'exact ten observed calls')
    for row,case in zip(value['rows'],cases['cases']):
        keys(row,('index','name_utf16','world_argument','distinct_string','name_same_as_first_pig','name_equals_first_pig','returned','escaped_class','escaped_message','result','entity_id_delta','world_before','world_after'),'factory row')
        i=case['index'];name=case['name']
        need(type(row['index']) is int and row['index']==i and same(row['name_utf16'],None if name is None else list(map(ord,name))) and row['world_argument']==case['world'] and row['distinct_string'] is case['distinct_string'],'exact original named call arguments')
        need(row['returned'] is True and row['escaped_class'] is None and row['escaped_message'] is None,'original factory returns; no invented escaped result')
        need(row['name_same_as_first_pig'] is (i==5) and row['name_equals_first_pig'] is (i in (5,6,7)),'actual String identity/value distinctions')
        need(same(row['world_before'],None if i<7 else world) and same(row['world_after'],None if i<7 else world),'World/list state preserved at every recorded call boundary')
        if i in (5,6,7):
            entity(row['result'],i-5,case['world']);need(row['entity_id_delta']==(None if i==5 else 1) and (i==5 or type(row['entity_id_delta']) is int),'actual source ID delta retained')
        else:need(row['result'] is None and row['entity_id_delta'] is None,'actual nullable result retained')
    rb=value['rebind'];keys(rb,('operation','before','after','world_before','world_after'),'original World setter')
    need(rb['operation']=='sn.a(Lfd;)V' and same(rb['world_before'],world) and same(rb['world_after'],world),'actual setter method and unchanged World projection')
    entity(rb['before'],0,None);entity(rb['after'],0,'W0')
    need(same(rb['before']['nondeterministic'],rb['after']['nondeterministic']),'same receiver entropy unchanged by original World store')
    need(same(rb['before'],value['rows'][5]['result']),'same retained original case5 receiver before rebind')
    expected_entries=[{name:cl for name,cl,i in REGISTRATIONS},{cl:name for name,cl,i in REGISTRATIONS},
        {str(i):cl for name,cl,i in REGISTRATIONS},{cl:i for name,cl,i in REGISTRATIONS}]
    expected_maps=[{'field':field,'class':'java.util.HashMap','size':24,'entries':entries} for field,entries in zip(('a','b','c','d'),expected_entries)]
    need(same(value['registry'],{'after_first_call':expected_maps,'after_all_calls':expected_maps}),'four exact original Class registration map contents; no iteration-order claim')
    need(same(value['completion'],{'rows':10,'rebinds':1,'retained_entities':3,'network':0,'native':0,'execs':0,'denied_writes':0,
        'write_checks':[{'operation':'write','role':'owned-session-lock'},{'operation':'write','role':'existing-copied-region-rw-open'}]}),'exact complete guarded original result')
def command_roles(mode,name,seed):
    flags=['-Xint'] if mode=='interpreter' else []
    return ['JDK/bin/java',*flags,'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow',
        '--add-opens=java.base/java.util=ALL-UNNAMED','-classpath','CLASSES;CLIENT_JAR;LIBRARIES','NamedFactoryObservation',
        'CLIENT_JAR','OWNED_COPY/'+mode+'/saves','DECLARED_PROFILES',name,str(seed)]
def stdout_transport(value,pins,line_ending):
    """Serialize the executed observer's selected output; not a new VM observation."""
    need(line_ending in ('\n','\r\n'),'recorded source stdout line ending')
    compact=lambda v:json.dumps(v,separators=(',',':'),ensure_ascii=True)
    origins={'jar_sha256':C.R.JAR_HASH,'classes':{n:pins[n]['class_sha256'] for n in C.CORE}}
    lines=['16 achievements','151 recipes','NAMED_FACTORY_ORIGINS '+compact(origins)]
    for row in value['rows']:
        if row['index']==7:lines.append('NAMED_FACTORY_WORLD '+compact(value['world']))
        lines.append('NAMED_FACTORY_ROW '+compact(row))
    for prefix,key in (('REBIND','rebind'),('REGISTRY','registry'),('COMPLETE','completion')):lines.append('NAMED_FACTORY_'+prefix+' '+compact(value[key]))
    return (line_ending.join(lines)+line_ending).encode('utf8')
def audit(blobs):
    need(set(blobs)==FILES|{'publication.json'},'exact text/hash-only public packet file set')
    manifest=parse(blobs['publication.json']);keys(manifest,('meaning','files'),'publication fingerprints')
    need(manifest['meaning']=='Postexecution publication-only helper/data fingerprints.' and set(manifest['files'])==FILES,'exact publication-only manifest')
    for name,pin in manifest['files'].items():need(same(pin,{'bytes':len(blobs[name]),'sha256':digest(blobs[name])}),'public packet bytes/hash '+name)
    cases=parse(blobs['cases.json']);declaration(cases)
    source=parse(blobs['input-world.json']);expected_world(source)
    finite=parse(blobs['finite.json']);keys(finite,('schema','observations','stable_projection'),'finite publication')
    need(finite['schema']=='ORIGINAL_CLIENT_NAMED_FACTORY_V1' and set(finite['observations'])==set(MODES),'exact original two-mode finite cohort')
    for value in finite['observations'].values():observations(value,cases,source)
    need(same(C.stable(finite['observations']['interpreter']),C.stable(finite['observations']['default'])) and
        same(finite['stable_projection'],C.stable(finite['observations']['interpreter'])),'only the five declared raw entropy fields omitted from cross-VM equality')
    provenance=parse(blobs['provenance.json'])
    keys(provenance,('scope','excluded','original_processes','original_modes','calls_per_mode','rebinds_per_mode','native_parity_claimed','public_java_is_executed_source',
        'packaging_scope','observation_projection_sha256','archive_pins_sha256','transport_projection_sha256','original_class_count','declared_method_count','code_method_count',
        'retained_capture_json','executed_inputs','compiled_products','java_version_lines','inherited_jvm_option_keys_removed_if_present','keys_present','stream_scope','stdout_line_ending'),'publication provenance')
    need(provenance['scope']=='Direct original client JC named factory and original SN World setter; same NULL World operand used at AG48, without entering AG.' and provenance['excluded']==EXCLUDED,'exact bounded execution scope')
    need(provenance['original_processes']==2 and provenance['original_modes']==list(MODES) and provenance['calls_per_mode']==10 and provenance['rebinds_per_mode']==1 and provenance['native_parity_claimed'] is False,'two genuine ordinary original VMs only')
    need(provenance['observation_projection_sha256']==digest(encoded(finite)),'complete finite projection bound by physical publisher')
    need(provenance['public_java_is_executed_source'] is True and provenance['executed_inputs']['observer-java']['before']==manifest['files']['NamedFactoryObservation.java'] and
        provenance['executed_inputs']['cases']['before']==manifest['files']['cases.json'],'exact executed Java/declaration inputs')
    need(provenance['packaging_scope']=='Portable runner, publication validator, controls, header and prose were authored after execution; they were not used to run the retained two VMs.','no invented execution of later helpers')
    for pin in provenance['executed_inputs'].values():need(same(pin['before'],pin['after']),'all selected executed inputs unchanged')
    for pin in provenance['compiled_products'].values():need(same(pin['before'],pin['after']),'both actual authored class products unchanged')
    need(len(provenance['executed_inputs'])==20 and len(provenance['compiled_products'])==2,'actual retained input/product census')
    pins=parse(blobs['source-pins.json']);keys(pins,('jar_sha256','classes','meaning'),'archive identity pins')
    need(pins['jar_sha256']==C.R.JAR_HASH and set(pins['classes'])==set(C.CORE) and pins['meaning']=='Archive Class/Code and actual loaded-resource identities; no entered/live-Code or call-order trace.','actual original archive identity scope')
    need(provenance['archive_pins_sha256']==digest(encoded(pins['classes'])) and provenance['original_class_count']==len(pins['classes'])==23 and
        provenance['declared_method_count']==sum(len(v['methods']) for v in pins['classes'].values()) and provenance['code_method_count']==sum('code_sha256' in m for v in pins['classes'].values() for m in v['methods']),'all original class/Code pins and exact summaries')
    named=next(m for m in pins['classes']['jc']['methods'] if m['name']=='a' and m['descriptor']=='(Ljava/lang/String;Lfd;)Lsn;')
    setter=next(m for m in pins['classes']['sn']['methods'] if m['name']=='a' and m['descriptor']=='(Lfd;)V')
    need(named['code_bytes']==57 and named['code_sha256']=='5ae348503fad80c947c0d66d2b09b5bf4d817a84a024c10abc788d62f05c2224' and setter['code_bytes']==6 and setter['code_sha256']=='86b367cbb274922e9683078c85b5650f2dd3cf86280c2daab98e01dac3dcc13d','exact static named overload and World-setter Code identities')
    receipts=parse(blobs['receipts.json']);need(set(receipts)=={'compile','java-version','javac-version',*MODES},'complete original/tool process receipts')
    need(provenance['transport_projection_sha256']==digest(encoded(receipts)),'all actual stream/receipt hashes and counts bound by physical publisher')
    for name,r in receipts.items():
        keys(r,('exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stderr_bytes','stdout_sha256','stderr_sha256',
            'stdout_limit','stderr_limit','command_representation','command_roles','raw_receipt'),'sanitized child receipt')
        need(type(r['exit']) is int and r['exit']==0 and r['timed_out'] is False and r['pipe_capture_complete'] is True and r['output_overflow'] is False and r['reader_errors']=={},'complete bounded successful child')
        need(r['stdout_limit']==4194304 and r['stderr_limit']==1048576 and r['command_representation']=='role projection, not verbatim private command','exact retained transport bounds/command scope')
        need(type(r['timeout_seconds']) is int and r['timeout_seconds']==(90 if name in MODES else 60 if name=='compile' else 20),'exact declared child timeout')
        for stream in ('stdout','stderr'):
            need(type(r[stream+'_bytes']) is int and 0<=r[stream+'_bytes']<=r[stream+'_limit'] and type(r[stream+'_sha256']) is str and re.fullmatch('[0-9a-f]{64}',r[stream+'_sha256']),'typed bounded actual stream count/hash')
        need(type(r['raw_receipt']['bytes']) is int and r['raw_receipt']['bytes']>0 and re.fullmatch('[0-9a-f]{64}',r['raw_receipt']['sha256']),'actual complete receipt fingerprint')
        if name=='compile':need(r['stdout_bytes']==r['stderr_bytes']==0 and r['stdout_sha256']==r['stderr_sha256']==digest(b''),'actual clean authored compilation streams')
        if name in MODES:
            need(r['command_roles']==command_roles(name,source['metadata']['name'],source['metadata']['seed']),'exact actual VM mode/arguments')
            wire=stdout_transport(finite['observations'][name],pins['classes'],provenance['stdout_line_ending'])
            need(r['stdout_bytes']==len(wire) and r['stdout_sha256']==digest(wire),'complete original selected stdout projection matches actual retained byte count/hash')
        else:
            expected=['JDK/bin/javac','-encoding','UTF-8','-implicit:none','-classpath','CLASSES;CLIENT_JAR;LIBRARIES','-d','CLASSES','OBSERVER_JAVA'] if name=='compile' else ['JDK/bin/'+('java' if name=='java-version' else 'javac'),'-version']
            need(r['command_roles']==expected,'exact tool command roles')
    diagnostics=parse(blobs['diagnostics.json']);need(set(diagnostics)==set(MODES),'both original stderr projections')
    for d in diagnostics.values():
        need(same(d,{'guard_warning_count':4,'caught_exception_count':2,'exception_class':'java.lang.InstantiationException',
            'original_source_frame':{'class':'jc','method':'a','file':'SourceFile','line':62},
            'observer_source_frame':{'class':'NamedFactoryObservation','method':'main','file':'NamedFactoryObservation.java','line':107}}),'actual original caught diagnostic and caller fields; raw stderr remains private')
    saves=parse(blobs['source-manifests.json']);keys(saves,('source_before','source_after','copies_after'),'saved file manifests')
    need(same(saves['source_before'],saves['source_after']) and len(saves['source_before'])==13 and set(saves['copies_after'])==set(MODES),'all source files unchanged and both copies retained')
    for inv in saves['copies_after'].values():
        need(set(inv)==set(saves['source_before']) and sorted(n for n,p in inv.items() if not same(p,saves['source_before'][n]))==['alpha/session.lock'] and inv['alpha/session.lock']['bytes']==8,'exactly owned SessionLock differs; other copied bytes exact')
    import publish
    need(blobs['native-observations.h']==publish.header(finite['stable_projection']),'native expected header derives exactly from finite original Pig fields')
    return finite
if __name__=='__main__':
    audit(files(HERE));print('PASS: strict original named-factory finite packet audit')
