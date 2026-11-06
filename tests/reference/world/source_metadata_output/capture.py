from pathlib import Path
import argparse,hashlib,json,os,struct,sys,tempfile,time,zipfile,zlib
sys.dont_write_bytecode=True
BASE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description='Capture genuine prepared original mx metadata output in independent ordinary/read-only-JDI VMs')
parser.add_argument('--source-root',type=Path,required=True)
parser.add_argument('--client-jar',type=Path,required=True)
parser.add_argument('--server-jar',type=Path,required=True)
parser.add_argument('--jdk-home',type=Path,required=True)
parser.add_argument('--work-dir',type=Path,required=True)
args=parser.parse_args()
ROOT=args.source_root.resolve(strict=True)
JDK=args.jdk_home.resolve(strict=True)
JAVA=JDK/'bin'/('java.exe' if (JDK/'bin/java.exe').is_file() else 'java')
JAVAC=JDK/'bin'/('javac.exe' if (JDK/'bin/javac.exe').is_file() else 'javac')
args.work_dir.mkdir(exist_ok=True,parents=True)
OUT=Path(tempfile.mkdtemp(prefix='metadata-reference-',dir=args.work_dir.resolve()))
sys.path.insert(0,str(ROOT/'tests/reference/client/statistics'))
from support import run_bounded
sys.path.insert(0,str(ROOT/'tests/reference/client/controller'))
from classfile import inspect_class
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
def require(b,why):
    if not b:raise ValueError(why)

def decode(path):
    packed=path.read_bytes();require(len(packed)<=65536,'bounded original gzip file')
    require(packed[:10]==bytes.fromhex('1f8b08000000000000ff'),'exact reached installed-JDK gzip header')
    d=zlib.decompressobj(31);data=d.decompress(packed,1048577)
    require(len(data)<=1048576 and d.eof and not d.unused_data and not d.unconsumed_tail,'one bounded complete gzip member, valid CRC/trailer')
    pos=0
    def take(n):
        nonlocal pos
        require(0<=n<=len(data)-pos,'bounded independent NBT extent');b=data[pos:pos+n];pos+=n;return b
    def number(fmt):return struct.unpack(fmt,take(struct.calcsize(fmt)))[0]
    def utf():
        b=take(number('>H'));units=[];i=0
        while i<len(b):
            c=b[i];i+=1
            if 1<=c<=127:units.append(c)
            elif c&0xe0==0xc0:
                require(i<len(b) and b[i]&0xc0==0x80,'MUTF8 continuation');v=((c&31)<<6)|(b[i]&63);i+=1;require(v==0 or v>=128,'MUTF8 two-byte shortest form');units.append(v)
            else:
                require(c&0xf0==0xe0 and i+1<len(b) and b[i]&0xc0==0x80 and b[i+1]&0xc0==0x80,'MUTF8 three-byte sequence');v=((c&15)<<12)|((b[i]&63)<<6)|(b[i+1]&63);i+=2;require(v>=2048,'MUTF8 three-byte shortest form');units.append(v)
        return units
    def tag(kind,depth=0):
        require(depth<64,'NBT depth')
        if kind==1:return [kind,number('>b')]
        if kind==3:return [kind,number('>i')]
        if kind==4:return [kind,number('>q')]
        if kind==8:return [kind,utf()]
        require(kind==10,'selected metadata tag domain');members={}
        while True:
            t=number('>B')
            if t==0:break
            name=utf();require(all(0<x<128 for x in name),'ASCII metadata keys');key=''.join(map(chr,name));require(key not in members,'unique compound member');members[key]=tag(t,depth+1)
        return [kind,members]
    kind=number('>B');name=utf();tree=tag(kind);require(name==[] and pos==len(data),'empty single compound root, complete payload')
    return tree,{'compressed_bytes':len(packed),'uncompressed_bytes':len(data),'raw_sha256':hashlib.sha256(data).hexdigest(),'gzip_sha256':sha(path),'header':packed[:10].hex(),'trailer':packed[-8:].hex()}

