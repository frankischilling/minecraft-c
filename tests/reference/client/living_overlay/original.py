"""Audit the actual original capture and project finite LivingEntity overlay events.

Post-capture analysis only. Nothing in this file is a target execution input.
"""
import argparse, collections, hashlib, json, re, os, subprocess, sys, zipfile, importlib.util
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
        elif os.name=='nt' and str(name).startswith(('/usr/','/home/')):foreign.append(name)
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
    return p


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]


def gv(frame, pcs=None):
    return frame['class']=='gv' and frame['method']=='a' and frame['descriptor']=='(Lls;DDDFF)V' and (pcs is None or frame['pc'] in pcs)


def caller(row, pcs):
    frames=row.get('source_frames',[])
    return next((f for f in frames if gv(f,pcs)),None)


def project(draw, events):
    index=draw['index'];selected=[r for r in events if r.get('case')==index]
    gl=draw['before']['gl'];depth=gl['depth_function'];color=gl['color'][:]
    caps={r['cap']:r['enabled'] for r in gl['caps']};calls=[];bpred=[];model=[];glcalls=[];pending_b=collections.defaultdict(list)
    for r in selected:
        if r['event']=='gl-entry':
            name=r['method'];args=r['arguments'];site=caller(r,set(range(400,738)))
            if site:glcalls.append({'sequence':r['sequence'],'site':site,'method':name,'descriptor':r['descriptor'],'arguments':args})
            if name=='glDepthFunc':depth=args[0]
            elif name=='glColor4f':color=[v['F'] for v in args]
            elif name=='glColor3f':color=[v['F'] for v in args]+['3f800000']
            elif name in ('glEnable','glDisable'):caps[args[0]]=name=='glEnable'
            elif name=='glCallList' and depth==514:
                site=caller(r,{488,540,648,702});need(site is not None,'every EQUAL list belongs to an actual overlay model call')
                calls.append({'sequence':r['sequence'],'list':args[0],'rgba':color[:],'depth':depth,'texture':caps[3553],'alpha':caps[3008],'blend':caps[3042],'model_caller':site})
        elif r['event']=='entry':
            loc=r['location'];frames=r.get('source_frames',[])
            if loc['method']=='b' and loc['descriptor']=='(Lls;IF)Z' and loc['class'] in ('gv','m') and len(frames)>=2 and gv(frames[1],{506,666}):
                pending_b[loc['class']].append(r)
            if loc['method']=='a' and loc['descriptor']=='(FFFFFF)V' and len(frames)>=2 and gv(frames[1],{488,540,648,702}):
                model.append({'sequence':r['sequence'],'location':loc,'receiver':r['receiver'],'arguments':r['arguments'],'caller':frames[1]})
        elif r['event']=='return':
            loc=r['location'];frames=r.get('source_frames',[])
            if loc['method']=='b' and loc['descriptor']=='(Lls;IF)Z' and loc['class'] in ('gv','m'):
                need(pending_b[loc['class']], 'actual b return has its observed direct entry')
                entry=pending_b[loc['class']].pop()
                need(entry['receiver']==r['receiver'] and entry['arguments']==r['arguments'], 'actual b receiver/arguments remain paired across entry and return')
                bpred.append({'sequence':r['sequence'],'entry_sequence':entry['sequence'],'location':loc,'receiver':r['receiver'],'arguments':r['arguments'],'result':r['result'],'caller':entry['source_frames'][1]})
    need(all(not v for v in pending_b.values()), 'no unreturned actual overlay b invocation')
    overlays={}
    for label,primary_pc,secondary_pc,predicate_pc in [('hurt',488,540,506),('packed',648,702,666)]:
        primary=[r for r in model if r['caller']['pc']==primary_pc];secondary=[r for r in model if r['caller']['pc']==secondary_pc]
        predicates=[r for r in bpred if r['caller']['pc']==predicate_pc]
        relevant=[r for r in calls if r['model_caller']['pc'] in (primary_pc,secondary_pc)]
        need(len(primary) in (0,1),'one actual primary model invocation per overlay')
        need(len(predicates)==(4 if primary else 0),'actual four predicate returns only after a real primary invocation')
        need(len(secondary)==sum(bool(r['result']) for r in predicates),'each true actual predicate enters one secondary model')
        overlays[label]={'entered':bool(primary),'primary_calls':primary,'secondary_calls':secondary,'predicates':predicates,'source_list_calls':relevant,'rgba':next((r['arguments'] for r in glcalls if r['site']['pc']==(466 if label=='hurt' else 626)),None)}
    actor=draw['before']['actor']['fields']
    need(draw['before']['entropy']==draw['after']['entropy'],'actual per-process draw leaves all five raw entropy contexts unchanged')
    need(draw['before']['actor']==draw['after']['actor'],'actual draw preserves full actor/bounds/watcher state')
    color_hooks=[r for r in selected if r['event']=='return' and r['location']['method']=='a' and r['location']['descriptor']=='(Lls;FF)I' and r['location']['class'] in ('gv','m')]
    need(len(color_hooks)==1 and type(color_hooks[0]['result']) is int,'one actual Living color virtual result')
    bridges=[r for r in selected if r['event']=='draw-entry']
    need(len(bridges)==1,'one actual observed source draw-entry identity bridge')
    return {'case':index,'kind':draw['kind'],'input':draw['input'],'outcome':draw['outcome'],'gl_error':draw['gl_error'],'color_hook_result':color_hooks[0]['result'],'entry_identity_bridge':bridges[0]['identity_bridge'],'selected_fields':{k:actor[k] for k in ('ls.aa','ls.ad')} | ({k:actor[k] for k in ('gb.a','gb.b')} if draw['kind']=='gb' else {}),'overlays':overlays,'gl_calls':glcalls,'source_list_calls':calls,'final_gl':draw['after']['gl'],'streams_preserved':True}


