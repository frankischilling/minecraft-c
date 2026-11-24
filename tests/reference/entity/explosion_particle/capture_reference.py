"""Capture genuine original GY constructors, GY ticks and inherited XW CPU drawing."""
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
SUPPORT=HERE.parents[1]/'client/controller'


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value


C=module('explosion_capture_support',SUPPORT/'capture.py')
I=module('explosion_classfile',SUPPORT/'classfile.py')
P=module('explosion_comparison',HERE/'compare.py')
A=module('explosion_entropy_audit',HERE/'audit.py')
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES={
 'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
REQUIRED={
 'gy':{('<init>','(Lfd;DDDDDD)V'),('w_','()V'),('a','(Lnw;FFFFFF)V')},
 'xw':{('<init>','(Lfd;DDDDDD)V'),('a','(Lnw;FFFFFF)V'),('c_','()I')},
 'sn':{('<init>','(Lfd;)V'),('b','(DDD)V'),('b','(FF)V'),('e','(DDD)V'),('K','()V'),('a','(F)F')},
 'nw':{('<init>','(I)V'),('b','()V'),('a','(I)V'),('a','(FFF)V'),('a','(DDDDD)V'),('a','(DDD)V')},
 'eq':{('d','(DDD)Leq;')},
 'fd':{('<init>','(Lwt;Ljava/lang/String;Lxa;J)V')},
 'lm':{('<init>','(Lfd;[BII)V')},'ok':set(),'wy':set()}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar',type=Path,required=True)
    parser.add_argument('--libraries',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();jar=args.jar.resolve(strict=True)
    C.ensure(C.digest(jar)==JAR,'verified original client archive')
    libraries=[(args.libraries/name).resolve(strict=True) for name in LIBRARIES]
    C.ensure(all(C.digest(p)==h for p,h in zip(libraries,LIBRARIES.values())),'pinned reference libraries')
    java,javac=Path(shutil.which('java')).resolve(),Path(shutil.which('javac')).resolve()
    C.ensure(java.parent==javac.parent,'single JDK installation')
    jvm=java.parent/'server/jvm.dll' if os.name=='nt' else java.parent.parent/'lib/server/libjvm.so'
    helper=HERE/'../mob_tick/ConstructorJson.java'
    inputs=[java,javac,java.parent.parent/'lib/modules',jvm,jar,*libraries,
            HERE/'ExplosionObservation.java',HERE/'ExplosionJdi.java',HERE/'audit.py',HERE/'compare.py',Path(__file__).resolve(),helper,
            SUPPORT/'capture.py',SUPPORT/'classfile.py']
    before={str(p.resolve()):C.digest(p) for p in inputs}
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    classes=output/'classes';classes.mkdir()
    report={'status':'FAIL','inputs_before':before,'scope':__doc__}
    try:
        pins={'client_jar_sha256':JAR,'classes':{}}
        with zipfile.ZipFile(jar) as archive:
            for name,signatures in REQUIRED.items():
                item=I.inspect_class(archive.read(name+'.class'))
                item['methods']=[m for m in item['methods'] if (m['name'],m['descriptor']) in signatures]
                C.ensure({(m['name'],m['descriptor']) for m in item['methods']}==signatures,'original methods '+name)
                pins['classes'][name]=item
        report['pins']=pins
        cp=os.pathsep.join(map(str,[classes,jar,*libraries]))
        report['compile']=C.capture([str(javac),'-encoding','UTF-8','--add-modules','jdk.jdi','-classpath',cp,'-d',str(classes),
                                    str(HERE/'ExplosionObservation.java'),str(HERE/'ExplosionJdi.java'),str(helper)],output,'compile',60,os.environ.copy())
        C.require_success(report['compile'],empty_stderr=True)
        report['helpers_before']={p.name:C.digest(p) for p in classes.iterdir()}
        C.ensure(set(report['helpers_before'])=={'ExplosionObservation.class','ExplosionObservation$1.class',
                 'ExplosionJdi.class','ExplosionJdi$Drain.class','ConstructorJson.class'},'only authored observation helpers')
        report['original']=C.capture([str(java),'-Xint','-Xverify:all','-Xmx256m','-Djava.awt.headless=true',
             '-Djava.security.manager=allow','--add-opens=java.base/java.util=ALL-UNNAMED',
             '--add-opens=java.base/java.lang=ALL-UNNAMED','-classpath',cp,'ExplosionObservation',str(jar)],
             output,'original',120,os.environ.copy(),output)
        C.require_success(report['original'])
        text=(output/report['original']['stdout_file']).read_text()
        C.ensure(text.splitlines()[-1]=='Original explosion particle: 7 ticks; 6 draws; network=0; native=0; write=0','original complete CPU and guard receipt')
        report['ticks']=P.tick_rows(text);report['draws']=P.draw_rows(text)
        report['constructors']=[json.loads(line.removeprefix('EXPLOSION_CONSTRUCTOR ')) for line in text.splitlines() if line.startswith('EXPLOSION_CONSTRUCTOR ')]
        C.ensure([r['profile'] for r in report['constructors']]==list(range(13)),'thirteen genuine measured constructors')
        report['ordinary_entropy']=A.ordinary(report['constructors'])
        debugger=output/'debugger';debugger.mkdir()
        report['debugger']=C.capture([str(java),'-Xmx256m','--add-modules','jdk.jdi','-classpath',str(classes),
            'ExplosionJdi',cp,str(jar),str(java.parent.parent),str(debugger)],debugger,'controller',180,os.environ.copy(),debugger)
        C.require_success(report['debugger'],empty_stderr=True)
        C.ensure(not (debugger/report['debugger']['stdout_file']).read_bytes(),'quiet read-only debugger controller')
        target=(debugger/'target.stdout').read_text()
        P.compare(P.tick_rows(target),report['ticks']);P.compare(P.draw_rows(target),report['draws'])
        constructors=[json.loads(line.removeprefix('EXPLOSION_CONSTRUCTOR ')) for line in target.splitlines() if line.startswith('EXPLOSION_CONSTRUCTOR ')]
        journal=[json.loads(line) for line in (debugger/'constructors.ndjson').read_text().splitlines()]
        report['debugger_entropy']=A.debugger(journal,constructors,pins)
        report['debugger_artifacts_sha256']={p.name:C.digest(p) for p in debugger.iterdir() if p.is_file()}
        report['status']='PASS'
    finally:
        report['inputs_after']={str(p.resolve()):C.digest(p) for p in inputs}
        report['helpers_after']={p.name:C.digest(p) for p in classes.iterdir()}
        if report['inputs_after']!=before or report.get('helpers_before',{})!=report['helpers_after']:
            report['status']='FAIL'
        (output/'capture.json').write_text(json.dumps(report,indent=2)+'\n')
    C.ensure(report['status']=='PASS','stable capture inputs')
    print('Original explosion: seven exact tick rows and six exact NW sprite rows; '+str(output/'capture.json'))


if __name__=='__main__':
    main()
