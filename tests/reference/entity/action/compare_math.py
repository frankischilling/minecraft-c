"""Separate JDK17 math-component scope; this is not an action-method claim."""
import argparse,json,os,re,shutil,tempfile
from pathlib import Path
from capture import capture,digest,ensure,require_success
HERE=Path(__file__).resolve().parent
def main(argv=None):
 p=argparse.ArgumentParser();p.add_argument('--executable',type=Path,required=True)
 p.add_argument('--java',default=shutil.which('java'));p.add_argument('--javac',default=shutil.which('javac'))
 p.add_argument('--work-dir',type=Path,required=True);a=p.parse_args(argv)
 a.work_dir.mkdir(parents=True,exist_ok=True);out=Path(tempfile.mkdtemp(prefix='action-math-',dir=a.work_dir));classes=out/'classes';classes.mkdir()
 java=Path(shutil.which(a.java) or '').resolve(strict=True);javac=Path(shutil.which(a.javac) or '').resolve(strict=True)
 ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),'one JDK bin required')
 source=HERE/'ActionMathObservation.java';inputs=[java,javac,a.executable,source,HERE/'compare_math.py',HERE/'capture.py']
 before={str(x.resolve()):digest(x) for x in inputs};report={'status':'FAIL','inputs_before':before}
 env=os.environ.copy()
 for name in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(name,None)
 try:
  versions=[]
  for name,tool in [('java',java),('javac',javac)]:
   r=capture([str(tool),'-version'],out,name+'-version',10,env);require_success(r)
   versions.append(((out/r['stdout_file']).read_bytes()+(out/r['stderr_file']).read_bytes()).decode())
  vm=re.search(r'version "(17\.[0-9]+\.[0-9]+)',versions[0]);compiler=re.search(r'javac (17\.[0-9]+\.[0-9]+)',versions[1])
  ensure(vm and compiler and vm.group(1)==compiler.group(1),'matching JDK17 required')
  r=capture([str(javac),'-encoding','UTF-8','-d',str(classes),str(source)],out,'compile',30,env);require_success(r,empty_stderr=True)
  helpers={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')};report['helper_before']=helpers
  ensure(set(helpers)=={'ActionMathObservation.class'},'unexpected helper classes')
  original=capture([str(java),'-Xint','-Xverify:all','-classpath',str(classes),'ActionMathObservation'],out,'jdk',60,env);require_success(original,empty_stderr=True)
  native=capture([str(a.executable.resolve())],out,'native',30,env);require_success(native,empty_stderr=True)
  left=(out/original['stdout_file']).read_bytes().splitlines();right=(out/native['stdout_file']).read_bytes().splitlines()
  ensure(left[-1]==right[-1]==b'Action math: 16384 records' and len(left)==len(right)==16385,'incomplete component records')
  ensure(left==right,{'first_difference':next(((i,x.decode(),y.decode()) for i,(x,y) in enumerate(zip(left,right)) if x!=y),None)})
  report.update(status='PASS',records=16384,jdk=versions,original=original,native=native,scope='Actual JDK17 Math.atan/StrictMath.atan and binary32 frem360 components only; no original game/action-method claim.')
 finally:
  report['inputs_after']={str(x.resolve()):digest(x) for x in inputs};report['helper_after']={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')}
  if report['inputs_after']!=before or ('helper_before' in report and report['helper_before']!=report['helper_after']):report['status']='FAIL';report['changed']=True
  (out/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
  ensure(not report.get('changed',False),'component inputs changed')
 print('Action math:16384 exact JDK records; report='+str((out/'comparison.json').resolve()))
if __name__=='__main__':main()
