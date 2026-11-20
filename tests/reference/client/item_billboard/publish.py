"""Publish the pinned original Item billboard capture without game execution."""
import argparse
import importlib.util
import json
from pathlib import Path
HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('billboard_publish_packet',HERE/'packet.py');B=importlib.util.module_from_spec(S);S.loader.exec_module(B)
CAPTURED=('ItemBillboardTarget.java','ItemBillboardJdi.java','ConstructorJson.java','capture.py','audit.py','declared-inputs.json')
PUBLICATION=('compare.py','packet.py','publish.py','test_packet.py')
def write(name,value):(HERE/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8',newline='\n')
def raw(path):return path.read_bytes().decode('utf8')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',type=Path,required=True);args=p.parse_args();directory=args.capture.resolve(strict=True)
 r=json.loads(raw(directory/'capture.json'),object_pairs_hook=B.P.unique)
 B.require(r['status']=='PASS' and r['inputs_before']==r['inputs_after'] and r['helpers_before']==r['helpers_after'],'actual source observation gates/input continuity')
 for path,expected in r['inputs_before'].items():B.require(B.digest(path)==expected,'unchanged executed input '+path)
 for name in CAPTURED:B.require(B.digest(HERE/name)==r['inputs_before'][str((HERE/name).resolve())],'captured helper '+name)
 observed={'schema':1,'ordinary_stdout':raw(directory/'original.stdout'),'ordinary_stderr':raw(directory/'original.stderr'),'jdi_stdout':raw(directory/'debugger/target.stdout'),'jdi_stderr':raw(directory/'debugger/target.stderr'),
  'events':[json.loads(line,object_pairs_hook=B.P.unique) for line in raw(directory/'debugger/journal.ndjson').splitlines()],'live_Code':r['live_Code']}
 result=B.audit_events(observed['ordinary_stdout'],observed['jdi_stdout'],observed['events'])
 write('finite.json',{'schema':1,'client_jar_sha256':B.A.JAR,'scope':'Ten local NW rows selected from four actual original BB parent calls; originaldraw1/nativeentered0. Raw bob/phase/World/alias/GL evidence remains separate, unnormalized.','rows':[B.from_draw(row) for row in result['draws']]})
 write('observations.json',observed);write('source-pins.json',{'schema':1,**r['pins'],'libraries':r['library_pins']})
 declarations=json.loads(raw(HERE/'declared-inputs.json'),object_pairs_hook=B.P.unique)
 provenance={'schema':1,'client_jar_sha256':B.A.JAR,'capture':{'path':str(directory),'sha256':B.digest(directory/'capture.json'),'status':r['status'],'actual_profiles':4,'actual_source_constructors':4,'actual_completed_NW_draws':10},
  'inputs_before':r['inputs_before'],'inputs_after':r['inputs_after'],'compiled_before':r['helpers_before'],'compiled_after':r['helpers_after'],
  'captured_helpers':{name:B.digest(HERE/name) for name in CAPTURED},'publication_helpers':{name:B.digest(HERE/name) for name in PUBLICATION},
  'outputs':{name:B.digest(HERE/name) for name in ('finite.json','observations.json','source-pins.json')},'streams':{name:__import__('hashlib').sha256(observed[name].encode('utf8')).hexdigest() for name in ('ordinary_stdout','ordinary_stderr','jdi_stdout','jdi_stderr')},
  'private_artifacts':{str(path.relative_to(directory)):B.digest(path) for path in (directory/'debugger/journal.ndjson',directory/'debugger/live-Code.json',directory/'compile.capture.json',directory/'original.capture.json',directory/'debugger/controller.capture.json')},
  'failed_cohorts':declarations['historical-failed-cohorts'],'superseded_prepare':declarations['superseded-prepare'],'assets':r['assets'],
  'owner_scope':'Genuine complete App/World/Engine/Font/Player, actual TH.frame, canonical BB, four full HL constructors with their actual distinct Stacks. Items retain actual World references but are not published to World list by helper. No OO close/drop caller, full World traversal or Block-flat branch is executed.',
  'entropy_scope':'Raw ordinary and JDI constructor phase/yaw/velocity remain independent and intact. JDI observes four actual Math returns per full constructor and checks formulas. Every process audits actual renderer seed after source setSeed187 and copy-count progression; JDI additionally observes real setSeed/nextFloat/copy translations and own-phase bob sine/GL arguments. No helper Random write, entropy rebinding, phase normalization or cross-VM alias is used.',
  'NW_scope':'Ten original BB draw776 calls complete, with480raw/340active/140owninactive words plus160unwritten tail words unchanged. Native shared prefix673through771 enters realNW and completes0draws; compare only selected local geometry/state after own-backing audit. GL parent setup/first reads and before/after state are retained; no native full parent/pixel claim.',
  'GL_scope':'Actual whole-parent queried matrices/stacks/flags/color/error and actual source GL call arguments/returns. Read-only JDI does not invoke GL queries inside a stopped target; no per-prefix driver-matrix read is claimed.',
  'descriptive_prepare_label':'The private capture retains copied generic two-draw preparation prose. It describes prepare-only setup; actual receipts and public scope are four BB calls/ten completed quads. Captured helper/result bytes are preserved.',
  'publication_scope':'Six executed helpers/input files frozen ASCII/LF before capture; post-capture validators separately hashed. Exact UTF8 stdout/stderr bytes preserved. No source class/World field/Stack/renderer substitute or repair. Root run_native.py is separate uncaptured integration and excluded from original manifests.'}
 write('provenance.json',provenance);print(B.audit_public())
if __name__=='__main__':main()
