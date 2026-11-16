"""Capture two bounded ordinary original virtual Block.h leaf cohorts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import sys
import zipfile
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r);return r
C=module('piston_mobility_transport',ROOT/'tests/reference/client/controller/capture.py')
I=module('piston_mobility_classfile',ROOT/'tests/reference/client/controller/classfile.py')
def write(path,v):path.write_text(json.dumps(v,indent=2)+'\n',encoding='utf8',newline='\n')
def pin(path):return {'bytes':path.stat().st_size,'sha256':C.digest(path)}
def selected(jar,jdk):
    suffix='.exe' if os.name=='nt' else ''
    files={'original-jar':jar,'java':jdk/'bin'/('java'+suffix),'javac':jdk/'bin'/('javac'+suffix),'jdk-modules':jdk/'lib/modules','jdk-release':jdk/'release'}
    for n in ('PistonMobilityObservation.java','cases.json','capture.py'):files['helper/'+n]=HERE/n
    for n in ('capture.py','classfile.py'):files['support/'+n]=ROOT/'tests/reference/client/controller'/n
    for p in sorted((jdk/'bin').glob('*.dll'))+sorted((jdk/'bin/server').glob('*.dll')):files['jdk/'+p.relative_to(jdk).as_posix()]=p
    return files
def command(java,classes,jar,mode):
    return [str(java),*(['-Xint'] if mode=='interpreter' else []),'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow','-cp',str(classes),'PistonMobilityObservation',str(jar)]
def check_transport(output,r,expected):
    C.ensure(r['command']==[str(v) for v in expected],'exact executed child command')
    C.ensure(type(r['exit']) is int and r['exit']==0 and r['timed_out'] is False and r['pipe_capture_complete'] is True and r['output_overflow'] is False and r['reader_errors']=={},'complete bounded successful child')
    for s in ('stdout','stderr'):
        C.ensure(r[s+'_file']==Path(r[s+'_file']).name,'local stream file')
        C.ensure(pin(output/r[s+'_file'])=={'bytes':r[s+'_bytes'],'sha256':r[s+'_sha256']},'actual stream count/hash')
def stdout(raw):
    lines=raw.decode('utf8').splitlines();C.ensure(lines[:2]==['16 achievements','151 recipes'],'exact genuine class-init banners')
    parsed=[]
    for line in lines[2:]:
        prefix,payload=line.split(' ',1);parsed.append((prefix,json.loads(payload,object_pairs_hook=unique)))
    C.ensure(len(parsed)>=3 and parsed[0][0]=='PISTON_MOBILITY_ORIGINS' and parsed[-1][0]=='PISTON_MOBILITY_COMPLETE' and all(p=='PISTON_MOBILITY_ROW' for p,v in parsed[1:-1]),'complete ordered leaf stream')
    return {'origins':parsed[0][1],'rows':[v for p,v in parsed[1:-1]],'complete':parsed[-1][1]}
def unique(pairs):
    r={}
    for k,v in pairs:
        C.ensure(k not in r,'duplicate JSON key');r[k]=v
    return r
def stderr(raw,classes):
    uri=classes.as_uri().replace('file:///','file:/')+'/'
    expected=['WARNING: A terminally deprecated method in java.lang.System has been called','WARNING: System::setSecurityManager has been called by PistonMobilityObservation ('+uri+')','WARNING: Please consider reporting this to the maintainers of PistonMobilityObservation','WARNING: System::setSecurityManager will be removed in a future release']
    C.ensure(raw.decode('utf8').splitlines()==expected,'exact retained JDK17 guard warnings, no other failure')
def audit_private(output,report):
    jar,jdk=Path(report['jar']),Path(report['jdk']);files=selected(jar,jdk)
    C.ensure({k:pin(p) for k,p in files.items()}==report['inputs_before']==report['inputs_after'],'current selected physical files match both captures')
    classes=output/'classes';products={p.name:pin(p) for p in classes.glob('*.class')}
    C.ensure(products==report['products_before']==report['products_after'] and products,'actual compiled products unchanged')
    suffix='.exe' if os.name=='nt' else '';java=jdk/'bin'/('java'+suffix);javac=jdk/'bin'/('javac'+suffix)
    check_transport(output,report['compile'],[javac,'-proc:none','-encoding','UTF-8','-d',classes,HERE/'PistonMobilityObservation.java'])
    C.ensure(report['compile']['stdout_bytes']==report['compile']['stderr_bytes']==0,'clean authored helper compilation')
    decoded={}
    for mode in ('interpreter','default'):
        check_transport(output,report[mode],command(java,classes,jar,mode));stderr((output/report[mode]['stderr_file']).read_bytes(),classes)
        decoded[mode]=stdout((output/report[mode]['stdout_file']).read_bytes())
    C.ensure(decoded['interpreter']==decoded['default']==report['finite'],'both streams reparsed and report projection exact')
    C.ensure(report['removed_environment']==['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'],'explicit removed JVM environment')
    with zipfile.ZipFile(jar) as z:
        pins={n:I.inspect_class(z.read(n+'.class')) for n in decoded['interpreter']['origins']['classes']}
    C.ensure(pins==report['class_pins'],'physical original archive Class/Code projection unchanged')
    return decoded['interpreter']
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--jar',type=Path,required=True);p.add_argument('--jdk',type=Path,required=True);a=p.parse_args()
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False);classes=output/'classes';classes.mkdir();jar=a.jar.resolve(strict=True);jdk=a.jdk.resolve(strict=True)
    C.ensure(C.digest(jar)==JAR,'verified untouched original client JAR');files=selected(jar,jdk);before={k:pin(v) for k,v in files.items()}
    env=os.environ.copy();removed=['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS']
    for k in removed:env.pop(k,None)
    suffix='.exe' if os.name=='nt' else '';java=jdk/'bin'/('java'+suffix);javac=jdk/'bin'/('javac'+suffix)
    report={'schema':'PRIVATE_PISTON_MOBILITY_CAPTURE_V1','jar':str(jar),'jdk':str(jdk),'inputs_before':before,'removed_environment':removed,'host':{'os':platform.platform(),'python':platform.python_version()}}
    try:
        report['compile']=C.capture([javac,'-proc:none','-encoding','UTF-8','-d',classes,HERE/'PistonMobilityObservation.java'],output,'compile',60,env);check_transport(output,report['compile'],report['compile']['command'])
        report['products_before']={p.name:pin(p) for p in classes.glob('*.class')}
        for mode in ('interpreter','default'):
            report[mode]=C.capture(command(java,classes,jar,mode),output,mode,60,env);check_transport(output,report[mode],command(java,classes,jar,mode));stderr((output/report[mode]['stderr_file']).read_bytes(),classes)
        report['finite']=stdout((output/'interpreter.stdout').read_bytes())
        with zipfile.ZipFile(jar) as z:report['class_pins']={n:I.inspect_class(z.read(n+'.class')) for n in report['finite']['origins']['classes']}
        report['inputs_after']={k:pin(v) for k,v in files.items()};report['products_after']={p.name:pin(p) for p in classes.glob('*.class')}
        audit_private(output,report);report['status']='PASS'
    except BaseException as e:
        report['status']='FAIL';report['error']={'type':type(e).__name__,'message':str(e)};raise
    finally:write(output/'capture.json',report)
    print('PASS: two fresh ordinary original Block.h cohorts; no World, tick, or native execution')
if __name__=='__main__':main()
