"""Publish bounded original cold failure and fresh-frame warm observations."""
import argparse
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('preview_publish_packet',HERE/'packet.py');B=importlib.util.module_from_spec(S);S.loader.exec_module(B)
CAPTURED=('PlayerPreviewCaughtTarget.java','PlayerPreviewCaughtJdi.java','ConstructorJson.java','capture.py','audit.py','declared-inputs.json','line_metadata.py')
PUBLICATION=('packet.py','publish.py','test_packet.py')
def write(name,value):(HERE/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8',newline='\n')
def raw(path):return path.read_bytes().decode('utf8')

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True);args=parser.parse_args()
 directory=args.capture.resolve(strict=True);report=json.loads(raw(directory/'capture.json'),object_pairs_hook=B.A.unique)
 B.require(report['status']=='PASS' and report['inputs_before']==report['inputs_after'] and report['helpers_before']==report['helpers_after'],'bounded observation protocol and unchanged input/helper bytes')
 for name,expected in report['inputs_before'].items():B.require(B.digest(name)==expected,'executed input unchanged '+name)
 for name in CAPTURED:B.require(B.digest(HERE/name)==report['inputs_before'][str((HERE/name).resolve())],'executed authored helper '+name)
 metadata={'schema':1,'client_jar_sha256':B.A.JAR,'original':report['line_metadata'],'authored':report['helper_line_metadata']}
 observed={'schema':1,'modes':{}};selected={};streams={};jdk={}
 for mode in ('cold','warm'):
  root=directory/mode;debug=root/'debugger';record={'ordinary_stdout':raw(root/'ordinary.stdout'),'ordinary_stderr':raw(root/'ordinary.stderr'),'jdi_stdout':raw(debug/'target.stdout'),'jdi_stderr':raw(debug/'target.stderr'),
   'ordinary_exit':report['modes'][mode]['ordinary']['exit'],'jdi_exit':report['modes'][mode]['target_exit'],'events':[json.loads(line,object_pairs_hook=B.A.unique) for line in raw(debug/'journal.ndjson').splitlines()],'live_Code':report['modes'][mode]['live_Code']}
  observed['modes'][mode]=record;selected[mode]=B.select_mode(mode,record,metadata)
  streams[mode]={key:__import__('hashlib').sha256(record[key].encode('utf8')).hexdigest() for key in ('ordinary_stdout','ordinary_stderr','jdi_stdout','jdi_stderr')}
  for key,value in record['live_Code'].items():
   if value['bootstrap-loader']:
    B.require(key not in jdk or jdk[key]==value,'stable actual bootstrap JDK Code identity');jdk[key]=value
 write('source-pins.json',{'schema':1,**report['pins']});write('line-metadata.json',metadata)
 write('jdk-live-code.json',{'schema':1,'scope':'Actual bootstrap-loader JDK17 Throwable print and NPE.getMessage Code/LineNumberTables; pinned runtime image/JVM inputs are in provenance.','methods':jdk})
 write('observations.json',observed)
 write('finite.json',{'schema':1,'client_jar_sha256':B.A.JAR,'scope':'One fatal cold UE attempt: caught Engine NPE is printed, then distinct camera NPE escapes GV suffix. Fresh process actual TH.frame then two warm UE returns. No cold success, Native, pixel or full-game qualification.','modes':selected})
 declarations=json.loads(raw(HERE/'declared-inputs.json'),object_pairs_hook=B.A.unique)
 provenance={'schema':1,'client_jar_sha256':B.A.JAR,'capture':{'path':str(directory),'sha256':B.digest(directory/'capture.json'),'status':report['status'],'scope':'PASS describes faithful bounded observation, including expected cold source exit1; it does not relabel that source call a success.'},
  'inputs_before':report['inputs_before'],'inputs_after':report['inputs_after'],'compiled_before':report['helpers_before'],'compiled_after':report['helpers_after'],
  'captured_helpers':{name:B.digest(HERE/name) for name in CAPTURED},'publication_helpers':{name:B.digest(HERE/name) for name in PUBLICATION},
  'outputs':{name:B.digest(HERE/name) for name in ('finite.json','observations.json','source-pins.json','line-metadata.json','jdk-live-code.json')},'streams':streams,
  'failed_cohort':declarations['historical-failed-cohort'],'assets':report['assets'],
  'private_artifacts':{str(path.relative_to(directory)):B.digest(path) for mode in ('cold','warm') for path in (directory/mode/'debugger/journal.ndjson',directory/mode/'debugger/live-Code.json',directory/mode/'debugger/target-exit.json')},
  'cold_scope':'Genuine complete constructor graph, LR(actual Settings) protocol and actual TH.a(World); e/h/k/p remain NULL. Actual first Engine NPE is caught and printed, then SN.f camera NPE escapes Source GV suffix. Authored finally only observes retained state, then JVM uncaught handler prints second diagnostic; no recovery or state/GL repair. First cold call fails, so a second cold call is not executed.',
  'warm_scope':'Fresh process/owners; actual TH.frame body called once with genuine World/Engine/Font/Player/Settings, then two successful UE/ID draws. No cache seeding or failed-scene field reset. Complete N World traversal is not executed by this probe.',
  'trace_scope':'Exact raw stderr agrees ordinary/JDI. The caught Throwable is the actual NPE; read-only JDI observes original identity, NULL detailMessage, materialized helpful extendedMessage and actual StackTraceElement array after real print. Original creation frames, authored caller and JVM uncaught printer callers remain distinct. No native or full-game caller tail equality is claimed.',
  'JDK_scope':'Source GV invokes Exception.printStackTrace; actual inherited declaring body is bootstrap Throwable. Helpful messages and JDK Code/line metadata qualify this pinned JDK17 context, not every historical JVM.',
  'GL_scope':'Real Display and selected queried matrices/stacks/flags/error state. Cold finally retains partial UE transform/bE1/depth2/rescale state. Cold DS cleanup/UE restoration do not complete. No pixels or full GL-state parity.',
  'publication_scope':'Seven executed helper/input files frozen LF before execution; publication validators authored afterward and hashed separately; raw UTF8 streams preserve original line endings. No original class, Throwable, PrintStream or Random is replaced; no field/stack repair.'}
 write('provenance.json',provenance);print(B.audit_public())

if __name__=='__main__':main()
