"""Fresh finite original/native Player movement comparison; no captured answers."""
import argparse,importlib.util,json,os,re,shutil,sys,tempfile,zipfile
from pathlib import Path
from capture import capture,digest,ensure,require_success
sys.dont_write_bytecode=True

HERE=Path(__file__).resolve().parent
CLIENT='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES={
 'lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}

def main(argv=None):
 p=argparse.ArgumentParser();p.add_argument('--executable',type=Path,required=True)
 p.add_argument('--client-jar',type=Path,required=True);p.add_argument('--libraries',type=Path,required=True)
 p.add_argument('--java',default=shutil.which('java'));p.add_argument('--javac',default=shutil.which('javac'))
 p.add_argument('--tested-commit',required=True)
 p.add_argument('--source-root',type=Path,default=HERE.parents[3])
 d=p.add_mutually_exclusive_group(required=True);d.add_argument('--output',type=Path);d.add_argument('--work-dir',type=Path)
 a=p.parse_args(argv)
 if a.work_dir is not None:
  a.work_dir.mkdir(parents=True,exist_ok=True);a.output=Path(tempfile.mkdtemp(prefix='player-movement-reference-',dir=a.work_dir))
 else:a.output.mkdir(parents=True,exist_ok=False)
 java=Path(shutil.which(a.java) or '').resolve(strict=True);javac=Path(shutil.which(a.javac) or '').resolve(strict=True)
 ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),'one actual JDK bin is required')
 libs=[a.libraries/name for name in LIBRARIES]
 ensure(digest(a.client_jar)==CLIENT,'client JAR pin differs')
 ensure(all(digest(x)==LIBRARIES[x.name] for x in libs),'dependency JAR pin differs')
 metadata_parser=a.source_root/'tests/reference/client/rendering/classfile.py'
 pins_path=HERE/'source-pins.json';pins=json.loads(pins_path.read_text())
 spec=importlib.util.spec_from_file_location('movement_class_metadata',metadata_parser)
 metadata=importlib.util.module_from_spec(spec);spec.loader.exec_module(metadata)
 with zipfile.ZipFile(a.client_jar) as archive:
  for key,expected in pins['classes'].items():
   if key.startswith('client/'):
    ensure(metadata.inspect_class(archive.read(key.removeprefix('client/')+'.class'))==expected,'original class/method metadata differs: '+key)
 sources=[HERE/'MovementObservation.java']
 inputs=[a.client_jar,a.executable,java,javac,*libs,*sources,HERE/'capture.py',HERE/'compare.py',HERE/'run_native.py',metadata_parser,pins_path]
 before={str(x.resolve()):digest(x) for x in inputs}
 report={'status':'FAIL','input_sha256':before,'tested_commit':a.tested_commit}
 classes=a.output/'classes';classes.mkdir()
 env=os.environ.copy()
 for key in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(key,None)
 try:
  versions=[]
  for name,tool in [('java',java),('javac',javac)]:
   version=capture([str(tool),'-version'],a.output,name+'-version',10,env);require_success(version)
   versions.append(((a.output/version['stdout_file']).read_bytes()+(a.output/version['stderr_file']).read_bytes()).decode(errors='strict'))
  vm=re.search(r'version "(17\.[0-9]+\.[0-9]+)',versions[0]);compiler=re.search(r'javac (17\.[0-9]+\.[0-9]+)',versions[1])
  ensure(vm and compiler and vm.group(1)==compiler.group(1),'matching JDK17 compiler/runtime required')
  classpath=os.pathsep.join(str(x.resolve()) for x in [classes,a.client_jar,*libs])
  build=capture([str(javac),'-encoding','UTF-8','-classpath',classpath,'-d',str(classes),*[str(x) for x in sources]],a.output,'compile',30,env)
  require_success(build,empty_stderr=True)
  helpers={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')}
  report['helper_before_sha256']=helpers
  ensure(set(helpers)=={'MovementObservation.class'},helpers)
  original=capture([str(java),'--add-opens','java.base/java.util=ALL-UNNAMED','-Xint','-Xverify:all','-Xmx1g',
   '-classpath',classpath,'MovementObservation',str((a.output/'original-worlds').resolve()),str(a.client_jar.resolve())],a.output,'original',180,env)
  require_success(original,empty_stderr=True)
  native=capture([str(a.executable.resolve()),'player-movement-reference',str((a.output/'native-worlds').resolve()).encode().hex()],a.output,'native',180,env)
  require_success(native,empty_stderr=True)
  lines=(a.output/original['stdout_file']).read_text().splitlines()
  ensure(lines[-1]=='Original Player movement: 21 rows','original terminal marker missing')
  ensure(len([x for x in lines if x.startswith('ORIGIN ')])==9,'actual runtime origins missing')
  other=(a.output/native['stdout_file']).read_text().splitlines()
  ensure(re.fullmatch(r'PASS: [1-9][0-9]* Player movement/step/sound checks',other[-1]),'native terminal marker missing')
  expected=[x for x in lines if x.startswith('CMP ')];actual=[x for x in other if x.startswith('CMP ')]
  ensure(len(expected)==len(actual)==21,'finite record count differs')
  ensure(expected==actual,{'first_difference':next(((i,x,y) for i,(x,y) in enumerate(zip(expected,actual)) if x!=y),None)})
  jdk=[x for x in lines if x.startswith('JDK ROUND ')];rounding=[x for x in other if x.startswith('JDK ROUND ')]
  ensure(len(jdk)==len(rounding)==12 and jdk==rounding,'reached statistic amount differs from actual JDK Math.round')
  ensure(helpers=={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')},'compiled helper changed')
  report.update(status='PASS',records=len(actual),jdk_round_records=12,jdk=versions,helper_class_sha256=helpers,original=original,native=native,
   scope='21 exact records: actual ct methods/identity, six real World LocalPlayer moves including snow/fence/water/Ore, portal, small fall and early damage. Source scalar/Chunk/RNG controls are explicit inputs; no startup/default entropy, whole World array, later Living damage, statistic, close or complete tick parity is claimed.')
 finally:
  report['helper_after_sha256']={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')}
  after={str(x.resolve()):digest(x) for x in inputs}
  report['input_after_sha256']=after
  if before!=after:report['status']='FAIL';report['input_changed']=True
  (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
  ensure(before==after,'authored/source/runtime inputs changed')
 print('Player movement original/native: 21 exact records; report='+str((a.output/'comparison.json').resolve()))

if __name__=='__main__':main()
