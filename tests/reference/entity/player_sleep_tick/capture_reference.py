"""Reproduce unresumed original prefixes into a fresh workspace .local tree."""
import argparse,copy,hashlib,importlib.util,json,os,re,sys,time,zipfile
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tests/reference/client/controller/capture.py').is_file())
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SERVER='033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d'
LIBS={'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':'sdk/lwjgl-2.9.0.jar','org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':'sdk/lwjgl_util-2.9.0.jar','net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':'sdk/jinput-2.0.5.jar','net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':'sdk/jutils-1.0.0.jar'}

def private_output(directory,root=ROOT):
    directory=Path(directory).resolve();private=(Path(root)/'.local').resolve()
    if not(directory.is_relative_to(private) and directory!=private and not directory.exists()):
        raise ValueError('capture requires a fresh private .local descendant')
    return directory
def require(value,message):
    if not value:raise ValueError(message)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
def pairs(items):
    result={}
    for key,value in items:require(key not in result,'duplicate JSON key');result[key]=value
    return result
def loads(text):
    def invalid(x):raise ValueError('nonfinite JSON token '+x)
    return json.loads(text,object_pairs_hook=pairs,parse_constant=invalid)
def load(path):return loads(Path(path).read_text(encoding='utf-8'))
def exact(a,b):return type(a) is type(b) and (a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a) if type(a) is dict else len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b)) if type(a) is list else a==b)
def keys(value,names,label):require(type(value) is dict and set(value)==set(names),label)
def clean(value):
    if type(value) is dict:return {k:clean(v) for k,v in value.items() if k!='raw_object_id'}
    if type(value) is list:return [clean(v) for v in value]
    return value
def inventory(directory):return {p.relative_to(directory).as_posix():digest(p) for p in sorted(directory.rglob('*')) if p.is_file()}
def preservation(raw):
    a,b=raw['before'],raw['after']
    allowed={'sn.aM','sn.aN','sn.aO','sn.aP','sn.aQ','sn.aR','sn.bg','sn.bh','sn.bf','gs.u','gs.a','gs.v','gs.w','gs.y','gs.b'}
    require(a['actor_fields'].keys()==b['actor_fields'].keys(),'same complete Actor field inventory')
    changed={k for k in a['actor_fields'] if not exact(a['actor_fields'][k],b['actor_fields'][k])}
    require(changed<=allowed,'unselected Actor mutation '+str(changed-allowed))
    changed={k for k in a['world_fields'] if not exact(a['world_fields'][k],b['world_fields'][k])}
    require(changed<={'fd.J'} and exact(a['provider_fields'],b['provider_fields']),'unselected World/provider mutation')
    require(exact(a['random'],b['random']),'within-call actual Random preservation')
    for phase in (a,b):
        require(phase['selected']['world_players']==phase['selected']['world_entities']==1,'genuine published World membership')
        require(phase['selected']['player_list_members']==[{'class':'dc','actor':True}] and phase['selected']['player_list_same'] is True,'actual captured List identity/membership')
        require(phase['selected']['client_null'] is True and phase['selected']['bed_static_same'] is True,'actual NULL client and final static identity')
    require(raw['worker_alive'] is True and raw['worker_returned'] is False,'unreturned genuine worker')
