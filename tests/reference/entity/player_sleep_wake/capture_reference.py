"""Fresh unchanged-original component reproduction, with all raw output private."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import zipfile
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE))
import packet as P
import validate as V

LIBRARIES={
 'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':'sdk/lwjgl-2.9.0.jar',
 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':'sdk/lwjgl_util-2.9.0.jar',
 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':'sdk/jinput-2.0.5.jar',
 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':'sdk/jutils-1.0.0.jar'}

def private_output(path):
    """Reject public/foreign/existing destinations before any mkdir or execution."""
    out=Path(path).resolve();private=(ROOT/'.local').resolve()
    V.require(out.is_relative_to(private) and out!=private,'all raw/canonical attempt output must be a new child of workspace .local')
    V.require(not out.exists(),'refuse to overwrite any existing attempt')
    return out

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v);return v

def inventory(root):return {p.relative_to(root).as_posix():V.digest(p) for p in sorted(root.rglob('*')) if p.is_file()}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--jar',type=Path,required=True)
    parser.add_argument('--jdk',type=Path,required=True)
    parser.add_argument('--libraries-root',type=Path,required=True)
    a=parser.parse_args();out=private_output(a.output)
    reference=P.read_reference();proof=V.load(HERE/'provenance.json')
    jar=a.jar.resolve(strict=True);jdk=a.jdk.resolve(strict=True)
    java=(jdk/'bin/java.exe').resolve(strict=True);javac=(jdk/'bin/javac.exe').resolve(strict=True)
    libs=[(a.libraries_root/n).resolve(strict=True) for n in LIBRARIES]
    tools={'jdk/bin/java.exe':java,'jdk/bin/javac.exe':javac,**{role:path for role,path in zip(LIBRARIES.values(),libs)}}
    V.require(V.digest(jar)==V.JAR and all(V.digest(path)==proof['binary_input_sha256'][role] for role,path in tools.items()),'actual historically pinned Windows JDK executables and imported SDK archives')
    support={'support/capture.py':ROOT/'tests/reference/client/controller/capture.py','support/classfile.py':ROOT/'tests/reference/entity/player/classfile.py'}
    V.require(all(V.digest(path)==proof['executed_producer_sha256'][role] for role,path in support.items()),'unchanged executed source support')
    C=module('sleep_wake_bounded_capture',support['support/capture.py']);F=module('sleep_wake_classfile',support['support/classfile.py'])
    pins=V.load(HERE/'source-pins.json')
    with zipfile.ZipFile(jar) as z:
        V.require(not any(n.startswith('BedNight') for n in z.namelist()),'helper class collision')
        for name,pin in pins['classes'].items():V.require(V.exact(F.parse_class(z.read(name+'.class')),pin),'original class and Code '+name)
    out.mkdir(parents=True,exist_ok=False);classes=out/'classes';classes.mkdir();frozen=out/'producer';frozen.mkdir()
    sources=[HERE/n for n in (*P.DATA,*P.SOURCES,'provenance.json')]
    inputs=[jar,*tools.values(),*support.values(),*sources]
    before={str(n):V.digest(n) for n in inputs}
    for n in sources:(frozen/n.name).write_bytes(n.read_bytes())
    env=os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(name,None)
    report={'status':'FAIL','input_before_sha256':before,'scope':'NEW reproduction of selected original components; does not revise historical proof or confer native/tick/GUI qualification.'};helpers=None
    try:
        for name,tool in (('java',java),('javac',javac)):
            r=C.capture([str(tool),'-version'],out,name,15,env);report[name]=r;C.require_success(r)
            text=''.join((out/r[k]).read_text() for k in ('stdout_file','stderr_file'));V.require('17.0.17' in text,'historical JDK17.0.17 version')
        cp=os.pathsep.join(map(str,[classes,jar,*libs]));r=C.capture([str(javac),'--add-modules','jdk.jdi','-encoding','UTF-8','-classpath',cp,'-d',str(classes),str(HERE/'BedNightObservation.java'),str(HERE/'BedNightJdi.java')],out,'compile',30,env);report['compile']=r;C.require_success(r,empty_stderr=True)
        helpers=inventory(classes);V.require(V.exact(helpers,proof['helper_sha256']),'same executed compiled helper identities');report['helper_before_sha256']=helpers
        projections=[];runs=[];pinned=dict(pins['classes']);pinned.update({p.stem:F.parse_class(p.read_bytes()) for p in classes.glob('*.class')})
        for index in range(2):
            folder=out/f'vm-{index}'
            r=C.capture([str(java),'--add-modules','jdk.jdi','-Xmx256m','-classpath',cp,'BedNightJdi',str(jdk),cp,str(jar),str(HERE/'profiles.tsv'),str(HERE/'class-names.txt'),str(folder),'76'],out,f'controller-{index}',240,env)
            runs.append(r);report['original']=runs;C.require_success(r,empty_stderr=True)
            text=(folder/'target.stdout').read_text(encoding='utf-8');V.source_bindings(text,pins);raw=V.raw_rows(text)
            journal=(folder/'journal.jsonl').read_text(encoding='utf-8');clean,code=V.journal(journal)
            for record in (V.loads(line) for line in journal.splitlines()):
                if record['event']=='journal':V.audit_journal(record,pinned,raw=True)
            projection=V.project(raw,clean,code);V.validate_canonical(projection);projections.append(projection)
        V.require(V.exact(projections[0],projections[1]) and V.exact(projections[0],reference),'two fresh original selected effects agree with frozen reference')
        (out/'finite.json').write_text(json.dumps(projections[0],indent=2)+'\n',encoding='utf-8',newline='\n')
        report.update(status='PASS_ORIGINAL_COMPONENTS_ONLY',finite_sha256=V.digest(out/'finite.json'),profiles_per_vm=76)
    finally:
        report['input_after_sha256']={str(n):V.digest(n) for n in inputs};report['helper_after_sha256']=inventory(classes)
        unchanged=before==report['input_after_sha256'] and (helpers is None or helpers==report['helper_after_sha256'])
        if not unchanged:report['status']='FAIL'
        (out/'capture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
        V.require(unchanged,'current reproduction inputs/helper unchanged')
    print('New private original component reproduction:',out/'capture.json')

if __name__=='__main__':main()
