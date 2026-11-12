"""UNRUN source controller: real ij/as and descriptor/GZIP/native projection.
No old capture is an input. Every run requires a new owned work subdirectory.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile
import zipfile
from stream_cases import CASES,authored_gzip
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
CAPTURE=ROOT/'tests/reference/client/notice/capture.py'
CLASSFILE=ROOT/'tests/reference/client/notice/classfile.py'
CAPTURE_SHA='4355d5e1f253355d239c6938762f05235b47059b4c3e8beb6e8306b4b34381e2'
AMBIENT=('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH')

def require(ok,message):
    if not ok:raise RuntimeError(message)
def pin(path):
    b=Path(path).read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def module(path,name):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value

def validate_value(value):
    at=0
    def take(pattern):
        nonlocal at
        m=re.match(pattern,value[at:]);require(m is not None,'exact value grammar at '+str(at));at+=len(m[0]);return m[0]
    def item(depth):
        nonlocal at
        require(depth<=256,'bounded projection depth')
        kind=take(r'[EBSIJFDATRL C]'.replace(' ',''))
        if kind=='E':return
        take(':')
        if kind in 'BSIJ':
            text=take(r'-?(?:0|[1-9][0-9]*)');n=int(text);bits={'B':8,'S':16,'I':32,'J':64}[kind];require(-(1<<(bits-1))<=n<(1<<(bits-1)),'scalar width');require(text!='-0','canonical integer');return
        if kind in 'FD':take(r'[0-9a-f]{'+str(8 if kind=='F' else 16)+'}');return
        if kind in 'ATR':
            text=take(r'[0-9a-f]*');require(len(text)%(2 if kind in 'AR' else 4)==0,'exact array/String units');return
        if kind=='L':
            text=take(r'-?(?:0|[1-9][0-9]*)');n=int(text);require(text!='-0' and -128<=n<=127,'canonical list byte width');take(r'\[')
            while at<len(value) and value[at]!=']':item(depth+1);take(';')
            take(r'\]');return
        take(r'\{');prior=None
        while at<len(value) and value[at]!='}':
            key=take(r'[0-9a-f]*');require(len(key)%4==0 and (prior is None or prior<key),'sorted unique UTF16 compound keys');prior=key;take('=');item(depth+1);take(';')
        take(r'\}')
    item(0);require(at==len(value),'no trailing value bytes')
def parse_native(data):
    require(len(data)<=1048576 and b'\x00' not in data,'bounded non-NUL native protocol')
    match=re.fullmatch(rb'([^\r\n]*)\r?\n',data);require(match is not None,'exact one terminated native result row')
    parts=match[1].decode('ascii').split('\t')
    require(len(parts)==4 and parts[0]=='RESULT' and parts[1] in ('OK','THROW') and parts[3] in ('OPEN','CLOSED'),'exact result fields')
    require(parts[2]!='' and all(32<=ord(c)<=126 for c in parts[2]),'nonempty ASCII result value')
    if parts[1]=='THROW':require(parts[2] in ('java.io.EOFException','java.io.UTFDataFormatException','java.lang.NegativeArraySizeException','java.lang.NullPointerException','java.util.zip.ZipException','java.io.IOException'),'exact reached source exception inventory')
    else:validate_value(parts[2])
    return tuple(parts)
def parse_original(data,jar):
    rows=data.decode('ascii').splitlines()
    require(len(rows)==4 and rows[:2]==['ORIGIN\tij\t'+jar.as_uri(),'ORIGIN\tas\t'+jar.as_uri()] and rows[-1]=='END\t13','exact helper origin/result/completion inventory')
    return parse_native((rows[2]+'\n').encode('ascii'))
def completed(capture,result,root,stem):
    capture.require_success(result,empty_stderr=True)
    require(result['reader_errors']=={} and result['pipe_capture_complete'],'explicit complete empty-reader-error capture')
    return (root/(stem+'.stdout')).read_bytes()
def bindings(jar,inspect,authority):
    evidence={}
    with zipfile.ZipFile(jar) as z:
        for name,expected in authority['classes'].items():
            actual=inspect(z.read(name+'.class'))
            require(actual['class_sha256']==expected['class_sha256'] and actual['class_bytes']==expected['class_bytes'],'actual game class '+name)
            methods={(m['name'],m['descriptor']):m for m in actual['methods']}
            for selected in expected['methods']:
                m=methods[(selected['name'],selected['descriptor'])]
                for key in ('access','code_bytes','code_sha256','code_attribute_sha256','max_stack','max_locals'):
                    if key in selected:require(m.get(key)==selected[key],'actual game Code '+name+' '+selected['name']+' '+key)
            evidence[name]={'class':pin_bytes(z.read(name+'.class')),'selected_methods':len(expected['methods'])}
    return evidence
def jdk_bindings(java,javac,inspect):
    require(java.parent==javac.parent,'one explicit JDK tool home')
    jmod=java.parent.parent/'jmods/java.base.jmod';require(jmod.is_file(),'actual explicit JDK java.base.jmod')
    authority=json.loads((HERE/'jdk-stream-code-pins.json').read_text(encoding='utf-8'));evidence={}
    with zipfile.ZipFile(jmod) as z:
        for name,expected in authority['classes'].items():
            raw=z.read('classes/'+name+'.class');actual=inspect(raw);methods={(m['name'],m['descriptor']):m for m in actual['methods']}
            for selected in expected['methods']:
                m=methods[(selected['name'],selected['descriptor'])]
                for key in ('access','code_bytes','code_sha256','code_attribute_sha256','max_stack','max_locals'):
                    if key in selected:require(m.get(key)==selected[key],'actual JDK reached Code '+name+' '+selected['name']+' '+key)
            evidence[name]={'actual_class':pin_bytes(raw),'selected_methods':len(expected['methods'])}
    return jmod,{'jmod':pin(jmod),'classes':evidence}
def pin_bytes(data):return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
def environment():
    require(not any(os.environ.get(k) for k in AMBIENT),'reject ambient Java/classpath options')
    env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1')
    for k in AMBIENT:env.pop(k,None)
    return env

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for arg in ('client-jar','native','java','javac','work-dir'):parser.add_argument('--'+arg,type=Path,required=True)
    args=parser.parse_args();env=environment()
    paths={key:getattr(args,key).resolve(strict=True) for key in ('client_jar','native','java','javac')}
    for key,path in paths.items():require(path.is_file() and not path.is_symlink(),'real input '+key)
    args.work_dir.mkdir(parents=True,exist_ok=True);parent=args.work_dir.resolve(strict=True);require(not parent.is_symlink(),'owned work parent')
    root=Path(tempfile.mkdtemp(prefix='source-stream-original-',dir=parent)).resolve();os.chdir(root)
    authority=json.loads((HERE/'stream-code-pins.json').read_text(encoding='utf-8'))
    require(pin(paths['client_jar'])==authority['client_jar'],'exact supplied client JAR')
    require(pin(CAPTURE)['sha256']==CAPTURE_SHA,'reviewed complete process-capture helper')
    capture=module(CAPTURE,'stream_capture');inspect=module(CLASSFILE,'stream_classfile').inspect_class
    source_paths=[HERE/'StreamObservation.java',HERE/'compare_stream.py',HERE/'stream_cases.py',HERE/'stream-code-pins.json',HERE/'jdk-stream-code-pins.json',HERE/'stream-provenance.json',CAPTURE,CLASSFILE]
    provenance=json.loads((HERE/'stream-provenance.json').read_text(encoding='utf-8'))
    for relative,expected in provenance['native_sources'].items():
        path=ROOT/relative;require(pin(path)==expected,'exact current native source '+relative);source_paths.append(path)
    for name,expected in provenance['reference_sources'].items():require(pin(HERE/name)==expected,'exact authored reference source '+name)
    for relative,expected in provenance['helper_inputs'].items():require(pin(ROOT/relative)==expected,'exact shared helper '+relative)
    before={str(p):pin(p) for p in [*source_paths,*paths.values()]}
    report={'status':'RUNNING','scope':'actual CLIENT ij/as CPU only; no hi handler/Store/game startup/Java-Native entropy claim','root':str(root),'inputs_before':before,'case_inventory':[(n,m,v) for n,m,_,v in CASES],'processes':[]}
    try:
        report['class_Code']=bindings(paths['client_jar'],inspect,authority)
        jmod,report['JDK_Code']=jdk_bindings(paths['java'],paths['javac'],inspect)
        jmod_before=pin(jmod)
        for tool,flag in [('java','-version'),('javac','-version')]:
            r=capture.capture([str(paths[tool]),flag],root,tool+'-version',30,env);report['processes'].append(r)
            # Version probes intentionally print their real stderr; they are
            # separately validated, never passed off as original empty stderr.
            require(r['exit']==0 and r['reader_errors']=={} and r['pipe_capture_complete'] and not r['timed_out'] and not r['output_overflow'],'completed tool version')
            text=(root/(tool+'-version.stdout')).read_bytes()+(root/(tool+'-version.stderr')).read_bytes();require(re.search(rb'(?:version\s+"|javac\s+)17\.',text) is not None,'explicit JDK17 '+tool)
        classes=root/'classes';classes.mkdir()
        r=capture.capture([str(paths['javac']),'-encoding','UTF-8','-d',str(classes),'-classpath',str(classes),str(HERE/'StreamObservation.java')],root,'javac',60,env);report['processes'].append(r);completed(capture,r,root,'javac')
        require(sorted(p.relative_to(classes).as_posix() for p in classes.rglob('*') if p.is_file())==['StreamObservation.class'],'exact helper-only generated class inventory')
        helper_before=pin(classes/'StreamObservation.class');report['helper_before']=helper_before
        results=[]
        for index,(name,mode,payload,variant) in enumerate(CASES):
            fixture=root/(name+'.gz');fixture.write_bytes(authored_gzip(payload,variant));fixture_before=pin(fixture);stem=f'case-{index:02d}'
            command=[str(paths['java']),'-Xint','-Xverify:all','-Xmx256m','-Xlog:class+init=info:file='+stem+'-class-init.log','-cp',str(classes),'StreamObservation',str(paths['client_jar']),mode,str(fixture)]
            r=capture.capture(command,root,stem+'-original',60,env);report['processes'].append(r);original=parse_original(completed(capture,r,root,stem+'-original'),paths['client_jar'])
            log_path=root/(stem+'-class-init.log');require(log_path.stat().st_size<=4*1024*1024,'complete bounded class-init log');log=log_path.read_text(encoding='utf-8')
            # Exact initialized supplied-game inventory: excluded client/device/
            # Registry/World owners must never initialize in this CPU profile.
            allowed=set(authority['classes'])
            with zipfile.ZipFile(paths['client_jar']) as z:game_names={n[:-6] for n in z.namelist() if n.endswith('.class')}
            initialized=[]
            for line in log.splitlines():
                match=re.search(r"Initializing '([^']+)'",line)
                if match and match[1] in game_names:initialized.append(match[1]);require(match[1] in allowed,'excluded real game class initialized '+match[1])
            r=capture.capture([str(paths['native']),mode,str(fixture)],root,stem+'-native',60,env);report['processes'].append(r);native=parse_native(completed(capture,r,root,stem+'-native'))
            require(native==original,'ordered actual result/state mismatch '+name);require(pin(fixture)==fixture_before,'fixture bytes unchanged')
            results.append({'name':name,'mode':mode,'variant':variant,'fixture':fixture_before,'original':original,'native':native,'initialized_game_classes':initialized,'class_init':pin(root/(stem+'-class-init.log'))})
        report['results']=results;require(pin(classes/'StreamObservation.class')==helper_before,'helper class unchanged')
        require({str(p):pin(p) for p in [*source_paths,*paths.values()]}==before,'source/tool/native/JAR before-after exact')
        require(pin(jmod)==jmod_before,'JDK class authority unchanged');report['status']='PASS'
    finally:
        report['inputs_after']={str(p):pin(p) for p in [*source_paths,*paths.values()]};(root/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n');print('Source stream artifacts: '+str(root))
    print('PASS: '+str(len(CASES))+' actual source-stream rows')
if __name__=='__main__':main()
