"""Java-free audit of original Item BB parent and selected native-prefix authority."""
import hashlib
import importlib.util
import json
from pathlib import Path
HERE=Path(__file__).resolve().parent
def module(name,file):
 s=importlib.util.spec_from_file_location(name,HERE/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
A=module('billboard_executed_audit','audit.py');P=module('billboard_compare','compare.py')
def require(value,text):P.require(value,'packet '+text)
def load(name):return json.loads((HERE/name).read_text(encoding='utf8'),object_pairs_hook=P.unique)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def from_draw(row):
 state=row['state'];words=row['words'][:48]
 return {'profile':row['profile'],'copy':row['copy'],'icon':row['icon'],'uv':[words[3],words[11],words[20],words[4]],'words':words,'prior':row['prior'][:48],
  'state':{'vertices':state['nw.h'],'source-vertices':state['nw.p'],'words':state['nw.o'],'mode':state['nw.r'],'building':state['nw.w'],'color':state['nw.l'],'texture':state['nw.m'],'normal':state['nw.n'],'translation':[state['nw.'+k] for k in ('s','t','u')]},'completed-draws':1}
def audit_events(ordinary,observed,events):
 result=A.audit({},ordinary,observed,events)
 q=[0,1,2,0,2,3];vertices=[[-.5,-.25,0,.3125,.25],[.5,-.25,0,.375,.25],[.5,.75,0,.375,.1875],[-.5,.75,0,.3125,.1875]]
 for row in result['draws']:
  for vertex,source in enumerate(q):
   require(row['words'][vertex*8:vertex*8+5]==[A.f(v) for v in vertices[source]],'independent Source constant local XYZ/UV arithmetic')
   if vertex in (0,1,2,5):require(row['words'][vertex*8+6]=='00007f00','actual Source positiveY packed normal')
 for profile in range(4):
  events_profile=[e for e in events if e['profile']==profile]
  vertices_events=[e for e in events_profile if e['event']=='entry' and e['method']=='nw.a(DDDDD)V']
  require(len(vertices_events)==4*A.COPIES[profile],'actual four Source vertices per copy')
  require([e['arguments'] for e in vertices_events]==[[A.d(v) for v in vertex] for copy in range(A.COPIES[profile]) for vertex in vertices],'actual original NW vertex-call arguments')
  normals=[e for e in events_profile if e['event']=='entry' and e['method']=='nw.b(FFF)V'];require(len(normals)==A.COPIES[profile] and all(e['arguments']==[A.f(0),A.f(1),A.f(0)] for e in normals),'actual Source normal681 per copy')
  gl_draws=[e for e in events_profile if e['event']=='gl-entry' and e['method']=='org.lwjgl.opengl.GL11.glDrawArrays(III)V'];require(len(gl_draws)==A.COPIES[profile] and all(e['arguments']==[4,0,6] for e in gl_draws),'actual original GL triangle submissions, not only entered buffers')
  phase=[e['pc'] for e in events_profile if e['event']=='phase']
  require(phase[0]==0 and phase[-1]==796 and phase.count(673)==phase.count(776)==phase.count(779)==A.COPIES[profile],'actual begin/draw/pop and full parent suffix')
  returns=[e for e in events_profile if e['event']=='return']
  require([e['return-value'] for e in returns if e['method']=='iz.b()I']==[53] and [e['return-value'] for e in returns if e['method']=='gm.a(I)I']==[53] and [e['return-value'] for e in returns if e['method']=='gm.f(I)I']==[16777215],'real first Source icon/color leaves')
  light=[e['return-value'] for e in returns if e['method']=='sn.a(F)F'];require(len(light)==1,'actual first World brightness leaf')
  color=[e for e in events_profile if e['event']=='gl-entry' and e['method']=='org.lwjgl.opengl.GL11.glColor4f(FFFF)V'];require(len(color)==1 and color[0]['arguments']==[light[0]]*3+[A.f(1)],'real Source brightness/color GL arguments')
  rotate=[e for e in events_profile if e['event']=='gl-entry' and e['method']=='org.lwjgl.opengl.GL11.glRotatef(FFFF)V'];yaw=A.floatword(A.target(observed)['input'][profile]['state']['manager-yaw'])
  require(len(rotate)==A.COPIES[profile] and all(e['arguments']==[A.f(A.f32(180-yaw)),A.f(0),A.f(1),A.f(0)] for e in rotate),'actual TH camera yaw field and Source billboard rotation')
 return result

def audit_public():
 finite=load('finite.json');observed=load('observations.json');pins=load('source-pins.json');provenance=load('provenance.json')
 require(finite['schema']==observed['schema']==provenance['schema']==1 and finite['client_jar_sha256']==pins['client_jar_sha256']==A.JAR,'unchanged supplied original archive')
 result=audit_events(observed['ordinary_stdout'],observed['jdi_stdout'],observed['events'])
 require(finite['rows']==[from_draw(row) for row in result['draws']],'unmodified selected actual NW records')
 P.validate(finite['rows'],1)
 for key,pin in observed['live_Code'].items():
  if pin['bootstrap-loader']:continue
  prefix,descriptor=key.split('(',1);owner,name=prefix.rsplit('.',1);cls=pins['libraries'][owner] if owner in pins['libraries'] else pins['classes'][owner]
  method=next(m for m in cls['methods'] if m['name']==name and m['descriptor']=='('+descriptor)
  require(all(method[k]==pin[k] for k in ('code_bytes','code_sha256')),'actual Source/library live Code identity')
 require(len(observed['live_Code'])==25,'14original/9LWJGL/2JDK live Code identities')
 for group in ('captured_helpers','publication_helpers','outputs'):
  for name,expected in provenance[group].items():require(digest(HERE/name)==expected,'unchanged '+group+' '+name)
 for name in ('ordinary_stdout','ordinary_stderr','jdi_stdout','jdi_stderr'):require(hashlib.sha256(observed[name].encode('utf8')).hexdigest()==provenance['streams'][name],'exact original UTF8 bytes')
 declarations=load('declared-inputs.json');require(provenance['failed_cohorts']==declarations['historical-failed-cohorts'] and provenance['superseded_prepare']==declarations['superseded-prepare'],'failed/unexecuted preparations retained separately')
 require(provenance['inputs_before']==provenance['inputs_after'] and provenance['compiled_before']==provenance['compiled_after'],'original execution input/helper continuity')
 return {k:result[k] for k in ('profiles','source_constructors','selected_draws','raw_words','active_words','inactive_words','unwritten_tail_words')}|{'original_completed_draws':10,'native_completed_draws_qualified_here':0,'live_Code_pins':25}
audit=audit_public
if __name__=='__main__':print(audit_public())
