"""Two ordinary guarded VMs over unchanged IM/XW/SN owners; no game code replacement."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES={
 'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
PRODUCERS=('RedstoneParticleObservation.java','profiles.tsv','declared-inputs.json','capture_reference.py','audit.py','compare.py')
CLASSES=('im','xw','sn','nw','fd','eq','in','lm','ok','wy','xa')


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--jar',type=Path,required=True);parser.add_argument('--libraries',type=Path,required=True)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args();jar=args.jar.resolve(strict=True)
 if digest(jar)!=JAR:raise ValueError('verified original archive')
 libraries=[(args.libraries/name).resolve(strict=True) for name in LIBRARIES]
 if any(digest(path)!=pin for path,pin in zip(libraries,LIBRARIES.values())):raise ValueError('pinned original dependencies')
 java,javac=Path(shutil.which('java')).resolve(),Path(shutil.which('javac')).resolve()
 if java.parent!=javac.parent:raise ValueError('one JDK installation')
 jdk=java.parent.parent;suffix='bin/server/jvm.dll' if os.name=='nt' else 'lib/server/libjvm.so'
 helper=ROOT/'tests/reference/entity/mob_tick/ConstructorJson.java'
 readerpath=ROOT/'tests/reference/client/controller/classfile.py';reader=module('redstone_classfile',readerpath)
 audit=module('redstone_capture_audit',HERE/'audit.py');compare=module('redstone_capture_compare',HERE/'compare.py')
 inputs={'original/client.jar':jar,'jdk/java':java,'jdk/javac':javac,'jdk/modules':jdk/'lib/modules','jdk/jvm':jdk/suffix,
         'jdk/java.base.jmod':jdk/'jmods/java.base.jmod','tools/classfile.py':readerpath,'tools/ConstructorJson.java':helper,
         **{'libraries/'+name:path for name,path in zip(LIBRARIES,libraries)},**{'authored/'+name:HERE/name for name in PRODUCERS}}
 before={name:digest(path) for name,path in inputs.items()}
 out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);classes=out/'classes';classes.mkdir()
 pins={'client_jar_sha256':JAR,'classes':{},'jdk_classes':{}}
 with zipfile.ZipFile(jar) as archive:
  for name in CLASSES:pins['classes'][name]=reader.inspect_class(archive.read(name+'.class'))
 with zipfile.ZipFile(jdk/'jmods/java.base.jmod') as archive:
  for name in ('java/util/Random','java/lang/Math','java/lang/Math$RandomNumberGeneratorHolder'):
   pins['jdk_classes'][name]=reader.inspect_class(archive.read('classes/'+name+'.class'))
 write(out/'source-code.json',pins)
 env=os.environ.copy();removed=[key for key in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS') if env.pop(key,None) is not None]
 cp=os.pathsep.join(map(str,[classes,jar,*libraries]))
 flags=['-Xint','-Xverify:all','-Xmx256m','-Djava.awt.headless=true','-Djava.security.manager=allow',
        '--add-opens=java.base/java.util=ALL-UNNAMED','--add-opens=java.base/java.lang=ALL-UNNAMED']
 commands=[[str(java),*flags,'-classpath',cp,'RedstoneParticleObservation',str(jar),str(HERE/'profiles.tsv')] for _ in range(2)]
 compilecommand=[str(javac),'-encoding','UTF-8','-implicit:none','-classpath',cp,'-d',str(classes),str(HERE/'RedstoneParticleObservation.java'),str(helper)]
 write(out/'freeze.json',{'inputs_before':before,'exact_paths':{k:str(v) for k,v in inputs.items()},'compile_argv':compilecommand,'ordinary_argv':commands,'removed_environment_names':removed})
 started=time.monotonic();report={'status':'FAIL','original_game_class_replacements':0,'ordinary_fresh_vms':2,'inputs_before':before,'runs':[], 'ordinary_flags':flags,'removed_environment_names':removed}
 def run(command,stem,timeout):
  tick=time.monotonic()
  try:r=subprocess.run(command,capture_output=True,env=env,cwd=out,timeout=timeout);code,stdout,stderr=r.returncode,r.stdout,r.stderr
  except subprocess.TimeoutExpired as e:code,stdout,stderr=-1,e.stdout or b'',e.stderr or b''
  (out/(stem+'.stdout')).write_bytes(stdout);(out/(stem+'.stderr')).write_bytes(stderr)
  row={'returncode':code,'stdout_sha256':digest(out/(stem+'.stdout')),'stderr_private_sha256':digest(out/(stem+'.stderr')),'elapsed_seconds':time.monotonic()-tick}
  write(out/(stem+'.json'),row);return row
 try:
  report['compile']=run(compilecommand,'compile',60)
  if report['compile']['returncode'] or (out/'compile.stderr').read_bytes():raise ValueError('authored helper compile')
  report['helpers_before']={p.name:digest(p) for p in classes.iterdir()}
  if set(report['helpers_before'])!={'RedstoneParticleObservation.class','RedstoneParticleObservation$1.class','ConstructorJson.class'}:raise ValueError('only authored helper classes')
  semantic=[];audits=[]
  for index,command in enumerate(commands):
   receipt=run(command,'ordinary-'+str(index),120);report['runs'].append(receipt)
   if receipt['returncode']:raise ValueError('original process failed')
   text=(out/('ordinary-'+str(index)+'.stdout')).read_text(encoding='ascii')
   component=compare.components(text);constructors=compare.rows(text,'REDSTONE_PARTICLE_CONSTRUCTOR')
   audits.append(audit.ordinary(constructors));semantic.append(component)
   write(out/('ordinary-'+str(index)+'.constructors.json'),constructors);write(out/('ordinary-'+str(index)+'.components.json'),component)
  compare.compare(*semantic)
  report['logical_components_identical']=True;report['own_constructor_audits']=audits;report['status']='PASS'
 except BaseException as error:
  report['failure']={'type':type(error).__name__,'message':str(error)}
  raise
 finally:
  report['inputs_after']={name:digest(path) for name,path in inputs.items()}
  report['inputs_unchanged']=report['inputs_before']==report['inputs_after']
  report['helpers_after']={p.name:digest(p) for p in classes.iterdir()};report['helpers_unchanged']=report.get('helpers_before')==report['helpers_after']
  report['elapsed_seconds']=time.monotonic()-started
  if not report['inputs_unchanged'] or not report['helpers_unchanged']:report['status']='FAIL'
  write(out/'provenance.json',report)
 if report['status']!='PASS':raise ValueError('original input/helper continuity')
 print('Original Redstone particles PASS:37 constructors/30 components x2 ordinary VMs; '+str(out))

if __name__=='__main__':main()
