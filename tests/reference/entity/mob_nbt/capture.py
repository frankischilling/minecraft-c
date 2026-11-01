"""Private original client Mob NBT prepared reference; no native/runtime build."""
from pathlib import Path
import argparse,hashlib,json,os,struct,sys,tempfile,time,zipfile
sys.dont_write_bytecode=True
BASE=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--client-jar',type=Path,required=True);p.add_argument('--jdk-home',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True)
a=p.parse_args();ROOT=a.source_root.resolve(strict=True);JDK=a.jdk_home.resolve(strict=True);jar=a.client_jar.resolve(strict=True)
a.work_dir.mkdir(parents=True,exist_ok=True);OUT=Path(tempfile.mkdtemp(prefix='client-mob-nbt-',dir=a.work_dir.resolve()));classes=OUT/'classes';classes.mkdir()
JAVA=JDK/'bin'/('java.exe'if(JDK/'bin/java.exe').exists()else'java');JAVAC=JDK/'bin'/('javac.exe'if(JDK/'bin/javac.exe').exists()else'javac')
sys.path.insert(0,str(ROOT/'tests/reference/client/statistics'));from support import run_bounded
sys.path.insert(0,str(ROOT/'tests/reference/client/controller'));from classfile import inspect_class
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def require(b,s):
    if not b:raise ValueError(s)
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
def lines(p):
    require(p.stat().st_size<=67108864,'bounded NDJSON before read');return[json.loads(s)for s in p.read_text().splitlines()]
def nbt(data):
    require(len(data)<=1048576,'bounded actual raw NBT');at=0
    def take(n):
        nonlocal at
        require(0<=n<=len(data)-at,'complete NBT span');v=data[at:at+n];at+=n;return v
    def number(fmt):return struct.unpack(fmt,take(struct.calcsize(fmt)))[0]
    def utf():
        encoded=take(number('>H'));units=[];i=0
        while i<len(encoded):
            b=encoded[i];i+=1
            if b<128:require(b>0,'MUTF8 raw zero excluded');units.append(b)
            elif b&224==192:
                require(i<len(encoded)and encoded[i]&192==128,'MUTF8 pair');v=((b&31)<<6)|(encoded[i]&63);i+=1;require(v==0 or v>=128,'MUTF8 pair canonical');units.append(v)
            else:
                require(b&240==224 and i+1<len(encoded)and encoded[i]&192==encoded[i+1]&192==128,'MUTF8 triple');v=((b&15)<<12)|((encoded[i]&63)<<6)|(encoded[i+1]&63);i+=2;require(v>=2048,'MUTF8 triple canonical');units.append(v)
        return units
    def payload(kind,depth=0):
        require(depth<32,'bounded typed NBT depth')
        if kind in[1,2,3,4]:return[kind,number({1:'>b',2:'>h',3:'>i',4:'>q'}[kind])]
        if kind in[5,6]:return[kind,take({5:4,6:8}[kind]).hex()]
        if kind==8:return[kind,utf()]
        if kind==9:
            child=number('>B');count=number('>i');require(0<=count<=64,'bounded List');return[kind,{'elementType':child,'items':[payload(child,depth+1)for _ in range(count)]}]
        require(kind==10,'selected actual NBT kind');fields={}
        while True:
            child=number('>B')
            if child==0:break
            key=''.join(map(chr,utf()));require(key not in fields,'no duplicate emitted keys');fields[key]=payload(child,depth+1)
        return[kind,fields]
    require(number('>B')==10 and utf()==[],'actual unnamed Compound root');tree=payload(10);require(at==len(data),'exact NBT complete consume');return tree
