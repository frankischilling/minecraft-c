"""Audit unchanged-original Workbench observations and source identities."""
import hashlib
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
SPEC=importlib.util.spec_from_file_location('workbench_packet_compare',HERE/'compare.py')
P=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(P)
E=P.E
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
CALLBACKS={
 'ob.a(IIIZLgs;)Liz;','ob.a(ILgs;)V','dw.a(IIZLgs;)Liz;','dw.a(Lgs;)V','dw.a(Liz;IIZ)V','it.a(I)Liz;','it.a(Lgs;)V','it.a(Llw;)V',
 'oo.h()V','id.h()V','gp.c()V','gp.c(Liz;)V','gp.a(I)Liz;','gp.a(Liz;)V','yv.b(Liz;)Z','yv.a(Liz;)V',
 'ix.a(ILiz;)V','ix.a(II)Liz;','ix.y_()V','ix.b(Liz;)V','mq.a(ILiz;)V','mq.a(II)Liz;','mq.y_()V',
 'wl.a(ILiz;)V','wl.a(II)Liz;','iz.a(I)Liz;','iz.k()Liz;','iz.b(Lfd;Lgs;)V','gs.a(Liz;)V','gs.a(Liz;Z)V',
 'gs.a(Lhl;)V','dc.a(Lvr;I)V'}

def load(name):return json.loads((HERE/name).read_text(encoding='utf-8'),object_pairs_hook=P.unique)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def require(value,text):P.require(value,'packet '+text)
def callbacks(calls):return [row for row in calls if row['method'] in CALLBACKS]

def compare_callback_trace(actual,expected):
 count=len(load('cases.json')['cases'])
 for values in (actual,expected):
  require(type(values) is list and len(values)==count,'all declared callback projections')
  for index,row in enumerate(values):
   require(type(row) is dict and set(row)=={'index','calls'} and type(row['index']) is int and row['index']==index and type(row['calls']) is list,'callback projection schema/order')
   for entry in row['calls']:
    require(type(entry) is dict and set(entry)=={'method','caller','pc'} and entry['method'] in CALLBACKS and type(entry['caller']) is str and type(entry['pc']) is int and entry['pc']>=0,'actual callback entry schema')
 require(actual==expected,'exact source callback/caller/PC order')

def graph(value):
 require(set(value)=={'slots','cached-size','window','world-same','position'} and value['window']==0 and value['cached-size']==46 and value['world-same'] is True and value['position']==[8,64,8],'genuine IT46 Container scalars')
 expected=[('yv','result',0,124,35)]
 expected += [('gp','craft',i,30+(i%3)*18,17+(i//3)*18) for i in range(9)]
 expected += [('gp','inventory',i,8+((i-9)%9)*18,84+((i-9)//9)*18) for i in range(9,36)]
 expected += [('gp','inventory',i,8+i*18,142) for i in range(9)]
 require(value['slots']==[dict(zip(('class','slot','backing','index','x','y'),(kind,n,back,index,x,y))) for n,(kind,back,index,x,y) in enumerate(expected)],'all46 exact Slot classes/backings/indices/coordinates')

def declared_inputs(rows,cases):
 require(len(rows)==len(cases),'declared invocation count')
 for row,case in zip(rows,cases):
  require(row['profile']==case['profile'] and row['operation']==case['operation'],'declared operation/profile preserved')
  before=row['before']
  for field,size in [('main',36),('armor',4),('craft',9)]:
   require(before[field]==[case[field].get(str(index)) for index in range(size)],'declared '+field+' aliases')
  require(before['cursor']==case['cursor'] and before['cached']==[None]*46,'declared cursor and genuine empty constructor cache')
  require(before['selected']==0 and before['changed'] is False and before['stats']=={} and before['default-same'] is False and before['active-same'] is True and before['inventory-owner-same'] is True,'genuine pre-call owner/stat/scalar state')
  for label,value in case['stacks'].items():
   require(before['stacks'][label]==[value[k] for k in ('id','count','damage','animation')],'all declared genuine Stack fields retained')


def audit():
 finite=load('finite.json');observed=load('observations.json');pins=load('source-pins.json');cases=load('cases.json')['cases']
 require(finite['schema']==1 and finite['client_jar_sha256']==JAR and pins['client_jar_sha256']==JAR,'original archive pin')
 require([r['profile'] for r in finite['rows']]==[r['profile'] for r in cases] and len(cases)==116,'all116 declared cases preserved')
 declared_inputs(finite['rows'],cases);declared_inputs(observed['jdi_rows'],cases)
 P.compare(observed['jdi_rows'],finite['rows']);E.constructors(observed['jdi_rows'],observed['call_audit'])
 for key,value in observed['live_Code'].items():
  owner,tail=key.split('.',1);name,descriptor=tail.split('(',1)
  method=next(m for m in pins['classes'][owner]['methods'] if m['name']==name and m['descriptor']=='('+descriptor)
  require(all(method[k]==v for k,v in value.items()),'live original Code agrees with archive')
 required={'it.<init>(Lix;Lfd;III)V','it.a(I)Liz;','it.a(Lgs;)V','oo.<init>(Lix;Lfd;III)V','oo.h()V','id.b()V','id.h()V'}
 require(required<=set(observed['live_Code']),'actual Workbench constructors/transfer/close and screen lifecycle entered')
 for call in observed['call_audit']:
  require(type(call['calls']) is list,'ordered source callback selection')
  for entry in call['calls']:
   require(set(entry)=={'method','caller','pc'} and entry['method'] in CALLBACKS and type(entry['caller']) is str and type(entry['pc']) is int and entry['pc']>=0,'typed source callback entry')
 graph(observed['graph'])
 screen=observed['screen'];require(screen['before']=={'class':'oo','container-class':'it','container-default-same':False,'default-active-same':True,'input':False,'width':176,'height':166,'pauses':False},'genuine OO constructor state')
 require(screen['after']=={'app-same':True,'font-same':True,'font-null':True,'scaled-width':427,'scaled-height':240,'active-same':True,'default-active-same':False,'pauses':False},'actual ID initialization/publication/unrelated key/live tick')
 require(all(screen[k] is True for k in ('closed-default-active-same','close-cursor-empty','close-grid-empty')),'returned empty direct OO close')
 provenance=load('provenance.json')
 for name,expected in provenance['public_helpers'].items():require(digest(HERE/name)==expected,'unchanged authored LF helper '+name)
 for name,expected in provenance['outputs'].items():require(digest(HERE/name)==expected,'unchanged published observation '+name)
 return {'cases':len(cases),'drops':sum(len(r['drops']) for r in finite['rows']),'constructor_audits':sum(len(r['constructors']) for r in observed['call_audit']),
         'source_callbacks':sum(len(r['calls']) for r in observed['call_audit']),'live_Code_pins':len(observed['live_Code']),'slots':46}

if __name__=='__main__':print(audit())
