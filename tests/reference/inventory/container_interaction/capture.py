"""Run unchanged original Container bodies on genuine complete CPU owners."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SUPPORT = ROOT / 'tests/reference/client/controller'
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
 'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
 'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
 'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
 'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
CORE = ('kq','net/minecraft/client/Minecraft','fd','ok','lm','dc','gs','sn','ls','ix','aa','dw','gp','pv','yv',
        'mq','wl','lw','iz','gm','ob','ue','id','da','bb','hl','xi','jl','ep','hk','ec','pm','kv','qb','vu','du','ub','ny','nh','do')

def module(name, path):
 spec=importlib.util.spec_from_file_location(name,path)
 result=importlib.util.module_from_spec(spec)
 spec.loader.exec_module(result)
 return result

C=module('container_subprocess_capture',SUPPORT/'capture.py')
I=module('container_classfile',SUPPORT/'classfile.py')

def unique(pairs):
 result=dict(pairs)
 if len(result)!=len(pairs): raise ValueError('Duplicate JSON field')
 return result

def declarations(cases):
 def locations(value): return ','.join(f'{i}={name}' for i,name in value.items()) or '-'
 result=[]
 for case in cases:
  stacks=';'.join(','.join(map(str,[name,*[s[key] for key in ('id','count','damage','animation')]]))
                  for name,s in case['stacks'].items()) or '-'
  result.append('\t'.join(map(str,[case['profile'],case['operation'],case['window'],case['slot'],case['button'],
    str(case['shift']).lower(),stacks,locations(case['main']),locations(case['armor']),locations(case['craft']),case['cursor'] or '-'])))
 return '\n'.join(result)+'\n'

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--jar',type=Path,required=True)
 parser.add_argument('--libraries',type=Path,required=True)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args()
 jar=args.jar.resolve(strict=True)
 C.ensure(C.digest(jar)==JAR,'untouched original JAR')
 libraries=[(args.libraries/name).resolve(strict=True) for name in LIBRARIES]
 C.ensure(all(C.digest(p)==value for p,value in zip(libraries,LIBRARIES.values())),'pinned supplied libraries')
 java,javac=[Path(shutil.which(name)).resolve(strict=True) for name in ('java','javac')]
 C.ensure(java.parent==javac.parent and '17.' in java.parent.parent.name,'same pinned JDK17')
 jvm=java.parent/'server/jvm.dll' if os.name=='nt' else java.parent.parent/'lib/server/libjvm.so'
 tools=[java,javac,java.parent.parent/'lib/modules',jvm]
 helpers=[HERE/'ContainerInteractionObservation.java',HERE/'ContainerInteractionJdi.java',Path(__file__).resolve(),HERE/'cases.json',SUPPORT/'capture.py',SUPPORT/'classfile.py']
 for name in ('compare.py','entropy.py','audit.py','test_packet.py'):
  if (HERE/name).is_file():helpers.append(HERE/name)
 C.ensure(all(b'\r' not in p.read_bytes() for p in helpers if p.parent==HERE),'public authored helper LF bytes before capture')
 inputs=[jar,*libraries,*tools,*helpers]
 before={str(p):C.digest(p) for p in inputs}
 output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
 classes=output/'classes';classes.mkdir()
 cases=json.loads((HERE/'cases.json').read_text(encoding='utf-8'),object_pairs_hook=unique)['cases']
 declaration=output/'declared-inputs.tsv';declaration.write_text(declarations(cases),encoding='utf-8',newline='\n')
 before[str(declaration)]=C.digest(declaration)
 environment=os.environ.copy()
 for name in ('APPDATA','TMP','TEMP'):
  directory=output/name.lower();directory.mkdir();environment[name]=str(directory)
 report={'status':'FAIL','scope':__doc__,'inputs_before':before}
 try:
  cp=os.pathsep.join(map(str,[classes,jar,*libraries]))
  report['compile']=C.capture([str(javac),'-encoding','UTF-8','-classpath',cp,'-d',str(classes),str(HERE/'ContainerInteractionObservation.java'),str(HERE/'ContainerInteractionJdi.java')],output,'compile',60,environment)
  C.require_success(report['compile'],empty_stderr=True)
  report['helpers_before']={p.name:C.digest(p) for p in classes.iterdir()}
  C.ensure(set(report['helpers_before'])=={'ContainerInteractionObservation.class','ContainerInteractionObservation$Listener.class',
      'ContainerInteractionObservation$Observer.class','ContainerInteractionObservation$1.class','ContainerInteractionJdi.class','ContainerInteractionJdi$Drain.class'},'only authored observation classes')
  report['original']=C.capture([str(java),'-Xint','-Xverify:all','-Xmx256m','-Djava.awt.headless=true','-Djava.security.manager=allow',
      '--add-opens=java.base/java.util=ALL-UNNAMED','-Djava.io.tmpdir='+environment['TMP'],'-classpath',cp,
      'ContainerInteractionObservation',str(jar),str(output/'game'),str(declaration)],output,'original',300,environment,output)
  C.require_success(report['original'])
  stdout=(output/'original.stdout').read_text(encoding='utf-8')
  report['rows']=[json.loads(line.removeprefix('CONTAINER_INTERACTION '),object_pairs_hook=unique)
      for line in stdout.splitlines() if line.startswith('CONTAINER_INTERACTION ')]
  report['item_names']=[json.loads(line.removeprefix('ITEM_NAME '),object_pairs_hook=unique)
      for line in stdout.splitlines() if line.startswith('ITEM_NAME ')]
  C.ensure([r['profile'] for r in report['rows']]==[c['profile'] for c in cases],'all declared original calls completed')
  C.ensure(stdout.splitlines()[-1]==f'Original container interaction: rows={len(cases)}; genuine_owners=1; random_rebinding=0; network=0; native=0','complete ordinary CPU observation')
  report['graph']=[json.loads(line.removeprefix('CONTAINER_GRAPH '),object_pairs_hook=unique) for line in stdout.splitlines() if line.startswith('CONTAINER_GRAPH ')]
  report['screen']=[json.loads(line.removeprefix('INVENTORY_SCREEN '),object_pairs_hook=unique) for line in stdout.splitlines() if line.startswith('INVENTORY_SCREEN ')]
  P=module('container_selected_compare',HERE/'compare.py')
  E=module('container_entropy_audit',HERE/'entropy.py')
  P.validate(report['rows'])
  report['jdi']=C.capture([str(java),'-classpath',cp,'ContainerInteractionJdi',cp,str(jar),str(java.parent.parent),
      str(output/'jdi'),str(output/'jdi-game'),str(declaration)],output,'jdi',300,environment,output)
  C.require_success(report['jdi'],empty_stderr=True)
  jdi_stdout=(output/'jdi/target.stdout').read_text(encoding='utf-8')
  report['jdi_rows']=P.rows(jdi_stdout)
  P.compare(report['jdi_rows'],report['rows'])
  for marker,key in [('ITEM_NAME ','item_names'),('CONTAINER_GRAPH ','graph'),('INVENTORY_SCREEN ','screen')]:
   observed=[json.loads(line.removeprefix(marker),object_pairs_hook=unique) for line in jdi_stdout.splitlines() if line.startswith(marker)]
   C.ensure(observed==report[key],'independent ordinary/JDI '+key+' exact equality')
  report['call_audit']=[json.loads(line,object_pairs_hook=unique) for line in (output/'jdi/calls.ndjson').read_text(encoding='utf-8').splitlines()]
  report['constructor_audit']=E.constructors(report['jdi_rows'],report['call_audit'])
  names=sorted(set(CORE)|{r['class'] for r in report['item_names']}|{r['owner'] for r in report['item_names']})
  with zipfile.ZipFile(jar) as archive:
   report['pins']={'client_jar_sha256':JAR,'scope':'Full original class/method Code inventories; source constructors, Container/Slot/controller/screen and actual Item name owners.',
      'classes':{name:I.inspect_class(archive.read(name+'.class')) for name in names}}
  live=json.loads((output/'jdi/live-Code.json').read_text(encoding='utf-8'),object_pairs_hook=unique)
  for key,value in live.items():
   owner,tail=key.split('.',1)
   method_name,descriptor=tail.split('(',1)
   method=next(m for m in report['pins']['classes'][owner]['methods'] if m['name']==method_name and m['descriptor']=='('+descriptor)
   C.ensure(all(method[k]==v for k,v in value.items()),'actual live Code matches unchanged archive '+key)
  report['live_Code']=live
  for name,value in (('source-pins.json',report['pins']),('selected-outcomes.json',{'schema':1,'rows':report['rows']}),('item-name-inventory.json',{'schema':1,'rows':report['item_names']})):
   (output/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
  report['status']='PASS'
 finally:
  report['inputs_after']={str(p):C.digest(p) for p in [*inputs,declaration]}
  report['helpers_after']={p.name:C.digest(p) for p in classes.iterdir()}
  if report['inputs_after']!=before or report.get('helpers_before')!=report['helpers_after']:report['status']='FAIL'
  (output/'capture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
 C.ensure(report['status']=='PASS','unchanged original inputs and observation helpers')
 print(f'Original container interaction PASS: {len(report["rows"])} rows; {len(report["item_names"])} Item names; {output}')

if __name__=='__main__':main()