def validate_journal(journal,profile,pinned):
    p=profile['values'];number=p[0]
    require(journal['case']==number and journal['root_returned'] is False and journal['terminal_resumed'] is False,'actual terminal prefix identity')
    worker=journal['source_worker']
    require(worker['name']=='original-gs-parent-'+str(number) and worker['class']=='PlayerSleepParentObservation$SourceWorker' and worker['daemon'] is True and worker['suspended'] is True and worker['suspend_count']>=1,'actual blocked worker identity')
    require(journal['source_receiver']['class']=='dc' and journal['source_receiver']['identity']=='actor','terminal actual source receiver')
    calls=journal['calls'];events=journal['events']
    require(0<len(calls)<=512 and 0<len(events)<=4096,'bounded actual selected trace')
    require([e['tick'] for e in events]==list(range(len(events))),'total source-event order')
    root=calls[0];require(root['site']=={'class':'gs','method':'w_','descriptor':'()V','pc':0} and root['caller']['class']=='PlayerSleepParentObservation$SourceWorker' and root['caller']['method']=='run' and root['receiver']['identity']=='actor' and root['arguments']==[] and root['completed'] is False,'ordinary genuine root and helper caller')
    require(journal['stack'][-1]['class']=='PlayerSleepParentObservation$SourceWorker' and any(s['class']=='gs' and s['method']=='w_' for s in journal['stack']),'actual original blocked stack')
    reached={c['site']['class']+'.'+c['site']['method']+c['site']['descriptor'] for c in calls}
    require('ls.w_()V' not in reached and 'dc.o()V' not in reached,'continuation111 never entered')
    expected_fault=None
    if p[5] and p[7]&2:expected_fault=('gs','w_',36)
    elif p[5] and not p[4] and p[7]&1:expected_fault=('gs','am',8)
    elif p[5] and not p[4] and p[2] and not p[3] and p[7]&4:expected_fault=('sn','e',86)
    terminal=journal['terminal_site']
    if expected_fault:
        require(journal['outcome']=='SOURCE_EXCEPTION' and journal['exception']=='java/lang/NullPointerException' and (terminal['class'],terminal['method'],terminal['pc'])==expected_fault,'actual declared first null prefix')
        require(events[-1]['kind']=='EXCEPTION' and events[-1]['site']==terminal,'exception is actual last source event')
    else:
        require(journal['outcome']=='PREFIX111' and journal['exception'] is None and terminal=={'class':'gs','method':'w_','descriptor':'()V','pc':111},'actual continuation111')
        require(events[-1]=={'kind':'PREFIX','site':terminal,'tick':len(events)-1},'actual final prefix event')
        require(all(c['completed'] is True for c in calls[1:]),'all reached children returned before111')
    wakes=[c for c in calls if c['site']['class']=='gs' and c['site']['method']=='a' and c['site']['descriptor']=='(ZZZ)V']
    should_wake=bool(p[5] and not p[4] and not(p[7]&3) and (p[3] or p[2]))
    require(len(wakes)==int(should_wake),'one reached actual parent wake or ordinary bypass')
    if wakes:
        missing=bool(p[3]);wake=wakes[0]
        require(wake['caller']=={'class':'gs','method':'w_','descriptor':'()V','pc':53 if missing else 73} and exact(wake['arguments'],[missing,True,not missing]),'actual reached caller and exact wake operands')
        require(wake['completed'] is (expected_fault is None),'actual reached wake return/failure')
        if missing:require('fd.f()Z' not in reached,'missing-Bed arm precedes daytime')
    if p[4] and p[5]:require('gs.am()Z' not in reached and not wakes,'actual remote bypass')
    for call in calls:
        site=call['site'];owner=site['class'];require(owner in pinned,'actual pinned call owner '+owner)
        require(any(m['name']==site['method'] and m['descriptor']==site['descriptor'] for m in pinned[owner]['methods']),'actual pinned method identity')
    return clean(journal)
