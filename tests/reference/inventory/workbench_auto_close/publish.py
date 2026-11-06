"""Publish retained source evidence without invoking original game or Java."""
import argparse
import base64
import hashlib
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
SPEC=importlib.util.spec_from_file_location('auto_close_publication_packet',HERE/'packet.py');Q=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(Q)

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')

def transport(path):
 data=path.read_bytes();return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'base64':base64.b64encode(data).decode('ascii')}

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True);args=parser.parse_args();capture=args.capture.resolve()
 report=json.loads((capture/'capture.json').read_text(encoding='utf-8'),object_pairs_hook=Q.unique)
 Q.require(report['status']=='PASS' and report['inputs_before']==report['inputs_after'] and report['helpers_before']==report['helpers_after'],'complete unchanged successful cohort')
 Q.require(all(digest(path)==value for path,value in report['inputs_before'].items()),'captured inputs unchanged at publication')
 ordinary=Q.P.rows((capture/'original.stdout').read_text(encoding='utf-8'));observed=Q.P.rows((capture/'debugger/target.stdout').read_text(encoding='utf-8'))
 events=[json.loads(line,object_pairs_hook=Q.unique) for line in (capture/'debugger/journal.ndjson').read_text(encoding='utf-8').splitlines()]
 Q.A.observations(ordinary,observed,events,report['cases']);Q.typed_rows(ordinary,report['cases']);Q.typed_rows(observed,report['cases']);Q.trace_identities(observed,events)
 # Preserve original serialized finite/source-pin bytes rather than regenerate them.
 for name in ('finite.json','source-pins.json'):(HERE/name).write_bytes((capture/name).read_bytes())
 observations={'schema':1,'rows':observed,'events':events,'live_Code':report['live_Code'],'library_pins':report['library_pins'],'jdk_pins':report['jdk_pins'],
  'transport':{name:transport(capture/name) for name in ('original.stdout','original.stderr','debugger/target.stdout','debugger/target.stderr','debugger/journal.ndjson')},'capture_audit':report['audit']}
 write(HERE/'observations.json',observations)
 helpers=('WorkbenchAutoCloseTarget.java','WorkbenchAutoCloseJdi.java','capture.py','audit.py','compare.py','entropy.py','declared-inputs.json')
 publication=('packet.py','sdk_compare.py','publish.py','test_packet.py','README.md')
 failed=[]
 for suffix,classification,detail in [
  ('1','observer-focus-recursion-assertion','Both processes completed all seven calls with no invocation exception; final observer audit incorrectly required one focus child and rejected actual source recursive display(null).'),
  ('2','archive-class-name-lookup','Both processes and source state/ordering/suffix/entropy audits passed; final pin lookup used dotted Minecraft name against the archive slash-separated key.')]:
  path=capture.parent/('workbench-auto-close-prepared-'+suffix)
  if not (path/'capture.json').is_file():
   # Reproduction need not recreate an already published historical observer failure.
   prior=json.loads((HERE/'provenance.json').read_text(encoding='utf-8'),object_pairs_hook=Q.unique)
   failed.append(next(item for item in prior['failed_cohorts'] if item['classification']==classification));continue
  old=json.loads((path/'capture.json').read_text(encoding='utf-8'))
  Q.require(old['status']=='FAIL' and old['original']['exit']==old['debugger']['exit']==0,'retained actual failed observer/pin cohort')
  failed.append({'path':str(path.relative_to(ROOT)).replace('\\','/'),'classification':classification,'detail':detail,'status':old['status'],'capture_sha256':digest(path/'capture.json'),
   'unchanged_inputs':old['inputs_before']==old['inputs_after'],'unchanged_compiled_helpers':old['helpers_before']==old['helpers_after'],
   'artifacts':{name:transport(path/name) for name in ('original.stdout','original.stderr','debugger/target.stdout','debugger/target.stderr','debugger/journal.ndjson')}})
 provenance={'schema':1,'status':'PASS','client_jar_sha256':Q.A.JAR,'capture_path':str(capture.relative_to(ROOT)).replace('\\','/'),'capture_sha256':digest(capture/'capture.json'),
  'scope':{'source-calls-per-process':7,'composed-gs-ticks-per-process':4,'composed-close-calls-per-process':3,'direct-dc-controls-per-process':1,'predicate-controls-per-process':2,'whole-world-ticks':0,'source-publications-per-process':9,'pixels':False,'native-execution':False},
  'capture_helpers':{name:report['inputs_before'][str((HERE/name).resolve())] for name in helpers},'public_helpers':{name:digest(HERE/name) for name in helpers},'compiled_helpers':report['helpers_before'],
  'publication_helpers':{name:digest(HERE/name) for name in publication},'outputs':{name:digest(HERE/name) for name in ('finite.json','observations.json','source-pins.json')},
  'verified_inputs':[{'path':name,'before':value,'after':report['inputs_after'][name]} for name,value in report['inputs_before'].items()],
  'failed_cohorts':failed,'limits':['Actual GS.w_ invocations do not include the enclosing World Entity schedule or later dropped-Item ticks.','Native Math.cos/sin do not emit JDI entry/return events on this JVM; reconstructed source arguments and pure-JDK leaves are independently bracket-audited, not described as intercepted native returns.','Source focus is sampled and executed naturally; no general desktop focus-stability result.','No original pixels, complete startup, saved-world or native result is supplied by this packet.']}
 write(HERE/'provenance.json',provenance)
 print(json.dumps(Q.audit_public(),indent=2))

if __name__=='__main__':main()
