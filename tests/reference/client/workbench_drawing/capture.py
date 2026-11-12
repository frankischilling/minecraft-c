"""Prepare, then explicitly observe genuine original Workbench drawing on a real Display."""
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
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
C=module('workbench_drawing_capture',ROOT/'tests/reference/client/controller/capture.py')
I=module('workbench_drawing_classfile',ROOT/'tests/reference/client/controller/classfile.py')
A=module('workbench_drawing_audit',HERE/'audit.py')
LIBRARIES={
 'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
CLASSES=('bb','iz','nh','kq','net/minecraft/client/Minecraft','oo','it','fi','vg','uu','gm','ck','rx','id','da','ub','th','ds','gv','bw','fh','ko','ps','tz','nw','fd','dc','gs','ix','aa','kv','gr','xi','vu','ik','ji','sj','ra','iy','u','ls','sn','lr','uo')
ASSETS=('gui/crafting.png','font/default.png','mob/char.png','misc/shadow.png','terrain.png','gui/items.png','pack.png')

def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
def only_deprecation(path):
 lines=Path(path).read_text(encoding='utf-8').splitlines()
 C.ensure(len(lines)==4 and all(line.startswith('WARNING:') for line in lines) and any('System::setSecurityManager' in line for line in lines),'no source body/caught-renderer stderr error')

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--output',type=Path,required=True)
 parser.add_argument('--observe',action='store_true',help='Run real Display GL only after desktop scheduling; default only compiles/prepares')
 parser.add_argument('--jar',type=Path,default=ROOT/'b1.7.3.jar')
 parser.add_argument('--libraries',type=Path,default=Path(os.environ.get('APPDATA',''))/'.minecraft/libraries')
 args=parser.parse_args();C.ensure(os.name=='nt','pinned Windows native profile')
 output=args.output.resolve()
 if not args.observe:
  output.mkdir(parents=True,exist_ok=False);classes=output/'classes';classes.mkdir();natives=output/'natives';natives.mkdir()
  jar=args.jar.resolve(strict=True);C.ensure(C.digest(jar)==A.JAR,'untouched original JAR')
  libraries=[args.libraries/name for name in LIBRARIES]
  C.ensure(all(C.digest(path)==value for path,value in zip(libraries,LIBRARIES.values())),'pinned libraries')
  archive=args.libraries/'org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar'
  C.ensure(C.digest(archive)=='68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43','pinned native archive')
  with zipfile.ZipFile(archive) as source:(natives/'lwjgl64.dll').write_bytes(source.read('lwjgl64.dll'))
  java,javac=[Path(shutil.which(name)).resolve(strict=True) for name in ('java','javac')]
  C.ensure(java.parent==javac.parent and '17.' in java.parent.parent.name,'single pinned JDK17')
  helpers=[HERE/name for name in ('WorkbenchDrawingTarget.java','WorkbenchDrawingJdi.java','ConstructorJson.java','capture.py','audit.py','declared-inputs.json')]
  C.ensure(all(b'\r' not in path.read_bytes() for path in helpers),'authored LF helper bytes before capture')
  files=[jar,*libraries,archive,natives/'lwjgl64.dll',java,javac,java.parent.parent/'lib/modules',java.parent/'server/jvm.dll',*java.parent.glob('*.dll'),*helpers,
    ROOT/'tests/reference/client/controller/capture.py',ROOT/'tests/reference/client/controller/classfile.py']
  environment=os.environ.copy()
  for name in ('APPDATA','TMP','TEMP'):
   directory=output/name.lower();directory.mkdir();environment[name]=str(directory)
  cp=os.pathsep.join(map(str,[classes,jar,*libraries]))
  report={'status':'PREPARED','scope':'Compiled genuine original two-draw Display observer; no GL execution in prepare mode.','inputs_before':{str(p.resolve()):C.digest(p) for p in files},
   'environment':{name:environment[name] for name in ('APPDATA','TMP','TEMP')},'java':str(java),'classpath':cp,'jar':str(jar),'natives':str(natives)}
  report['compile']=C.capture([str(javac),'-proc:none','-encoding','UTF-8','--add-modules','jdk.jdi','-classpath',cp,'-d',str(classes),*[str(HERE/name) for name in ('WorkbenchDrawingTarget.java','WorkbenchDrawingJdi.java','ConstructorJson.java')]],output,'compile',60,environment,output)
  C.require_success(report['compile'],empty_stderr=True)
  report['helpers_before']={p.name:C.digest(p) for p in classes.iterdir()}
  C.ensure(set(report['helpers_before'])=={'WorkbenchDrawingTarget.class','WorkbenchDrawingTarget$1.class','WorkbenchDrawingJdi.class','WorkbenchDrawingJdi$Drain.class','ConstructorJson.class'},'only authored helper classes')
  with zipfile.ZipFile(jar) as source:
   report['pins']={'client_jar_sha256':A.JAR,'classes':{name:I.inspect_class(source.read(name+'.class')) for name in CLASSES}}
   report['assets']={name:{'bytes':len(source.read(name)),'sha256':__import__('hashlib').sha256(source.read(name)).hexdigest()} for name in ASSETS}
  report['inputs_after_prepare']={name:C.digest(name) for name in report['inputs_before']}
  C.ensure(report['inputs_after_prepare']==report['inputs_before'],'unchanged prepared inputs')
  write(output/'source-pins.json',report['pins']);write(output/'capture.json',report)
  print('PREPARED only; no GL execution: '+str(output));return
 report=json.loads((output/'capture.json').read_text(encoding='utf-8'),object_pairs_hook=A.unique)
 C.ensure(report['status']=='PREPARED','fresh prepared capture state')
 C.ensure(all(C.digest(path)==value for path,value in report['inputs_before'].items()),'all prepared bytes unchanged before actual GL run')
 environment=os.environ.copy();environment.update(report['environment']);java=Path(report['java']);cp=report['classpath'];jar=report['jar'];natives=report['natives'];report['status']='FAIL'
 try:
  report['original']=C.capture([str(java),'-Xint','-Xverify:all','-Xmx512m','-Djava.awt.headless=true','-Djava.security.manager=allow','-Djava.io.tmpdir='+environment['TMP'],
   '-Dorg.lwjgl.librarypath='+natives,'-classpath',cp,'WorkbenchDrawingTarget',jar,str(output/'game'),natives],output,'original',180,environment,output)
  C.require_success(report['original']);only_deprecation(output/'original.stderr')
  original=(output/'original.stdout').read_text(encoding='utf-8');A.target(original)
  debug=output/'debugger';debug.mkdir()
  report['debugger']=C.capture([str(java),'--add-modules','jdk.jdi','-Xmx512m','-classpath',str(output/'classes'),'WorkbenchDrawingJdi',str(debug),str(java.parent.parent),natives,cp,jar],debug,'controller',240,environment,debug)
  C.require_success(report['debugger'],empty_stderr=True);only_deprecation(debug/'target.stderr')
  observed=(debug/'target.stdout').read_text(encoding='utf-8')
  events=[json.loads(line,object_pairs_hook=A.unique) for line in (debug/'journal.ndjson').read_text(encoding='utf-8').splitlines()]
  report['audit']=A.audit(report,original,observed,events)
  report['live_Code']=json.loads((debug/'live-Code.json').read_text(encoding='utf-8'),object_pairs_hook=A.unique)
  for key,pin in report['live_Code'].items():
   owner,tail=key.split('.',1);name,descriptor=tail.split('(',1)
   expected=next(m for m in report['pins']['classes'][owner]['methods'] if m['name']==name and m['descriptor']=='('+descriptor)
   C.ensure(all(expected[k]==v for k,v in pin.items()),'actual live original Code agrees with JAR '+key)
  write(output/'finite.json',{'schema':1,'scope':report['audit']['scope'],'rows':report['audit']['finite']});report['status']='PASS'
 finally:
  report['inputs_after']={path:C.digest(path) for path in report['inputs_before']};report['helpers_after']={p.name:C.digest(p) for p in (output/'classes').iterdir()}
  if report['inputs_after']!=report['inputs_before'] or report['helpers_after']!=report['helpers_before']:report['status']='FAIL'
  write(output/'capture.json',report)
 C.ensure(report['status']=='PASS','actual original draws, graph, raw buffers and helpers qualified')
 print('Original Workbench drawing PASS: two genuine returned draws/four selected NW batches; '+str(output))

if __name__=='__main__':main()
