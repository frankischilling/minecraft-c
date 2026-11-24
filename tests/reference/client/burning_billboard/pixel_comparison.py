"""Read-only admission of a finite Windows ordinary/pixel supplement.

Primary Mesa admission is unchanged. This module compares all channels of14
prepared raw frames on the pinned NVIDIA device across distinct SDK4.6/Core3.3
contexts. It provides no Native build/producer, JDI, live-matrix or gameplay credit.
"""
import copy,hashlib,importlib.util,json,os,re,zipfile
from pathlib import Path
BASE=Path(__file__).resolve().parent
ROOT=next(p for p in BASE.parents if(p/'CMakeLists.txt').is_file())

def need(value,message):
    if not value:raise AssertionError(message)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb')as f:
        for b in iter(lambda:f.read(1048576),b''):h.update(b)
    return h.hexdigest()

def physical(name):
    text=str(name).replace('\\','/')
    if os.name=='nt'and re.match(r'^/mnt/[a-z]/',text):return Path(text[5].upper()+':/'+text[7:])
    if os.name!='nt'and re.match(r'^[A-Za-z]:/',text):return Path('/mnt')/text[0].lower()/text[3:]
    p=Path(text);return p if p.is_absolute()else ROOT/p

def fingerprints(expected):
    for name,pin in expected.items():need(physical(name).is_file()and sha(physical(name))==pin,'current actual captured input '+name)

def rows(path):return[json.loads(l)for l in path.read_text(encoding='utf-8').splitlines()]

def metadata():
    m=json.loads((BASE/'windows-ordinary-evidence.json').read_text(encoding='utf-8'))
    need(m['schema']==1 and m['chronology']['public_paths_were_capture_start_inputs']is False,'explicit post-capture public chronology')
    for name,pin in m['captured_authored_sha256'].items():need(sha(BASE/name)==pin,'exact authored Windows capture-start source '+name)
    return m

def _source_rows(data,late):
    draws=[r for r in data if r['event']=='draw'];need([r['index']for r in draws]==list(range(7)),'all7 ordered Source post calls')
    need([r['kind']for r in draws]==['cn','uw','xt','gb','cn','gb','gb'],'actual finite actor profiles')
    for r in draws:
        need(r['outcome']==1 and r['failure']is None and r['gl_error']==0,'actual Source call returned without GLerror')
        for key in ('entropy','actor','camera'):need(r['before'][key]==r['after'][key],'actual same-VM complete Source '+key+' invariance')
    need(late['target_main_returned']is False and late['source_calls_already_returned']==7 and late['raw_frames_already_written']==14,'retain actual late helper failure without whole-main return credit')
    need(late['class']=='java.nio.file.NoSuchFileException'and late['file'].replace('\\','/').endswith('/proc/self/maps')and late['display_created_after_target_finally']is False,'actual late Linux-only metadata failure and reached SDK cleanup')
    return draws

