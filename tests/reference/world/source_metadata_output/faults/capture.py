from pathlib import Path
import argparse,hashlib,json,os,sys,tempfile,time,zipfile,zlib
sys.dont_write_bytecode=True
BASE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description='Observe unchanged original as compressed-NBT authored failure controls')
parser.add_argument('--source-root',type=Path,required=True)
parser.add_argument('--client-jar',type=Path,required=True)
parser.add_argument('--jdk-home',type=Path,required=True)
parser.add_argument('--work-dir',type=Path,required=True)
args=parser.parse_args()
ROOT=args.source_root.resolve(strict=True);JDK=args.jdk_home.resolve(strict=True)
JAVA=JDK/'bin'/('java.exe' if (JDK/'bin/java.exe').is_file() else 'java')
JAVAC=JDK/'bin'/('javac.exe' if (JDK/'bin/javac.exe').is_file() else 'javac')
args.work_dir.mkdir(parents=True,exist_ok=True);OUT=Path(tempfile.mkdtemp(prefix='as-fault-reference-',dir=args.work_dir.resolve()))
sys.path.insert(0,str(ROOT/'tests/reference/client/statistics'));from support import run_bounded
sys.path.insert(0,str(ROOT/'tests/reference/client/controller'));from classfile import inspect_class
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
def require(b,s):
    if not b:raise ValueError(s)