LIVING={'Health','HurtTime','DeathTime','AttackTime'}
OWN={'Spider':set(),'Zombie':set(),'Skeleton':set(),'Creeper':{'powered'},'Slime':{'Size'},'Pig':{'Saddle'},'Sheep':{'Sheared','Color'},'Cow':set(),'Chicken':set(),'Wolf':{'Angry','Sitting','Owner'},'Squid':set()}
BASEFIELDS={'Pos','Motion','Rotation','FallDistance','Fire','Air','OnGround'}
def audit_target(directory):
    rows=lines(directory/'target.ndjson');require(rows[-1]['event']=='complete'and rows[-1]['kinds']==11 and rows[-1]['writes']==62,'complete 11-kind 62-write cohort');writes=[r for r in rows if r['event']=='write'];require(len(writes)==62,'exact finite writes');require(len(list((directory/'nbt').glob('*.nbt')))==62,'exact NBT artifact inventory');index={}
    for r in writes:
        f=directory/'nbt'/r['file'];require(f.stat().st_size==r['bytes']and sha(f)==r['sha256'],'actual raw NBT hash/count');require(nbt(f.read_bytes())==r['tree'],'independent raw binary decode equals observed actual tag graph');index[r['kind'],r['scenario'],r['operation']]=r
        tree=r['tree'][1];require(tree['Keep']==[3,0x1234],'unchanged unrelated marker');state=r['state'];expectedHealth=((state['ls.Y']+32768)%65536)-32768;require(tree.get('Health',[2,expectedHealth])==[2,expectedHealth],'writer real Health short narrowing')
    for kind,extra in OWN.items():
        ctor=index[kind,'constructor','own'];auth=index[kind,'authored','own'];missing=index[kind,'missing','own'];expectedExtra=extra-({'powered'}if kind=='Creeper'else set())
        require(set(ctor['tree'][1])=={'Keep'}|LIVING|expectedExtra,'constructor actual writer key presence')
        for key in LIVING:require(ctor['tree'][1][key][0]==2,'Living always Short')
        if kind=='Slime':require(ctor['tree'][1]['Size']==[3,ctor['state']['slimeSize']-1]and ctor['state']['ls.Y']==ctor['state']['slimeSize']**2,'constructor independent entropy relation')
        require(set(auth['tree'][1])=={'Keep'}|LIVING|extra,'authored own exact key presence');expectedHealth=16 if kind=='Slime'else-123
        require(auth['tree'][1]['Health']==[2,expectedHealth]and auth['tree'][1]['HurtTime']==[2,-32768]and auth['tree'][1]['DeathTime']==[2,32767]and auth['tree'][1]['AttackTime']==[2,-1],'actual typed authored Living fields and Slime later reset')
        for operation in['direct','storage']:
            r=index[kind,'authored',operation];require(set(r['tree'][1])=={'Keep'}|LIVING|extra|BASEFIELDS|({'id'}if operation=='storage'else set()),'full versus direct storage exact key presence');require(all(r['tree'][1][key]==auth['tree'][1][key]for key in LIVING|extra),'full writer real virtual own dispatch')
            require(r['tree'][1]['Fire']==[2,-7]and r['tree'][1]['Air']==[2,250]and r['tree'][1]['OnGround']==[1,1],'base typed normalization')
            if operation=='storage':require(r['gate']is True and r['tree'][1]['id']==[8,list(map(ord,kind))],'genuine known exact class ID and storage true')
        require(missing['tree'][1]['Health']==[2,1 if kind=='Slime'else 10]and all(missing['tree'][1][key]==[2,0]for key in LIVING-{'Health'}),'actual missing Living default and later Slime size reset')
        if kind=='Creeper':require('powered'not in missing['tree'][1],'read false then conditional powered omission')
        if kind=='Pig':require(auth['tree'][1]['Saddle']==[1,1]and missing['tree'][1]['Saddle']==[1,0],'Pig Boolean normalization/default')
        if kind=='Sheep':require(auth['tree'][1]['Sheared']==[1,1]and auth['tree'][1]['Color']==[1,15]and missing['tree'][1]['Sheared']==[1,0]and missing['tree'][1]['Color']==[1,0],'Sheep bits and signed Color masking')
        if kind=='Wolf':
            require(auth['tree'][1]['Owner']==[8,[79,119,110,101,114,0,937,55357,56378]]and missing['tree'][1]['Owner']==auth['tree'][1]['Owner'],'real MUTF8/UTF16 owner and missing-owner retention');require(auth['state']['watcher']['16']==7 and missing['state']['watcher']['16']==4,'actual sitting/angry/tamed bits');require(missing['tree'][1]['Angry']==missing['tree'][1]['Sitting']==[1,0],'Wolf missing boolean defaults')
        if kind=='Chicken':require(auth['state']['chickenEggTimer']==missing['state']['chickenEggTimer']and 6000<=auth['state']['chickenEggTimer']<12000,'real constructor egg entropy unchanged by read/write')
    removed=index['Pig','removed','storage'];require(removed['gate']is False and removed['tree']==[10,{'Keep':[3,0x1234]}],'actual removed Boolean gate untouched tag graph');require(set(index['Pig','removed','direct']['tree'][1])=={'Keep'}|LIVING|BASEFIELDS|{'Saddle'},'direct d bypasses removed gate')
    for scenario,health,size,stored in[('size-1',0,0,-1),('size127',16384,-128,-129),('size255',65536,0,-1)]:
        r=index['Slime',scenario,'own'];require(r['state']['ls.Y']==health and r['state']['slimeSize']==size and r['tree'][1]['Size']==[3,stored],'Slime full int square then signed watcher byte writer')
    require('powered'not in index['Creeper','watcher2','own']['tree'][1]and index['Creeper','watcher2-existing','own']['tree'][1]['powered']==[1,1],'exact watcher Byte1 condition and preexisting key retention')
    wrong=[r for r in rows if r['event']=='wrong-tag'];require(len(wrong)==1 and wrong[0]['exception']=='java.lang.ClassCastException','actual wrong scalar class cast');return{'complete':rows[-1],'writes':len(writes),'raw_nbt_independently_decoded':len(writes),'wrong_tag':wrong[0],'rows':rows}
