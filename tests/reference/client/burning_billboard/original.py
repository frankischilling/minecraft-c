"""Audit the actual original capture and project finite BW burning billboard events.

Post-capture analysis only. Nothing in this file is a target execution input.
"""
import argparse, collections, hashlib, json, re, os, subprocess, sys, zipfile, importlib.util, struct
from pathlib import Path


def need(value, message):
    if not value:
        raise AssertionError(message)


def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            digest.update(block)
    return digest.hexdigest()


PACKET=Path(__file__).resolve().parent
ROOT=next(p for p in PACKET.parents if (p/'CMakeLists.txt').is_file())


def physical(name):
    text=str(name).replace('\\','/')
    if os.name=='nt' and re.match(r'^/mnt/[a-z]/',text):return Path(text[5].upper()+':/'+text[7:])
    if os.name!='nt' and re.match(r'^[A-Za-z]:/',text):return Path('/mnt')/text[0].lower()/text[3:]
    p=Path(text)
    return p if p.is_absolute() else ROOT/p


def fingerprints(expected):
    observed={};foreign=[]
    for name in expected:
        p=physical(name)
        if p.is_file():observed[name]=sha(p)
        elif os.name=='nt' and str(name).startswith(('/usr/','/home/','/lib/','/lib64/','/etc/')):foreign.append(name)
        else:raise AssertionError('missing current original prerequisite: '+str(name))
    if foreign:
        code="import hashlib,json,sys;from pathlib import Path\ndef digest(path):\n h=hashlib.sha256()\n with Path(path).open('rb') as f:\n  for b in iter(lambda:f.read(1048576),b''):h.update(b)\n return h.hexdigest()\nprint(json.dumps({p:digest(p) for p in json.load(sys.stdin)}))"
        r=subprocess.run(['wsl.exe','-d','Ubuntu','--exec','python3','-c',code],input=json.dumps(foreign),capture_output=True,text=True,timeout=60)
        need(r.returncode==0 and not r.stderr,'actual read-only foreign Linux tool fingerprint audit')
        observed.update(json.loads(r.stdout))
    need(observed==expected,'current original inputs/tools/helpers differ from captured expected pins')
    return {physical(n):h for n,h in expected.items() if physical(n).is_file()}


def public_inputs():
    p=json.loads((PACKET/'provenance.json').read_text(encoding='utf-8'))
    need(p['schema']==1 and p['chronology']['public_paths_were_capture_start_inputs'] is False,'explicit authored public-after-capture chronology')
    need(p['captured_authored_sha256']==p['public_authored_sha256'],'exact copied already-LF observer/preparation/profile sources')
    for name,pin in p['public_authored_sha256'].items():need(sha(PACKET/name)==pin,'current public authored observer input: '+name)
    for name,pin in p.get('post_capture_dependency_sha256',{}).items():need(sha(ROOT/name)==pin,'current post-capture comparison dependency: '+name)
    return p


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]


def native_elf(path):
    raw=path.read_bytes();need(raw[:6]==b"\x7fELF\x02\x01",'actual little-endian64 observer ELF')
    kind,machine=struct.unpack_from('<HH',raw,16);need((kind,machine)==(3,62),'actual x8664 shared observer')
    start=struct.unpack_from('<Q',raw,40)[0];stride,count,names=struct.unpack_from('<HHH',raw,58)
    need(stride==64 and 0<names<count,'complete ELF section table')
    table=[struct.unpack_from('<IIQQQQIIQQ',raw,start+i*stride)for i in range(count)]
    string=table[names];strings=raw[string[4]:string[4]+string[5]];selected={}
    for row in table:
        end=strings.index(0,row[0]);name=strings[row[0]:end].decode('ascii')
        if name in ('.text','.rodata','.data'):
            content=raw[row[4]:row[4]+row[5]];need(len(content)==row[5],'complete actual ELF section')
            selected[name]={'bytes':len(content),'sha256':hashlib.sha256(content).hexdigest()}
    need('.text'in selected and '.rodata'in selected,'actual native code and constants')
    return {'class':'ELF64LE','machine':machine,'type':kind,'sections':selected}


def fire_site(row,pcs=None):
    return next((f for f in row.get('source_frames',[]) if f['class']=='bw' and f['method']=='a' and f['descriptor']=='(Lsn;DDDF)V' and (pcs is None or f['pc'] in pcs)),None)


