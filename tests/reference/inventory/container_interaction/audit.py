"""Audit published packet identities, source pins and retained independent observations."""
import hashlib
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
SPEC=importlib.util.spec_from_file_location('container_packet_compare',HERE/'compare.py')
P=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(P)
E=P.E
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
# Explicit source side-effect/dispatch selection. Full read/recipe lookup entry
# transport remains retained privately by capture hash, not silently rewritten.
CALLBACKS={
 'ob.a(IIIZLgs;)Liz;','ob.a(ILgs;)V','dw.a(IIZLgs;)Liz;','dw.a(Lgs;)V','dw.a(Liz;IIZ)V','aa.a(I)Liz;','aa.a(Lgs;)V','aa.a(Llw;)V',
 'gp.c()V','gp.c(Liz;)V','gp.a(I)Liz;','gp.a(Liz;)V','pv.b(Liz;)Z','pv.d()I','yv.b(Liz;)Z','yv.a(Liz;)V',
 'ix.a(ILiz;)V','ix.a(II)Liz;','ix.y_()V','ix.b(Liz;)V','mq.a(ILiz;)V','mq.a(II)Liz;','mq.y_()V',
 'wl.a(ILiz;)V','wl.a(II)Liz;','iz.a(I)Liz;','iz.k()Liz;','iz.b(Lfd;Lgs;)V','gs.a(Liz;)V','gs.a(Liz;Z)V',
 'gs.a(Lhl;)V','dc.a(Lvr;I)V'}

def load(name):return json.loads((HERE/name).read_text(encoding='utf-8'),object_pairs_hook=P.unique)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def require(value,text):P.require(value,'packet '+text)

def callbacks(calls):
 return [row for row in calls if row['method'] in CALLBACKS]

def compare_callback_trace(actual,expected):
 for values in (actual,expected):
  require(type(values) is list and len(values)==73,'all73 callback projections')
  for index,row in enumerate(values):
   require(type(row) is dict and set(row)=={'index','calls'} and type(row['index']) is int and row['index']==index and type(row['calls']) is list,'callback projection schema/order')
   for entry in row['calls']:
    require(type(entry) is dict and set(entry)=={'method','caller','pc'} and entry['method'] in CALLBACKS and type(entry['caller']) is str and type(entry['pc']) is int and entry['pc']>=0,'actual callback entry schema')
 require(actual==expected,'exact original source callback/caller/PC order')

def name_inventory(value,pins):
 require(set(value)=={'schema','client_jar_sha256','scope','rows','resolved_methods'} and value['schema']==1 and value['client_jar_sha256']==JAR,'name inventory header')
 rows=value['rows'];require(type(rows) is list and len(rows)==202,'202 original Item IDs')
 require([r['id'] for r in rows]==sorted({r['id'] for r in rows}),'unique sorted actual Item IDs')
 owners={}
 for row in rows:
  require(set(row)=={'id','class','owner','samples'} and type(row['id']) is int and all(type(row[k]) is str for k in ('class','owner')),'name row shape')
  require(type(row['samples']) is list and [s['damage'] for s in row['samples']]==[0,15],'metadata0/15 observations')
  for sample in row['samples']:
   require(set(sample)=={'damage','value','exception','same-repeat','same-noarg','translated','trimmed'},'exact name sample shape')
   require(type(sample['damage']) is int and all(sample[k] is None or type(sample[k]) is str for k in ('value','translated','trimmed')),'typed actual String values')
   require(sample['exception'] in ('none','java.lang.ArrayIndexOutOfBoundsException') and all(sample[k] is None or type(sample[k]) is bool for k in ('same-repeat','same-noarg')),'name identity/exception types')
   if sample['exception']=='none':
    require(sample['same-repeat'] is (row['owner'] in ('gm','ck','rx')),'actual field/literal versus builder result identity')
   else:require(row['id']==44 and sample['damage']==15 and all(sample[k] is None for k in ('value','same-repeat','same-noarg','translated','trimmed')),'actual slab15 exception preserved')
  require(row['class'] not in owners or owners[row['class']]==row['owner'],'one actual name resolution per runtime class')
  owners[row['class']]=row['owner']
 require(len(owners)==38 and set(owners.values())=={'gm','ck','bi','en','km','rx'},'38 actual classes/six bodies')
 require(set(value['resolved_methods'])==set(owners),'all38 reflection resolutions pinned')
 for kind,owner in owners.items():
  method=next(m for m in pins['classes'][owner]['methods'] if m['name']=='a' and m['descriptor']=='(Liz;)Ljava/lang/String;')
  require(value['resolved_methods'][kind]=={'owner':owner,**method},'actual declaring Code pin')
 return len(rows)

def audit():
 finite=load('finite.json');observed=load('observations.json');pins=load('source-pins.json')
 require(finite['schema']==1 and finite['client_jar_sha256']==JAR and pins['client_jar_sha256']==JAR,'original archive pin')
 cases=load('cases.json')['cases']
 require([r['profile'] for r in finite['rows']]==[r['profile'] for r in cases] and len(cases)==73,'all73 declared cases preserved')
 P.compare(observed['jdi_rows'],finite['rows'])
 E.constructors(observed['jdi_rows'],observed['call_audit'])
 for key,value in observed['live_Code'].items():
  owner,tail=key.split('.',1);name,descriptor=tail.split('(',1)
  method=next(m for m in pins['classes'][owner]['methods'] if m['name']==name and m['descriptor']=='('+descriptor)
  require(all(method[k]==v for k,v in value.items()),'live original Code agrees with archive')
 require(len(observed['live_Code'])==59,'all59 actual entered live Codes')
 for call in observed['call_audit']:
  require(type(call['calls']) is list,'ordered source callback selection')
  for entry in call['calls']:
   require(set(entry)=={'method','caller','pc'} and entry['method'] in CALLBACKS and type(entry['caller']) is str and type(entry['pc']) is int and entry['pc']>=0,'typed actual source callback entry')
 require(observed['graph']['window']==0 and observed['graph']['cached-size']==45 and len(observed['graph']['slots'])==45,'genuine AA45 Slot graph')
 screen=observed['screen'];require(screen['before']['class']=='ue' and screen['before']['open-stat']==1 and screen['before']['open-granted'] is True and screen['after']['font-null'] is True and screen['before']['pauses'] is False,'genuine finite headless UE lifecycle scope')
 name_inventory(load('item-name-inventory.json'),pins)
 provenance=load('provenance.json')
 for name,expected in provenance['public_helpers'].items():require(digest(HERE/name)==expected,'unchanged authored LF helper '+name)
 for name,expected in provenance['outputs'].items():require(digest(HERE/name)==expected,'unchanged published observation '+name)
 return {'cases':73,'drops':sum(len(r['drops']) for r in finite['rows']),'constructor_audits':sum(len(r['constructors']) for r in observed['call_audit']),
         'source_callbacks':sum(len(r['calls']) for r in observed['call_audit']),'live_Code_pins':59,'item_names':202}

if __name__=='__main__':print(audit())