require(sha(jar)=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c','verified untouched client');require('JAVA_VERSION="17.'in(JDK/'release').read_text(),'full installed JDK17')
inputs=[Path(__file__),BASE/'PREPARATION.md',BASE/'ATTEMPTS.md',BASE/'scenarios.json',*sorted((BASE/'inputs').glob('*.java')),jar,JDK/'release',JAVA,JAVAC,*sorted((JDK/'bin').rglob('*.dll')),*sorted(q for q in(JDK/'lib').rglob('*')if q.is_file()and'.so'in q.name),JDK/'lib/modules',JDK/'jmods/java.base.jmod',ROOT/'tests/reference/client/statistics/support.py',ROOT/'tests/reference/client/controller/classfile.py',Path(sys.executable)]
before={str(q):sha(q)for q in inputs};write(OUT/'input-freeze.json',{'status':'FROZEN_BEFORE_JAVAC_ORIGINAL_RUNTIME','input_sha256':before});authority={}
with zipfile.ZipFile(jar)as z:
    for name in['sn','ls','ii','gz','bg','ar','cn','uz','fr','gb','uw','wh','dl','bx','ww','gi','xt','jc','nu','ij','sp','ud','ma','gr','as','p','sz','qp','ul','pp','mi','xb','hn','lh']:authority[name]=inspect_class(z.read(name+'.class'))
write(OUT/'original-Code.json',authority)
record={'status':'FAIL','scope':'Actual client 11 Mob constructors and authored source NBT hooks/storage gate/registry only; no actual World/Chunk caller, native, server or Nether credit','input_before_sha256':before,'commands':{},'host_platform':sys.platform,'jdk_release':(JDK/'release').read_text(),'python_version':sys.version};env=os.environ.copy()
for k in['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH']:env.pop(k,None)
env['PYTHONDONTWRITEBYTECODE']='1'
try:
    command=[JAVAC,'--add-modules','jdk.jdi','-d',classes,*sorted((BASE/'inputs').glob('*.java'))];record['commands']['compile']=list(map(str,command));r=run_bounded(command,OUT,'compile',timeout=120,limit=2097152,env=env);record['compile_exit']=r.returncode;require(r.returncode==0,'helper compile');record['helper_before_sha256']={q.name:sha(q)for q in classes.glob('*.class')};write(OUT/'helper-Code.json',{q.name:inspect_class(q.read_bytes())for q in classes.glob('*.class')})
    for mode in['ordinary','debugger']:
        directory=OUT/mode;directory.mkdir();cp=str(classes)
        command=[JAVA,'-Xint','-Xverify:all','-Xmx256m','-Xlog:class+init=info:file=class-init.log','-Xlog:library=info:file=library-load.log','-cp',cp,'MobNbtTarget',directory,jar]if mode=='ordinary'else[JAVA,'--add-modules','jdk.jdi','-cp',cp,'MobNbtJdi',directory,JDK,jar,cp]
        record['commands'][mode]=list(map(str,command));print('START',mode,flush=True);started=time.monotonic();r=run_bounded(command,directory,'process',timeout=300,limit=8388608,env=env);record[mode+'_exit']=r.returncode;record[mode+'_seconds']=time.monotonic()-started;require(r.returncode==0 and r.stderr==b'',mode+' complete exit0 empty stderr')
        source_stdout=r.stdout if mode=='ordinary'else(directory/'target.stdout').read_bytes();require(source_stdout.splitlines()==[b'16 achievements',b'151 recipes'],'actual Session preparation initialization stdout')
        if mode=='debugger':require(r.stdout==b''and(directory/'target.stderr').read_bytes()==b'','complete debugger/target extra streams')
        record[mode]=audit_target(directory);print('OBSERVED',mode,round(record[mode+'_seconds'],2),flush=True)
    stable=[]
    for r,s in zip(record['ordinary']['rows'],record['debugger']['rows']):
        require((r['event'],r.get('kind'),r.get('scenario'),r.get('operation'))==(s['event'],s.get('kind'),s.get('scenario'),s.get('operation')),'same finite independent sequence')
        if r['event']=='write'and r['scenario']!='constructor':
            left=dict(r['tree'][1]);right=dict(s['tree'][1])
            if r['scenario']=='removed'and r['operation']=='direct':left.pop('Rotation');right.pop('Rotation')
            require(left==right and r['gate']==s['gate'],'independent authored VM semantic tree/gate equality with constructor rotation excluded');stable.append(r['file'])
    record['stable_authored_binary_trees_compared']=len(stable);record['independent_entropy_exclusions']=['Constructor-only Slime size/health correlate within each receiver','Fresh removed-Pig direct full writer Rotation retains independent constructor yaw; omitted only from cross-VM equality','Chicken egg timer is checked within each actual receiver, not cross-VM equality']
    events=lines(OUT/'debugger/checkpoints.ndjson');pins=[e for e in events if e['event']=='code-pin'];record['jdi_events']=len(events);record['verified_observed_code_pins']=len(pins)
    for pin in pins:
        methods=[m for m in authority[pin['class']]['methods']if m['name']==pin['method']and m['descriptor']==pin['descriptor']];require(len(methods)==1 and methods[0]['code_sha256']==pin['sha256']and methods[0]['code_bytes']==pin['bytes'],'observed actual original Code identity')
    factory=[e for e in events if e['event']=='entry'and e['location']['class']=='jc'and e['location']['descriptor']=='(Lnu;Lfd;)Lsn;'];require(len(factory)==11 and[e['phase']for e in factory]==list(range(11)),'11 genuine known-ID original factory calls')
    errors=[e for e in events if e['event']=='exception'and e['phase']==13 and e['exception']['class']=='java.lang.ClassCastException'];require(errors and any(e['location']['class']=='nu'for e in errors),'actual wrong typed Health cast at source getter')
    gates=[e for e in events if e['event']=='return'and e['location']['class']=='sn'and e['location']['method']=='c'and e['location']['descriptor']=='(Lnu;)Z'];require(len(gates)==12 and[g['result']for g in gates]==[True]*11+[False],'independent true/removedfalse SN.c returns')
    record['jdi_gate_results']=[g['result']for g in gates];record['jdi_wrong_tag_exceptions']=len(errors);record['jdi_known_factory_entries']=len(factory);record['status']='OBSERVED_AND_AUDITED_ORIGINAL_CLIENT_11_MOB_NBT'
except BaseException as e:record['error']=repr(e)
finally:
    record['input_after_sha256']={str(q):sha(q)for q in inputs};record['helper_after_sha256']={q.name:sha(q)for q in classes.glob('*.class')};record['inputs_unchanged']=before==record['input_after_sha256']and record.get('helper_before_sha256',{})==record['helper_after_sha256']
    if not record['inputs_unchanged']:record['status']='FAIL'
    record['artifact_sha256']={str(q.relative_to(OUT)):sha(q)for q in OUT.rglob('*')if q.is_file()and q.name!='receipt.json'};write(OUT/'receipt.json',record);print(json.dumps({'retained_work':str(OUT),**{k:record.get(k)for k in['status','error','compile_exit','ordinary_exit','debugger_exit','jdi_events','verified_observed_code_pins','inputs_unchanged']}}),flush=True)
require(record['status']=='OBSERVED_AND_AUDITED_ORIGINAL_CLIENT_11_MOB_NBT','original Mob NBT qualification failed')
