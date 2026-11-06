"""Three fresh guarded original VMs: two ordinary and one read-only JDI."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import time
import traceback
import zipfile

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
SUPPORT=HERE.parents[1]/'client/controller'


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result


C=module('torch_particle_capture',SUPPORT/'capture.py')
I=module('torch_particle_classfile',SUPPORT/'classfile.py')
A=module('torch_particle_audit',HERE/'audit.py')
P=module('torch_particle_comparison',HERE/'compare.py')
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES={
 'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
CLASSES=('xn','qu','xw','sn','nw','fd','eq','in','lm','ok','wy','xa')
PRODUCERS=('TorchParticleObservation.java','TorchParticleJdi.java','profiles.tsv','declared-inputs.json',
           'capture_reference.py','audit.py','compare.py')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar',type=Path,required=True)
    parser.add_argument('--libraries',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();jar=args.jar.resolve(strict=True)
    C.ensure(C.digest(jar)==JAR,'verified original client archive')
    libraries=[(args.libraries/name).resolve(strict=True) for name in LIBRARIES]
    C.ensure(all(C.digest(p)==h for p,h in zip(libraries,LIBRARIES.values())),'pinned original dependencies')
    java,javac=Path(shutil.which('java')).resolve(),Path(shutil.which('javac')).resolve()
    C.ensure(java.parent==javac.parent,'single original-observer JDK installation')
    jvm=java.parent/'server/jvm.dll' if os.name=='nt' else java.parent.parent/'lib/server/libjvm.so'
    helper=HERE/'../mob_tick/ConstructorJson.java'
    inputs=[java,javac,java.parent.parent/'lib/modules',jvm,jar,*libraries,
            *[HERE/name for name in PRODUCERS],helper,SUPPORT/'capture.py',SUPPORT/'classfile.py']
    before={str(p.resolve()):C.digest(p) for p in inputs}
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    classes=output/'classes';classes.mkdir()
    report={'status':'FAIL','scope':__doc__,'inputs_before':before,
            'producers_before':{name:C.digest(HERE/name) for name in PRODUCERS},'ordinary':[]}
    (output/'frozen-inputs.json').write_text(json.dumps(report,indent=2)+'\n')
    start=time.monotonic()
    try:
        pins={'client_jar_sha256':JAR,'classes':{}}
        with zipfile.ZipFile(jar) as archive:
            for name in CLASSES:
                pins['classes'][name]=I.inspect_class(archive.read(name+'.class'))
        report['pins']=pins
        cp=os.pathsep.join(map(str,[classes,jar,*libraries]))
        report['compile']=C.capture([str(javac),'-encoding','UTF-8','--add-modules','jdk.jdi','-classpath',cp,'-d',str(classes),
            str(HERE/'TorchParticleObservation.java'),str(HERE/'TorchParticleJdi.java'),str(helper)],output,'compile',60,os.environ.copy())
        C.require_success(report['compile'],empty_stderr=True)
        report['helpers_before']={p.name:C.digest(p) for p in classes.iterdir()}
        C.ensure(set(report['helpers_before'])=={'TorchParticleObservation.class','TorchParticleObservation$1.class',
            'TorchParticleJdi.class','TorchParticleJdi$Drain.class','ConstructorJson.class'},'only authored helper classes')
        expected=None
        for ordinal in range(2):
            folder=output/('ordinary-'+str(ordinal));folder.mkdir();tick=time.monotonic()
            receipt=C.capture([str(java),'-Xint','-Xverify:all','-Xmx256m','-Djava.awt.headless=true',
                '-Djava.security.manager=allow','--add-opens=java.base/java.util=ALL-UNNAMED',
                '--add-opens=java.base/java.lang=ALL-UNNAMED','-classpath',cp,'TorchParticleObservation',str(jar),str(HERE/'profiles.tsv')],
                folder,'original',120,os.environ.copy(),folder)
            item={'receipt':receipt,'elapsed_seconds':time.monotonic()-tick};report['ordinary'].append(item)
            C.require_success(receipt)
            text=(folder/receipt['stdout_file']).read_text()
            actual=P.components(text)
            if expected is None:expected=actual
            else:P.compare(actual,expected)
            item['entropy']=A.ordinary(P.rows(text,'TORCH_PARTICLE_CONSTRUCTOR'))
            item['components']=actual
        debugger=output/'debugger';debugger.mkdir();tick=time.monotonic()
        report['debugger']=C.capture([str(java),'-Xmx256m','--add-modules','jdk.jdi','-classpath',str(classes),
            'TorchParticleJdi',cp,str(jar),str(java.parent.parent),str(debugger),str(HERE/'profiles.tsv')],
            debugger,'controller',340,os.environ.copy(),debugger)
        report['debugger_elapsed_seconds']=time.monotonic()-tick
        C.require_success(report['debugger'],empty_stderr=True)
        C.ensure(not (debugger/report['debugger']['stdout_file']).read_bytes(),'quiet read-only JDI controller')
        text=(debugger/'target.stdout').read_text();actual=P.components(text);P.compare(actual,expected)
        constructors=[json.loads(line) for line in (debugger/'constructors.jsonl').read_text().splitlines()]
        events=[json.loads(line) for line in (debugger/'events.jsonl').read_text().splitlines()]
        code=[json.loads(line) for line in (debugger/'live-code.jsonl').read_text().splitlines()]
        report['debugger_entropy']=A.debugger(constructors,P.rows(text,'TORCH_PARTICLE_CONSTRUCTOR'),pins)
        report['debugger_trace']=P.trace(events,code,pins,actual)
        report['debugger_artifacts_sha256']={p.name:C.digest(p) for p in debugger.iterdir() if p.is_file()}
        report['status']='PASS'
    except BaseException as error:
        report['failure']={'type':type(error).__name__,'message':str(error),'traceback':traceback.format_exc()}
        raise
    finally:
        report['inputs_after']={str(p.resolve()):C.digest(p) for p in inputs}
        report['producers_after']={name:C.digest(HERE/name) for name in PRODUCERS}
        report['helpers_after']={p.name:C.digest(p) for p in classes.iterdir()}
        report['elapsed_seconds']=time.monotonic()-start
        if report['inputs_after']!=before or report.get('helpers_before',{})!=report['helpers_after']:
            report['status']='FAIL'
        (output/'capture.json').write_text(json.dumps(report,indent=2)+'\n')
    C.ensure(report['status']=='PASS','stable exact original cohort')
    print('Original Torch particles PASS:37 constructors/30 components x3 fresh VMs; '+str(output/'capture.json'))


if __name__=='__main__':main()
