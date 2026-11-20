from pathlib import Path
import argparse,hashlib,json,os,struct,sys,tempfile,time,zipfile,zlib
sys.dont_write_bytecode=True
BASE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description='Capture genuine prepared original source region output')
parser.add_argument('--source-root',type=Path,required=True)
parser.add_argument('--client-jar',type=Path,required=True)
parser.add_argument('--jdk-home',type=Path,required=True)
parser.add_argument('--work-dir',type=Path,required=True)
args=parser.parse_args();ROOT=args.source_root.resolve(strict=True);JDK=args.jdk_home.resolve(strict=True)
args.work_dir.mkdir(parents=True,exist_ok=True);OUT=Path(tempfile.mkdtemp(prefix='source-region-output-',dir=args.work_dir.resolve()))
JAVA=JDK/'bin'/('java.exe'if(JDK/'bin/java.exe').is_file()else'java');JAVAC=JDK/'bin'/('javac.exe'if(JDK/'bin/javac.exe').is_file()else'javac')
sys.path.insert(0,str(ROOT/'tests/reference/client/statistics'));from support import run_bounded
sys.path.insert(0,str(ROOT/'tests/reference/client/controller'));from classfile import inspect_class
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def require(b,s):
    if not b:raise ValueError(s)
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
def lines(path):
    require(path.stat().st_size<=67108864,'bounded complete JSON-line artifact');return[json.loads(s)for s in path.read_text().splitlines()]
def payload(length):
    state=0x12345678;out=bytearray()
    for _ in range(length):
        state^=(state<<13)&0xffffffff;state^=state>>17;state^=(state<<5)&0xffffffff;out.append(state&255)
    return bytes(out)
def verify_nbt(data,length):
    require(len(data)==length+18 and data[:17]==b'\x0a\x00\x00\x07\x00\x07Payload'+struct.pack('>i',length) and data[17:-1]==payload(length)and data[-1:]==b'\x00','exact independently constructed one-ByteArray typed NBT')
