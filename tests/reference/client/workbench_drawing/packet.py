"""Java-free audit of published original Workbench drawing evidence."""
import hashlib
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent

def module(name,file):
 spec=importlib.util.spec_from_file_location(name,HERE/file);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
A=module('workbench_captured_audit','audit.py')
P=module('workbench_quad_compare','compare.py')

def load(name):return json.loads((HERE/name).read_text(encoding='utf-8'),object_pairs_hook=P.unique)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def require(value,text):P.require(value,'packet '+text)

def from_draw(row):
 state=row['state']
 return {'profile':row['profile'],'kind':row['kind'],'words':row['words'],'prior':row['prior'],
  'state':{'vertices':state['nw.h'],'source-vertices':state['nw.p'],'words':state['nw.o'],'mode':state['nw.r'],
    'building':state['nw.w'],'color':state['nw.l'],'texture':state['nw.m'],'normal':state['nw.n'],
    'translation':[state['nw.'+name] for name in ('s','t','u')]},'completed-draws':1}

def typed_target(text):
 result=A.target(text)
 for row in result['inputs']+result['returns']:
  scene=row['scene']
  for name in ('main','armor','craft','result'):
   require(all(stack is None or type(stack) is list and len(stack)==4 and all(type(v) is int for v in stack) for stack in scene[name]),'typed actual '+name+' Stack fields')
  require(scene['cursor'] is None or type(scene['cursor']) is list and len(scene['cursor'])==4 and all(type(v) is int for v in scene['cursor']),'typed actual cursor fields')
 for row in result['scalars']:
  require(type(row['id']) is int and type(row['method']) is str and type(row['class']) is str and type(row['owner']) is str and type(row['arguments']) is list and all(type(v) is int for v in row['arguments']),'typed signed scalar inputs')
  require(set(row)==({'id','class','method','owner','arguments','return','before','after'} if row['method']=='g()V' else {'id','class','method','owner','arguments','return'}),'exact scalar query schema')
  require(row['return'] is None if row['method']=='g()V' else type(row['return']) is int,'typed actual scalar result')
 return result

def audit_events(ordinary,observed,events):
 result=A.audit({},ordinary,observed,events);typed_target(ordinary);typed_target(observed)
 for event in events:
  require(type(event['profile']) is int and event['profile'] in (0,1),'typed actual source event profile')
  if event['event']=='phase':
   require(type(event['pc']) is int and all(type(v) is bool for v in event['owners'].values()),'typed actual source phase/owners')
  if event['event'] in ('nw-draw','nw-other-draw','nw-return'):require(type(event['ordinal']) is int,'typed actual source NW order')
  if event['event'] in ('nw-draw','nw-other-draw'):
   require(all(type(event['state']['nw.'+name]) is bool for name in ('l','m','n','w')),'typed actual source NW attribute flags')
 for profile in (0,1):
  entries=[e for e in events if e['event']=='entry' and e['profile']==profile]
  texture=[e for e in entries if e['method']=='ji.b(Ljava/lang/String;)I' and e['arguments']==['/gui/crafting.png']]
  require(len(texture)==1 and any(f=={'class':'oo','method':'a','descriptor':'(F)V','pc':9} for f in texture[0]['frames']),'actual OO texture source call site')
  labels=[e for e in entries if e['method']=='sj.b(Ljava/lang/String;III)V' and e['arguments'][0] in ('Crafting','Inventory')]
  require([next(f['pc'] for f in e['frames'] if f['class']=='oo' and f['method']=='k') for e in labels]==[12,34],'actual OO label call sites')
  cursor=[]
  for method,pc in [('bb.a(Lsj;Lji;Liz;II)V',266),('bb.b(Lsj;Lji;Liz;II)V',302)]:
   calls=[e for e in entries if e['method']==method and any(f=={'class':'id','method':'a','descriptor':'(IIF)V','pc':pc} for f in e['frames'])]
   require(len(calls)==profile,'actual cursor child source call site')
   if calls:cursor.append(calls[0]['arguments'][2])
  if profile==1:require(cursor[0]==cursor[1] and cursor[0]['class']=='iz','same actual cursor Stack captured by both source children')
  if profile==0:
   returns=[e for e in events if e['event']=='return' and e['profile']==0]
   require([e['return-value'] for e in returns if e['method']=='iz.l()Ljava/lang/String;']==['tile.log'],'actual Log Stack name result')
   require([e['return-value'] for e in returns if e['method']=='nh.b(Ljava/lang/String;)Ljava/lang/String;']==['Wood'],'actual NH translated Log result')
 return result


def failed_cohorts(value):
 expected=load('declared-inputs.json')['historical-failed-cohorts']
 require(value==expected and len(value)==2 and all(row['status']=='FAIL' and row['reused'] is False for row in value),'both unchanged historical observer/pin-inventory failures retained')


def audit_public():
 finite=load('finite.json');observed=load('observations.json');pins=load('source-pins.json');provenance=load('provenance.json')
 failed_cohorts(provenance['failed_cohorts'])
 require(finite['schema']==observed['schema']==provenance['schema']==1 and finite['client_jar_sha256']==pins['client_jar_sha256']==A.JAR,'supplied unchanged original archive')
 result=audit_events(observed['ordinary_stdout'],observed['jdi_stdout'],observed['events'])
 draws=[row for row in observed['events'] if row['event']=='nw-draw']
 require(finite['rows']==[from_draw(row) for row in draws],'declared selection from actual original draw records, no replacement values')
 P.validate(finite['rows'],1)
 require(sum(len(P.active_indices(row)) for row in finite['rows'])==108,'all108 active source words')
 for key,value in observed['live_Code'].items():
  owner,tail=key.split('.',1);name,descriptor=tail.split('(',1)
  original=next(m for m in pins['classes'][owner]['methods'] if m['name']==name and m['descriptor']=='('+descriptor)
  require(all(original[k]==v for k,v in value.items()),'actual live source Code pin')
 require(load('block-scalars.json')['rows']==typed_target(observed['ordinary_stdout'])['scalars'],'unmodified selection of all132 original scalar rows')
 require(len(observed['live_Code'])==15,'all15 actual source Codes retained')
 for key in ('captured_helpers','publication_helpers','outputs'):
  for name,expected in provenance[key].items():require(digest(HERE/name)==expected,'unchanged '+key+' '+name)
 for field,payload in [('original_stdout_sha256','ordinary_stdout'),('jdi_stdout_sha256','jdi_stdout'),('original_stderr_sha256','ordinary_stderr'),('jdi_stderr_sha256','jdi_stderr')]:
  require(hashlib.sha256(observed[payload].encode('utf-8')).hexdigest()==provenance[field],'actual stdout byte preservation')
 for key in ('ordinary_stderr','jdi_stderr'):
  lines=observed[key].splitlines()
  require(len(lines)==4 and all(line.startswith('WARNING:') for line in lines) and any('System::setSecurityManager' in line for line in lines),'no caught-renderer/body stderr error')
 return {'profiles':result['profiles'],'original_completed_draws':4,'native_completed_draws_qualified_here':0,
         'raw_words':192,'active_words':108,'own_inactive_words':84,'all_NW_draws':result['all_NW_draws'],'all_NW_words':result['all_NW_words'],'scalar_queries':132,'live_Code_pins':15}

audit=audit_public
if __name__=='__main__':print(audit_public())