def projection(folder,profiles,pins,helper_pins):
    stdout=(folder/'target.stdout').read_text(encoding='utf-8');lines=stdout.splitlines();count=len(profiles)
    require(lines[-1]==f'SLEEP_PARENT_COMPLETE cases={count} parked={count} network=0 native=0 writes=0 exec=0 whole_parent_returns=0 GUI=0','normal helper termination guard')
    class_lines=[line for line in lines if line.startswith('CLASS ')]
    require(class_lines==['CLASS '+n+' '+str(p['class_bytes'])+' '+p['class_sha256'] for n,p in pins['classes'].items()],'actual original class origins/bytes')
    raw=[loads(line[len('SLEEP_PARENT '):]) for line in lines if line.startswith('SLEEP_PARENT ')]
    require(len(raw)==count,'all prepared source rows')
    stderr=(folder/'target.stderr').read_text(encoding='utf-8').splitlines()
    require(len(stderr)==4 and all(line.startswith('WARNING:') for line in stderr) and 'System::setSecurityManager' in '\n'.join(stderr),'only actual JDK security-manager warnings')
    journal=[loads(line) for line in (folder/'journal.jsonl').read_text(encoding='utf-8').splitlines()]
    codes=[r for r in journal if r['event']=='code'];js=[r for r in journal if r['event']=='journal'];completed=[r for r in journal if r['event']=='complete']
    require(len(js)==count and len(completed)==1,'read-only controller completion extent')
    complete=completed[0]
    require(complete['profiles']==complete['parked_workers']==count and all(complete[k]==0 for k in ('source_returns','terminal_resumes','jdi_invocations','jdi_writes','jdi_forced_returns')) and complete['planned_target_termination']=='JDWP_VM_EXIT_0' and complete['vm_exit_requests']==1,'explicit planned termination distinct from source return')
    require(len(complete['verified_parked'])==count,'all parked frames reverified immediately before termination')
    for i,(record,j) in enumerate(zip(complete['verified_parked'],js)):
        require(record['case']==i and record['site']==j['terminal_site'] and record['suspended'] is True and record['suspend_count']>=1 and record['raw_worker_id']==j['source_worker']['raw_object_id'],'same actual blocked source worker before VM exit')
    all_pins=dict(pins['classes']);all_pins.update(helper_pins)
    for code in codes:
        matches=[m for m in all_pins[code['class']]['methods'] if m['name']==code['method'] and m['descriptor']==code['descriptor']]
        require(len(matches)==1 and matches[0]['code_length']==code['bytes'] and matches[0]['code_sha256']==code['sha256'],'actual live Code bytes '+str(code))
    rows=[]
    for profile,r,j in zip(profiles,raw,js):
        require(r['schema']=='SLEEP_PARENT_RAW_V1' and r['case']==profile['values'][0] and exact(r['input'],profile['values']),'exact authored source inputs')
        preservation(r);j=validate_journal(j,profile,all_pins)
        rows.append({'case':r['case'],'name':profile['name'],'input':r['input'],'before':r['before']['selected'],'after':r['after']['selected'],'random_before':r['before']['random'],'random_after':r['after']['random'],'unselected_actor_preserved':True,'unselected_world_preserved':True,'journal':j})
    return {'schema':'PLAYER_SLEEP_PARENT_PREFIX_FINITE_V1','rows':rows,'live_code':sorted(codes,key=lambda x:(x['class'],x['method'],x['descriptor'])),'qualification':{'whole_parent_return':False,'source_boundary':111,'source_frames_resumed':False,'gui':False,'native_implementation':False,'null_final_T_excluded':True}}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--preflight',action='store_true');parser.add_argument('--repeats',type=int,default=2);parser.add_argument('--jdk',type=Path,required=True);parser.add_argument('--libraries',type=Path,required=True)
    a=parser.parse_args();out=private_output(a.output);require(1<=a.repeats<=2,'bounded two fresh target VMs')
    jdk=a.jdk.resolve();libraries=a.libraries.resolve();jar=ROOT/'b1.7.3.jar';server=ROOT/'b1.7.3_server.jar'
    java=jdk/'bin/java.exe';javac=jdk/'bin/javac.exe';libs=[libraries/n for n in LIBS]
    C=module('parent_bounded_capture',ROOT/'tests/reference/client/controller/capture.py');F=module('parent_original_classfile',ROOT/'tests/reference/entity/player/classfile.py')
    profile_path=HERE/(('preflight-' if a.preflight else '')+'profiles.json');tsv=profile_path.with_suffix('.tsv');profiles=load(profile_path);pins=load(HERE/'source-pins.json')
    require(len(profiles)==(4 if a.preflight else 36),'fixed bounded prepared matrix')
    require(digest(jar)==JAR and digest(server)==SERVER,'both unchanged original archives')
    historical=load(ROOT/'tests/reference/entity/player_sleep_wake/provenance.json')['binary_input_sha256']
    require(digest(java)==historical['jdk/bin/java.exe'] and digest(javac)==historical['jdk/bin/javac.exe'] and all(digest(path)==historical[role] for path,role in zip(libs,LIBS.values())),'actual pinned JDK17 tools and SDK jars')
    with zipfile.ZipFile(jar) as archive:
        for name,pin in pins['classes'].items():require(exact(F.parse_class(archive.read(name+'.class')),pin),'original complete class/Code pin '+name)
    source_paths=[p for p in HERE.iterdir() if p.is_file() and p.suffix in ('.py','.java','.json','.tsv','.txt')]
    support=[ROOT/'tests/reference/client/controller/capture.py',ROOT/'tests/reference/entity/player/classfile.py',ROOT/'src/client/provenance/source-bindings.json']
    support=[p for p in support if p.is_file()]
    binary={**{'jdk/'+p.relative_to(jdk).as_posix():p for n in ('bin','lib') for p in sorted((jdk/n).rglob('*')) if p.is_file()},**{role:path for path,role in zip(libs,LIBS.values())}}
    legacy={}
    for name in ('tests/reference/entity/active_tick','tests/reference/entity/player_sleep','tests/reference/entity/player_sleep_wake'):
        directory=ROOT/name
        for p in directory.rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts:legacy[p.relative_to(ROOT).as_posix()]=p
    inputs={'archive/client.jar':jar,'archive/server.jar':server,**binary,**{'producer/'+p.name:p for p in source_paths},**{'support/'+p.relative_to(ROOT).as_posix():p for p in support}}
    before={name:digest(path) for name,path in inputs.items()};old_before={name:digest(path) for name,path in legacy.items()}
    require(out.resolve().is_relative_to((ROOT/'.local').resolve()) and out.resolve()!=(ROOT/'.local').resolve(),'capture output must be a fresh private .local descendant')
    out.mkdir(parents=True,exist_ok=False);classes=out/'classes';classes.mkdir();frozen=out/'producer';frozen.mkdir()
    for p in source_paths:(frozen/p.name).write_bytes(p.read_bytes())
    env=os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(name,None)
    report={'schema':'PLAYER_SLEEP_PARENT_CAPTURE_V1','status':'FAIL','historical_prepared_input_head':'21effab7','scope':'Actual original GS.w_ prefix111 or first exception; unresumed source workers; no parent-return/native/GUI qualification','preflight':a.preflight,'profiles_per_vm':len(profiles),'fresh_target_vms':a.repeats,'input_before_sha256':before,'legacy_before_sha256':old_before,'binary_input_sha256':{n:before[n] for n in binary},'null_final_T_excluded':True};helpers=None
    try:
        for name,tool in (('java',java),('javac',javac)):
            r=C.capture([str(tool),'-version'],out,name,15,env);report[name]=r;C.require_success(r);require('17.0.17' in ''.join((out/r[k]).read_text() for k in ('stdout_file','stderr_file')),'explicit matching JDK17 version')
        cp=os.pathsep.join(map(str,[classes,jar,*libs]));r=C.capture([str(javac),'--add-modules','jdk.jdi','-encoding','UTF-8','-classpath',cp,'-d',str(classes),str(HERE/'PlayerSleepParentObservation.java'),str(HERE/'PlayerSleepParentJdi.java')],out,'compile',30,env);report['compile']=r;C.require_success(r,empty_stderr=True)
        helpers=inventory(classes);report['helper_before_sha256']=helpers;helper_pins={F.parse_class(p.read_bytes())['name']:F.parse_class(p.read_bytes()) for p in classes.rglob('*.class')}
        projections=[];runs=[]
        for index in range(a.repeats):
            folder=out/f'vm-{index}';started=time.monotonic()
            r=C.capture([str(java),'--add-modules','jdk.jdi','-Xmx256m','-classpath',cp,'PlayerSleepParentJdi',str(jdk),cp,str(jar),str(tsv),str(HERE/'class-names.txt'),str(folder),str(len(profiles))],out,f'controller-{index}',300,env)
            r['wall_seconds']=time.monotonic()-started;runs.append(r);report['original']=runs;C.require_success(r,empty_stderr=True)
            projections.append(projection(folder,profiles,pins,helper_pins))
            (out/f'projection-{index}.json').write_text(json.dumps(projections[-1],indent=2)+'\n',encoding='utf-8',newline='\n')
        require(all(exact(projections[0],p) for p in projections),'fresh original selected prefix projections agree')
        (out/'finite.json').write_text(json.dumps(projections[0],indent=2)+'\n',encoding='utf-8',newline='\n')
        report.update(status='PASS_ORIGINAL_PREFIX_COMPONENTS_ONLY',finite_sha256=digest(out/'finite.json'),selected_calls=sum(len(r['journal']['calls']) for r in projections[0]['rows']),selected_events=sum(len(r['journal']['events']) for r in projections[0]['rows']),live_code_methods=len(projections[0]['live_code']),whole_parent_returns=0,source_terminal_resumes=0)
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)};raise
    finally:
        report['input_after_sha256']={name:digest(path) for name,path in inputs.items()};report['legacy_after_sha256']={name:digest(path) for name,path in legacy.items()};report['helper_after_sha256']=inventory(classes)
        unchanged=before==report['input_after_sha256'] and old_before==report['legacy_after_sha256'] and (helpers is None or helpers==report['helper_after_sha256'])
        report['all_inputs_unchanged']=unchanged
        if not unchanged:report['status']='FAIL'
        (out/'capture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n');require(unchanged,'all producer/helper/original/legacy inputs unchanged')
    print('New private original parent prefix capture:',out/'capture.json')
if __name__=='__main__':main()
