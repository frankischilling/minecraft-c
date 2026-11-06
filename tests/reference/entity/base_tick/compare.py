"""Exact fresh actual-dc.U observations; complete calls and prefixes stay distinct."""
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
 values=[x for x in lines if x.startswith(('STATE ','EVENT '))]
 states=[x for x in values if x.startswith('STATE ')]
 ensure(len(states)==31 and [int(x.split()[1]) for x in states]==list(range(31)),'state inventory changed')
 ensure(sum(int(x.split()[2]) for x in states)==22,'complete/prefix inventory changed')
 return values

def main(argv=None):
 p=argparse.ArgumentParser();p.add_argument('--executable',type=Path,required=True)
 p.add_argument('--client-jar',type=Path,required=True);p.add_argument('--libraries',type=Path,required=True)
 p.add_argument('--java',default=shutil.which('java'));p.add_argument('--javac',default=shutil.which('javac'))
 p.add_argument('--tested-commit',required=True);p.add_argument('--source-root',type=Path,default=HERE.parents[3])
 dest=p.add_mutually_exclusive_group(required=True);dest.add_argument('--output',type=Path);dest.add_argument('--work-dir',type=Path)
 a=p.parse_args(argv)
 if a.work_dir is not None:
  a.work_dir.mkdir(parents=True,exist_ok=True);a.output=Path(tempfile.mkdtemp(prefix='player-base-tick-reference-',dir=a.work_dir))
 else:a.output.mkdir(parents=True,exist_ok=False)
 java=Path(shutil.which(a.java) or '').resolve(strict=True);javac=Path(shutil.which(a.javac) or '').resolve(strict=True)
 ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),'one matching JDK bin required')
 libs=[a.libraries/name for name in LIBRARIES]
 ensure(digest(a.client_jar)==CLIENT,'original JAR differs');ensure(all(digest(x)==LIBRARIES[x.name] for x in libs),'dependency JAR differs')
 parser=a.source_root/'tests/reference/client/rendering/classfile.py';pins_path=HERE/'source-pins.json'
 spec=importlib.util.spec_from_file_location('base_tick_metadata',parser);metadata=importlib.util.module_from_spec(spec);spec.loader.exec_module(metadata)
 pins=json.loads(pins_path.read_text())
 with zipfile.ZipFile(a.client_jar) as archive:
  for name,expected in pins['classes'].items():ensure(metadata.inspect_class(archive.read(name+'.class'))==expected,'class/Code metadata differs: '+name)
 inputs=[java,javac,a.client_jar,a.executable,*libs,parser,pins_path,HERE/'BaseTickObservation.java',HERE/'capture.py',HERE/'compare.py',HERE/'run_native.py']
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
  r=capture([str(javac),'-encoding','UTF-8','-classpath',cp,'-d',str(classes),str(HERE/'BaseTickObservation.java')],a.output,'compile',30,env);require_success(r,empty_stderr=True)
  helper={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')};report['helper_before_sha256']=helper
  ensure(set(helper)=={'BaseTickObservation.class','BaseTickObservation$Observer.class'},'unexpected helper classes')
  original=capture([str(java),'--add-opens','java.base/java.util=ALL-UNNAMED','-Xint','-Xverify:all','-Xmx1g','-classpath',cp,
   'BaseTickObservation',str((a.output/'original-worlds').resolve()),str(a.client_jar.resolve())],a.output,'original',180,env);require_success(original,empty_stderr=True)
  native=capture([str(a.executable.resolve()),'player-base-tick-reference',str((a.output/'native-worlds').resolve()).encode().hex()],a.output,'native',180,env);require_success(native,empty_stderr=True)
  left=(a.output/original['stdout_file']).read_text().splitlines();right=(a.output/native['stdout_file']).read_text().splitlines()
  ensure(left[-1]=='Original Player base tick: 31 states','original terminal marker missing')
  ensure(len([x for x in left if x.startswith('ORIGIN ')])==9,'actual runtime origins missing')
  ensure(re.fullmatch(r'Player base tick: [1-9][0-9]* checks',right[-1]),'native terminal marker missing')
  expected,actual=rows(left),rows(right)
  ensure(len(expected)==len(actual),'record counts differ')
  ensure(expected==actual,{'first_difference':next(((i,x,y) for i,(x,y) in enumerate(zip(expected,actual)) if x!=y),None)})
  report.update(status='PASS',states=31,completed_calls=22,interrupted_prefixes=9,event_records=len(actual)-31,records=len(actual),
   jdk=versions,original=original,native=native,scope='Actual unchanged dc.U (ls.U -> sn.U), real World/Chunk/typed WorldAccess.22 completed calls and9 interrupted prefixes compare exact raw state/events/RNG/pool cursors. Prefixes stop at actual null-World/null-Minecraft access or authored WorldAccess failure; no completed larger damage, close, full w_/action/travel/World tick or default entropy claim.')
 finally:
  after={str(x.resolve()):digest(x) for x in inputs};report['input_after_sha256']=after
  report['helper_after_sha256']={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')}
  if before!=after or ('helper_before_sha256' in report and report['helper_before_sha256']!=report['helper_after_sha256']):report['status']='FAIL';report['input_changed']=True
  (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
  ensure(before==after and not report.get('input_changed',False),'source/runtime/helper inputs changed')
 print('Player base tick original/native: '+str(report['records'])+' exact records (22 complete,9 prefixes); report='+str((a.output/'comparison.json').resolve()))
if __name__=='__main__':main()