def audit_target(directory):
    rows=[json.loads(line) for line in (directory/'target.ndjson').read_text().splitlines()]
    require(rows[-1]=={'event':'complete','sourceCalls':7},'complete genuine source-call footer')
    after=[r for r in rows if r['event']=='after'];before=[r for r in rows if r['event']=='before'];require(len(after)==len(before)==7,'seven actual before/after calls')
    require(all(r['returned'] is True for r in after),'all original fm calls return incl caught open failure')
    decoded={};receipts={}
    for row in after:
        phase=row['phase'];name=row['case'];files=row['files'];require(files['session.lock']['bytes']==8,'real constructor session token')
        if phase in (0,1,2,3,6):
            path=directory/'snapshots'/f'{phase}-level.dat';tree,receipt=decode(path);require(tree[0]==10 and set(tree[1])=={'Data'} and tree[1]['Data'][0]==10,'fresh Data-only root')
            data=tree[1]['Data'][1];expected={'RandomSeed':[4,-9223372036854775807],'SpawnX':[3,-17],'SpawnY':[3,65],'SpawnZ':[3,1003],'Time':[4,-1234567890123 if phase in (2,3) else 23999],'SizeOnDisk':[4,9876543210],'version':[3,19132],'rainTime':[3,-345],'raining':[1,1],'thunderTime':[3,678],'thundering':[1,0],'LevelName':[8,[ord(x) for x in ('Second' if phase in (2,3) else 'First')]+[0,937,55357,56832]]}
            require(set(data)==set(expected)|{'LastPlayed'},'complete metadata keys, original omits loaded Player and Unknown')
            require(all(data[k]==v for k,v in expected.items()),'exact typed original metadata values including modified UTF16')
            require(data['LastPlayed'][0]==4 and row['beforeMillis']<=data['LastPlayed'][1]<=row['afterMillis'],'actual original LastPlayed lies in per-process call interval')
            decoded[str(phase)]=tree;receipts[str(phase)]=receipt
            require('level.dat_new' not in files,'real new File retired by successful source rotation')
    require((directory/'snapshots/2-level.dat_old').read_bytes()==(directory/'snapshots/1-level.dat').read_bytes(),'exact intra-process preceding-current backup preservation')
    for phase in (3,4,5):
        b=before[phase]['files'];a=after[phase]['files'];sentinels=[n for n in b if n.endswith('/sentinel')]
        require(sentinels and all(a[n]==b[n] for n in sentinels),'obstruction sentinels exactly preserved')
    require(after[3]['files']['level.dat_old']['kind']=='directory' and after[3]['files']['level.dat']['kind']=='file','old refusal continues through current deletion and replacement')
    require(after[4]['files']==before[4]['files'],'both nonempty directories unchanged and final temp deleted after ignored rename failure')
    require(after[5]['files']==before[5]['files'],'real FOS open failure has no compression or rotation effects')
    require(after[2]['files']['level.dat_old']['sha256']==after[1]['files']['level.dat']['sha256'],'actual old exact content identity')
    return {'calls':after,'decoded':decoded,'files':receipts}

