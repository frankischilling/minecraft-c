"""Finite unchanged-original observations; pending stats are prefix evidence."""
import argparse,importlib.util,json,os,re,shutil,tempfile,zipfile
from pathlib import Path
from capture import capture,digest,ensure,require_success
from run_native import main as native_main,validate_report
HERE=Path(__file__).resolve().parent
CLIENT='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
NATIVE='86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7'
LIBRARIES={
 'lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
def main(argv=None):
 p=argparse.ArgumentParser();p.add_argument('--executable',type=Path)
 p.add_argument('--native-run',type=Path);p.add_argument('--client-jar',type=Path,required=True)
 p.add_argument('--libraries',type=Path,required=True);p.add_argument('--native-dir',type=Path,required=True)
 p.add_argument('--java',default=shutil.which('java'));p.add_argument('--javac',default=shutil.which('javac'))
 p.add_argument('--source-root',type=Path,default=HERE.parents[3])
 d=p.add_mutually_exclusive_group(required=True);d.add_argument('--output',type=Path);d.add_argument('--work-dir',type=Path)
 a=p.parse_args(argv);ensure((a.executable is None)!=(a.native_run is None),'choose executable or native-run')
 if a.work_dir:
  a.work_dir.mkdir(parents=True,exist_ok=True);a.output=Path(tempfile.mkdtemp(prefix='player-travel-reference-',dir=a.work_dir))
 else:a.output.mkdir(parents=True,exist_ok=False)
 ensure(digest(a.client_jar)==CLIENT,'client JAR pin differs')
 libs=[a.libraries/n for n in LIBRARIES];ensure(all(digest(x)==LIBRARIES[x.name] for x in libs),'library pins differ')
 ensure(digest(a.native_dir/'lwjgl64.dll')==NATIVE,'actual LWJGL x64 pin differs')
 java=Path(shutil.which(a.java) or '').resolve(strict=True);javac=Path(shutil.which(a.javac) or '').resolve(strict=True)
 ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent),'one actual JDK bin required')
 env=os.environ.copy()
 for name in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(name,None)
 versions=[]
 for name,tool in [('java',java),('javac',javac)]:
  r=capture([str(tool),'-version'],a.output,name+'-version',10,env);require_success(r)
  versions.append(((a.output/r['stdout_file']).read_bytes()+(a.output/r['stderr_file']).read_bytes()).decode())
 vm=re.search(r'version "(17\.[0-9]+\.[0-9]+)',versions[0]);compiler=re.search(r'javac (17\.[0-9]+\.[0-9]+)',versions[1])
 ensure(vm and compiler and vm.group(1)==compiler.group(1),'matching JDK17 runtime/compiler required')
 parser=a.source_root/'tests/reference/client/rendering/classfile.py'
 spec=importlib.util.spec_from_file_location('travel_metadata',parser);metadata=importlib.util.module_from_spec(spec);spec.loader.exec_module(metadata)
 pins=json.loads((HERE/'source-pins.json').read_text())
 with zipfile.ZipFile(a.client_jar) as archive:
  for name,pin in pins['classes'].items():ensure(metadata.inspect_class(archive.read(name+'.class'))==pin,'class/Code pin differs: '+name)
 classes=a.output/'classes';classes.mkdir()
 source=HERE/'TravelObservation.java';cp=os.pathsep.join(str(x.resolve()) for x in [classes,a.client_jar,*libs])
 sources={str(x.resolve()):digest(x) for x in [source,a.client_jar,java,javac,a.native_dir/'lwjgl64.dll',parser,HERE/'source-pins.json',*libs,*HERE.glob('*.py')]}
 r=capture([str(javac),'-encoding','UTF-8','-classpath',cp,'-d',str(classes),str(source)],a.output,'compile',30,env);require_success(r,empty_stderr=True)
 helpers={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')};ensure(set(helpers)=={'TravelObservation.class'},helpers)
 original=capture([str(java),'-Xint','-Xverify:all','-Xmx1g','-Dorg.lwjgl.librarypath='+str(a.native_dir.resolve()),
  '-classpath',cp,'TravelObservation',str((a.output/'worlds').resolve()),str(a.client_jar.resolve())],a.output,'original',180,env)
 require_success(original,empty_stderr=True)
 ensure(helpers=={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')},'authored helper bytes changed')
 ensure(all(digest(Path(x))==pin for x,pin in sources.items()),'pinned input bytes changed')
 math_metadata=metadata.inspect_class((a.output/'worlds/jdk-Math.class').read_bytes())
 ensure([m for m in math_metadata['methods'] if m['name']=='round']==pins['jdk17_round'],'actual JDK Math.round Code pin differs')
 original.update(kind='original_player_travel',sources=sources,helpers=helpers,java_version=versions[0],javac_version=versions[1],matching_jdk17=vm.group(1),game_class_replacements=False,jdk_math_metadata=math_metadata)
 (a.output/'original-run.json').write_text(json.dumps(original,indent=2)+'\n',newline='\n')
 if a.executable:
  native_main(['--executable',str(a.executable),'--output',str(a.output/'native')]);a.native_run=a.output/'native/run.json'
 _,native_log=validate_report(a.native_run)
 lines=(a.output/original['stdout_file']).read_text().splitlines()
 expected=[x for x in lines if x.startswith('CMP ')];actual=[x for x in native_log.read_text().splitlines() if x.startswith('CMP ')]
 final=re.fullmatch(r'Original Player travel: ([1-9][0-9]*) rows',lines[-1]) if lines else None
 ensure(final and len(expected)==int(final.group(1)),'incomplete original records')
 ensure(actual==expected,{'first_differences':[(i,x,y) for i,(x,y) in enumerate(zip(actual,expected)) if x!=y][:5],'native_records':len(actual),'original_records':len(expected)})
 jdk=[x for x in lines if x.startswith('JDK ')];native_jdk=[x for x in native_log.read_text().splitlines() if x.startswith('JDK ')]
 ensure(jdk==native_jdk and len(jdk)==4133,'JDK round edge records differ')
 report={'status':'PASS','original':str((a.output/'original-run.json').resolve()),'native':str(a.native_run.resolve()),
  'records':len(actual),'jdk_round_records':len(jdk),'scope':'ls body/helpers complete tested cases; native actual null-Minecraft dc statistic failures compare movement prefixes and original genuine-writer descriptor/amount, not equal application state or exception depth; full-App callsite completion has separate native tests',
  'native_stdout_sha256':digest(native_log),'original_stdout_sha256':digest(a.output/original['stdout_file'])}
 (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',newline='\n');print(json.dumps(report))
if __name__=='__main__':main()