def project(draw,events,driver):
    i=draw['index'];ev=[r for r in events if r.get('case')==i]
    entries=[r for r in ev if r['event']=='entry' and r['location']['class']=='bw' and r['location']['method']=='a' and r['location']['descriptor']=='(Lsn;DDDF)V'];returns=[r for r in ev if r['event']=='return' and r['location']['class']=='bw' and r['location']['method']=='a' and r['location']['descriptor']=='(Lsn;DDDF)V']
    need(len(entries)==len(returns)==1 and returns[0]['location']['pc']==462,'one genuine BW fire entry/return462')
    need(entries[0]['receiver']==returns[0]['receiver'] and entries[0]['arguments']==returns[0]['arguments'],'actual fire receiver/arguments continuity')
    post=[r for r in ev if r['event']=='entry'and r['location']['class']=='bw'and r['location']['method']=='b'and r['location']['descriptor']=='(Lsn;DDDFF)V'];post_return=[r for r in ev if r['event']=='return'and r['location']['class']=='bw'and r['location']['method']=='b'and r['location']['descriptor']=='(Lsn;DDDFF)V']
    need(len(post)==len(post_return)==1 and post_return[0]['location']['pc']==100,'actual public BW post entry/return100')
    need(post[0]['receiver']==post_return[0]['receiver']==entries[0]['receiver']and post[0]['arguments']==post_return[0]['arguments'],'actual public/private receiver continuity')
    pa=post[0]['arguments'];need(pa[1:4]==draw['input']['xyz']and pa[4]==draw['input']['yaw']and pa[5]==draw['input']['partial']and entries[0]['arguments']==pa[:4]+[pa[5]],'actual public/private input operands')
    burning=[r for r in ev if r['event']=='return'and r['location']['class']=='sn'and r['location']['method']=='ak'and r['location']['descriptor']=='()Z'];need(len(burning)==1 and burning[0]['result']is True and burning[0]['receiver']==pa[0],'actual SN.ak true on selected actor')
    vertices=[r for r in ev if r['event']=='entry' and r['location']['class']=='nw' and r['location']['method']=='a' and r['location']['descriptor']=='(DDDDD)V' and fire_site(r,{332,358,385,411})]
    begin=[r for r in ev if r['event']=='entry' and r['location']['class']=='nw' and r['location']['method']=='b' and r['location']['descriptor']=='()V' and fire_site(r,{187})]
    draws=[r for r in ev if r['event']=='entry' and 'batch'in r and fire_site(r,{450})]
    slots=[r for r in ev if r['event']=='fire-slots'];need([r['location']['pc'] for r in slots].count(187)==[r['location']['pc'] for r in slots].count(450)==[r['location']['pc'] for r in slots].count(462)==1,'actual initial/draw/return slot snapshots')
    initial=next(r for r in slots if r['location']['pc']==187)['slots'];final=next(r for r in slots if r['location']['pc']==450)['slots'];layers=final['23'];need(type(layers)is int and layers>=0,'actual final integer layer local')
    need(len(vertices)==4*layers and len(begin)==len(draws)==1,'actual four calls per observed layer and one begin/draw including empty case')
    need([fire_site(r)['pc'] for r in vertices]==[332,358,385,411]*layers,'actual four-vertex call order')
    tests=[r for r in slots if r['location']['pc']==193];need(len(tests)==layers+1 and [r['slots']['23'] for r in tests]==list(range(layers+1)),'all actual loop tests')
    raw=draws[0]['batch'];need(raw['nw.h']==6*layers and raw['nw.o']==48*layers and raw['nw.p']==4*layers and raw['nw.w']is True,'genuine original NW triangulation and live writer before draw')
    batches=[]
    for r,g in driver:
        if r.get('case')==i:
            need(r['batch']['nw.w']is False,'real NWdraw closes writer before driver call')
            batches.append({'source':r,'gl':g,'fire':fire_site(r,{450})is not None})
    need(sum(r['fire']for r in batches)==int(layers>0),'empty fire draw makes no physical draw')
    calls=[r for r in ev if r['event']=='gl-entry' and fire_site(r)]
    def at(pc,name):
        a=[r for r in calls if fire_site(r,{pc})and r['method']==name];need(len(a)==1,'actual fire GL call at '+str(pc));return a[0]['arguments']
    need(at(3,'glDisable')==[2896] and at(459,'glEnable')==[2896],'actual unconditional lighting prefix/suffix')
    lookup=[r for r in ev if r['event']=='entry' and r['location']['class']=='ji' and r['location']['method']=='b' and r['location']['descriptor']=='(Ljava/lang/String;)I']
    need([r['arguments'][0]for r in lookup]==['%clamp%/misc/shadow.png','/terrain.png'],'actual ordered default-shadow/terrain lookups')
    need(draw['before']['entropy']==draw['after']['entropy'],'all five raw per-process entropy contexts preserved')
    need(draw['before']['actor']==draw['after']['actor'] and draw['before']['camera']==draw['after']['camera'],'complete observed actor/camera/bounds/watcher invariance within this VM')
    return {'case':i,'kind':draw['kind'],'input':draw['input'],'entry':entries[0],'return':returns[0],'post_entry':post[0],'post_return':post_return[0],'burning_return':burning[0],'layers':layers,'vertices':vertices,'begin':begin[0],'draw':draws[0],'slots':slots,'initial_slots':initial,'final_slots':final,'batches':batches,'gl_calls':calls,'texture_lookups':lookup,'scale_arguments':at(98,'glScalef'),'z_arguments':at(169,'glTranslatef'),'rotation_arguments':at(154,'glRotatef')}


