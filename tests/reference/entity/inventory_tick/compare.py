"""Bounded unchanged-client inventory and complete Map lookup comparisons."""
import argparse,importlib.util,json,os,re,shutil,tempfile,zipfile
from pathlib import Path
from capture import capture,digest,ensure,require_success
from run_native import main as native_main,validate_report
HERE=Path(__file__).resolve().parent
CLIENT='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES={
 'lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
def main(argv=None):
 p=argparse.ArgumentParser();n=p.add_mutually_exclusive_group(required=True)
 n.add_argument('--executable',type=Path);n.add_argument('--native-run',type=Path)
 p.add_argument('--client-jar',type=Path,required=True);p.add_argument('--libraries',type=Path,required=True)
 p.add_argument('--java',default=shutil.which('java'));p.add_argument('--javac',default=shutil.which('javac'))
 p.add_argument('--source-root',type=Path,default=HERE.parents[3])
 d=p.add_mutually_exclusive_group(required=True);d.add_argument('--output',type=Path);d.add_argument('--work-dir',type=Path)
 a=p.parse_args(argv)
 if a.work_dir:
  a.work_dir.mkdir(parents=True,exist_ok=True);a.output=Path(tempfile.mkdtemp(prefix='inventory-tick-reference-',dir=a.work_dir))
 else:a.output.mkdir(parents=True,exist_ok=False)
 ensure(digest(a.client_jar)==CLIENT,'supplied client JAR pin differs')
 libs=[a.libraries/n for n in LIBRARIES];ensure(all(digest(x)==LIBRARIES[x.name] for x in libs),'library pin differs')
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
 spec=importlib.util.spec_from_file_location('inventory_metadata',parser);meta=importlib.util.module_from_spec(spec);spec.loader.exec_module(meta)
 pins=json.loads((HERE/'source-pins.json').read_text())
 with zipfile.ZipFile(a.client_jar) as archive:
  for name,pin in pins['classes'].items():ensure(meta.inspect_class(archive.read(name+'.class'))==pin,'class/Code pin differs: '+name)
 classes=a.output/'classes';classes.mkdir();source=HERE/'InventoryTickObservation.java';cold_source=HERE/'ColdItemObservation.java'
 cp=os.pathsep.join(str(x.resolve()) for x in [classes,a.client_jar,*libs])
 inputs={str(x.resolve()):digest(x) for x in [source,cold_source,a.client_jar,java,javac,parser,HERE/'source-pins.json',*libs,*HERE.glob('*.py')]}
 r=capture([str(javac),'-encoding','UTF-8','-classpath',cp,'-d',str(classes),str(source),str(cold_source)],a.output,'compile',30,env)
 require_success(r,empty_stderr=True)
 helpers={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')}
 ensure(set(helpers)=={'InventoryTickObservation.class','ColdItemObservation.class'},helpers)
 cold=capture([str(java),'-Xint','-Xverify:all','-Xmx1g','-classpath',cp,'ColdItemObservation',str(a.client_jar.resolve())],a.output,'cold-original',60,env)
 require_success(cold,empty_stderr=True)
 ensure((a.output/cold['stdout_file']).read_text().splitlines()[-1:]==['Original cold Item: 2 calls'],'incomplete cold observer')
 cold.update(kind='original_cold_item',calls=2,scope='independent unchanged-original initialization-order/animation-prefix control; no complete native Throwable parity claim')
 (a.output/'cold-original-run.json').write_text(json.dumps(cold,indent=2)+'\n',newline='\n')
 original=capture([str(java),'-Xint','-Xverify:all','-Xmx1g','-classpath',cp,'InventoryTickObservation',
  str((a.output/'worlds').resolve()),str(a.client_jar.resolve())],a.output,'original',180,env)
 require_success(original,empty_stderr=True)
 ensure(helpers=={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')},'authored helper changed')
 ensure(all(digest(Path(path))==pin for path,pin in inputs.items()),'input bytes changed')
 lines=(a.output/original['stdout_file']).read_text().splitlines();expected=[x for x in lines if x.startswith('CMP ')]
 final=re.fullmatch(r'Original inventory tick: ([1-9][0-9]*) rows',lines[-1]) if lines else None
 ensure(final and len(expected)==int(final.group(1)),'original incomplete records')
 original.update(kind='original_inventory_tick',records=len(expected),sources=inputs,helper_class_sha256=helpers,
  java_version=versions[0],javac_version=versions[1],matching_jdk17=vm.group(1),game_class_replacements=False,
  scope='Actual supplied ix/iz/gm ancestors and wr Map lookup; no Player Map tracking or selected terrain completion claim')
 (a.output/'original-run.json').write_text(json.dumps(original,indent=2)+'\n',newline='\n')
 if a.executable:
  native_main(['--executable',str(a.executable),'--output',str(a.output/'native')]);a.native_run=a.output/'native/run.json'
 _,native_log=validate_report(a.native_run);actual=[x for x in native_log.read_text().splitlines() if x.startswith('CMP ')]
 ensure(actual==expected,{'native_records':len(actual),'original_records':len(expected),
  'first_differences':[(i,x,y) for i,(x,y) in enumerate(zip(actual,expected)) if x!=y][:5]})
 report={'status':'PASS','records':len(actual),'original':str((a.output/'original-run.json').resolve()),
  'native':str(a.native_run.resolve()),'native_stdout_sha256':digest(native_log),
  'original_stdout_sha256':digest(a.output/original['stdout_file']),'original_cold_calls':2,
  'scope':'Complete tested inventory/base-Item/remote-Map/Map-lookup cases; pending Player tracker and selected terrain are source-pinned native boundaries only, not full update or exception parity'}
 (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',newline='\n');print(json.dumps(report))
if __name__=='__main__':main()
