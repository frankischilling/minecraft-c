"""Audit actual source NW buffers, default-model returns and UE success continuations."""
import hashlib
import json
from pathlib import Path
import struct

JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
BODY=('ue.a(IIF)V','id.a(IIF)V','ue.a(F)V','th.a(Lsn;DDDFF)V','ds.a(Lsn;DDDFF)V','ds.a(Lgs;DDDFF)V','gv.a(Lls;DDDFF)V','bw.b(Lsn;DDDFF)V','fh.a(FFFFFF)V')
PHASES=[0,347,350,361,397,400,403,408]


def require(value,text):
 if not value:raise ValueError('Original Inventory draw: '+text)

def unique(pairs):
 result=dict(pairs);require(len(result)==len(pairs),'duplicate JSON field');return result

def tagged(text,prefix):return [json.loads(line.removeprefix(prefix+' '),object_pairs_hook=unique) for line in text.splitlines() if line.startswith(prefix+' ')]
def f(value):return struct.pack('>f',value).hex()
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def source_buffer(row):
 require(row['kind'] in ('gradient','tile') and row['profile'] in (0,1),'selected finite quad')
 words,prior,state=row['words'],row['prior'],row['state']
 require(len(words)==len(prior)==48 and all(type(w) is str and len(w)==8 and all(c in '0123456789abcdef' for c in w) for w in words+prior),'all48 actual raw words and own pre-begin backing')
 require(state['nw.h']==6 and state['nw.o']==48 and state['nw.r']==7 and state['nw.w'] is True,'actual source quad/default triangle layout')
 require(all(state['nw.'+name]=='0000000000000000' for name in ('s','t','u')),'actual unchanged zero source translation, never a forced reset')
 gradient=row['kind']=='gradient'
 require(state['nw.l'] is gradient and state['nw.m'] is (not gradient) and state['nw.n'] is False,'actual source color/UV/normal flags')
 # Triangle expansion copies XYZ and only the active UV/color attributes.
 # Every inactive UV/color/normal/reserved word keeps its own original index.
 inactive=(3,4,6,7) if gradient else (5,6,7)
 for vertex in range(6):
  for slot in inactive:require(words[8*vertex+slot]==prior[8*vertex+slot],'own same-index inactive backing with actual conditional NW copies')
 q=(0,1,2,0,2,3)
 if gradient:
  vertices=[(427,0),(0,0),(0,240),(427,240)]
  active=[]
  for i,source in enumerate(q):
   expected=[f(vertices[source][0]),f(vertices[source][1]),f(0)]
   require(words[8*i:8*i+3]==expected and words[8*i+5]==('c0101010' if source<2 else 'd0101010'),'exact full-screen gradient XYZ and source RGBA')
   active.append([*expected,words[8*i+5]])
 else:
  vertices=[(125,203,0,166),(301,203,176,166),(301,37,176,0),(125,37,0,0)]
  active=[]
  for i,source in enumerate(q):
   x,y,u,v=vertices[source];expected=[f(x),f(y),f(0),f(u/256),f(v/256)]
   require(words[8*i:8*i+5]==expected,'exact centered Inventory tile XYZ/UV')
   active.append(expected)
 return {'profile':row['profile'],'kind':row['kind'],'active':active,'words':words,'prior':prior}

