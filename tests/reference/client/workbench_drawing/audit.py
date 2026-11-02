"""Audit unchanged-original OO/ID returns, source hooks and actual NW backing."""
import hashlib
import json
from pathlib import Path
import struct

JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
ID_PHASES=[0,32,206,305,329,395,468,478,490]
OO_PHASES=[0,9,12,20,28,31,70,73]
WATCH={'id.a(IIF)V','oo.a(F)V','oo.k()V','id.a(Lgp;)V','id.a(Lgp;II)Z','bb.a(Lsj;Lji;Liz;II)V','bb.b(Lsj;Lji;Liz;II)V',
 'ji.b(Ljava/lang/String;)I','ji.b(I)V','sj.b(Ljava/lang/String;III)V','sj.a(Ljava/lang/String;III)V','iz.l()Ljava/lang/String;','nh.b(Ljava/lang/String;)Ljava/lang/String;'}

def require(value,text):
 if not value:raise ValueError('Original Workbench draw: '+text)
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
   require(words[8*i:8*i+5]==expected,'exact centered Workbench tile XYZ/UV')
   active.append(expected)
 return {'profile':row['profile'],'kind':row['kind'],'active':active,'words':words,'prior':prior}


def raw_buffer(row):
 state=row['state'];words=row['words'];prior=row['prior']
 require(type(words) is list and type(prior) is list and len(words)==len(prior)==state['nw.o'],'actual arbitrary batch extent')
 require(state['nw.r']==7 and state['nw.w'] is True and state['nw.h']*8==len(words) and state['nw.p']%4==0 and state['nw.h']==state['nw.p']//4*6,'actual default converted quad counts')
 require(all(type(w) is str and len(w)==8 and all(c in '0123456789abcdef' for c in w) for w in words+prior),'all actual raw32 backing words')
 active=set()
 for vertex in range(state['nw.h']):
  slots=[0,1,2]
  if state['nw.m']:slots.extend((3,4))
  if state['nw.l']:slots.append(5)
  if state['nw.n'] and vertex%6 in (0,1,2,5):slots.append(6)
  active.update(8*vertex+slot for slot in slots)
 for index,(word,before) in enumerate(zip(words,prior)):
  if index not in active:require(word==before,'every actual same-index inactive NW backing word')
 return {'words':len(words),'active':len(active),'inactive':len(words)-len(active)}

def scalar_rows(text):
 rows=tagged(text,'WORKBENCH_BLOCK_LEAF');signed=[-(1<<31),-17,-1,0,1,2,3,15,16,(1<<31)-1];sides=[-(1<<31),-1,0,1,2,3,4,5,6,(1<<31)-1]
 expected=[]
 for id in (17,58):
  expected.extend((id,'b(I)I',[m]) for m in signed);expected.append((id,'g()V',[]))
  expected.extend((id,'a(II)I',[side,m]) for side in sides for m in signed) if id==17 else expected.extend((id,'a(I)I',[side]) for side in sides)
 require([(r['id'],r['method'],r['arguments']) for r in rows]==expected and len(rows)==132,'all132 ordered signed source scalar queries')
 for row in rows:
  id,method,args=row['id'],row['method'],row['arguments'];require(row['class']==('vg' if id==17 else 'fi'),'actual registered Block class')
  if method=='b(I)I':require(set(row)=={'id','class','method','owner','arguments','return'} and row['owner']=='uu' and row['return']==16777215,'actual inherited UU white color')
  elif method=='g()V':require(set(row)=={'id','class','method','owner','arguments','return','before','after'} and row['owner']=='uu' and row['return'] is None and row['before']==row['after'],'actual inherited UU.g no scalar mutation')
  elif id==17:
   side,metadata=args;value=21 if side in (0,1) else 116 if metadata==1 else 117 if metadata==2 else 20
   require(row['owner']=='vg' and row['return']==value,'actual VG side0/1 first, then exact signed metadata branches')
  else:
   side=args[0];value=43 if side==1 else 4 if side==0 else 60 if side in (2,4) else 59
   require(row['owner']=='fi' and row['return']==value,'actual FI exact signed side branches')
 return rows

def target(text):
 owners=tagged(text,'WORKBENCH_DRAW_OWNERS');device=tagged(text,'WORKBENCH_DRAW_DEVICE');complete=tagged(text,'WORKBENCH_DRAW_COMPLETE')
 require(len(owners)==len(device)==len(complete)==1,'complete genuine ordinary target receipts')
 require(device[0]['display-created'] is True and device[0]['pbuffer'] is False and [device[0]['width'],device[0]['height']]==[854,480],'actual Display-created context')
 names={'app':'kq','world':'fd','player':'dc','inventory':'ix','default-container':'aa','container':'it','screen':'oo','engine':'ji','packs':'ik','font':'sj'}
 require(all(owners[0][k]==v for k,v in names.items()) and owners[0]['active-screen-same'] is True and owners[0]['default-active-same'] is False,'actual constructor graph/ID.b active IT publication')
 require(owners[0]['input-null'] is True and owners[0]['app-screen-null'] is True and owners[0]['world-member'] is False and owners[0]['downloads-before']==0,'declared constructor scene without unused Input attachment, App display replacement or World publication')
 require(complete[0]['profiles']==2 and complete[0]['network']==0 and complete[0]['game-substitution'] is False and complete[0]['random-rebinding'] is False and all(complete[0][k]==0 for k in ('raw-allocations','nw-clears','player-field-repairs','world-field-repairs','source-input-attachments')),'bounded unchanged original profile')
 require(complete[0]['declared-cursor-setters']==1 and complete[0]['declared-inventory-setters']==4,'declared genuine source setters')
 inputs=tagged(text,'WORKBENCH_DRAW_INPUT');returns=tagged(text,'WORKBENCH_DRAW_RETURN')
 require([r['profile'] for r in inputs]==[0,1] and [r['profile'] for r in returns]==[0,1],'two genuine completed inherited draw calls')
 for index,(before,after) in enumerate(zip(inputs,returns)):
  require(before['mouse']==[[134,180],[-60,-70]][index] and before['partial']==f(.375),'declared exact call arguments')
  scene=before['scene'];require(scene==after['scene'],'actual returned draw preserves all declared Inventory and owner state')
  require(scene['main']==[[17,3,0,5],[58,2,0,5]]+[None]*7+[[5,4,0,5]]+[None]*26 and scene['armor']==[None]*4 and scene['craft']==[[17,1,0,5]]+[None]*8 and scene['result']==[[5,4,0,0]],'exact actual source input Stacks and recipe-produced result')
  require(scene['cursor']==(None if index==0 else [58,2,0,5]) and scene['active-screen-same'] is True and scene['default-active-same'] is False and scene['input-null'] is True,'source cursor profile and retained owner graph')
  require(after['gl-error']==0 and after['downloads-after']==0,'actual completed GL return without downloader')
  require(all(before['gl'][k]==after['gl'][k] for k in ('modelview','projection','modelview-depth','projection-depth','matrix-mode')),'actual whole-draw matrix/stack restoration')
  require(after['gl']['lighting'] is True and after['gl']['depth'] is True,'actual ID final GL continuation')
 return {'inputs':inputs,'returns':returns,'owners':owners[0],'device':device[0],'scalars':scalar_rows(text)}

def audit(report,ordinary,observed,events):
 first=target(ordinary);second=target(observed);require(first['device']==second['device'] and first['scalars']==second['scalars'],'independent backend and scalar observations agree')
 require(not any(e['event']=='exception' for e in events),'no caught or uncaught source draw exception')
 draws=[e for e in events if e['event']=='nw-draw'];begins=[e for e in events if e['event']=='nw-begin'];done=[e for e in events if e['event']=='nw-return' and e['kind']!='other']
 expected=[(i,k) for i in (0,1) for k in ('gradient','tile')]
 require(all([(r['profile'],r['kind']) for r in values]==expected for values in (draws,begins,done)),'four actual selected begin/draw/return batches')
 finite=[source_buffer(row) for row in draws]
 all_draws=[e for e in events if e['event'] in ('nw-draw','nw-other-draw')];all_returns=[e for e in events if e['event']=='nw-return']
 require([r['ordinal'] for r in all_draws]==list(range(len(all_draws))) and [(r['profile'],r['kind'],r['ordinal']) for r in all_draws]==[(r['profile'],r['kind'],r['ordinal']) for r in all_returns],'every actual NW draw has its real return')
 backing=[raw_buffer(row) for row in all_draws]
 for profile in (0,1):
  for method in ('id.a(IIF)V','oo.a(F)V','oo.k()V'):
   require(sum(e['event']=='entry' and e['profile']==profile and e['method']==method for e in events)==1 and sum(e['event']=='return' and e['profile']==profile and e['method']==method for e in events)==1,'actual inherited draw/OO hook returns')
  for method in WATCH:
   entries=[e for e in events if e['event']=='entry' and e['profile']==profile and e['method']==method];returns=[e for e in events if e['event']=='return' and e['profile']==profile and e['method']==method]
   require([e['receiver'] for e in entries]==[e['receiver'] for e in returns],'every entered source body returns on the same receiver')
  for method in ('id.a(Lgp;)V','id.a(Lgp;II)Z'):
   require(sum(e['event']=='entry' and e['profile']==profile and e['method']==method for e in events)==46,'all46 genuine Slot visits/hit checks')
  for method in ('bb.a(Lsj;Lji;Liz;II)V','bb.b(Lsj;Lji;Liz;II)V'):
   require(sum(e['event']=='entry' and e['profile']==profile and e['method']==method for e in events)==46+profile,'actual Slot and cursor source Item draw calls')
  phases=[e for e in events if e['event']=='phase' and e['profile']==profile and e['method']=='id.a(IIF)V'];require([e['pc'] for e in phases]==[v for v in ID_PHASES if v!=395 or profile==0],'actual ID Slot/cursor/foreground/tooltip/pop/return continuation PCs')
  require([e['pc'] for e in events if e['event']=='phase' and e['profile']==profile and e['method']=='oo.a(F)V']==OO_PHASES,'actual OO texture query/bind/tile returned phases')
  require(all(e['owners']=={'active-screen-same':True,'default-active-same':False,'input-null':True} for e in phases),'actual ID phases retain genuine owner graph')
  entries=[e for e in events if e['event']=='entry' and e['profile']==profile]
  require(sum(e['method']=='ji.b(Ljava/lang/String;)I' and e['arguments']==['/gui/crafting.png'] for e in entries)==1,'actual OO crafting texture lookup')
  labels=[e['arguments'] for e in entries if e['method']=='sj.b(Ljava/lang/String;III)V' and e['arguments'][0] in ('Crafting','Inventory')]
  require(labels==[['Crafting',28,6,4210752],['Inventory',8,72,4210752]],'actual OO labels and source coordinates')
  tooltip=[e for e in entries if e['method']=='nh.b(Ljava/lang/String;)Ljava/lang/String;'];require(len(tooltip)==(1-profile),'actual tooltip gated by source cursor state')
  if profile==0:
   require(any(e['method']=='iz.l()Ljava/lang/String;' for e in entries),'actual tooltip Stack name child')
   require(any(e['method']=='sj.a(Ljava/lang/String;III)V' and e['arguments'][0]=='Wood' for e in entries),'actual translated and drawn Log tooltip')
 return {'profiles':2,'selected_draws':4,'raw_words':192,'all_NW_draws':len(all_draws),'all_NW_words':sum(v['words'] for v in backing),'all_NW_active_words':sum(v['active'] for v in backing),'all_NW_inactive_words':sum(v['inactive'] for v in backing),
  'scalar_queries':132,'scope':'Genuine Workbench constructor scene, complete inherited ID draw and OO hooks, selected/all raw NW backing evidence; no native/pixel/full-game claim.','finite':finite}
