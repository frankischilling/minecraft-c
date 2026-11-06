"""Publish a completed pinned Workbench capture without rerunning game code."""
import argparse
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('workbench_publication_audit',HERE/'audit.py')
A=importlib.util.module_from_spec(S);S.loader.exec_module(A)
LIFECYCLE={'oo.<init>(Lix;Lfd;III)V','it.<init>(Lix;Lfd;III)V','dw.<init>()V','mq.<init>(Ldw;II)V','wl.<init>()V',
 'id.<init>(Ldw;)V','da.<init>()V','da.a(Lnet/minecraft/client/Minecraft;II)V','id.b()V','da.b()V',
 'id.a(CI)V','id.a()V','da.a()V','id.c()Z','gp.<init>(Llw;III)V','yv.<init>(Lgs;Llw;Llw;III)V'}

def write(name,value):
 (HERE/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8',newline='\n')

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True);args=parser.parse_args()
 directory=args.capture.resolve(strict=True);source=directory/'capture.json'
 report=json.loads(source.read_text(encoding='utf8'),object_pairs_hook=A.P.unique)
 A.require(report['status']=='PASS' and report['inputs_before']==report['inputs_after'] and report['helpers_before']==report['helpers_after'],'completed immutable capture')
 for raw,expected in report['inputs_before'].items():A.require(A.digest(raw)==expected,'capture input still unchanged '+raw)
 for name in ('audit.py','test_packet.py','compare.py','entropy.py','capture.py','cases.json','WorkbenchInteractionObservation.java','WorkbenchInteractionJdi.java'):
  A.require(report['inputs_before'][str((HERE/name).resolve())]==A.digest(HERE/name),'captured public helper '+name)
 A.declared_inputs(report['rows'],A.load('cases.json')['cases']);A.P.compare(report['jdi_rows'],report['rows'])
 A.E.constructors(report['jdi_rows'],report['call_audit'])
 observed={'schema':1,'client_jar_sha256':A.JAR,'jdi_rows':report['jdi_rows'],'graph':report['graph'][0],'screen':report['screen'][0],
  'call_audit':[{**row,'calls':A.callbacks(row['calls'])} for row in report['call_audit']],
  'screen_audit':{**report['screen_audit'],'calls':[row for row in report['screen_audit']['calls'] if row['method'] in LIFECYCLE or row['method'] in A.CALLBACKS]},
  'live_Code':report['live_Code']}
 write('finite.json',{'schema':1,'client_jar_sha256':A.JAR,'scope':'116 genuine unchanged-original Workbench CPU invocations; raw ordinary state and independent entropy retained. Direct OO.h is not the DC.r keyboard close path.','rows':report['rows']})
 write('observations.json',observed);write('source-pins.json',report['pins'])
 helpers=['WorkbenchInteractionObservation.java','WorkbenchInteractionJdi.java','capture.py','cases.json','compare.py','entropy.py','audit.py','test_packet.py','publish.py']
 provenance={'schema':1,'client_jar_sha256':A.JAR,'scope':'Genuine KQ/FD/DC/IX/AA plus OO/IT/ID headless components. No native or network linking, source substitution, RAW allocation, Random rebinding or invocation-state repair.',
  'capture':{'path':str(directory),'sha256':A.digest(source),'status':report['status'],'ordinary_rows':len(report['rows']),'jdi_rows':len(report['jdi_rows']),
    'genuine_empty_lifecycle':1,'live_Code_pins':len(report['live_Code']),'raw_method_entries':sum(len(r['calls']) for r in report['call_audit']),
    'selected_callback_entries':sum(len(r['calls']) for r in observed['call_audit']),'published_Items':sum(len(r['drops']) for r in report['rows'])},
  'inputs_before':report['inputs_before'],'inputs_after':report['inputs_after'],'compiled_helpers_before':report['helpers_before'],'compiled_helpers_after':report['helpers_after'],
  'private_artifacts':{str(f.relative_to(directory)):A.digest(f) for f in [directory/'original.stdout',directory/'original.stderr',directory/'jdi/target.stdout',directory/'jdi/target.stderr',directory/'jdi/calls.ndjson',directory/'jdi/live-Code.json']},
  'public_helpers':{name:A.digest(HERE/name) for name in helpers},'outputs':{name:A.digest(HERE/name) for name in ('finite.json','observations.json','source-pins.json')},
  'publication_helper_scope':'publish.py was authored after execution and does not run game code; execution helpers/cases/auditors were pinned before and after both original processes.',
  'entropy_scope':'Each process audits actual Player four-nextFloat drop words and untouched World stream. Actual JDI HL constructors retain four independent shared Math returns. Pure JDK trig is recomputed by the observer on the actual reconstructed argument; independent Decimal bracketing checks the accuracy contract, not cross-backend bitwise equality.',
  'owner_scope':'Every row constructs genuine OO from actual Player Inventory and World. Actual DA initialization invokes ID.b and publishes IT as Player.e; constructor AA remains Player.d. App.r screen replacement is not exercised.',
  'screen_scope':'Genuine OO constructor, DA.initialize, inherited ID.b, unrelated key, live tick, no-pause query and direct OO.h returned. Actual Font remains null; no Display, texture, Font draw or completed Workbench draw.',
  'close_scope':'Controller close visits active IT once and restores e=d; direct IT close preserves e; direct OO.h with e=IT visits IT twice; explicit repeat-direct-close repeats IT without clearing grid. Actual DC.r key path resets e before screen close and is not qualified here.',
  'native_comparison':'Pending independent native fixture comparison; this original packet does not claim full UI, keyboard-close, pixels, whole-frame parity, focus stability or FPS.'}
 write('provenance.json',provenance)
 print(A.audit())

if __name__=='__main__':main()