def target(text):
 owners=tagged(text,'INVENTORY_DRAW_OWNERS');device=tagged(text,'INVENTORY_DRAW_DEVICE');complete=tagged(text,'INVENTORY_DRAW_COMPLETE')
 require(len(owners)==len(device)==len(complete)==1,'complete genuine ordinary target receipts')
 require(device[0]['display-created'] is True and device[0]['pbuffer'] is False and [device[0]['width'],device[0]['height']]==[854,480],'actual Display-created context, no substituted Pbuffer')
 names={'app':'kq','world':'fd','player':'dc','inventory':'ix','container':'aa','screen':'ue','engine':'ji','packs':'ik','font':'sj','manager':'th','renderer':'ds','model':'fh'}
 require(all(owners[0][k]==v for k,v in names.items()) and owners[0]['view-player-same'] is True and owners[0]['open-stat']==1,'actual constructor graph/achievement/canonical DS and same Player view')
 require(owners[0]['input']=='lr' and owners[0]['input-settings-same'] is True and owners[0]['input-key-count']==10 and owners[0]['source-sneaking-before'] is False,'actual source LR/Settings attachment, not substituted sneaking')
 require(owners[0]['world-member'] is False and owners[0]['downloads-before']==0,'declared constructor scene: no helper World publication/downloader preparation')
 require(complete[0]['profiles']==2 and complete[0]['network']==0 and complete[0]['game-substitution'] is False and complete[0]['random-rebinding'] is False and all(complete[0][k]==0 for k in ('raw-allocations','nw-clears','player-field-repairs','world-field-repairs')),'bounded unchanged original profile')
 inputs=tagged(text,'INVENTORY_DRAW_INPUT');returns=tagged(text,'INVENTORY_DRAW_RETURN')
 require([r['profile'] for r in inputs]==[0,1] and [r['profile'] for r in returns]==[0,1],'both genuine returned draws')
 for index,(before,after) in enumerate(zip(inputs,returns)):
  mouse=[[-30,-40],[-60,-70]][index]
  require(before['mouse']==mouse and before['partial']==f(.375),'declared exact call arguments')
  expected_lag=[0,0] if index==0 else [-30,-40]
  require([before['pose'][k] for k in ('mouse-x','mouse-y')]==[f(v) for v in expected_lag],'actual previous returned mouse lag, no field repair')
  require(all(before['pose'][k]==after['pose'][k] for k in ('H','aS','aT')) and after['pose']['bE']==f(0) and after['pose']['manager-i']==f(180),'actual successful pose restores/bE reset and retained TH.i')
  require([after['pose'][k] for k in ('mouse-x','mouse-y')]==[f(v) for v in mouse] and after['gl-error']==0 and after['default-active-same'] is True and after['downloads-after']==0,'actual UE mouse stores/Container/default texture path/GL return')
  require(all(before['gl'][k]==after['gl'][k] for k in ('modelview','projection','modelview-depth','projection-depth','matrix-mode')),'actual complete draw matrix/stack continuations')
  require(all(m['held'] is False and m['sneak'] is False for m in after['models']),'actual default-state DS model cleanup')
 return {'inputs':inputs,'returns':returns,'owners':owners[0],'device':device[0]}

def audit(report,ordinary,observed,events):
 first=target(ordinary);second=target(observed)
 require(first['device']==second['device'],'same recorded original graphics backend')
 require(not any(e['event']=='gv-caught-exception' or e['event']=='exception' and e['gv-caught'] for e in events),'no swallowed GV Exception false success')
 draws=[e for e in events if e['event']=='nw-draw'];begins=[e for e in events if e['event']=='nw-begin'];done=[e for e in events if e['event']=='nw-return']
 require([(r['profile'],r['kind']) for r in draws]==[(i,k) for i in (0,1) for k in ('gradient','tile')],'four actual gradient/tile draws in source order')
 require([(r['profile'],r['kind']) for r in begins]==[(i,k) for i in (0,1) for k in ('gradient','tile')] and [(r['profile'],r['kind']) for r in done]==[(i,k) for i in (0,1) for k in ('gradient','tile')],'each selected begin and actual completed NW draw')
 finite=[source_buffer(row) for row in draws]
 for index in (0,1):
  phases=[e for e in events if e['event']=='ue-phase' and e['profile']==index]
  require([p['pc'] for p in phases]==PHASES,'actual TH success/pose restores/pop/lighting/rescale continuation PCs')
  for key in ('H','aS','aT'):
   require(phases[0]['pose'][key]==phases[-1]['pose'][key],'actual per-process pose restored at UE408')
   require(phases[1]['pose'][key]==phases[2]['pose'][key],'actual preview child retains caller temporary pose')
  require(phases[1]['pose']['bE']==f(1) and phases[2]['pose']['bE']==f(1) and all(p['pose']['bE']==f(0) for p in phases[3:]),'actual preview-only bE lifetime')
  require(phases[1]['pose']['manager-i']==f(180),'actual source TH.i store before preview')
  for method in BODY:
   require(sum(e['event']=='entry' and e['profile']==index and e['method']==method for e in events)==1 and sum(e['event']=='return' and e['profile']==index and e['method']==method for e in events)==1,'actual canonical body entry/return '+method)
  sneaking=[e for e in events if e['event']=='return' and e['profile']==index and e['method']=='dc.t()Z']
  actor=next(e for e in events if e['event']=='entry' and e['profile']==index and e['method']=='th.a(Lsn;DDDFF)V')['arguments'][0]['id']
  require(sneaking and all(e['return-value'] is False and e['receiver']==actor for e in sneaking),'actual DC.t source returns on the same genuine Input-backed Player')
  parts={e['receiver'] for e in events if e['event']=='return' and e['profile']==index and e['method']=='ps.a(F)V' and e['primary-part']}
  require(len(parts)==7,'all seven genuine primary model Parts returned')
  compile_parts={e['receiver'] for e in events if e['event']=='return' and e['profile']==index and e['method']=='ps.d(F)V' and e['primary-part']}
  require(compile_parts==parts if index==0 else not compile_parts,'source model compilation history preserved across the two draws')
 return {'profiles':2,'selected_draws':4,'raw_words':192,'primary_part_returns':14,'first_draw_compilations':7,'scope':'Genuine default-Inventory constructor scene, actual complete UE returns and selected raw NW/pose success evidence; no native/pixel/full-game claim.','finite':finite}
