"""Independent original tick/close ordering, suffix and constructor audit."""
import hashlib
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
SPEC=importlib.util.spec_from_file_location('auto_close_compare',HERE/'compare.py');P=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(P)
unique=P.unique
require=P.require
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def observations(ordinary,observed,events,cases):
 P.validate(ordinary,cases);P.validate(observed,cases);P.compare(observed,ordinary)
 require(type(events) is list and all(type(e) is dict and type(e.get('index')) is int and e['index'] in range(7) for e in events),'typed ordered event index')
 require(not any(e['event']=='exception' for e in events),'no caught or uncaught invocation exception')
 receipts=[]
 for index,(row,case) in enumerate(zip(observed,cases)):
  group=[e for e in events if e['index']==index];require(group[0]['event']=='begin' and group[-1]['event']=='end','actual declared source boundaries')
  require(sum(e['event']=='begin' for e in group)==sum(e['event']=='end' for e in group)==1,'one actual invocation boundary pair')
  entered=[e for e in group if e['event']=='entry'];stack=[]
  for event in group:
   if event['event']=='entry':stack.append((event['method'],event['receiver']))
   elif event['event']=='return':require(stack and stack.pop()==(event['method'],event['receiver']),'all entered selected source children return in actual order')
  require(not stack,'no unfinished source child accepted as success')
  def calls(method,caller=None,pc=None):return [e for e in entered if e['method']==method and (caller is None or e['caller']=={'method':caller,'pc':pc})]
  def one(method,caller=None,pc=None):require(len(calls(method,caller,pc))==1,'one actual '+method+' child at '+str(pc))
  phases=[e for e in group if e['event']=='phase'];pcs=[e['pc'] for e in phases]
  tick=case['operation']=='gs-tick';close=case['profile'] in ('tick-removed','tick-distance','tick-removed-alias','direct-dc-removed')
  if tick:
   require(pcs==([0,111,114,136,139,143,146,151,154,382,390,402] if close else [0,111,114,136,139,154,382,390,402]),'actual GS prefix/eligibility/close/fresh-store/smoothing/statistic/return instruction order')
   one('ls.w_()V','gs.w_()V',111);one('it.b(Lgs;)Z','gs.w_()V',136)
   require(next(e['value'] for e in group if e['event']=='return' and e['method']=='it.b(Lgs;)Z') is (not close),'actual predicate return drives tick branch')
   require(len(calls('java.util.Random.nextInt(I)I'))==1 and calls('java.util.Random.nextInt(I)I')[0]['arguments']==[1000],'actual LS.U random prefix')
   one('dc.a(Lvr;I)V','gs.w_()V',387)
   if close:one('dc.r()V','gs.w_()V',143)
  else:require(not phases and not calls('gs.w_()V') and not calls('ls.w_()V'),'component controls do not claim composed tick')
  if close:
   one('gs.r()V','dc.r()V',1);one('net.minecraft.client.Minecraft.a(Lda;)V','dc.r()V',9);one('oo.h()V','net.minecraft.client.Minecraft.a(Lda;)V',22)
   one('id.h()V','oo.h()V',1);one('ob.a(ILgs;)V','id.h()V',32);one('aa.a(Lgs;)V','ob.a(ILgs;)V',5);one('it.a(Lgs;)V','oo.h()V',15)
   require(len(calls('it.a(Lgs;)V'))==1 and len(calls('aa.a(Lgs;)V'))==1,'AA cursor prefix and one IT grid close, no direct-OO duplicate conflation')
   focus=calls('net.minecraft.client.Minecraft.g()V','net.minecraft.client.Minecraft.a(Lda;)V',165)
   active_returns=[e['value'] for e in group if e['event']=='return' and e['method']=='org.lwjgl.opengl.Display.isActive()Z']
   require(active_returns and all(type(v) is bool for v in active_returns),'actual source Display activity returns')
   recurse=active_returns[0] and not row['focus-before']['N']
   require(len(focus)==len(active_returns)==1+int(recurse),'actual focus child count includes genuine recursive display(null)')
   require(len(calls('net.minecraft.client.Minecraft.a(Lda;)V','net.minecraft.client.Minecraft.g()V',29))==int(recurse) and len(calls('vy.a()V','net.minecraft.client.Minecraft.g()V',24))==int(recurse),'actual focus grab/recursive display order without manual repair')
   require(row['focus-after']['N']==(row['focus-before']['N'] or recurse),'actual focus N store/early-return branch')
   if tick:
    at146=next(e for e in phases if e['pc']==146)['state'];at154=next(e for e in phases if e['pc']==154)['state']
    require(at146['active-default'] and at154['active-default'] and at146['app-screen-null'] and at154['app-screen-null'],'actual DC returned before fresh GS store151 and suffix')
  else:require(not calls('dc.r()V') and not calls('oo.h()V') and not row['drops'],'eligible/predicate controls have no invented close')
  P.E.constructors(row,group)
  P.E.random_trace(row,group)
  # The source Player-private stream is checked independently of shared Math and actual focus timing.
  randoms=[e for e in entered if e['method'] in ('java.util.Random.nextInt(I)I','java.util.Random.nextFloat()F')]
  player_id=group[0]['state']['player'];player_random=None
  for event in randoms:
   if event['method']=='java.util.Random.nextInt(I)I':player_random=event['receiver']
  if tick:require(player_random is not None,'actual prefix private Random receiver')
  receipts.append({'profile':row['profile'],'source-entries':len(entered),'gs-phases':pcs,'drops':len(row['drops']),'suffix-returned':tick,'direct-control':case['operation']=='direct-dc'})
 return {'scope':'Four actual GS.w_ calls including three composed closes; two separate predicates and one direct DC close. Ordinary/read-only JDI real Display. No whole World tick or pixels.','rows':receipts,'drops':sum(len(r['drops']) for r in observed)}

def audit_public():
 def load(name):return json.loads((HERE/name).read_text(encoding='utf-8'),object_pairs_hook=unique)
 finite=load('finite.json');observed=load('observations.json');pins=load('source-pins.json');provenance=load('provenance.json');cases=load('declared-inputs.json')['cases']
 require(finite['client_jar_sha256']==pins['client_jar_sha256']==JAR,'untouched original archive identity')
 result=observations(finite['rows'],observed['rows'],observed['events'],cases)
 for key,pin in observed['live_Code'].items():
  prefix,descriptor=key.split('(',1);owner,name=prefix.rsplit('.',1);inventory=observed['library_pins'].get(owner) or observed['jdk_pins'].get(owner) or pins['classes'][owner.replace('.','/')]
  expected=next(m for m in inventory['methods'] if m['name']==name and m['descriptor']=='('+descriptor);require(all(expected[k]==v for k,v in pin.items()),'live entered original Code '+key)
 for table in ('public_helpers','outputs'):
  for name,value in provenance[table].items():require(digest(HERE/name)==value,'unchanged published bytes '+name)
 return result

audit=audit_public
if __name__=='__main__':print(audit_public())