def manifest(receipt,proof):
    need(receipt.get('status')==proof['status'] and receipt.get('inputs_unchanged') is True,'actual completed primary capture status')
    need(receipt.get('input_before_sha256')==receipt.get('input_after_sha256')==proof['input_sha256'],'complete immutable captured input inventory')
    need(receipt.get('helper_before_sha256')==receipt.get('helper_after_sha256')==proof['helper_sha256'],'complete immutable helper inventory')
    need(receipt.get('artifacts_sha256')==proof['artifacts_sha256'],'complete immutable consumed output inventory')


def manifest_negatives(receipt,proof):
    import copy
    controls=[]
    changes=[]
    bad=copy.deepcopy(receipt);del bad['artifacts_sha256']['debugger/checkpoints.ndjson'];changes.append(('missing_actual_JDI_artifact',bad))
    bad=copy.deepcopy(receipt);bad['artifacts_sha256']['normal/target.ndjson']='0'*64;changes.append(('changed_actual_original_observation',bad))
    bad=copy.deepcopy(receipt);del bad['input_before_sha256'][next(iter(bad['input_before_sha256']))];changes.append(('missing_captured_input',bad))
    bad=copy.deepcopy(receipt);bad['helper_before_sha256'][next(iter(bad['helper_before_sha256']))]='0'*64;changes.append(('changed_actual_helper_bytes',bad))
    for label,bad in changes:
        try:manifest(bad,proof)
        except AssertionError:controls.append(label)
        else:raise AssertionError('original manifest admitted corruption: '+label)
    return controls


