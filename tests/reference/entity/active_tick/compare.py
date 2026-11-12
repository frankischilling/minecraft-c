"""Exact inherited gs.w_ observations on genuine dc; complete calls and prefixes remain distinct."""
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

def rows(lines):
 values=[x for x in lines if x.startswith('TICK ')]
 expected=[i for i in range(24) if i!=22]+list(range(100,116))
 ensure(len(values)==39 and [int(x.split()[1]) for x in values]==expected,'active tick record inventory changed')
 ensure(sum(int(x.split()[2]) for x in values)==34,'completed/prefix inventory changed')
 return values

def main(argv=None):
 p=argparse.ArgumentParser();p.add_argument('--executable',type=Path,required=True)
 p.add_argument('--natives',type=Path,required=True);p.add_argument('--client-jar',type=Path,required=True);p.add_argument('--libraries',type=Path,required=True)
 p.add_argument('--java',default=shutil.which('java'));p.add_argument('--javac',default=shutil.which('javac'))
 p.add_argument('--tested-commit',required=True);p.add_argument('--source-root',type=Path,default=HERE.parents[3])
 dest=p.add_mutually_exclusive_group(required=True);dest.add_argument('--output',type=Path);dest.add_argument('--work-dir',type=Path)
 a=p.parse_args(argv)
 ensure(os.name=='nt','Actual Minecraft profile currently qualified on Windows x64 only; native/component Linux profiles are separate')
 if a.work_dir is not None:
  a.work_dir.mkdir(parents=True,exist_ok=True);a.output=Path(tempfile.mkdtemp(prefix='player-active-reference-',dir=a.work_dir))
 else:a.output.mkdir(parents=True,exist_ok=False)
 java=Path(shutil.which(a.java) or '').resolve(strict=True);javac=Path(shutil.which(a.javac) or '').resolve(strict=True)
 ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),'one matching JDK bin required')
 libs=[a.libraries/name for name in LIBRARIES]
 ensure(digest(a.natives/'lwjgl64.dll')=='86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7','original Windows native differs');ensure(digest(a.client_jar)==CLIENT,'original JAR differs');ensure(all(digest(x)==LIBRARIES[x.name] for x in libs),'dependency JAR differs')
 parser=a.source_root/'tests/reference/client/rendering/classfile.py';pins_path=HERE/'source-pins.json'

 spec=importlib.util.spec_from_file_location('action_metadata',parser);metadata=importlib.util.module_from_spec(spec);spec.loader.exec_module(metadata)
 pins=json.loads(pins_path.read_text())
 ensure(set(pins['resources'])=={'lang/en_US.lang','lang/stats_US.lang','achievement/map.txt'},'resource allowlist differs')
 with zipfile.ZipFile(a.client_jar) as archive:
  for name,expected in pins['classes'].items():ensure(metadata.inspect_class(archive.read(name.replace('.','/')+'.class'))==expected,'class/Code metadata differs: '+name)
 inputs=[java,javac,a.client_jar,a.executable,*libs,parser,pins_path,HERE/'ActiveTickObservation.java',HERE/'capture.py',HERE/'compare.py',HERE/'run_native.py',a.natives/'lwjgl64.dll']
 before={str(x.resolve()):digest(x) for x in inputs};report={'status':'FAIL','tested_commit':a.tested_commit,'input_before_sha256':before}
 classes=a.output/'classes';classes.mkdir();env=os.environ.copy()
 for key in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(key,None)
 try:
  versions=[]
  for name,tool in [('java',java),('javac',javac)]:
   r=capture([str(tool),'-version'],a.output,name+'-version',10,env);require_success(r)
   versions.append(((a.output/r['stdout_file']).read_bytes()+(a.output/r['stderr_file']).read_bytes()).decode(errors='strict'))
  vm=re.search(r'version "(17\.[0-9]+\.[0-9]+)',versions[0]);compiler=re.search(r'javac (17\.[0-9]+\.[0-9]+)',versions[1])
  ensure(vm and compiler and vm.group(1)==compiler.group(1),'matching JDK17 runtime/compiler required')
  cp=os.pathsep.join(str(x.resolve()) for x in [classes,a.client_jar,*libs])
  r=capture([str(javac),'-encoding','UTF-8','-classpath',cp,'-d',str(classes),str(HERE/'ActiveTickObservation.java')],a.output,'compile',30,env);require_success(r,empty_stderr=True)
  helper={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')};report['helper_before_sha256']=helper
  ensure(set(helper)=={'ActiveTickObservation.class'},'unexpected helper classes')
  original=capture([str(java),'--add-opens','java.base/java.util=ALL-UNNAMED','-Xint','-Xverify:all','-Xmx1g','-Xlog:class+init=info:file=class-init.log::filesize=1m,filecount=1','-Dorg.lwjgl.librarypath='+str(a.natives.resolve()),'-classpath',cp,
   'ActiveTickObservation',str((a.output/'original-worlds').resolve()),str(a.client_jar.resolve())],a.output,'original',180,env,working_directory=a.output.resolve());require_success(original,empty_stderr=True)
  init_log=a.output/'class-init.log'
  ensure(init_log.is_file(),'class initialization log missing')
  ensure(init_log.stat().st_size<1024*1024 and len(list(a.output.glob('class-init.log*')))==1,'class initialization log exceeded bounded complete profile')
  initialized=init_log.read_text(errors='strict')
  ensure(not re.search(r'Initializing \'org/lwjgl/(?:opengl/Display|openal/AL)\'',initialized),'unexpected Display/OpenAL initialization')
  report['class_initialization_log_sha256']=digest(init_log)
  resources=a.output/'resources'
  with zipfile.ZipFile(a.client_jar) as archive:
   for name,expected in pins['resources'].items():
    data=archive.read(name);ensure(len(data)==expected['bytes'] and __import__('hashlib').sha256(data).hexdigest()==expected['sha256'],'original resource differs')
    path=resources/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
  resource_before={x.relative_to(resources).as_posix():digest(x) for x in resources.rglob('*') if x.is_file()};report['resource_before_sha256']=resource_before
  native=capture([str(a.executable.resolve()),'player-active-reference',str((a.output/'native-worlds').resolve()).encode().hex(),str(resources.resolve()).encode().hex()],a.output,'native',180,env);require_success(native,empty_stderr=True)
  left=(a.output/original['stdout_file']).read_text().splitlines();right=(a.output/native['stdout_file']).read_text().splitlines()
  ensure(left[-1]=='Original Player active tick: 39 states','original terminal marker missing')
  ensure(len([x for x in left if x.startswith('ORIGIN ')])==21,'actual runtime origins missing')
  ensure(re.fullmatch(r'Player active tick: [1-9][0-9]* checks',right[-1]),'native terminal marker missing')
  expected,actual=rows(left),rows(right)
  ensure(len(expected)==len(actual),'record counts differ')
  ensure(expected==actual,{'first_difference':next(((i,x,y) for i,(x,y) in enumerate(zip(expected,actual)) if x!=y),None)})
  expected_push=[x for x in left if x.startswith('PUSH ')];actual_push=[x for x in right if x.startswith('PUSH ')]
  ensure(len(expected_push)==10 and [int(x.split()[1]) for x in expected_push]==list(range(10)) and expected_push==actual_push,'private actual push-out records differ')
  resource_after={x.relative_to(resources).as_posix():digest(x) for x in resources.rglob('*') if x.is_file()};report['resource_after_sha256']=resource_after
  ensure(resource_before==resource_after,'copied original resources changed')
  report.update(status='PASS',pushout_records=10,states=39,completed_calls=34,interrupted_prefixes=5,records=len(actual),
   jdk=versions,original=original,native=native,scope='Actual inherited gs.w_ on genuine published dc, including ls.w_ -> sn.w_/virtual U -> dc.o -> gs.o -> genuine Input/Inventory/travel/statistic descendants. 34 complete calls and five actual null-reference prefixes. Includes a 16-tick forward progression. Absolute wall-clock notice milliseconds are independently bounded, not compared numerically; the native-only unimplemented GUI close boundary is excluded. No full Minecraft constructor/startup, World update wrapper or full gameplay loop claim.')
 finally:
  after={str(x.resolve()):digest(x) for x in inputs};report['input_after_sha256']=after
  report['helper_after_sha256']={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')}
  if before!=after or ('helper_before_sha256' in report and report['helper_before_sha256']!=report['helper_after_sha256']):report['status']='FAIL';report['input_changed']=True
  (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
  ensure(before==after and not report.get('input_changed',False),'source/runtime/helper inputs changed')
 print('Player active tick original/native: '+str(report['records'])+' exact tick records (34 complete,5 prefixes) and10 push-out records; report='+str((a.output/'comparison.json').resolve()))
if __name__=='__main__':main()
