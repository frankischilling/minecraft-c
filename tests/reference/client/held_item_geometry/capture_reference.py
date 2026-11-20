import argparse,importlib.util,json,os,shutil,sys,zipfile
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
C=module('geometry_capture',ROOT/'tests/reference/client/controller/capture.py')
I=module('geometry_classfile',ROOT/'tests/reference/client/controller/classfile.py')
G=module('geometry_source_audit',HERE/'audit.py')
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES={
 'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n')
def tagged(text,prefix):return [json.loads(l.removeprefix(prefix+' ')) for l in text.splitlines() if l.startswith(prefix+' ')]
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--observe',action='store_true');p.add_argument('--jar',type=Path,default=ROOT/'b1.7.3.jar');p.add_argument('--libraries',type=Path,default=Path(os.environ.get('APPDATA',''))/'.minecraft/libraries');a=p.parse_args()
 C.ensure(os.name=='nt','this pinned native/Pbuffer capture profile is Windows x64')
 out=a.output.resolve()
 if a.observe:C.ensure(out.is_dir(),'prepared output exists')
 else:out.mkdir(parents=True,exist_ok=False)
 if not a.observe:
  classes=out/'classes';classes.mkdir();natives=out/'natives';natives.mkdir()
  jar=a.jar.resolve(strict=True);C.ensure(C.digest(jar)==JAR,'untouched original archive')
  libroot=a.libraries.resolve(strict=True);libraries=[libroot/n for n in LIBRARIES]
  C.ensure(all(C.digest(p)==v for p,v in zip(libraries,LIBRARIES.values())),'pinned libraries')
  archive=libroot/'org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar'
  C.ensure(C.digest(archive)=='68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43','pinned native archive')
  with zipfile.ZipFile(archive) as z:(natives/'lwjgl64.dll').write_bytes(z.read('lwjgl64.dll'))
  java,javac=[Path(shutil.which(n)).resolve(strict=True) for n in ('java','javac')]
  C.ensure(java.parent==javac.parent and '17.' in java.parent.parent.name,'JDK17')
  files=[jar,*libraries,archive,natives/'lwjgl64.dll',java,javac,java.parent.parent/'lib/modules',java.parent/'server/jvm.dll',
   HERE/'HeldGeometryTarget.java',HERE/'HeldGeometryJdi.java',HERE/'ConstructorJson.java',HERE/'audit.py',HERE/'compare.py',HERE/'declared-inputs.json',Path(__file__).resolve(),
   ROOT/'tests/reference/client/controller/capture.py',ROOT/'tests/reference/client/controller/classfile.py']
  files += [q for q in java.parent.glob('*.dll') if q.is_file()]
  env=os.environ.copy()
  for n in ('APPDATA','TMP','TEMP'):
   d=out/n.lower();d.mkdir();env[n]=str(d)
  cp=os.pathsep.join(map(str,[classes,jar,*libraries]))
  report={'status':'PREPARED','inputs_before':{str(p.resolve()):C.digest(p) for p in files},'environment':{n:env[n] for n in ('APPDATA','TMP','TEMP')},'java':str(java),'classpath':cp,'jar':str(jar),'natives':str(natives)}
  report['compile']=C.capture([str(javac),'-encoding','UTF-8','--add-modules','jdk.jdi','-cp',cp,'-d',str(classes),*[str(HERE/n) for n in ('HeldGeometryTarget.java','HeldGeometryJdi.java','ConstructorJson.java')]],out,'compile',60,env,out)
  C.require_success(report['compile'],empty_stderr=True)
  report['helpers_before']={p.name:C.digest(p) for p in classes.iterdir()}
  with zipfile.ZipFile(jar) as z:
   names=['kq','net/minecraft/client/Minecraft','ra','nw','ls','gs','iz','gm','qd','dc','ji','ik','iy','sj','fd','lm','qz']
   report['pins']={'client_jar_sha256':JAR,'classes':{n:I.inspect_class(z.read(n+'.class')) for n in names}}
   report['resources']={n:{'bytes':len(z.read(n)),'sha256':__import__('hashlib').sha256(z.read(n)).hexdigest()} for n in ('gui/items.png','font/default.png','pack.png')}
  write(out/'source-pins.json',report['pins']);write(out/'capture.json',report);print('Prepared genuine-owner hidden-context probe: '+str(out));return
 report=json.loads((out/'capture.json').read_text());C.ensure(report['status']=='PREPARED','prepared state')
 C.ensure(all(C.digest(p)==v for p,v in report['inputs_before'].items()),'unchanged prepared inputs')
 env=os.environ.copy();env.update(report['environment']);java=Path(report['java']);natives=report['natives'];cp=report['classpath'];jar=report['jar']
 report['status']='FAIL'
 try:
  report['original']=C.capture([str(java),'-Xint','-Xverify:all','-Xmx256m','-Djava.awt.headless=true','-Djava.security.manager=allow','-Dorg.lwjgl.librarypath='+natives,'-cp',cp,'HeldGeometryTarget',jar,str(out/'game'),natives],out,'original',120,env,out)
  C.require_success(report['original'])
  original=(out/'original.stdout').read_text();C.ensure(len(tagged(original,'HELD_GEOMETRY_COMPLETE'))==1,'complete ordinary genuine draw')
  debug=out/'debugger';debug.mkdir()
  report['debugger']=C.capture([str(java),'--add-modules','jdk.jdi','-Xmx256m','-cp',str(out/'classes'),'HeldGeometryJdi',str(debug),str(java.parent.parent),natives,cp,jar],debug,'controller',240,env,debug)
  C.require_success(report['debugger'],empty_stderr=True)
  target=(debug/'target.stdout').read_text();C.ensure(len(tagged(target,'HELD_GEOMETRY_COMPLETE'))==1,'complete JDI genuine draw')
  for label in ('HELD_GEOMETRY_INITIAL','HELD_GEOMETRY_RETURN'):
   C.ensure(tagged(original,label)==tagged(target,label),'ordinary/read-only JDI exact final buffers and actual NW defaults')
  events=[json.loads(l) for l in (debug/'journal.ndjson').read_text().splitlines()]
  report['source_audit']=G.audit(report,original,target,events)
  batches=[e for e in events if e['event']=='nw-draw'];C.ensure(len(batches)==36,'six actual batches per profile')
  inputs=tagged(target,'HELD_GEOMETRY_INPUT');profiles={i['profile']:i for i in inputs}
  finite=[{'icon':profiles[b['profile']]['icon'],'batch':b['batch'],'words':b['words']} for b in batches]
  C.ensure([(r['icon'],r['batch']) for r in finite]==[(i,b) for i in (64,65,66,67,68,21) for b in range(6)],'exact Sword/Bow profile order')
  for row in finite:
   C.ensure(len(row['words'])==(48 if row['batch']<2 else 768),'exact actual raw word count')
   C.ensure(all(row['words'][i]=='00000000' for i in range(len(row['words'])) if i%8 in (5,7)),'observed fresh inactive color/reserved slots remain zero')
  for pin in [e for e in events if e['event']=='live-code']:
   actual=next(m for m in report['pins']['classes'][pin['class']]['methods'] if m['name']==pin['method'] and m['descriptor']==pin['descriptor'])
   C.ensure(actual['code_sha256']==pin['sha256'] and actual['code_bytes']==pin['bytes'],'live unchanged source Code')
  report['finite']=finite;report['ordinary_device']=tagged(original,'HELD_GEOMETRY_DEVICE')[0];report['ordinary_owners']=tagged(original,'HELD_GEOMETRY_OWNERS')[0]
  report['status']='PASS';write(out/'finite.json',{'scope':'Direct unchanged-original RA six actual NW draw-entry buffers for five Swords and Bow, with genuine App constructor/Engine/Player/Stack/NW and hidden Pbuffer; no fullframe/pixel claim.','rows':finite})
 finally:
  report['inputs_after']={p:C.digest(p) for p in report['inputs_before']};report['helpers_after']={p.name:C.digest(p) for p in (out/'classes').iterdir()}
  if report['inputs_before']!=report['inputs_after'] or report['helpers_before']!=report['helpers_after']:report['status']='FAIL'
  write(out/'capture.json',report)
 C.ensure(report['status']=='PASS','complete unchanged original capture');print('Direct original Sword/Bow geometry PASS:36 buffers; '+str(out))
if __name__=='__main__':main()
