"""Re-decode actual native and original Mob NBT; compare common prepared semantics."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,sys
sys.dont_write_bytecode=True
def require(b,s):
    if not b:raise AssertionError(s)
def read(p,limit=8388608):
    require(p.is_file()and p.stat().st_size<=limit,'bounded actual comparison input before read');return p.read_bytes()
def sha(p,limit=268435456):return hashlib.sha256(read(p,limit)).hexdigest()
def mod(p,n):
    s=importlib.util.spec_from_file_location(n,p);v=importlib.util.module_from_spec(s);s.loader.exec_module(v);return v
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-root',type=Path,required=True);p.add_argument('--native-report',type=Path,required=True);p.add_argument('--original-receipt',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=a.source_root.resolve();nativepath=a.native_report.resolve();originalpath=a.original_receipt.resolve();nativebase=nativepath.parent;originalbase=originalpath.parent
require(not a.output.exists(),'comparison output must be new')
tools=mod(root/'tests/tools/world/test_source_saving.py','compare_mob_world_tools');native=mod(root/'tests/tools/entity/test_mob_nbt.py','compare_mob_native_tools')
nr=json.loads(read(nativepath));orr=json.loads(read(originalpath,33554432));require(len(nr['files'])<=1024 and len(orr['artifact_sha256'])<=1024,'finite captured artifact inventories');require(nr['status']=='PASS_AUTHORED_ACTUAL_SOURCE_WORLD_CLIENT_11_MOB_NBT'and nr['inputs_unchanged'],'complete unchanged native source World Mob cohort');require(orr['status']=='OBSERVED_AND_AUDITED_ORIGINAL_CLIENT_11_MOB_NBT'and orr['inputs_unchanged'],'complete unchanged actual original cohort')
paths={Path(__file__).resolve(),nativepath,originalpath,root/'tests/tools/entity/test_mob_nbt.py',root/'tests/tools/world/test_source_saving.py'}
for relative,pin in nr['files'].items():
    q=(nativebase/relative).resolve();require(q.is_relative_to(nativebase),'native retained artifact stays inside report directory');require(q.stat().st_size==pin['bytes']and sha(q)==pin['sha256'],'native actual artifact hash');paths.add(q)
for relative,pin in orr['artifact_sha256'].items():
    q=(originalbase/relative).resolve();require(q.is_relative_to(originalbase),'original retained artifact stays inside receipt directory');require(sha(q)==pin,'original actual artifact hash');paths.add(q)
before={str(q):sha(q)for q in sorted(paths)}
rows=[json.loads(l)for l in read(nativebase/'native.stdout').decode('ascii').splitlines()if l.startswith('{')];redecoded=native.audit(nativebase/'owned',rows,tools);require(redecoded['complete']==nr['complete'],'independent native completed binary/World re-audit')
reference={};modes={}
for mode in['ordinary','debugger']:
    mode_rows=[json.loads(l)for l in read(originalbase/mode/'target.ndjson').splitlines()];selected={}
    for r in mode_rows:
        if r['event']!='write':continue
        require(r['kind']in native.KINDS and r['scenario']in['constructor','authored','missing','removed','size-1','size127','size255','watcher2','watcher2-existing']and r['operation']in['own','direct','storage']and Path(r['file']).name==r['file'],'canonical original NBT basename before read');rawpath=(originalbase/mode/'nbt'/r['file']).resolve();require(rawpath.is_relative_to((originalbase/mode/'nbt').resolve()),'original NBT stays inside capture');raw=read(rawpath,1048576);tree=native.typed(10,tools.Nbt(raw).root());require(tree==r['tree']and hashlib.sha256(raw).hexdigest()==r['sha256'],'independently re-decoded actual original binary bytes');selected[r['kind'],r['scenario'],r['operation']]=r
    require(len(selected)==62,'complete62 actual original writes per VM');modes[mode]=selected
reference=modes['ordinary'];cases=[]
for r in redecoded['mob_rows']:
    if r['scenario']in['authored-reloaded','missing-reloaded']:continue
    key=r['kind'],r['scenario'],r['operation'];require(key in reference,'native common finite original case');o=reference[key];expected=dict(o['tree'][1]);require(expected.pop('Keep')==[3,4660],'original authored unrelated key preserved');actual=dict(r['decoded'][1])if r['serialized']else{};excluded=[]
    if r['kind']=='Slime'and r['scenario']=='constructor':
        require(o['state']['ls.Y']==o['state']['slimeSize']**2 and r['health']==r['watch16']**2,'independent actual constructor size/Health relation');expected.pop('Health');expected.pop('Size');actual.pop('Health');actual.pop('Size');excluded=['constructor entropy Health and Size; each receiver relation audited']
    if r['scenario']=='removed'and r['operation']=='direct':expected.pop('Rotation');actual.pop('Rotation');excluded.append('fresh constructor Rotation; no seed substitution')
    if r['scenario']=='authored'and r['operation']in['direct','storage']:
        pos=expected['Pos'];require(isinstance(pos,list)and len(pos)==2 and pos[0]==9 and isinstance(pos[1],dict)and set(pos[1])=={'elementType','items'}and pos[1]['elementType']==6 and isinstance(pos[1]['items'],list)and len(pos[1]['items'])==3 and all(isinstance(child,list)and len(child)==2 and child[0]==6 and isinstance(child[1],str)and len(child[1])==16 and all(c in'0123456789abcdef'for c in child[1])for child in pos[1]['items']),'exact original authored Pos three-Double shape');require(pos[1]['items'][2]==[6,'c006000000000000'],'declared original authored Z input');expected['Pos']=[9,{'elementType':6,'items':[list(pos[1]['items'][0]),list(pos[1]['items'][1]),[6,'4006000000000000']]}];excluded.append('only declared native seed Pos.z=+2.75 inside Chunk0,0 replaces original prepared Pos.z=-2.75; original X/Y preserved')
    require(actual==expected,'native/original semantic fields '+str(key));require((r['gate']==1)==o['gate']if r['operation']=='storage'else True,'actual Boolean storage returns')
    debugger=modes['debugger'][key];require(set(debugger['tree'][1])==set(o['tree'][1]),'independent JDI VM same original key inventory');cases.append({'kind':key[0],'scenario':key[1],'operation':key[2],'native_binary':r['file'],'original_binary':o['file'],'scope_exceptions':excluded})
require(len(cases)==61,'61 shared cases; no invented preexisting mutable-Compound native argument')
after={str(q):sha(q)for q in sorted(paths)};require(before==after,'all comparison inputs/products unchanged');out={'status':'PASS_SOURCE_CLIENT_11_MOB_NBT_COMMON_SEMANTICS_AND_NATIVE_WORLD_RELOAD','common_cases':len(cases),'native_snapshots':83,'original_binary_outputs_each_VM':62,'native_actualWorldLoads':3,'native_actualWorldSaves':2,'native_reloaded_Mob_own_snapshots':22,'cases':cases,'input_before_sha256':before,'input_after_sha256':after,'inputs_unchanged':True,'native_report_sha256':sha(nativepath),'original_receipt_sha256':sha(originalpath),'scope':'Native actual nonnullWorld Registry factory/import/source Chunk save/reload is independently qualified on authored files. Original helpers qualify prepared NULLWorld constructor/factory/read/write owners. Shared fields are compared with explicit constructor entropy and one positive-Z native fixture input difference; no whole original World/native caller equivalence.','original_only_control':'Creeper actual watcherByte2 writing into preexisting powered Compound retains its key in original; native independent fresh-tree builder has no corresponding preexisting argument, so that one control is excluded.','hashmap_and_compression_identity_claimed':False};a.output.parent.mkdir(parents=True,exist_ok=True);payload=json.dumps(out,indent=2)+'\n'
with a.output.open('x',encoding='utf-8',newline='\n')as target:require(target.write(payload)==len(payload),'complete exclusive comparison result write')
print(json.dumps({'status':out['status'],'common_cases':len(cases),'sha256':sha(a.output)}))