classes=OUT/'classes';classes.mkdir();jar=args.client_jar.resolve(strict=True);server=args.server_jar.resolve(strict=True)
require('JAVA_VERSION="17.' in (JDK/'release').read_text(),'explicit JDK17 profile')
require(sha(jar)=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c','client JAR authority');require(sha(server)=='033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d','server hash identity only, no server runtime')
inputs=[Path(__file__),BASE/'README.md',BASE/'scenarios.json',*sorted(BASE.glob('*.java')),jar,server,JDK/'release',JAVA,JAVAC,*sorted((JDK/'bin').rglob('*.dll')),*sorted(p for p in (JDK/'lib').rglob('*') if p.is_file() and '.so' in p.name),JDK/'lib/modules',JDK/'jmods/java.base.jmod',ROOT/'tests/reference/client/statistics/support.py',ROOT/'tests/reference/client/controller/classfile.py',Path(sys.executable)]
before={str(p):sha(p) for p in inputs};write(OUT/'input-freeze.json',{'status':'FROZEN_BEFORE_HELPER_COMPILE_AND_ORIGINAL_RUNTIME','inputs_sha256':before})
authority={}
with zipfile.ZipFile(jar) as z:
    for name in ('mx','fm','ei','as','nu','ij'):authority[name]=inspect_class(z.read(name+'.class'))
with zipfile.ZipFile(JDK/'jmods/java.base.jmod') as z:
    for name in ('java.io.File','java.io.FileOutputStream','java.io.DataOutputStream','java.io.FilterOutputStream','java.util.zip.GZIPOutputStream','java.util.zip.DeflaterOutputStream'):authority[name]=inspect_class(z.read('classes/'+name.replace('.','/')+'.class'))
write(OUT/'original-Code.json',authority)
env=os.environ.copy()
for k in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH'):env.pop(k,None)
env['PYTHONDONTWRITEBYTECODE']='1';receipt={'status':'FAIL','input_before_sha256':before,'scope':'Prepared original client handler boundary on the recorded host/JDK profile only; no native C, whole World or Player serializer claim','commands':{},'host_platform':sys.platform,'python_version':sys.version,'jdk_release':(JDK/'release').read_text()}
try:
    command=[JAVAC,'--add-modules','jdk.jdi','-d',classes,*sorted(BASE.glob('*.java'))]
    receipt['commands']['compile']=list(map(str,command));r=run_bounded(command,OUT,'compile',timeout=120,limit=2097152,env=env);require(r.returncode==0,'helper compilation');receipt['compile_exit']=r.returncode
    receipt['helper_before_sha256']={p.name:sha(p) for p in classes.glob('*.class')};write(OUT/'helper-Code.json',{p.name:inspect_class(p.read_bytes()) for p in classes.glob('*.class')})
    for mode in ('ordinary','debugger'):
        directory=OUT/mode;directory.mkdir();cp=str(classes)
        if mode=='ordinary':command=[JAVA,'-Xint','-Xverify:all','-Xmx256m','-Djava.awt.headless=true','-Xlog:class+init=info:file=class-init.log','-Xlog:library=info:file=library-load.log','-cp',cp,'MetadataTarget',directory,jar]
        else:command=[JAVA,'--add-modules','jdk.jdi','-cp',cp,'MetadataJdi',directory,JDK,jar,cp]
        receipt['commands'][mode]=list(map(str,command));start=time.monotonic();print('START',mode,flush=True);r=run_bounded(command,directory,'process',timeout=240,limit=8388608,env=env);receipt[mode+'_exit']=r.returncode;receipt[mode+'_seconds']=time.monotonic()-start;require(r.returncode==0 and r.stdout==b'',mode+' complete original process and empty process stdout')
        stderr=r.stderr if mode=='ordinary' else (directory/'target.stderr').read_bytes()
        if mode=='debugger':require(r.stderr==b'' and (directory/'target.stdout').read_bytes()==b'','debugger and target complete extra streams')
        require(stderr.startswith(b'java.io.FileNotFoundException:') and stderr.count(b'java.io.FileNotFoundException:')==1 and b'open-failure' in stderr,'one retained actual original caught open-failure diagnostic')
        receipt[mode]=audit_target(directory);print('OBSERVED',mode,round(receipt[mode+'_seconds'],2),flush=True)
    for phase in receipt['ordinary']['decoded']:
        def stable(t):
            d=dict(t[1]['Data'][1]);d.pop('LastPlayed');return d
        require(stable(receipt['ordinary']['decoded'][phase])==stable(receipt['debugger']['decoded'][phase]),'exact stable typed values across independent VMs; sampled clocks separately interval checked')
    events=[json.loads(line) for line in (OUT/'debugger/checkpoints.ndjson').read_text().splitlines()];receipt['jdi_events']=len(events);pins=[e for e in events if e['event']=='code-pin']
    for pin in pins:
        if pin['native'] or pin['abstract']:continue
        owner=authority[pin['class']];method=[m for m in owner['methods'] if m['name']==pin['method'] and m['descriptor']==pin['descriptor']];require(len(method)==1 and method[0]['code_sha256']==pin['sha256'] and method[0]['code_bytes']==pin['bytes'],'observed original exact Code pin')
    receipt['verified_observed_code_pins']=len(pins)
    all_file_returns=[e for e in events if e['event']=='return' and e['location']['class']=='java.io.File']
    direct=lambda e:len(e['frames'])>1 and e['frames'][1]['class']=='fm' and e['frames'][1]['descriptor']=='(Lei;Ljava/util/List;)V'
    file_returns={str(p):[{'method':e['location']['method'],'path':e['receiver_fields'].get('java.io.File.path'),'caller':e['frames'][1],'result':e['result']} for e in all_file_returns if e['phase']==p and direct(e)] for p in range(7)}
    receipt['incidental_file_returns']=[e for e in all_file_returns if not direct(e)]
    receipt['file_return_scope']='Exact direct caller fm.a(ei,List) frame; incidental native-library/class-loader File calls retained separately without source rotation credit'
    receipt['source_file_returns']=file_returns
    expected={0:[('exists',False),('renameTo',False),('exists',False),('renameTo',True),('exists',False)],1:[('exists',True),('delete',True),('renameTo',True),('exists',False),('renameTo',True),('exists',False)],2:[('exists',True),('delete',True),('renameTo',True),('exists',False),('renameTo',True),('exists',False)],3:[('exists',True),('delete',False),('renameTo',False),('exists',True),('delete',True),('renameTo',True),('exists',False)],4:[('exists',True),('delete',False),('renameTo',False),('exists',True),('delete',False),('renameTo',False),('exists',True),('delete',True)],5:[],6:[('exists',False),('renameTo',False),('exists',False),('renameTo',True),('exists',False)]}
    for phase,expect in expected.items():require([(e['method'],e['result']) for e in file_returns[str(phase)]]==expect,'actual original ordered ignored booleans phase'+str(phase))
    for phase in range(7):
        exits=[e for e in events if e['event']=='return' and e['phase']==phase and e['location']['class']=='fm' and e['location']['descriptor']=='(Lei;Ljava/util/List;)V'];require(len(exits)==1 and exits[0]['location']['pc']==150,'actual original handler return150 including caught failure')
    failure=[e for e in events if e['event']=='exception' and e['phase']==5];require(any(e['exception']['class']=='java.io.FileNotFoundException' and e['catch']['class']=='fm' and e['catch']['pc']==143 for e in failure),'genuine FOS open exception catches at fm143')
    require(not any(e['phase']==5 and e['event']=='entry' and e['location']['class']=='as' for e in events),'FOS open failure does not reach as')
    receipt['status']='OBSERVED_AND_AUDITED_ORIGINAL_METADATA_OUTPUT_PREPARED_BOUNDARY'
except BaseException as error:receipt['error']=repr(error)
finally:
    receipt['input_after_sha256']={str(p):sha(p) for p in inputs};receipt['helper_after_sha256']={p.name:sha(p) for p in classes.glob('*.class')};receipt['inputs_unchanged']=before==receipt['input_after_sha256'] and receipt.get('helper_before_sha256',{})==receipt['helper_after_sha256']
    if not receipt['inputs_unchanged']:receipt['status']='FAIL'
    receipt['artifacts_sha256']={str(p.relative_to(OUT)):sha(p) for p in OUT.rglob('*') if p.is_file() and p.name!='receipt.json'};write(OUT/'receipt.json',receipt)
    print(json.dumps({'retained_work':str(OUT),**{k:receipt.get(k) for k in ('status','error','compile_exit','ordinary_exit','debugger_exit','jdi_events','verified_observed_code_pins','inputs_unchanged')}}),flush=True)
require(receipt['status']=='OBSERVED_AND_AUDITED_ORIGINAL_METADATA_OUTPUT_PREPARED_BOUNDARY','capture did not qualify')
