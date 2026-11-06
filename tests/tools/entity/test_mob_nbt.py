"""Actual source World11-Mob NBT load/save/reload on authored disposable records."""
import argparse,gzip,hashlib,importlib.util,json,os,struct,sys,tempfile,zlib
from pathlib import Path
sys.dont_write_bytecode=True
BOUND=1048576
KINDS=['Spider','Zombie','Skeleton','Creeper','Slime','Pig','Sheep','Cow','Chicken','Wolf','Squid']
LIVING={'Health','HurtTime','DeathTime','AttackTime'}
EXTRA={'Spider':set(),'Zombie':set(),'Skeleton':set(),'Creeper':{'powered'},'Slime':{'Size'},'Pig':{'Saddle'},'Sheep':{'Sheared','Color'},'Cow':set(),'Chicken':set(),'Wolf':{'Angry','Sitting','Owner'},'Squid':set()}
OWNER=[79,119,110,101,114,0,937,55357,56378]
def require(b,s):
    if not b:raise AssertionError(s)
def bounded(path,limit=BOUND):
    require(path.is_file()and path.stat().st_size<=limit,'bounded actual input before read: '+str(path));return path.read_bytes()
def fingerprint(path,limit=BOUND):
    raw=bounded(path,limit);return{'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
def module(path,name):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def units(text):return list(map(ord,text))
def mutf(value):
    result=bytearray()
    for x in value:
        if 0<x<128:result.append(x)
        elif x<2048:result.extend([192|(x>>6),128|(x&63)])
        else:result.extend([224|(x>>12),128|((x>>6)&63),128|(x&63)])
    return struct.pack('>H',len(result))+result
def string(text):return mutf(units(text))
def named(t,name,body):return bytes([t])+string(name)+body
def compound(fields):return b''.join(fields)+b'\0'
def authored(kind):
    fields=[named(8,'id',string(kind)),named(9,'Pos',b'\6'+struct.pack('>i3d',3,1.25,65.5,2.75)),named(9,'Motion',b'\6'+struct.pack('>i3d',3,.125,-.25,.5)),named(9,'Rotation',b'\5'+struct.pack('>i2f',2,35.,-10.)),named(5,'FallDistance',struct.pack('>f',1.5)),named(2,'Fire',struct.pack('>h',-7)),named(2,'Air',struct.pack('>h',250)),named(1,'OnGround',b'\xff')]
    fields +=[named(2,key,struct.pack('>h',value))for key,value in[('Health',-123),('HurtTime',-32768),('DeathTime',32767),('AttackTime',-1)]]
    fields +=[named(1,key,b'\xff')for key in['Saddle','Sheared','Color','powered','Angry']]
    fields +=[named(1,'Sitting',b'\1'),named(8,'Owner',mutf(OWNER)),named(3,'Size',struct.pack('>i',3)),named(3,'Age',struct.pack('>i',999)),named(3,'EggLayTime',struct.pack('>i',1)),named(2,'Fuse',struct.pack('>h',1)),named(1,'ExplosionRadius',b'*'),named(3,'Unknown',struct.pack('>i',42))]
    return compound(fields)
def seed(source_tools):
    info,_=source_tools.seed('empty');fields=[named(3,'xPos',bytes(4)),named(3,'zPos',bytes(4)),named(4,'LastUpdate',struct.pack('>q',6000))]
    for key,count in[('Blocks',32768),('Data',16384),('SkyLight',16384),('BlockLight',16384),('HeightMap',256)]:fields.append(named(7,key,struct.pack('>i',count)+bytes([64 if key=='HeightMap'else 0])*count))
    fields +=[named(1,'TerrainPopulated',b'\1'),named(9,'Entities',b'\12'+struct.pack('>i',11)+b''.join(authored(k)for k in KINDS)),named(9,'TileEntities',b'\12'+bytes(4)),named(4,'LevelExtra',struct.pack('>q',902))]
    raw=named(10,'',compound([named(4,'RootExtra',struct.pack('>q',901)),named(10,'Level',compound(fields))]));encoded=zlib.compress(raw);sectors=(len(encoded)+5+4095)//4096;require(sectors<256,'bounded authored seed sectors');image=bytearray((2+sectors)*4096);struct.pack_into('>I',image,0,(2<<8)|sectors);struct.pack_into('>i',image,4096,123);record=struct.pack('>i',len(encoded)+1)+b'\2'+encoded;image[8192:8192+len(record)]=record;return info,bytes(image),raw
def typed(kind,value):
    if kind==10:return[10,{key:typed(child[0],child[1])for key,child in value.items()}]
    if kind==9:return[9,{'elementType':value[0],'items':[typed(value[0],x)for x in value[1]]}]
    if kind==8:return[8,units(value)]
    if kind in[5,6]:return[kind,format(value,'08x'if kind==5 else'016x')]
    if kind==7:return[7,{'bytes':len(value),'sha256':hashlib.sha256(value).hexdigest()}]
    return[kind,value]
def audit(owned,rows,source_tools):
    require(rows[-1]['event']=='complete'and rows[-1]['kinds']==11 and rows[-1]['writes']==83 and rows[-1]['actualWorldLoads']==3 and rows[-1]['actualWorldSaves']==2 and rows[-1]['cleanup']==1,'actual finite83snapshot/3Worldload/2save completion')
    mob=[r for r in rows if r['event']=='mob'];require(len(mob)==83 and[r['sequence']for r in mob]==list(range(83)),'exact snapshot inventory');index={};decoded={}
    for r in mob:
        key=(r['kind'],r['scenario'],r['operation']);require(key not in index and r['nonnullWorld']==1,'unique actual nonnullWorld snapshot');require(r['kind']in KINDS and r['scenario']in['constructor','authored','authored-reloaded','missing','missing-reloaded','removed','size-1','size127','size255','watcher2']and r['operation']in['own','direct','storage'],'bounded canonical native snapshot labels');index[key]=r
        if r['serialized']:
            require(Path(r['file']).name==r['file']and r['file']==str(r['sequence']).zfill(3)+'-'+r['kind']+'-'+r['scenario']+'-'+r['operation']+'.nbt','canonical actual native NBT basename before read');path=owned/'nbt'/r['file'];require(path.resolve().is_relative_to((owned/'nbt').resolve()),'native NBT stays inside owned directory');raw=bounded(path);require(len(raw)==r['bytes'],'actual emitted native NBT length');tree=typed(10,source_tools.Nbt(raw).root());decoded[r['file']]=tree;r['decoded']=tree;r['fingerprint']=fingerprint(path)
            require(tree[1]['Health']==[2,((r['health']+32768)%65536)-32768],'native actual Health short narrowing')
        else:require(key==('Pig','removed','storage')and r['gate']==0 and r['file']==''and r['bytes']==0,'actual false gate leaves independent output absent')
    require(len(list((owned/'nbt').glob('*.nbt')))==82,'exact native binary NBT inventory')
    for kind in KINDS:
        auth=index[kind,'authored','own'];missing=index[kind,'missing','own'];require(auth['source']==missing['source']==1,'actual loaded provenance owner remains distinct from semantic writer');require(auth['decoded'][1]['Health']==[2,16 if kind=='Slime'else -123],'actual authored Living and later Slime reader')
        require(set(auth['decoded'][1])==LIVING|EXTRA[kind]and set(missing['decoded'][1])==LIVING|(EXTRA[kind]-({'powered'}if kind=='Creeper'else set())),'independently expected exact own-key inventories')
        require(auth['decoded'][1]['HurtTime']==[2,-32768]and auth['decoded'][1]['DeathTime']==[2,32767]and auth['decoded'][1]['AttackTime']==[2,-1],'actual authored signed Living Short values')
        require(missing['decoded'][1]['Health']==[2,1 if kind=='Slime'else 10]and all(missing['decoded'][1][key]==[2,0]for key in LIVING-{'Health'}),'actual missing default output')
        require(index[kind,'authored-reloaded','own']['decoded']==auth['decoded']and index[kind,'missing-reloaded','own']['decoded']==missing['decoded'],'actual newWorld factory/import round trips both source save records')
        for operation in['direct','storage']:
            r=index[kind,'authored',operation];require(all(r['decoded'][1][key]==auth['decoded'][1][key]for key in LIVING|EXTRA[kind]),'full actual virtual own semantics');require('id'in r['decoded'][1]if operation=='storage'else'id'not in r['decoded'][1],'direct/storage namespace distinction')
        if kind=='Slime':
            ctor=index[kind,'constructor','own'];require(ctor['health']==ctor['watch16']**2 and ctor['decoded'][1]['Size']==[3,ctor['watch16']-1],'independent constructor entropy relation')
        if kind=='Wolf':require(auth['watch16']==7 and missing['watch16']==4 and auth['decoded'][1]['Owner']==missing['decoded'][1]['Owner']==[8,OWNER],'real owner UTF16 and retain-owner/tame semantics')
        if kind=='Creeper':require(auth['decoded'][1]['powered']==[1,1]and auth['watch17']==1 and missing['watch17']==0,'Creeper nonzero Byte reader normalizes and later false omits')
        if kind=='Pig':require(auth['decoded'][1]['Saddle']==[1,1]and missing['decoded'][1]['Saddle']==[1,0],'Pig Byte Boolean normalization/default')
        if kind=='Sheep':require(auth['decoded'][1]['Sheared']==[1,1]and auth['decoded'][1]['Color']==[1,15]and missing['decoded'][1]['Sheared']==[1,0]and missing['decoded'][1]['Color']==[1,0],'Sheep actual high-bit preservation/Color low-nibble output')
        if kind=='Chicken':require(6000<=auth['chickenEggTimer']<12000 and 6000<=missing['chickenEggTimer']<12000,'each actual constructor timer bounds; no equality across reload/new constructors')
    wrong=[r for r in rows if r['event']=='wrong-tag'];require(len(wrong)==1 and wrong[0]['living_pc']==4 and wrong[0]['health']==10 and wrong[0]['source_exception']==1 and wrong[0]['exception']=='java/lang/ClassCastException','wrong Health type fails at exact actual source hook before store')
    for label,health,watch,stored in[('size-1',0,0,-1),('size127',16384,-128,-129),('size255',65536,0,-1)]:
        r=index['Slime',label,'own'];require(r['health']==health and r['watch16']==watch and r['decoded'][1]['Size']==[3,stored],'full-int Slime resize then signed byte writer')
    require('powered'not in index['Creeper','watcher2','own']['decoded'][1],'actual production watcher setter Byte2 conditional omission')
    states={r['phase']:r for r in rows if r['event']=='state'};require(set(states)=={'loaded','authored-saved','authored-reloaded','missing-saved','missing-reloaded'},'five actual source World states');worlds={}
    for phase in['authored-saved','missing-saved']:
        row=states[phase];expected_time=20000 if phase=='authored-saved'else 20001;require(row['time']==row['last_save']==expected_time,'actual logical World Time/save eligibility advances through Info setter');raw,stamp,extent=source_tools.region(owned/'snapshots'/(phase+'-region.mcr'));require(set(raw)=={'Level'},'source save fresh root has no native preservation overlay');level=raw['Level'][1];require(level['LastUpdate']==[4,expected_time],'actual source Chunk LastUpdate records reached WorldTime');records=level['Entities'][1][1];require(len(records)==11 and[record['id'][1]for record in records]==KINDS,'actual source Chunk emitsall11 occurrences in their same-slice source List order');entities={}
        for record in records:
            kind=record['id'][1];require(kind in KINDS and kind not in entities,'unique real named Mob in source record');tree=typed(10,record);fields=dict(tree[1]);fields.pop('id');own=index[kind,'authored'if phase=='authored-saved'else'missing','own']['decoded'][1];require(all(fields[k]==v for k,v in own.items()),'actual source World record own fields match held Mob');require('Unknown'not in fields and 'Age'not in fields and 'EggLayTime'not in fields and 'Fuse'not in fields and 'ExplosionRadius'not in fields,'semantic source save drops retained unknown fields');entities[kind]=tree
        require(row['first']//1000<=stamp<=row['last']//1000 and all(row[k]for k in['root','level','attached','prefix','entities','tiles','written','closed','returned','has_output','output_closed','commit','commit_returned'])and row['source_closes']==1 and row['failed']==0,'real clock interval and actual completed World/LD/source stream closes');metadata=source_tools.compact(source_tools.inflate(bounded(owned/'snapshots'/(phase+'-level.dat')),31));require(metadata['Data'][1]['Time']==[4,expected_time],'actual source metadata persists logical Info.Time per phase');worlds[phase]={'entities':entities,'timestamp':stamp,'extent':extent,'metadata':metadata}
    for saved,reloaded in[('authored-saved','authored-reloaded'),('missing-saved','missing-reloaded')]:
        for suffix in['region.mcr','level.dat']:require(fingerprint(owned/'snapshots'/(saved+'-'+suffix))==fingerprint(owned/'snapshots'/(reloaded+'-'+suffix)),'reopen/readback leaves committed files byte identical within run')
    return{'mob_rows':mob,'world_states':states,'decoded_world_saves':worlds,'wrong_tag':wrong[0],'complete':rows[-1]}
def audit_wolf_owner(owned,rows):
    require(len(rows)==1 and rows[0].get('event')=='wolf-owner-complete','one separate finite native Wolf ownership receipt; no Mob comparison rows')
    row=rows[0];require(row['kind']=='Wolf'and row['checks']>0,'actual constructed Wolf native controls')
    require(row['oldUnits']==[79,108,100,0,937,55357,56378,55296,57343]and row['currentUnits']==[78,101,119,0,937,55357,56378,55296,57343],'external actual UTF16 String holds survive Entity and World retirement, including NUL and unpaired units')
    require(row['ownerBytes']>2*len(row['oldUnits'])and row['equalBudget']==0 and row['rejectedBudget']==2*row['ownerBytes']-1 and row['acceptedBudget']==2*row['ownerBytes'],'native old-plus-new peak bound dynamically derived from observed owner bytes')
    require(row['health']==-27 and row['hurt']==-26 and row['death']==-25 and row['attack']==-24 and row['watch16']==6,'earlier Living and Angry/Sitting source effects survive native owner-budget rejection')
    require(row['concretePc']==41 and row['childPc']==7 and row['rejectedNativePolicy']==1 and row['sourceException']==0,'native allocation policy at actual Wolf Owner setter, no original OOM/exception claim')
    require(all(row[key]==1 for key in['identityReuse','replacementDistinct','rejectionRetainsIdentity','postEntity','postWorld','cleanup'])and row['externalReleases']==2 and row['actualWorldLoads']==1 and row['actualWorldSaves']==0,'equal/replacement/rejection identities, actual destructor/World teardown and balanced external releases')
    require(not list((owned/'nbt').iterdir())and not list((owned/'snapshots').iterdir()),'ownership profile emits no comparison NBT or World-save snapshots')
    for actual,seed_file in[(owned/'world/level.dat',owned.parent/'seed-level.dat'),(owned/'world/region/r.0.0.mcr',owned.parent/'seed-region.mcr')]:require(fingerprint(actual)==fingerprint(seed_file),'native ownership-only profile leaves authored World files unchanged')
    return{'wolf_owner':row}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--source-root',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True);p.add_argument('--profile',choices=['mob','wolf-owner'],default='mob');a=p.parse_args();root=a.source_root.resolve();support=module(root/'tests/reference/client/statistics/support.py','mob_nbt_transport');source_tools=module(root/'tests/tools/world/test_source_saving.py','mob_nbt_world_codec')
    selected=[Path(__file__).resolve(),a.executable.resolve(),root/'tests/reference/client/statistics/support.py',root/'tests/tools/world/test_source_saving.py',root/'tests/world/runtime/saving/test_mob_nbt.c',root/'tests/world/runtime/saving/mob_nbt_source_support.inc',root/'tests/world/runtime/tile_entities/test_tile_entities.c',root/'src/entity/mob/nbt.c',root/'src/entity/mob/construction.c',root/'src/entity/nbt/nbt.c',root/'src/entity/nbt/factory.c',root/'src/entity/nbt/storage.c',root/'src/entity/nbt/errors.c',root/'include/mc/entity/nbt/errors.h',root/'src/world/storage/source_chunk_save.inc',root/'src/world/runtime/loading/loading.c',root/'src/world/runtime/saving/saving.c']
    if a.profile=='wolf-owner':selected +=[root/'src/entity/entity.c',root/'src/entity/player/strings.c',root/'src/entity/mob/nbt_internal.h']
    freeze=lambda:{str(p):fingerprint(p,268435456)for p in selected};before=freeze();a.work_dir.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='client-mob-nbt-'if a.profile=='mob'else'client-mob-wolf-owner-',dir=a.work_dir.resolve()));owned=work/'owned';(owned/'world/region').mkdir(parents=True);(owned/'snapshots').mkdir();(owned/'nbt').mkdir();info,image,raw=seed(source_tools);(owned/'world/level.dat').write_bytes(info);(owned/'world/region/r.0.0.mcr').write_bytes(image);(work/'seed-chunk.nbt').write_bytes(raw);(work/'seed-level.dat').write_bytes(info);(work/'seed-region.mcr').write_bytes(image);(work/'INPUT-FREEZE.json').write_text(json.dumps(before,indent=2)+'\n',newline='\n');record={'status':'FAIL','root':str(owned),'input_before':before,'profile':a.profile};print(json.dumps({'retained_work':str(work),'stage':'FROZEN_BEFORE_NATIVE_EXECUTION'}),flush=True)
    expected='PASS_AUTHORED_ACTUAL_SOURCE_WORLD_CLIENT_11_MOB_NBT'if a.profile=='mob'else'PASS_NATIVE_WOLF_UTF16_OWNERSHIP_LIMITS'
    try:
        command=[a.executable.resolve(),owned]+(['wolf-owner']if a.profile=='wolf-owner'else[]);capture=support.run_bounded(command,work,'native',timeout=180,limit=8388608);record['capture']=capture.capture_report;require(capture.returncode==0 and capture.stderr==b'','native complete exit0 empty stderr');console=capture.stdout.decode('ascii').splitlines();require(all(x in['16 achievements','151 recipes']for x in console if not x.startswith('{')),'only real Registry preparation console');rows=[json.loads(x)for x in console if x.startswith('{')]
        if a.profile=='wolf-owner':
            record.update(audit_wolf_owner(owned,rows));record['scope']='Native counted UTF16 String identity, overlapping allocation bound, real constructed Wolf destructor and actual World teardown. No original OOM equivalence or change to the83-row original/native comparison cohort.'
        else:
            record.update(audit(owned,rows,source_tools));record['scope']='Actual nonnullWorld Registry factory/import/provenance/current source Chunk save/reload for11Mobs on authored disposable files, plus actual own/full/Boolean hooks. No whole original World or natural gameplay equivalence.';record['original_only_control']='Existing powered-key retention uses Java mutable Compound; native independent fresh-result builder has no preexisting-Compound argument. That original control is not counted as native parity.'
        record['status']=expected
    except BaseException as error:record['error']=repr(error)
    finally:
        record['input_after']=freeze();record['inputs_unchanged']=before==record['input_after'];record['files']={str(p.relative_to(work)).replace('\\','/'):fingerprint(p,8388608)for p in work.rglob('*')if p.is_file()and p.name!='REPORT.json'}
        if not record['inputs_unchanged']:record['status']='FAIL'
        (work/'REPORT.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({'report':str(work/'REPORT.json'),'status':record['status'],'error':record.get('error')}),flush=True)
    require(record['status']==expected,'native Mob NBT selected profile qualification failed')
if __name__=='__main__':main()