def manifest(receipt,proof):
    need(receipt['status']=='OBSERVED_SEVEN_ORIGINAL_BURNING_BILLBOARD_PROFILES' and receipt['inputs_unchanged']is True,'completed unchanged original seven-profile dataset')
    for key in ['input_before_sha256','input_after_sha256']:need(receipt[key]==proof['input_sha256'],'complete immutable original input map')
    for key in ['helper_before_sha256','helper_after_sha256']:need(receipt[key]==proof['helper_sha256'],'complete immutable helper map')
    need(receipt['artifacts_sha256']==proof['artifacts_sha256'],'complete immutable raw output map')
    need(receipt['observer_before_sha256']==receipt['observer_after_sha256']==proof['observer_sha256'],'actual passive native observer bytes')
    need(receipt['loaded_runtime_files_sha256']==proof['loaded_runtime_files_sha256'],'actual mapped runtime closure')


def manifest_negatives(receipt,proof):
    manifest(receipt,proof)
    import copy
    changes=[]
    for name,key in [('missing_raw_original','artifacts_sha256'),('missing_helper','helper_before_sha256'),('missing_input','input_before_sha256')]:
        bad=copy.deepcopy(receipt);del bad[key][next(iter(bad[key]))];changes.append((name,bad))
    bad=copy.deepcopy(receipt);bad['observer_after_sha256']='0'*64;changes.append(('changed_driver_observer',bad))
    rejected=[]
    for name,bad in changes:
        try:manifest(bad,proof)
        except AssertionError:rejected.append(name)
        else:raise AssertionError('admitted corrupt original manifest: '+name)
    return rejected