def narrow(n):return((n+0x80000000)&0xffffffff)-0x80000000
def audit_target(directory):
    rows=lines(directory/'target.ndjson');require(rows[-1]['event']=='complete'and rows[-1]['outputs']==9 and rows[-1]['normalCommits']==6 and rows[-1]['caughtClosedRafCommit']==1 and rows[-1]['unclosedOutputs']==2,'complete finite original cohort')
    snapshots={(r['phase'],r['stage']):r for r in rows[:-1]};require(len(snapshots)==45,'exact actual snapshot inventory')
    growths=[12288,0,20480,0,0,12288];lengths=[32,32,20000,32,12000,32];expected_offsets=[513,513,773,513,771,513];summaries={}
    for phase in range(6):
        begin=snapshots[phase,'begin'];written=snapshots[phase,'written'];closed=snapshots[phase,'closed'];repeat=snapshots[phase,'repeat-closed'];growth=snapshots[phase,'growth'];reset=snapshots[phase,'growth-reset'];owner=closed['owner'];slot=(closed['x']&31)+32*(closed['z']&31)
        require(begin['owner']['hzCapacity']==8096 and begin['owner']['hzCount']==0 and begin['owner']['deflateBuffer']==512 and begin['owner']['usesDefaultDeflater']is True,'actual initial Hz8096/defaultdeflater512')
        require(written['owner']['dosWritten']==lengths[phase]+18 and written['owner']['outerClosed']is False,'actual streaming DOS count before close')
        require(owner['outerClosed']is True and owner['deflaterClosed']is True and owner['offsets'][slot]==expected_offsets[phase],'actual source close/offset branch')
        require(owner['offsets']==repeat['owner']['offsets']and owner['timestamps']==repeat['owner']['timestamps']and owner['free']==repeat['owner']['free']and closed['files']==repeat['files'],'repeated outer close performs no commit replay')
        require(growth['details']['returned']==growths[phase]and reset['details']['returned']==0 and growth['owner']['growth']==reset['owner']['growth']==0,'late actual rj.b growth read/reset twice')
        interval=closed['details'];before,after=interval['beforeMillis'],interval['afterMillis'];require(before<=after and after-before<300000,'bounded actual clock interval');stamp=owner['timestamps'][slot];require(any(narrow(sec)==stamp for sec in range(before//1000,after//1000+1)),'actual timestamp integer narrowing in own real interval')
        path=directory/'snapshots'/f'{phase}-closed-hz.bin';encoded=path.read_bytes();require(len(encoded)==owner['hzCount']and sha(path)==owner['hzBytes']['sha256'],'actual retained Hz prefix');d=zlib.decompressobj();raw=d.decompress(encoded,1048577);require(d.eof and not d.unused_data and not d.unconsumed_tail and len(raw)<=1048576,'complete bounded zlib stream');verify_nbt(raw,lengths[phase]);require((len(encoded)+5)//4096+1==(owner['offsets'][slot]&255),'source sector arithmetic for actual compressed length')
        name=owner['file'];file=directory/'snapshots'/f'{phase}-closed-{name}';disk=file.read_bytes();require(len(disk)%4096==0 and len(disk)==len(owner['free'])*4096,'actual region extent/free list');require(list(struct.unpack('>1024i',disk[:4096]))==owner['offsets']and list(struct.unpack('>1024i',disk[4096:8192]))==owner['timestamps'],'actual disk headers match RAM');sector=owner['offsets'][slot]>>8;at=sector*4096;count=struct.unpack('>i',disk[at:at+4])[0];require(count==len(encoded)+1 and disk[at+4]==2 and disk[at+5:at+5+len(encoded)]==encoded,'actual selector2/length/payload span')
        summaries[str(phase)]={'case':begin['details']['case'],'x':closed['x'],'z':closed['z'],'payload_bytes':lengths[phase],'offset':owner['offsets'][slot],'timestamp':stamp,'clock_interval':[before,after],'growth':growths[phase],'compressed':owner['hzBytes'],'free':owner['free'],'qj':owner['qj'],'raf':owner['raf'],'region_file':name}
    main=snapshots[5,'growth-reset'];abandoned=snapshots[6,'abandoned-unclosed'];require(main['files']==abandoned['files']and abandoned['owner']['offsets'][2]==0 and abandoned['owner']['outerClosed']is False,'abandon does not source-close or commit')
    old=snapshots[7,'written'];flushed=snapshots[7,'cache-flushed'];replacement=snapshots[8,'replacement-unclosed'];failed=snapshots[9,'old-closed-source-returned'];live=snapshots[9,'replacement-still-unclosed']
    require(old['owner']['qj']==flushed['owner']['qj']==failed['owner']['qj']and old['owner']['raf']==flushed['owner']['raf']==failed['owner']['raf'],'same strongly retained old Hz/QJ/RAF across flush and close')
    require(replacement['owner']['qj']!=old['owner']['qj']and replacement['owner']['raf']!=old['owner']['raf'],'actual replacement QJ/RAF identity')
    require(flushed['owner']['rafPointer']['status']==flushed['owner']['rafLength']['status']=='exception'and flushed['owner']['cache']=={},'source cache flush closes actual old RAF and clears cache')
    require(failed['files']==old['files']==replacement['files']==live['files']and failed['owner']['offsets']==old['owner']['offsets']and failed['owner']['timestamps']==old['owner']['timestamps']and failed['owner']['free']==old['owner']['free'],'caught old closed-RAF commit retains actual common prefix without disk effects')
    require(failed['owner']['outerClosed']is True and failed['owner']['deflaterClosed']is True and live['owner']['outerClosed']is False,'actual caught source return versus still-unclosed replacement')
    require(snapshots[0,'begin']['owner']['qj']==snapshots[4,'begin']['owner']['qj']==abandoned['owner']['qj']==old['owner']['qj'],'ordinary cache reuses same main QJ')
    require(snapshots[5,'begin']['owner']['file']=='r.-1.-2.mcr'and snapshots[5,'begin']['owner']['localX']==snapshots[5,'begin']['owner']['localZ']==31,'signed region shifts and local masks')
    return{'normal':summaries,'snapshots':len(snapshots),'complete':rows[-1],'flush_live':{'old_qj':old['owner']['qj'],'old_raf':old['owner']['raf'],'new_qj':replacement['owner']['qj'],'new_raf':replacement['owner']['raf'],'original_caught_close_returned':True,'native_scope':'Only common prefix through actual qj441 diagnostic dependency; no native caught return or later replacement/growth credit'}}

classes=OUT/'classes';classes.mkdir();jar=args.client_jar.resolve(strict=True);require(sha(jar)=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c','untouched original archive');require('JAVA_VERSION="17.'in(JDK/'release').read_text(),'explicit installed JDK17')
inputs=[Path(__file__),BASE/'README.md',BASE/'scenarios.json',*sorted(BASE.glob('*.java')),jar,JDK/'release',JAVA,JAVAC,*sorted((JDK/'bin').rglob('*.dll')),*sorted(p for p in(JDK/'lib').rglob('*')if p.is_file()and'.so'in p.name),JDK/'lib/modules',JDK/'jmods/java.base.jmod',ROOT/'tests/reference/client/statistics/support.py',ROOT/'tests/reference/client/controller/classfile.py',Path(sys.executable)]
before={str(p):sha(p)for p in inputs};write(OUT/'input-freeze.json',{'status':'FROZEN_BEFORE_JAVAC_ORIGINAL_RUNTIME','input_sha256':before});authority={}
with zipfile.ZipFile(jar)as z:
    for name in['rj','qj','hz','as','nu','ij']:authority[name]=inspect_class(z.read(name+'.class'))
with zipfile.ZipFile(JDK/'jmods/java.base.jmod')as z:
    for name in['java.io.RandomAccessFile','java.io.DataOutputStream','java.io.FilterOutputStream','java.io.ByteArrayOutputStream','java.util.zip.DeflaterOutputStream']:authority[name]=inspect_class(z.read('classes/'+name.replace('.','/')+'.class'))
write(OUT/'original-Code.json',authority);record={'status':'FAIL','scope':'Genuine original lower region output/cache owner prepared cohort; no native, source Chunk/World caller or full save credit','input_before_sha256':before,'commands':{},'host_platform':sys.platform,'jdk_release':(JDK/'release').read_text(),'python_version':sys.version};env=os.environ.copy()
for k in['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH']:env.pop(k,None)
env['PYTHONDONTWRITEBYTECODE']='1'
try:
    command=[JAVAC,'--add-modules','jdk.jdi','-d',classes,*sorted(BASE.glob('*.java'))];record['commands']['compile']=list(map(str,command));r=run_bounded(command,OUT,'compile',timeout=120,limit=2097152,env=env);record['compile_exit']=r.returncode;require(r.returncode==0,'helper compile');record['helper_before_sha256']={p.name:sha(p)for p in classes.glob('*.class')};write(OUT/'helper-Code.json',{p.name:inspect_class(p.read_bytes())for p in classes.glob('*.class')})
    for mode in['ordinary','debugger']:
        directory=OUT/mode;directory.mkdir();cp=str(classes)
        if mode=='ordinary':command=[JAVA,'-Xint','-Xverify:all','-Xmx256m','--add-opens=java.base/java.io=ALL-UNNAMED','--add-opens=java.base/java.util.zip=ALL-UNNAMED','-Xlog:class+init=info:file=class-init.log','-Xlog:library=info:file=library-load.log','-cp',cp,'RegionOutputTarget',directory,jar]
        else:command=[JAVA,'--add-modules','jdk.jdi','-cp',cp,'RegionOutputJdi',directory,JDK,jar,cp]
        record['commands'][mode]=list(map(str,command));print('START',mode,flush=True);start=time.monotonic();r=run_bounded(command,directory,'process',timeout=300,limit=8388608,env=env);record[mode+'_exit']=r.returncode;record[mode+'_seconds']=time.monotonic()-start;require(r.returncode==0 and r.stdout==b'',mode+' complete exit0/empty stdout');stderr=r.stderr if mode=='ordinary'else(directory/'target.stderr').read_bytes();require(stderr.startswith(b'java.io.IOException:')and stderr.count(b'java.io.IOException:')==1 and b'qj.a('in stderr,'one genuine caught old closed-RAF diagnostic');
        if mode=='debugger':require(r.stderr==b''and(directory/'target.stdout').read_bytes()==b'','complete debugger/target extra streams')
        record[mode]=audit_target(directory);print('OBSERVED',mode,round(record[mode+'_seconds'],2),flush=True)
    for phase in map(str,range(6)):
        a=dict(record['ordinary']['normal'][phase]);b=dict(record['debugger']['normal'][phase]);
        for row in[a,b]:row.pop('timestamp');row.pop('clock_interval')
        require(a==b,'independent VMs exact stable allocation/free/growth/payload/owner relations')
    events=lines(OUT/'debugger/checkpoints.ndjson');pins=[e for e in events if e['event']=='code-pin'];record['jdi_events']=len(events);record['verified_observed_code_pins']=len(pins)
    for pin in pins:
        if pin['native']or pin['abstract']:continue
        methods=[m for m in authority[pin['class']]['methods']if m['name']==pin['method']and m['descriptor']==pin['descriptor']];require(len(methods)==1 and methods[0]['code_sha256']==pin['sha256']and methods[0]['code_bytes']==pin['bytes'],'actual observed original/JDK Code identity')
    entries=[e for e in events if e['event']=='entry'and e['location']['class']=='qj'and e['location']['descriptor']=='(II[BI)V'];exits=[e for e in events if e['event']=='return'and e['location']['class']=='qj'and e['location']['descriptor']=='(II[BI)V'];require(len(entries)==len(exits)==7 and[e['phase']for e in entries]==[0,1,2,3,4,5,9],'six normal commits, no repeated/abandoned commit, one actual closed-RAF commit');require(all(e['location']['pc']==444 for e in exits),'original returns including caught commit');failure=[e for e in events if e['event']=='exception'and e['phase']==9];require(any(e['exception']['class']=='java.io.IOException'and e['catch']['class']=='qj'and e['catch']['pc']==437 for e in failure),'genuine old RAF IOException catches at qj437')
    prefixes={str(p):[e['location']['pc']for e in events if e['event']=='source-prefix'and e['phase']==p and e['location']['class']=='qj'and e['location']['descriptor']=='(II[BI)V']for p in range(10)};record['qj_commit_prefixes']=prefixes
    for p in[0,2,5]:require(403 in prefixes[str(p)]and 417 in prefixes[str(p)]and prefixes[str(p)].index(403)<prefixes[str(p)].index(417),'append payload precedes offset header')
    for p in[3,4]:require(261 in prefixes[str(p)]and 303 in prefixes[str(p)]and prefixes[str(p)].index(261)<prefixes[str(p)].index(303),'reuse offset precedes payload')
    require(74 in prefixes['1']and 437 in prefixes['9']and 441 in prefixes['9']and 431 not in prefixes['9'],'same-size rewrite and actual caught prefix without timestamp');record['status']='OBSERVED_AND_AUDITED_ORIGINAL_SOURCE_REGION_OUTPUT_COHORT'
except BaseException as e:record['error']=repr(e)
finally:
    record['input_after_sha256']={str(p):sha(p)for p in inputs};record['helper_after_sha256']={p.name:sha(p)for p in classes.glob('*.class')};record['inputs_unchanged']=before==record['input_after_sha256']and record.get('helper_before_sha256',{})==record['helper_after_sha256']
    if not record['inputs_unchanged']:record['status']='FAIL'
    record['artifact_sha256']={str(p.relative_to(OUT)):sha(p)for p in OUT.rglob('*')if p.is_file()and p.name!='receipt.json'};write(OUT/'receipt.json',record);print(json.dumps({'retained_work':str(OUT),**{k:record.get(k)for k in['status','error','compile_exit','ordinary_exit','debugger_exit','jdi_events','verified_observed_code_pins','inputs_unchanged']}}),flush=True)
require(record['status']=='OBSERVED_AND_AUDITED_ORIGINAL_SOURCE_REGION_OUTPUT_COHORT','original cohort qualification failed')
