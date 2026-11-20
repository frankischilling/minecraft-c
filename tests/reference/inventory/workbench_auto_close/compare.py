"""Strict deterministic state comparison; entropy is audited independently."""
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
SPEC=importlib.util.spec_from_file_location('auto_close_entropy',HERE/'entropy.py');E=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(E)

def require(value,text):
 if not value:raise ValueError('Workbench auto-close: '+text)

def unique(pairs):
 result=dict(pairs);require(len(result)==len(pairs),'duplicate JSON field');return result

def rows(stdout):
 result=[]
 for line in stdout.splitlines():
  if line.startswith('WORKBENCH_AUTO_CLOSE '):result.append(json.loads(line[len('WORKBENCH_AUTO_CLOSE '):],object_pairs_hook=unique))
 return result

FIELDS={'profile','operation','exception','return','before','after','events','drops','player-random-before','player-random-after','world-random-before','world-random-after','focus-before','focus-after','gl-error'}
SNAPSHOT={'inventory','craft','result','cursor','cached','active-it','active-default','default-aa','app-screen-same','app-screen-null','selected','changed','stats','player','world-items','stacks'}
DROP={'ref','value','pickup-delay','world-member','chunk-members','position','velocity','private-random','player-random-before','player-random-after','trig'}

def validate(values,cases=None):
 require(type(values) is list and len(values)==7,'exact seven declared rows')
 if cases is None:cases=json.loads((HERE/'declared-inputs.json').read_text(encoding='utf-8'),object_pairs_hook=unique)['cases']
 for row,case in zip(values,cases):
  require(type(row) is dict and set(row)==FIELDS,'exact row schema')
  require(row['profile']==case['profile'] and row['operation']==case['operation'],'declared profile/operation/order')
  require(row['exception']=='none' and type(row['gl-error']) is int and row['gl-error']==0,'actual returned source call and GL continuation')
  require(row['return']==(case['profile']=='predicate-distance64') if row['operation']=='predicate' else row['return'] is None,'actual method return')
  for which in ('before','after'):
   snap=row[which];require(type(snap) is dict and set(snap)==SNAPSHOT,'exact state schema')
   for name,size in [('inventory',40),('craft',9),('result',1),('cached',46)]:require(type(snap[name]) is list and len(snap[name])==size,'actual '+name+' extent')
   for name in ('active-it','active-default','default-aa','app-screen-same','app-screen-null','changed'):require(type(snap[name]) is bool,'typed owner/state '+name)
   require(snap['default-aa'] is True and snap['selected']==0,'constructor default/selected retained')
   require(type(snap['stats']) is dict and type(snap['stacks']) is dict and type(snap['player']) is dict,'typed actual state maps')
   for stack in snap['stacks'].values():require(type(stack) is list and len(stack)==4 and all(type(v) is int for v in stack),'actual Stack scalar tuple')
  before=row['before'];after=row['after'];alias=case['alias'];first='cursor' if alias else 'grid0';last='cursor' if alias else None
  require(before['inventory']==[None]*40 and before['cached']==[None]*46 and before['result']==[None] and before['cursor']=='cursor' and before['craft']==[first]+[None]*7+[last],'declared Inventory/grid/cursor aliases')
  require(before['active-it'] and not before['active-default'] and before['app-screen-same'] and not before['app-screen-null'],'genuine source opening/publication')
  require(before['stats']=={} and before['world-items']==0,'actual fresh statistics/World membership')
  expected={'cursor':[280,2,0,0]} if alias else {'cursor':[5,7,0,0],'grid0':[280,2,0,0]}
  require(before['stacks']==expected,'declared genuine Stack fields')
  close=row['operation']=='direct-dc' or row['profile'].startswith('tick-removed') or row['profile']=='tick-distance'
  require(after['active-default']==close and after['app-screen-null']==close and after['active-it']!=close and after['app-screen-same']!=close,'actual close/retain owner outcome')
  refs=([r for r in ['cursor',first,last] if r is not None]) if close else []
  require([d['ref'] for d in row['drops']]==refs,'actual AA cursor then one IT grid loop drop order/aliases')
  require(after['craft']==before['craft'] and after['result']==before['result'] and after['stacks']==before['stacks'],'source IT retains grid and genuine Stack objects')
  require(after['cursor']==(None if close else 'cursor') and after['inventory']==before['inventory'] and after['world-items']==len(refs),'actual cursor clear/publications')
  require(row['events']==[{'event':'drop','index':i,'ref':r} for i,r in enumerate(refs)],'actual ordered publications')
  for drop in row['drops']:
   require(type(drop) is dict and set(drop)==DROP and drop['value']==after['stacks'][drop['ref']],'exact retained drop schema/scalars')
   for name in ('position','velocity'):require(type(drop[name]) is list and len(drop[name])==3,'raw drop vector extent')
  for which in ('focus-before','focus-after'):require(type(row[which]) is dict and set(row[which])=={'display-active','N','mouse-grabbed'} and all(type(v) is bool for v in row[which].values()),'actual focus fields are observed booleans')
  E.audit(row)
 return values

def projection(row):
 return {key:row[key] for key in ('profile','operation','exception','return','before','after','events','gl-error')} | {'drops':[{key:d[key] for key in ('ref','value','pickup-delay','world-member','chunk-members','position')} for d in row['drops']]}

def compare(actual,expected):
 validate(actual);validate(expected);require([projection(r) for r in actual]==[projection(r) for r in expected],'exact deterministic states/aliases/drop order after independent entropy audits')
 return {'rows':7,'composed-gs-ticks':4,'separate-predicates':2,'separate-direct-dc':1}