def admit_windows(path):
    path=path.resolve(strict=True);m=metadata();proof=m['source'];need(sha(path/'receipt.json')==proof['receipt_sha256'],'exact immutable Windows ordinary receipt')
    r=json.loads((path/'receipt.json').read_text(encoding='utf-8'));need(r['status']==proof['status']and r['target_main_returned']is False and r['default_constructor_entropy_rebound']is False,'finite Source calls with acknowledged Target.main failure; no entropy rebinding')
    need(r['input_before_sha256']==r['input_after_sha256']==proof['input_sha256']and r['inputs_unchanged']is True,'complete actual stable Windows input inventory')
    need(r['frozen_helper_before_sha256']==r['frozen_helper_after_sha256']==proof['helper_sha256'],'exact4 frozen SourceD helper class bytes')
    need(r['wrapper_before_sha256']==r['wrapper_after_sha256']==proof['wrapper_sha256'],'actual separately pinned outer launcher class')
    need(r['artifacts_sha256']==proof['artifacts_sha256'],'complete immutable actual Windows artifacts')
    fingerprints(proof['input_sha256'])
    for name,pin in proof['artifacts_sha256'].items():need(sha(path/name)==pin,'immutable actual Windows output '+name)
    late=json.loads((path/'late-metadata-failure.json').read_text());need(late==proof['late_authored_metadata_failure'],'exact retained actual late helper failure')
    data=rows(path/'target.ndjson');draws=_source_rows(data,late);device=next(r for r in data if r['event']=='device');need(device==proof['context'],'actual Source device/effective framebuffer')
    authority_path=physical(proof['authority_path']);authority=json.loads(authority_path.read_text());need(len(authority['classes'])==747 and sha(physical(proof['jar_path']))==authority['jar_sha256']=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c','unchanged actual original JAR and complete class authority')
    spec=importlib.util.spec_from_file_location('windows_pixel_actual_classfile',physical(proof['classfile_path']));classfile=importlib.util.module_from_spec(spec);spec.loader.exec_module(classfile)
    with zipfile.ZipFile(physical(proof['jar_path']))as jar:actual={n[:-6]:classfile.inspect_class(jar.read(n))for n in jar.namelist()if n.endswith('.class')}
    need(actual==authority['classes'],'current original whole-class and Code identities')
    binds=rows(path/'original-bindings.ndjson');need([r['class']for r in binds]==sorted(actual),'all747 original runtime bindings, without forced initialization')
    for b in binds:
        c=actual[b['class']];need(b['origin']=='file:/C:/Users/admin/minecraft/b1.7.3.jar'and b['bytes']==c['class_bytes']and b['sha256']==c['class_sha256'],'actual unchanged original class origin/identity')
    initialized=sorted({n for n in re.findall(r"Initializing '([^']+)'",(path/'class-init.log').read_text())if n in actual});need(initialized==proof['actual_initialized_original_classes']and len(initialized)==245,'actual initialized-original inventory')
    need(sha(BASE/'windows-wrapper-code-pins.json')==proof['wrapper_Code_sha256']==sha(path/'wrapper-Code.json'),'actual authored wrapper Code metadata')
    need({p.name:classfile.inspect_class(p.read_bytes())for p in(path/'wrapper-classes').glob('*.class')}==json.loads((BASE/'windows-wrapper-code-pins.json').read_text()),'actual complete current launcher class/Code identities')
    frames={name:(path/name).read_bytes()for name in _frame_names()};need(all(len(v)==262144 for v in frames.values()),'complete14 raw Source backbuffers')
    admitted={'path':path,'metadata':m,'receipt':r,'draws':draws,'data':data,'late':late,'frames':frames,'source_receipt_sha256':proof['receipt_sha256']};recheck(admitted);return admitted

def _frame_names():return{phase+str(i)+'.rgba'for i in range(7)for phase in ('baseline','draw')}

def recheck(admitted):
    m=metadata();need(m==admitted['metadata'],'unchanged supplemental authored metadata');p=m['source'];fingerprints(p['input_sha256']);need(sha(admitted['path']/'receipt.json')==p['receipt_sha256'],'current immutable Source receipt')
    for name,pin in p['artifacts_sha256'].items():need(sha(admitted['path']/name)==pin,'current immutable Source output '+name)
    _source_rows(admitted['data'],admitted['late'])
    for name,raw in admitted['frames'].items():need(hashlib.sha256(raw).hexdigest()==p['artifacts_sha256'][name],'cached raw Source frame still matches immutable captured bytes')

def _native(path):
    path=path.resolve(strict=True);need({p.name for p in path.glob('*.rgba')}==_frame_names(),'exact complete Native prepared frame inventory')
    files=[path/'billboard.ndjson',*[path/name for name in sorted(_frame_names())]];pins={str(p):sha(p)for p in files};return rows(path/'billboard.ndjson'),{name:(path/name).read_bytes()for name in _frame_names()},pins

def compare_data(data,frames,admitted):
    _source_rows(admitted['data'],admitted['late']);m=admitted['metadata'];devices=[r for r in data if r['event']=='device'];need(len(devices)==1 and devices[0]==m['finite_native_context'],'exact finite Native hardware/driver/APIcontext/framebuffer metadata')
    source=m['source']['context'];native=devices[0]
    for k in ('vendor','renderer','framebuffer'):need(native[k]==source[k],'actual same hardware/effectiveFB '+k)
    versions=[re.fullmatch(r'(\d+\.\d+\.\d+) NVIDIA ([\d.]+)',d['version'])for d in (native,source)];need(all(versions)and versions[0][2]==versions[1][2]=='581.42'and versions[0][1]=='3.3.0'and versions[1][1]=='4.6.0','distinct pinned Core3.3/SDK4.6 contexts on same actual NVIDIA581.42 driver')
    need(set(frames)==_frame_names()and all(len(v)==262144 for v in frames.values()),'all14 complete Native raw frames')
    inputs=[r for r in data if r['event']=='input'];states=[r for r in data if r['event']=='state'];fires=[r for r in data if r['event']=='fire'];need([r['case']for r in inputs]==[r['case']for r in fires]==list(range(7))and[(r['case'],r['phase'])for r in states]==[(i,p)for i in range(7)for p in ('before','after')],'actual ordered completed Native7profile transport')
    results=[]
    for i,o in enumerate(admitted['draws']):
        n=inputs[i];oi=o['input'];need(n['actor']==o['kind']and n['bv']==oi['bv']==20,'actual selected Native actor/burning input')
        for a,b,t in [('width','width','F'),('height','height','F'),('y','y','D'),('min_y','bounds_min_y','D'),('view_yaw','manager_yaw','F')]:need(n[a]==oi[b][t],'actual selected finite caller '+a)
        need(states[2*i]['entropy']==states[2*i+1]['entropy'],'actual Native same-process raw entropy invariance')
        for phase in ('baseline','draw'):
            name=phase+str(i)+'.rgba';a,b=frames[name],admitted['frames'][name];different=sum(a[j:j+4]!=b[j:j+4]for j in range(0,len(a),4));need(a==b,f'exact RGBA failure {name}: {different} different pixels; no tolerance or normalization')
            results.append({'case':i,'phase':phase,'different_RGBA_pixels':different,'Native_sha256':hashlib.sha256(a).hexdigest(),'Source_sha256':hashlib.sha256(b).hexdigest()})
    return {'status':'FINITE_WINDOWS_PREPARED_EXACT_RGBA_PASS','scope':'All14prepared rawRGBAframes on pinnedRTX3060/NVIDIA581.42/effectiveFB across distinctSDK4.6/Core3.3 contexts. No whole-game, Source-main-return, JDI, livebatch/fullGLmatrix or Native build/producer credit.','source_receipt_sha256':admitted['source_receipt_sha256'],'exact_baselines':7,'exact_draws':7,'Source_context':source,'Native_context':native,'target_main_returned':False,'late_authored_metadata_failure_retained':True,'Source_SDK_cleanup_verified':True,'raw_entropy_not_crosspaired':True,'results':results}

def compare_native(path,admitted):
    recheck(admitted);data,frames,pins=_native(path);result=compare_data(data,frames,admitted);need(all(sha(physical(name))==pin for name,pin in pins.items()),'consumed actual Native output bytes unchanged');recheck(admitted);result['consumed_native_output_sha256']=pins;return result

def audit_result(reported,data,frames,admitted):
    need(reported==compare_data(data,frames,admitted),'reported finite pixel result differs from actual immutable observations')

def negatives(path,admitted):
    recheck(admitted);data,frames,pins=_native(path);baseline=compare_data(data,frames,admitted);audit_result(baseline,data,frames,admitted);rejected=[]
    def reject(name,operation):
        try:operation()
        except AssertionError:rejected.append(name)
        else:raise AssertionError('admitted copied observation corruption '+name)
    bad=dict(frames);changed=bytearray(bad['draw0.rgba']);changed[0]^=1;bad['draw0.rgba']=bytes(changed);reject('one_RGBA_byte_changed',lambda:compare_data(data,bad,admitted))
    missing=dict(frames);del missing['baseline6.rgba'];reject('missing_frame',lambda:compare_data(data,missing,admitted))
    wrong=copy.deepcopy(data);next(r for r in wrong if r['event']=='device')['renderer']='unobserved-device';reject('wrong_device',lambda:compare_data(wrong,frames,admitted))
    forged=copy.deepcopy(baseline);forged['results'][0]['different_RGBA_pixels']=1;reject('forged_reported_result',lambda:audit_result(forged,data,frames,admitted))
    wrong_source=dict(admitted);wrong_source['late']=dict(admitted['late'],target_main_returned=True);reject('forged_Target_main_return',lambda:compare_data(data,frames,wrong_source))
    need(all(sha(physical(n))==pin for n,pin in pins.items()),'actual Native output untouched by copied controls');recheck(admitted);return rejected

def audit_rotation(path):
    proof=json.loads((BASE/'rotation-evidence.json').read_text());need(sha(BASE/'RotationProbe.java')==proof['authored_source_sha256'],'exact authored SDK-only probe source');need(sha(path/'receipt.json')==proof['receipt_sha256'],'actual standalone SDK-only receipt');r=json.loads((path/'receipt.json').read_text());need(r['status']=='PASS'and r['inputs_unchanged']and r['input_before_sha256']==r['input_after_sha256']==proof['input_sha256']and r['artifacts_sha256']==proof['artifacts_sha256'],'complete immutable actual standalone SDK-only probe');fingerprints(proof['input_sha256'])
    for name,pin in proof['artifacts_sha256'].items():need(sha(path/name)==pin,'actual immutable SDK-only output')
    common=proof['common_25degree_prepared_word'];mesa=physical(common['actual_primary_Mesa_artifact_path']);need(sha(mesa)==common['actual_primary_Mesa_artifact_sha256']and rows(mesa)[0]['modelview'][0]==common['actual_primary_Mesa_word']=='3f6803c9','actual immutable primaryMesa common25degree word')
    data=rows(path/'observations.ndjson');need(len(data)==79 and all(r['error']==0 for r in data),'actual79SDKrows noGLerrors');need(data[0]==proof['actual_device'],'actual SDK-only NVIDIAdevice/profilemask/FB');need(next(r for r in data if r.get('name')=='composition0_rotateY')['words'][0]==proof['common_25degree_prepared_word']['actual_NVIDIA_word']=='3f6803c9','actual finite25degree prepared word');need(next(r for r in data if r.get('name')=='axis1_67.5')['words'][0]==proof['counterexample_67_5deg_cardinal']['actual_NVIDIA_word']=='3ec3ef10','actual finite NVIDIAcounterexample');return {'status':'FINITE_SDK_ROTATION_EVIDENCE_AUDIT_PASS','rows':79,'common25word':'3f6803c9','counterexample67_5word':'3ec3ef10','scope':'Independent SDK-only observations; no originalgame/liveMatrix/Nativeparity claim.'}
