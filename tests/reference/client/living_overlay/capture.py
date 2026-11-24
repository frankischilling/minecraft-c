"""Capture seven actual unchanged-original overlay scenarios into a new local directory.

Linux ordinary/read-only-JDI capture uses Xvfb. --ordinary-only --primary runs
an additional same-frozen-helper ordinary supplement, including on Windows.
New captures need separately reviewed immutable metadata before comparison;
this CLI never relabels a fresh run as the historical published packet.
"""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys,time,zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'CMakeLists.txt').is_file())
def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as stream:
  for block in iter(lambda:stream.read(1048576),b''):h.update(block)
 return h.hexdigest()
def need(v,m):
 if not v:raise RuntimeError(m)
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--client-jar',type=Path,required=True);p.add_argument('--sdk-library',type=Path,action='append',required=True);p.add_argument('--native-archive',type=Path,required=True);p.add_argument('--jdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--timeout',type=int,default=1800);p.add_argument('--ordinary-only',action='store_true');p.add_argument('--primary',type=Path);a=p.parse_args()
 need(a.timeout>0,'positive process timeout');need(os.name!='nt' or a.ordinary_only,'primary full capture requires Linux/Xvfb; Windows is ordinary supplement only');need(bool(a.primary)==a.ordinary_only,'ordinary supplement requires an actual completed primary capture')
 proof=json.loads((HERE/'provenance.json').read_text());authority=json.loads((HERE/'source-code-pins.json').read_text());jar=a.client_jar.resolve(strict=True);need(sha(jar)==authority['jar_sha256'],'unchanged supplied original client JAR')
 expected={Path(n).name:h for n,h in proof['primary']['input_sha256'].items() if '/.minecraft/libraries/' in n and n.endswith('.jar')};sdk=[v.resolve(strict=True) for v in a.sdk_library];need(len(sdk)==len(expected)==4 and {v.name:sha(v) for v in sdk}==expected,'four pinned original SDK library archives')
 archive=a.native_archive.resolve(strict=True);captured=proof['windows_normal_supplement']['input_sha256'] if os.name=='nt' else proof['primary']['input_sha256'];archive_pins=[h for n,h in captured.items() if n.replace('\\','/').endswith('/'+archive.name)];need(len(archive_pins)==1 and sha(archive)==archive_pins[0],'pinned matching native archive')
 jdk=a.jdk.resolve(strict=True);java=jdk/('bin/java.exe' if os.name=='nt' else 'bin/java');javac=jdk/('bin/javac.exe' if os.name=='nt' else 'bin/javac');need(java.is_file() and javac.is_file(),'JDK runtime and compiler available')
 native_name='lwjgl64.dll' if os.name=='nt' else 'liblwjgl64.so';out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);natives=out/'natives';natives.mkdir()
 with zipfile.ZipFile(archive) as z:(natives/native_name).write_bytes(z.read(native_name))
 lib_pins=[h for n,h in captured.items() if n.replace('\\','/').endswith('/'+native_name)];need(len(lib_pins)==1 and sha(natives/native_name)==lib_pins[0],'actual selected native library bytes')
 spec=importlib.util.spec_from_file_location('overlay_capture_classfile',ROOT/'tests/reference/client/controller/classfile.py');classfile=importlib.util.module_from_spec(spec);spec.loader.exec_module(classfile)
 spec=importlib.util.spec_from_file_location('overlay_capture_bounded',ROOT/'tests/reference/entity/player/support.py');bounded=importlib.util.module_from_spec(spec);spec.loader.exec_module(bounded)
 with zipfile.ZipFile(jar) as z:actual={n[:-6]:classfile.inspect_class(z.read(n)) for n in z.namelist() if n.endswith('.class')}
 need(actual==authority['classes'],'all actual original full classes and Code identities');write(out/'authority.json',authority)
 classes=(a.primary/'classes').resolve(strict=True) if a.primary else out/'classes'
 if not a.primary:classes.mkdir()
 jvm=jdk/('bin/server/jvm.dll' if os.name=='nt' else 'lib/server/libjvm.so')
 inputs=[Path(__file__),HERE/'provenance.json',HERE/'source-code-pins.json',jar,*sdk,archive,natives/native_name,java,javac,jvm,jdk/'lib/modules',ROOT/'tests/reference/client/controller/classfile.py',ROOT/'tests/reference/entity/player/support.py',*[HERE/n for n in proof['public_authored_sha256']]]
 xvfb=[]
 if os.name!='nt':
  for name in ['xvfb-run','Xvfb','xauth']:
   tool=shutil.which(name);need(tool,'missing reference prerequisite: '+name);inputs.append(Path(tool).resolve())
  xvfb=[shutil.which('xvfb-run'),'-a','-s','-screen 0 640x480x24 -noreset']
 if a.primary:
  old=json.loads((a.primary/'receipt.json').read_text());need(old['inputs_unchanged'] and old['helper_before_sha256']==old['helper_after_sha256'],'actual stable primary compiled helpers');need({v.name:sha(v) for v in classes.glob('*.class')}==old['helper_before_sha256'],'same actual compiled helper bytes');inputs.extend([a.primary/'receipt.json',*classes.glob('*.class')])
 before={str(v):sha(v) for v in inputs};write(out/'input-freeze.json',{'scope':'Fresh actual capture inputs before compile/execution; not historical metadata','input_sha256':before});record={'status':'FAIL','historical_published_capture':False,'ordinary_only':a.ordinary_only,'input_before_sha256':before,'commands':{},'default_constructor_entropy_rebound':False};env=os.environ.copy()
 for name in ['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH','DISPLAY']:env.pop(name,None)
 if os.name!='nt':env['LIBGL_ALWAYS_SOFTWARE']='1'
 try:
  if not a.primary:
   cmd=[javac,'--add-modules','jdk.jdi','-cp',os.pathsep.join(map(str,[jar,*sdk])),'-d',classes,*sorted(HERE.glob('*.java'))];record['compile_command']=list(map(str,cmd));r=bounded.run_bounded(cmd,out,'compile',timeout=min(a.timeout,120),env=env);record['compile_exit']=r.returncode;need(r.returncode==0,'actual helper compilation failed')
  record['helper_before_sha256']={v.name:sha(v) for v in classes.glob('*.class')};write(out/'helper-Code.json',{v.name:classfile.inspect_class(v.read_bytes()) for v in classes.glob('*.class')})
  for mode in (['normal'] if a.ordinary_only else ['normal','debugger']):
   directory=out/mode;directory.mkdir();(directory/'resources').mkdir();(directory/'classes.txt').write_text('\n'.join(sorted(actual))+'\n');cp=os.pathsep.join(map(str,[classes,directory/'resources',jar,*sdk]))
   if mode=='normal':cmd=[*xvfb,java,'-Xint','-Xmx512m','-Xverify:all','--add-opens=java.base/java.util=ALL-UNNAMED','--add-opens=java.base/java.lang=ALL-UNNAMED','-Djava.awt.headless=true','-Djava.security.manager=allow','-Dorg.lwjgl.librarypath='+str(natives),'-Dmobdraw.classes='+str(directory/'classes.txt'),'-Xlog:class+init=info:file=class-init.log','-cp',cp,'OverlayDrawTarget',directory,HERE]
   else:cmd=[*xvfb,java,'--add-modules','jdk.jdi','-cp',classes,'OverlayDrawJdi',directory,jdk,natives,cp,jar,HERE]
   record['commands'][mode]=list(map(str,cmd));start=time.monotonic();r=bounded.run_bounded(cmd,directory,'process',timeout=a.timeout,env=env);record[mode+'_seconds']=time.monotonic()-start;record[mode+'_exit']=r.returncode;need(r.returncode==0,'actual '+mode+' execution failed')
   rows=[json.loads(line) for line in (directory/'target.ndjson').read_text().splitlines()];draws=[r for r in rows if r['event']=='draw'];need([r['index'] for r in draws]==list(range(7)) and all(r['outcome']==1 and r['failure'] is None and r['gl_error']==0 for r in draws),'all actual original overlay calls returned without GL errors');need(all(r['before']['entropy']==r['after']['entropy'] for r in draws),'actual per-process raw entropy remains unchanged')
  record['status']='FRESH_ORDINARY_SUPPLEMENT_OBSERVED' if a.ordinary_only else 'FRESH_SEVEN_ORIGINAL_OVERLAY_PROFILES_OBSERVED'
 except BaseException as e:record['error']=repr(e)
 finally:
  record['input_after_sha256']={str(v):sha(v) for v in inputs};record['helper_after_sha256']={v.name:sha(v) for v in classes.glob('*.class')};record['inputs_unchanged']=before==record['input_after_sha256'] and record.get('helper_before_sha256')==record['helper_after_sha256']
  if not record['inputs_unchanged']:record['status']='FAIL'
  record['artifacts_sha256']={str(v.relative_to(out)):sha(v) for v in out.rglob('*') if v.is_file() and v.name!='receipt.json'};write(out/'receipt.json',record)
 print(record['status'],out/'receipt.json');need(record['status']!='FAIL','fresh observation failed; retained exact receipt/logs')

if __name__=='__main__':main()
