"""Fresh bounded unchanged-original GS/CW capture from portable local inputs."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SOURCES=('PlayerSleepObservation.java','PlayerSleepJdi.java','prepare.py','capture_reference.py',
         'validate.py','packet.py','test_packet.py','profiles.json','profiles.tsv','source-pins.json',
         'class-names.txt','README.md')


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


C=module('sleep_bounded_capture',ROOT/'tests/reference/client/controller/capture.py')
CF=module('sleep_classfile',ROOT/'tests/reference/entity/player/classfile.py')
SDK=module('sleep_sdk_pins',ROOT/'tests/reference/client/picking/observe.py')
V=module('sleep_source_validation',HERE/'validate.py')


def inventory(path):
    return {p.relative_to(path).as_posix():C.digest(p) for p in sorted(path.rglob('*')) if p.is_file()}


def private_output(path):
    out=Path(path).resolve()
    V.require(out.is_relative_to(ROOT/'.local') and out!=ROOT/'.local',
              'all raw attempt outputs must remain under workspace .local')
    return out


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--jar',type=Path,default=ROOT/'b1.7.3.jar')
    p.add_argument('--libraries-root',type=Path,default=Path(os.environ.get('APPDATA',''))/'.minecraft/libraries')
    p.add_argument('--java',type=Path,default=Path(shutil.which('java')))
    p.add_argument('--javac',type=Path,default=Path(shutil.which('javac')))
    a=p.parse_args()
    out=private_output(a.output)
    out.mkdir(parents=True,exist_ok=False)
    java,javac,jar=[n.resolve(strict=True) for n in (a.java,a.javac,a.jar)]
    libraries=[(a.libraries_root/n).resolve(strict=True) for n in SDK.LIBRARIES]
    V.require(C.digest(jar)==JAR and all(C.digest(n)==h for n,h in zip(libraries,SDK.LIBRARIES.values())),
              'unchanged original archive and SDK inputs')
    pins=V.load(HERE/'source-pins.json')
    V.require(pins['archive_sha256']==JAR,'original pin namespace')
    with zipfile.ZipFile(jar) as z:
        for name,expected in pins['classes'].items():
            actual=CF.parse_class(z.read(name+'.class'));actual.pop('fields')
            selected={(m['name'],m['descriptor']) for m in expected['methods']}
            actual['methods']=[m for m in actual['methods'] if (m['name'],m['descriptor']) in selected]
            V.require(actual==expected,'whole original class/Code pin '+name)
        V.require(not any(n.startswith('PlayerSleep') for n in z.namelist()),'no helper class collision')
    support=[ROOT/n for n in ('tests/reference/client/controller/capture.py',
        'tests/reference/entity/player/classfile.py','tests/reference/client/picking/observe.py')]
    jdk=java.parent.parent
    vm=jdk/('bin/server/jvm.dll' if os.name=='nt' else 'lib/server/libjvm.so')
    inputs=[java,javac,jar,*libraries,*[HERE/n for n in SOURCES],*support,jdk/'lib/modules',vm,
            *java.parent.glob('*.dll')]
    before={str(n):C.digest(n) for n in inputs}
    report={'status':'FAIL','input_before_sha256':before,
            'scope':'Prepared GS.b early returns, null faults and pre-resize119 snapshots; later completions segregated and uncredited.'}
    classes=out/'classes';classes.mkdir();frozen=out/'frozen';frozen.mkdir()
    for n in SOURCES:shutil.copy2(HERE/n,frozen/n)
    env=os.environ.copy()
    for n in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(n,None)
    helper=None
    try:
        for name,tool in (('java',java),('javac',javac)):
            r=C.capture([str(tool),'-version'],out,name,10,env);C.require_success(r)
            version=(out/r['stdout_file']).read_text()+(out/r['stderr_file']).read_text()
            V.require('17.0.17' in version,'qualified JDK17.0.17')
            report[name+'_version']=r
        r=C.capture([str(javac),'--add-modules','jdk.jdi','-encoding','UTF-8','-d',str(classes),
                     str(HERE/'PlayerSleepObservation.java'),str(HERE/'PlayerSleepJdi.java')],out,'compile',30,env)
        C.require_success(r,empty_stderr=True);report['compile']=r
        helper=inventory(classes)
        V.require(set(helper)=={'PlayerSleepObservation.class','PlayerSleepObservation$Guard.class','PlayerSleepJdi.class'},'authored helper inventory')
        report['helper_before_sha256']=helper
        runs=[]
        for i in range(2):
            work=out/f'work-{i}';target=out/f'vm-{i}'
            command=[str(java),'--add-modules','jdk.jdi','-Xmx256m','-classpath',str(classes),'PlayerSleepJdi',
                     str(jar),str(work),str(HERE/'profiles.tsv'),str(HERE/'class-names.txt'),str(work/'bindings.tsv'),
                     os.pathsep.join(map(str,libraries)),str(target),str(jdk),str(classes),'48']
            r=C.capture(command,out,f'controller-{i}',240,env)
            runs.append(r);report['original']=runs;C.require_success(r,empty_stderr=True)
            V.require((target/'target.stdout').read_text().splitlines()[-1]==
                      'Original Player sleep: 48 profiles; network=0; native=0; rejected_writes=0','target completion/effect counters')
            expected=[str(case)+'\t'+name+'\t'+str(value['class_bytes'])+'\t'+value['class_sha256']
                      for case in range(48) for name,value in pins['classes'].items()]
            V.require((work/'bindings.tsv').read_text().splitlines()==expected,'every isolated original class origin/bytes')
        report['profiles_per_vm']=48;report['status']='PASS'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)}
        raise
    finally:
        report['input_after_sha256']={str(n):C.digest(n) for n in inputs}
        report['helper_after_sha256']=inventory(classes)
        unchanged=report['input_after_sha256']==before and (helper is None or report['helper_after_sha256']==helper)
        if not unchanged:report['status']='FAIL'
        (out/'capture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        V.require(unchanged,'unchanged source/tool/dependency/helper inputs')
    projection=V.capture(out)
    V.validate_data(V.load(HERE/'finite.json'),projection)
    (out/'finite.json').write_text(json.dumps(projection,indent=2)+'\n',encoding='utf-8',newline='\n')
    print('Player sleep fresh public-source capture:',out)


if __name__=='__main__':main()