classes=OUT/'classes';classes.mkdir();jar=args.client_jar.resolve(strict=True)
require('JAVA_VERSION="17.' in (JDK/'release').read_text(),'explicit JDK17 profile')
require(sha(jar)=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c','supplied original JAR')
inputs=[Path(__file__),BASE/'README.md',BASE/'scenarios.json',*sorted(BASE.glob('*.java')),jar,JDK/'release',JAVA,JAVAC,*sorted((JDK/'bin').rglob('*.dll')),*sorted(p for p in (JDK/'lib').rglob('*') if p.is_file() and '.so' in p.name),JDK/'lib/modules',JDK/'jmods/java.base.jmod',ROOT/'tests/reference/client/statistics/support.py',ROOT/'tests/reference/client/controller/classfile.py',Path(sys.executable)]
before={str(p):sha(p)for p in inputs};write(OUT/'input-freeze.json',{'status':'FROZEN_BEFORE_JAVAC_ORIGINAL_EXECUTION','input_sha256':before})
authority={}
with zipfile.ZipFile(jar)as z:
    for name in ['as','nu','ij']:authority[name]=inspect_class(z.read(name+'.class'))
with zipfile.ZipFile(JDK/'jmods/java.base.jmod')as z:
    for name in ['java.io.File','java.io.FileOutputStream','java.io.DataOutputStream','java.io.FilterOutputStream','java.util.zip.GZIPOutputStream','java.util.zip.DeflaterOutputStream']:authority[name]=inspect_class(z.read('classes/'+name.replace('.','/')+'.class'))
write(OUT/'original-Code.json',authority)
record={'status':'FAIL','scope':'Authored unchanged original as/JDK compressed-NBT OutputStream controls; not original OS/native FOS/World/Game fault evidence','input_before_sha256':before,'commands':{},'host_platform':sys.platform,'python_version':sys.version,'jdk_release':(JDK/'release').read_text()}
env=os.environ.copy()
for k in ['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH']:env.pop(k,None)
env['PYTHONDONTWRITEBYTECODE']='1'
try:
    command=[JAVAC,'--add-modules','jdk.jdi','-d',classes,*sorted(BASE.glob('*.java'))];record['commands']['compile']=list(map(str,command));r=run_bounded(command,OUT,'compile',timeout=120,limit=2097152,env=env);require(r.returncode==0,'helper compile');record['compile_exit']=r.returncode
    record['helper_before_sha256']={p.name:sha(p)for p in classes.glob('*.class')};write(OUT/'helper-Code.json',{p.name:inspect_class(p.read_bytes())for p in classes.glob('*.class')})
    for mode in ['ordinary','debugger']:
        directory=OUT/mode;directory.mkdir();cp=str(classes)
        if mode=='ordinary':command=[JAVA,'-Xint','-Xverify:all','-Xmx256m','-Djava.awt.headless=true','-Xlog:class+init=info:file=class-init.log','-Xlog:library=info:file=library-load.log','-cp',cp,'StreamFaultTarget',directory,jar]
        else:command=[JAVA,'--add-modules','jdk.jdi','-cp',cp,'StreamFaultJdi',directory,JDK,jar,cp]
        record['commands'][mode]=list(map(str,command));print('START',mode,flush=True);start=time.monotonic();r=run_bounded(command,directory,'process',timeout=240,limit=8388608,env=env);record[mode+'_exit']=r.returncode;record[mode+'_seconds']=time.monotonic()-start;require(r.returncode==0 and r.stdout==r.stderr==b'',mode+' exit0/empty process streams')
        if mode=='debugger':require((directory/'target.stdout').read_bytes()==(directory/'target.stderr').read_bytes()==b'','complete empty target streams')
        rows=[json.loads(line)for line in(directory/'target.ndjson').read_text().splitlines()];require(rows[-1]=={'event':'complete','sourceCalls':9},'nine complete source calls');outcomes=rows[:-1];require(len(outcomes)==9 and all(x['event']=='outcome'and x['phase']==i for i,x in enumerate(outcomes)),'nine exact outcome identities')
        expected=[(True,None,1),(False,'write-1',0),(False,'write-2',1),(False,'close-1',1),(False,'close-1',1),(False,'write-2',0),(False,None,0),(False,'flush-1',1),(False,'close-1',1)]
        for phase,(returned,message,closes)in enumerate(expected):
            row=outcomes[phase];require(row['returned']==returned and row['closes']==closes,'actual source return/close count '+str(phase));data=(directory/f'{phase}-committed.bin').read_bytes();require(sha(directory/f'{phase}-committed.bin')==row['sha256']and len(data)==row['committedBytes'],'actual committed file bytes')
            if phase==6:require(row['exception']['class']=='java.lang.NullPointerException'and row['writes']==0,'actual null original source exception')
            elif not returned:require(row['exception']['class']=='java.io.IOException'and row['exception']['message']==message,'actual propagated IOException selection')
            require((row['exception']or{}).get('suppressed',[])==([{'class':'java.io.IOException','message':'flush-1'}]if phase==8 else []),'actual as no suppression / JDK Filter suppression selection')
            if phase!=6:require(data[:10]==bytes.fromhex('1f8b08000000000000ff'),'actual original emitted header')
        require(outcomes[1]['committedBytes']==10 and outcomes[1]['flushes']==0,'committed constructor header fault has no DOS flush/close')
        require(outcomes[2]['flushes']==outcomes[4]['flushes']==1,'source write failure enters exactly one finally close')
        require(outcomes[5]['flushes']==1 and outcomes[5]['closes']==0,'normal finish failure never reaches underlying close')
        require(zlib.decompress((directory/'0-committed.bin').read_bytes(),31)==b'\x0a\x00\x00\x03\x00\x05Value\xff\xed\x29\x79\x00','independent exact original small NBT payload')
        record[mode]=outcomes;print('OBSERVED',mode,round(record[mode+'_seconds'],2),flush=True)
    require(record['ordinary']==record['debugger'],'exact deterministic outcomes/counters/committed bytes across independent VMs')
    events=[json.loads(line)for line in(OUT/'debugger/checkpoints.ndjson').read_text().splitlines()];pins=[e for e in events if e['event']=='code-pin']
    for pin in pins:
        if pin['native']or pin['abstract']:continue
        methods=[m for m in authority[pin['class']]['methods']if m['name']==pin['method']and m['descriptor']==pin['descriptor']];require(len(methods)==1 and methods[0]['code_sha256']==pin['sha256']and methods[0]['code_bytes']==pin['bytes'],'observed unchanged original/JDK Code pin')
    record['jdi_events']=len(events);record['verified_observed_code_pins']=len(pins)
    prefixes={str(p):[e['location']['pc']for e in events if e['phase']==p and e['event']=='source-prefix'and e['location']['class']=='as'and e['location']['descriptor']=='(Lnu;Ljava/io/OutputStream;)V']for p in range(9)};record['source_prefixes']=prefixes
    for phase in range(9):
        pcs=prefixes[str(phase)]
        if phase in [1,6]:require(9 in pcs and 12 not in pcs and 22 not in pcs and 30 not in pcs,'constructor failure no DOS/finally')
        elif phase in [2,4]:require(18 in pcs and 28 in pcs and 30 in pcs and 22 not in pcs,'serialization failure actual finally30 only')
        else:require(22 in pcs and 30 not in pcs,'normal source close22 has no second finally30')
    record['status']='OBSERVED_AND_AUDITED_ORIGINAL_AS_AUTHORED_FAILURE_CONTROLS'
except BaseException as e:record['error']=repr(e)
finally:
    record['input_after_sha256']={str(p):sha(p)for p in inputs};record['helper_after_sha256']={p.name:sha(p)for p in classes.glob('*.class')};record['inputs_unchanged']=before==record['input_after_sha256']and record.get('helper_before_sha256',{})==record['helper_after_sha256']
    if not record['inputs_unchanged']:record['status']='FAIL'
    record['artifacts_sha256']={str(p.relative_to(OUT)):sha(p)for p in OUT.rglob('*')if p.is_file()and p.name!='receipt.json'};write(OUT/'receipt.json',record);print(json.dumps({'retained_work':str(OUT),**{k:record.get(k)for k in ['status','error','compile_exit','ordinary_exit','debugger_exit','jdi_events','verified_observed_code_pins','inputs_unchanged']}}),flush=True)
require(record['status']=='OBSERVED_AND_AUDITED_ORIGINAL_AS_AUTHORED_FAILURE_CONTROLS','capture qualification failed')
