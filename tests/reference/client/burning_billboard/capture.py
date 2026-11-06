"""New post-capture Linux reproduction CLI. A fresh output is not automatically
admitted by the pinned dataset provenance; review and publish new pins first.
"""
from pathlib import Path
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jar',type=Path,required=True)
parser.add_argument('--jdk',type=Path,required=True)
parser.add_argument('--sdk',type=Path,action='append',required=True)
parser.add_argument('--native-archive',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
a=parser.parse_args()
import hashlib,json,os,subprocess,sys,time,zipfile
sys.dont_write_bytecode=True
ROOT=next(p for p in Path(__file__).resolve().parents if(p/'CMakeLists.txt').is_file());BASE=INPUT=Path(__file__).resolve().parent;OUT=a.output.resolve();JDK=a.jdk.resolve()
sys.path.insert(0,str(ROOT/'tests/reference/entity/player'));from support import run_bounded
sys.path.insert(0,str(ROOT/'tests/reference/client/controller'));from classfile import inspect_class
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8',newline='\n')
OUT.mkdir(exist_ok=False);classes=OUT/'classes';classes.mkdir();natives=OUT/'natives';natives.mkdir()
jar=a.jar.resolve();archive=a.native_archive.resolve();sdk=[p.resolve()for p in a.sdk]
assert len(sdk)==4 and sha(jar)=='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c','exact original JAR and four declared SDK jars'
with zipfile.ZipFile(archive) as z:(natives/'liblwjgl64.so').write_bytes(z.read('liblwjgl64.so'))
fixed=[jar,archive,*sdk,JDK/'bin/java',JDK/'bin/javac',JDK/'lib/modules',JDK/'lib/server/libjvm.so']
assert sha(natives/'liblwjgl64.so')=='60a9a67061a3dc6b9d055946c58f1f64792abb2127ba0156eaf735dbee5bc729'
inputs=[*fixed,Path('/usr/bin/gcc'),Path('/usr/bin/ld'),Path('/usr/include/GL/gl.h'),Path('/usr/include/GL/glx.h'),natives/'liblwjgl64.so',Path(__file__),INPUT/'preparation.md',Path('/usr/bin/xvfb-run'),Path('/usr/bin/Xvfb'),Path('/usr/bin/xauth'),Path(sys.executable).resolve(),ROOT/'tests/reference/entity/player/support.py',ROOT/'tests/reference/client/controller/classfile.py',*[INPUT/n for n in ('BurningBillboardTarget.java','BurningBillboardJdi.java','ConstructorJson.java','driver_observer.c','profiles.tsv','preparation.tsv')]]
closure=subprocess.check_output(['/usr/bin/ldd','/usr/lib/x86_64-linux-gnu/libGL.so.1','/usr/lib/x86_64-linux-gnu/dri/swrast_dri.so'],text=True);(OUT/'driver-library-closure.txt').write_text(closure)
import re
inputs += [Path(p) for p in sorted(set(re.findall(r'(?:=> )?(/[^\s]+)',closure))) if Path(p).is_file()]
inputs += [Path('/usr/bin/ldd'),Path('/usr/bin/as'),Path(subprocess.check_output(['/usr/bin/gcc','-print-prog-name=cc1'],text=True).strip())]
before={str(p):sha(p) for p in inputs};write(OUT/'input-freeze.json',{'scope':'Before actual helper compile and ordinary/read-only-JDI unchanged original execution','input_sha256':before})
with zipfile.ZipFile(jar) as z:authority={n[:-6]:inspect_class(z.read(n)) for n in z.namelist() if n.endswith('.class')}
write(OUT/'authority.json',{'jar_sha256':sha(jar),'classes':authority})
names=sorted(authority);env=os.environ.copy()
for k in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH','DISPLAY'):env.pop(k,None)
env['LIBGL_ALWAYS_SOFTWARE']='1'
record={'status':'FAIL','scope':'Fresh unchanged original seven declared BW burning billboard profiles; no native parity or original full-game claim','default_constructor_entropy_rebound':False,'input_before_sha256':before,'commands':{},'environment':{}}
try:
 cmd=['/usr/bin/gcc','-std=c11','-O2','-fPIC','-shared','-fno-fast-math','-o',OUT/'driver_observer.so',INPUT/'driver_observer.c','-ldl','-lGL'];record['observer_compile_command']=list(map(str,cmd));r=run_bounded(cmd,OUT,'observer-compile',timeout=120,env=env);record['observer_compile_exit']=r.returncode;assert r.returncode==0
 record['observer_before_sha256']=sha(OUT/'driver_observer.so')
 cmd=[JDK/'bin/javac','--add-modules','jdk.jdi','-cp',os.pathsep.join(map(str,[jar,*sdk])),'-d',classes,*sorted(INPUT.glob('*.java'))]
 record['compile_command']=list(map(str,cmd));r=run_bounded(cmd,OUT,'compile',timeout=120,env=env);record['compile_exit']=r.returncode;assert r.returncode==0
 record['helper_before_sha256']={p.name:sha(p) for p in classes.glob('*.class')};write(OUT/'helper-Code.json',{p.name:inspect_class(p.read_bytes()) for p in classes.glob('*.class')})
 for mode in ('normal','debugger','nohook'):
  directory=OUT/mode;directory.mkdir();(directory/'resources').mkdir();(directory/'classes.txt').write_text('\n'.join(names)+'\n')
  cp=os.pathsep.join(map(str,[classes,directory/'resources',jar,*sdk]));xvfb=['/usr/bin/xvfb-run','-a','-s','-screen 0 640x480x24 -noreset']
  if mode!='debugger':cmd=[*xvfb,JDK/'bin/java','-Xint','-Xmx512m','-Xverify:all','--add-opens=java.base/java.util=ALL-UNNAMED','--add-opens=java.base/java.lang=ALL-UNNAMED','-Djava.awt.headless=true','-Djava.security.manager=allow','-Dorg.lwjgl.librarypath='+str(natives),'-Dmobdraw.classes='+str(directory/'classes.txt'),'-Xlog:class+init=info:file=class-init.log','-cp',cp,'BurningBillboardTarget',directory,INPUT]
  else:cmd=[*xvfb,JDK/'bin/java','--add-opens=jdk.jdi/com.sun.tools.jdi=ALL-UNNAMED','--add-modules','jdk.jdi','-cp',classes,'BurningBillboardJdi',directory,JDK,natives,cp,jar,INPUT]
  mode_env=env.copy();mode_env['LD_PRELOAD']=str(OUT/'driver_observer.so');mode_env['BILLBOARD_GL_LOG']=str(directory/'driver-gl.ndjson');# Environment is recorded after optional observer removal below.
  if mode=='nohook':mode_env.pop('LD_PRELOAD',None);mode_env.pop('BILLBOARD_GL_LOG',None)
  record['environment'][mode]={k:mode_env[k]for k in ('LD_PRELOAD','BILLBOARD_GL_LOG','LIBGL_ALWAYS_SOFTWARE')if k in mode_env}
  record['commands'][mode]=list(map(str,cmd));print('START',mode,flush=True);start=time.monotonic();r=run_bounded(cmd,directory,'process',timeout=1800,env=mode_env);record[mode+'_seconds']=time.monotonic()-start;record[mode+'_exit']=r.returncode;assert r.returncode==0,mode
  rows=[json.loads(l) for l in (directory/'target.ndjson').read_text().splitlines()];draws=[r for r in rows if r['event']=='draw'];record[mode+'_draws']=[{k:r[k] for k in ('index','kind','outcome','failure','gl_error')} for r in draws];assert len(draws)==7 and all(r['outcome']==1 and r['gl_error']==0 for r in draws),mode+'draws';print('PASS',mode,record[mode+'_seconds'],flush=True)
 record['driver_draws']={m:len((OUT/m/'driver-gl.ndjson').read_text().splitlines()) for m in ('normal','debugger')};assert record['driver_draws']=={'normal':13,'debugger':13},record['driver_draws']
 record['loaded_runtime_files_sha256']={}
 for m in ('normal','debugger','nohook'):
  maps=(OUT/m/'process-maps.txt').read_text();paths=sorted(set(re.findall(r'(/[^\n]+)',maps)));record['loaded_runtime_files_sha256'][m]={p:sha(p) for p in paths if Path(p).is_file()}
 record['status']='OBSERVED_SEVEN_ORIGINAL_BURNING_BILLBOARD_PROFILES'
except BaseException as e:record['error']=repr(e)
finally:
 record['observer_after_sha256']=sha(OUT/'driver_observer.so') if (OUT/'driver_observer.so').exists() else None
 record['input_after_sha256']={str(p):sha(p) for p in inputs};record['helper_after_sha256']={p.name:sha(p) for p in classes.glob('*.class')};record['inputs_unchanged']=before==record['input_after_sha256'] and record.get('helper_before_sha256',{})==record['helper_after_sha256'] and record.get('observer_before_sha256')==record['observer_after_sha256']
 if not record['inputs_unchanged']:record['status']='FAIL'
 record['artifacts_sha256']={str(p.relative_to(OUT)):sha(p) for p in OUT.rglob('*') if p.is_file() and p.name!='receipt.json'};write(OUT/'receipt.json',record);print(json.dumps({k:record.get(k) for k in ('status','error','compile_exit','normal_exit','debugger_exit','inputs_unchanged')}),flush=True)
assert record['status']=='OBSERVED_SEVEN_ORIGINAL_BURNING_BILLBOARD_PROFILES'