def audit(capture):
    provenance=public_inputs();proof=provenance['primary'];receipt_path=capture/'receipt.json'
    need(sha(receipt_path)==proof['receipt_sha256'],'exact immutable actual primary original receipt')
    receipt=json.loads(receipt_path.read_text());manifest(receipt,proof);need(receipt['status']=='OBSERVED_SEVEN_ORIGINAL_OVERLAY_PROFILES','completed exact seven-case Original capture')
    need(receipt['input_before_sha256']==proof['input_sha256'] and receipt['helper_before_sha256']==proof['helper_sha256'] and receipt['artifacts_sha256']==proof['artifacts_sha256'],'complete exact captured input/helper/artifact inventories; no omitted consumed output')
    need(receipt['inputs_unchanged'] and receipt['input_before_sha256']==receipt['input_after_sha256'] and receipt['helper_before_sha256']==receipt['helper_after_sha256'],'actual compile/execution input and helper stability')
    fingerprints(proof['input_sha256'])
    for name,pin in receipt['artifacts_sha256'].items():need(sha(capture/name)==pin,'immutable actual output: '+name)
    for name,pin in receipt['helper_before_sha256'].items():need(sha(capture/'classes'/name)==pin,'actual compiled helper bytes: '+name)
    need(sha(capture/'authority.json')==proof['authority_sha256']==sha(PACKET/'source-code-pins.json'),'exact supplied original class/Code authority bytes')
    need(sha(capture/'helper-Code.json')==proof['helper_Code_sha256']==sha(PACKET/'helper-code-pins.json'),'actual complete compiled helper Code metadata')
    authority=json.loads((capture/'authority.json').read_text());need(len(authority['classes'])==747,'all original class identities')
    spec=importlib.util.spec_from_file_location('overlay_actual_classfile',ROOT/'tests/reference/client/controller/classfile.py');classfile=importlib.util.module_from_spec(spec);spec.loader.exec_module(classfile)
    jars=[physical(n) for n in proof['input_sha256'] if str(n).endswith('/b1.7.3.jar')];need(len(jars)==1 and sha(jars[0])==authority['jar_sha256'],'exact unchanged original JAR')
    with zipfile.ZipFile(jars[0]) as z:actual={n[:-6]:classfile.inspect_class(z.read(n)) for n in z.namelist() if n.endswith('.class')}
    need(actual==authority['classes'],'all actual original whole classes and Code-bearing methods')
    helpers={p.name:classfile.inspect_class(p.read_bytes()) for p in (capture/'classes').glob('*.class')}
    need(helpers==json.loads((PACKET/'helper-code-pins.json').read_text()),'all actual compiled helper class and Code identities')
    bymode={}
    for mode in ('normal','debugger'):
        directory=capture/mode;stdout=(directory/('process.stdout' if mode=='normal' else 'target.stdout')).read_text()
        binds={}
        for l in stdout.splitlines():
            if l.startswith('BIND '):
                _,name,origin,size,pin=l.split();need(name not in binds,'unique actual runtime original class binding');binds[name]={'origin':origin,'bytes':int(size),'sha256':pin}
        need(set(binds)==set(authority['classes']),'complete 747 runtime original class origins')
        for name,bound in binds.items():
            a=authority['classes'][name];need(bound['origin']=='file:/mnt/c/Users/admin/minecraft/b1.7.3.jar' and bound['bytes']==a['class_bytes'] and bound['sha256']==a['class_sha256'],'exact original runtime class origin/bytes')
        observed=rows(directory/'target.ndjson');draws=[r for r in observed if r['event']=='draw'];need([r['index'] for r in draws]==list(range(7)),'all seven actual ordered calls')
        need(all(r['outcome']==1 and r['failure'] is None and r['gl_error']==0 for r in draws),'actual complete original returns without GL errors')
        for draw in draws:need(draw['before']['entropy']==draw['after']['entropy'],'same-VM raw streams remain unchanged')
        initialized=sorted({n for n in re.findall(r"Initializing '([^']+)'",(directory/'class-init.log').read_text()) if n in authority['classes']})
        bymode[mode]={'draws':draws,'bindings':binds,'initialized_original_classes':initialized,'device':[r for r in observed if r['event']=='device']}
    events=rows(capture/'debugger/checkpoints.ndjson');pins=[r for r in events if r['event']=='code-pin']
    for r in pins:
        name=r['class'].replace('.','/');m=[m for m in authority['classes'][name]['methods'] if m['name']==r['method'] and m['descriptor']==r['descriptor']]
        need(len(m)==1 and m[0]['code_bytes']==r['bytes'] and m[0]['code_sha256']==r['sha256'],'actual live unchanged original Code identity')
    need(len(pins)==proof['live_Code_methods'] and len(events)==proof['JDI_events'],'complete raw live Code/event inventories')
    need({k:len(v['initialized_original_classes']) for k,v in bymode.items()}==proof['initialized_original_counts'],'exact captured initialized-original inventories')
    summaries=[project(draw,events) for draw in bymode['debugger']['draws']]
    fingerprints(proof['input_sha256'])
    for name,pin in proof['artifacts_sha256'].items():need(sha(capture/name)==pin,'final immutable original artifact boundary: '+name)
    return receipt,bymode,events,{'status':'ORIGINAL_OVERLAY_SELECTED_PROJECTION_PASS','scope':'Post-capture current input/output/code audit and finite actual overlay event projection; no Native comparison. Whole actor/state retained, no entropy normalization.','capture_receipt_sha256':sha(receipt_path),'authority_sha256':sha(capture/'authority.json'),'actual_original_classes':747,'live_Code_methods':len(pins),'JDI_events':len(events),'initialized_original_counts':{k:len(v['initialized_original_classes']) for k,v in bymode.items()},'cases':summaries}


def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    receipt,modes,events,summary=audit(a.capture)
    a.output.write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({k:summary[k] for k in ('status','actual_original_classes','live_Code_methods','JDI_events','initialized_original_counts')}))
    for r in summary['cases']:
        print(r['case'],r['kind'],{k:(v['entered'],len(v['primary_calls']),len(v['secondary_calls']),len(v['source_list_calls']),[p['result'] for p in v['predicates']]) for k,v in r['overlays'].items()})

if __name__=='__main__':main()