def audit(capture):
    provenance=public_inputs();proof=provenance['primary'];receipt_path=capture/'receipt.json';need(sha(receipt_path)==proof['receipt_sha256'],'exact actual source receipt')
    receipt=json.loads(receipt_path.read_text());manifest(receipt,proof);fingerprints(proof['input_sha256'])
    for name,pin in proof['artifacts_sha256'].items():need(sha(capture/name)==pin,'immutable actual source output '+name)
    for name,pin in proof['helper_sha256'].items():need(sha(capture/'classes'/name)==pin,'actual compiled helper '+name)
    need(sha(capture/'driver_observer.so')==proof['observer_sha256'],'current actual native observer');need(native_elf(capture/'driver_observer.so')==proof['observer_ELF'],'actual compiled native observer code and constants')
    for expected in proof['loaded_runtime_files_sha256'].values():fingerprints(expected)
    need(sha(capture/'authority.json')==sha(PACKET/'source-code-pins.json')==proof['authority_sha256'],'complete source class/Code authority')
    need(sha(capture/'helper-Code.json')==sha(PACKET/'helper-code-pins.json')==proof['helper_Code_sha256'],'complete compiled helper Code authority')
    authority=json.loads((capture/'authority.json').read_text());need(len(authority['classes'])==747,'all original class identities')
    spec=importlib.util.spec_from_file_location('burning_actual_classfile',ROOT/'tests/reference/client/controller/classfile.py');classfile=importlib.util.module_from_spec(spec);spec.loader.exec_module(classfile)
    jar=next(physical(n)for n in proof['input_sha256']if n.endswith('/b1.7.3.jar'));need(sha(jar)==authority['jar_sha256'],'exact unchanged original archive')
    with zipfile.ZipFile(jar)as z:actual={n[:-6]:classfile.inspect_class(z.read(n))for n in z.namelist()if n.endswith('.class')}
    need(actual==authority['classes'],'all current original whole classes and Code methods')
    need({p.name:classfile.inspect_class(p.read_bytes())for p in(capture/'classes').glob('*.class')}==json.loads((PACKET/'helper-code-pins.json').read_text()),'all current helper Code identities')
    modes={}
    for mode in ('normal','debugger','nohook'):
        directory=capture/mode;stdout=(directory/('target.stdout'if mode=='debugger'else'process.stdout')).read_text();bindings={}
        for line in stdout.splitlines():
            if line.startswith('BIND '):
                _,name,origin,size,pin=line.split();need(name not in bindings,'unique binding');a=authority['classes'][name];need(origin=='file:/mnt/c/Users/admin/minecraft/b1.7.3.jar'and int(size)==a['class_bytes']and pin==a['class_sha256'],'actual unchanged runtime source origin');bindings[name]=pin
        need(set(bindings)==set(authority['classes']),'all747 actual runtime bindings')
        data=rows(directory/'target.ndjson');draws=[r for r in data if r['event']=='draw'];need([r['index']for r in draws]==list(range(7)),'all7 ordered genuine calls')
        for d in draws:
            need(d['outcome']==1 and d['failure']is None and d['gl_error']==0,'genuine complete original return')
            need(d['before']['entropy']==d['after']['entropy'],'same-VM raw entropy invariance')
            need(d['before']['actor']==d['after']['actor']and d['before']['camera']==d['after']['camera'],'full same-VM actor/camera invariance')
        initialized=sorted({n for n in re.findall(r"Initializing '([^']+)'",(directory/'class-init.log').read_text())if n in authority['classes']})
        need(initialized==proof['initialized_original_classes'][mode],'exact actual initialized original inventory')
        modes[mode]={'draws':draws,'all_rows':data,'device':next(r for r in data if r['event']=='device'),'initialized_original_classes':initialized}
    events=rows(capture/'debugger/checkpoints.ndjson');need(len(events)==proof['JDI_events'],'complete actual read-only event inventory')
    pins=[r for r in events if r['event']=='code-pin'];need(len(pins)==proof['live_Code_methods'],'complete live Code pins')
    for r in pins:
        method=[m for m in authority['classes'][r['class']]['methods']if m['name']==r['method']and m['descriptor']==r['descriptor']];need(len(method)==1 and method[0]['code_bytes']==r['bytes']and method[0]['code_sha256']==r['sha256'],'actual unchanged reached Code identity')
    driver_events=[r for r in events if r['event']=='driver-draw-entry'];driver=rows(capture/'debugger/driver-gl.ndjson');need(len(driver)==len(driver_events)==13,'all actual driver calls')
    for r,g in zip(driver_events,driver):need(r['method']==g['method']=='glDrawArrays'and r['arguments']==[g['mode'],g['first'],g['count']],'read-only event/actual-driver ordinal coupling')
    need(rows(capture/'normal/driver-gl.ndjson')==driver,'ordinary/observed actual driver state agrees exactly for this finite dataset')
    # A separate original process uses identical Target bytes without LD_PRELOAD.
    # Frozen capture driver removes both observer variables before its launch.
    for mode in ('normal','nohook'):
        for i,d in enumerate(modes[mode]['draws']):
            ref=modes['debugger']['draws'][i];need(d['input']['width']==ref['input']['width']and d['input']['height']==ref['input']['height'],'same declared finite dimensions')
            for phase in ('baseline_pixels','pixels'):need(d[phase]['sha256']==ref[phase]['sha256'],'hooked/JDI/unhooked finite Source backbuffer identity')
    cases=[project(d,events,list(zip(driver_events,driver)))for d in modes['debugger']['draws']]
    need([r['layers']for r in cases]==[2,2,2,5,0,3,4],'actual observed finite layer counts')
    return receipt,modes,events,{'status':'ACTUAL_ORIGINAL_BURNING_BILLBOARD_AUDIT_PASS','capture_receipt_sha256':sha(receipt_path),'actual_original_classes':747,'live_Code_methods':len(pins),'JDI_events':len(events),'initialized_original_counts':{k:len(v['initialized_original_classes'])for k,v in modes.items()},'cases':cases,'scope':'Seven exact post-only calls; raw constructors/entropy retained, actual slots/NW/GL/order/state. No complete constructor-method trace, powered branch, Native or general pixel parity.'}
