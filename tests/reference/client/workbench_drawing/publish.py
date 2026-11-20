"""Publish the pinned real-Display Workbench capture without game execution."""
import argparse
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('workbench_drawing_publish_packet',HERE/'packet.py')
B=importlib.util.module_from_spec(S);S.loader.exec_module(B)
CAPTURED=('WorkbenchDrawingTarget.java','WorkbenchDrawingJdi.java','ConstructorJson.java','capture.py','audit.py','declared-inputs.json')
PUBLICATION=('compare.py','packet.py','test_packet.py','publish.py')

def write(name,value):(HERE/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8',newline='\n')
def raw(path):return Path(path).read_bytes().decode('utf8')

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True);args=parser.parse_args()
 directory=args.capture.resolve(strict=True);report=json.loads((directory/'capture.json').read_text(encoding='utf8'),object_pairs_hook=B.P.unique)
 B.require(report['status']=='PASS' and report['inputs_before']==report['inputs_after'] and report['helpers_before']==report['helpers_after'],'all capture gates and input continuity')
 for path,expected in report['inputs_before'].items():B.require(B.digest(path)==expected,'unchanged executed input '+path)
 for name in CAPTURED:B.require(report['inputs_before'][str((HERE/name).resolve())]==B.digest(HERE/name),'executed public helper '+name)
 ordinary=raw(directory/'original.stdout');observed=raw(directory/'debugger/target.stdout')
 events=[json.loads(line,object_pairs_hook=B.P.unique) for line in raw(directory/'debugger/journal.ndjson').splitlines()]
 summary=B.audit_events(ordinary,observed,events)
 evidence={'schema':1,'ordinary_stdout':ordinary,'ordinary_stderr':raw(directory/'original.stderr'),'jdi_stdout':observed,'jdi_stderr':raw(directory/'debugger/target.stderr'),
  'events':events,'live_Code':report['live_Code']}
 draws=[event for event in events if event['event']=='nw-draw']
 write('finite.json',{'schema':1,'client_jar_sha256':B.A.JAR,'scope':'Four complete original gradient/tile draws selected from two genuine OO/ID Workbench draws; native entered prefixes have their own scope.','rows':[B.from_draw(row) for row in draws]})
 write('block-scalars.json',{'schema':1,'client_jar_sha256':B.A.JAR,'scope':'132 actual ordinary VG17/FI58 registered-Block scalar calls; independent JDI-process target results agree. UU.g retains its own exact primitive before/after state. No source field is modified for these queries.','declaration-source':'captured WorkbenchDrawingTarget.scalars() literals, frozen before execution','signed-metadata':[-(1<<31),-17,-1,0,1,2,3,15,16,(1<<31)-1],'signed-sides':[-(1<<31),-1,0,1,2,3,4,5,6,(1<<31)-1],'rows':B.typed_target(ordinary)['scalars']})
 write('observations.json',evidence);write('source-pins.json',report['pins'])
 declarations=json.loads((HERE/'declared-inputs.json').read_text(encoding='utf8'),object_pairs_hook=B.P.unique)
 provenance={'schema':1,'client_jar_sha256':B.A.JAR,'capture':{'path':str(directory),'sha256':B.digest(directory/'capture.json'),'status':report['status'],'audit':{key:value for key,value in summary.items() if key!='finite'}},
  'scope':'Genuine complete KQ constructor and OO/IT/ID/Font/Engine real LWJGL Display component scene; complete ordinary and read-only-JDI inherited draw returns. No pixel/native/full-startup qualification.',
  'inputs_before':report['inputs_before'],'inputs_after':report['inputs_after'],'compiled_helpers_before':report['helpers_before'],'compiled_helpers_after':report['helpers_after'],
  'captured_helpers':{name:B.digest(HERE/name) for name in CAPTURED},'publication_helpers':{name:B.digest(HERE/name) for name in PUBLICATION},
  'outputs':{name:B.digest(HERE/name) for name in ('finite.json','block-scalars.json','observations.json','source-pins.json')},
  'original_stdout_sha256':B.digest(directory/'original.stdout'),'jdi_stdout_sha256':B.digest(directory/'debugger/target.stdout'),
  'original_stderr_sha256':B.digest(directory/'original.stderr'),'jdi_stderr_sha256':B.digest(directory/'debugger/target.stderr'),
  'private_artifacts':{str(path.relative_to(directory)):B.digest(path) for path in (directory/'debugger/journal.ndjson',directory/'debugger/live-Code.json',directory/'compile.capture.json',directory/'original.capture.json',directory/'debugger/controller.capture.json')},
  'failed_cohorts':declarations['historical-failed-cohorts'],'assets':report['assets'],'device':B.A.target(ordinary)['device'],
  'owner_scope':'Actual OO(actual IX,actual World,8,64,8) constructor; DA.initialize invokes inherited ID.b publishing e=j. Constructor AA remains d. Actual source setters declare main/crafting inputs and second-call cursor. No invocation field repair, RAW owner, game subclass/substitution, entropy rebinding or backing clear.',
  'input_scope':'No DC.t/Input edge is reached in OO/ID draw; genuine constructor-null Input remains null. No UE preview/TH/DS qualification is inferred.',
  'NW_scope':'All72 NW invocations within the two source draws return. Each complete buffer retains exact same-index own inactive backing. Selected gradient/tile four records retain192raw/108active/84inactive words; all72 retain3456raw/2400active/1056inactive. Precompiled Font glyph display-list geometry is outside the live NW capture; actual Font source calls and labels/tooltip arguments/returns are retained.',
  'publication_scope':'Executed six authored helper/input files were frozen LF before capture; publication validators were authored separately afterward and run no original game code. UTF8 stdout/stderr retain their exact original bytes including line endings.',
  'native_scope':'No native execution is represented by this original packet; separate native receipts qualify entered-prefix comparison and full SDK independently.'}
 write('provenance.json',provenance);print(B.audit_public())

if __name__=='__main__':main()
