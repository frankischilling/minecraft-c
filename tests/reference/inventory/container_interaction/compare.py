"""Strict selected Container state and alias comparison after independent entropy audits."""
import argparse
import importlib.util
import json
from pathlib import Path
import re

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('container_entropy',HERE/'entropy.py')
E=importlib.util.module_from_spec(spec);spec.loader.exec_module(E)
FIELDS={'profile','operation','exception','before','after','return','events','drops',
        'player-random-before','player-random-after','world-random-before','world-random-after'}
STATE={'main','armor','craft','result','cursor','cached','selected','changed','default-same','active-same','inventory-owner-same','stats','stacks'}
DROP={'ref','value','pickup-delay','world-member','chunk-members','position','velocity','private-random','trig'}
BOOLS={'changed','default-same','active-same','inventory-owner-same'}
REF=re.compile('[A-Z]+|N[1-9][0-9]*')

def require(value,text):
 if not value: raise ValueError('Container comparison: '+text)

def unique(pairs):
 result=dict(pairs)
 require(len(result)==len(pairs),'duplicate JSON field')
 return result

def integer(value):return type(value) is int and -(1<<31)<=value<(1<<31)
def stack(value):return type(value) is list and len(value)==4 and all(integer(v) for v in value)
def ref(value):return value is None or type(value) is str and REF.fullmatch(value) is not None

def snapshot(value):
 require(type(value) is dict and set(value)==STATE,'exact snapshot schema')
 require(all(type(value[k]) is bool for k in BOOLS) and integer(value['selected']),'typed Inventory scalar state')
 require(type(value['stats']) is dict and all(type(k) is str and re.fullmatch('[0-9]+',k) is not None and integer(v) for k,v in value['stats'].items()),'typed actual statistic map')
 require(type(value['stacks']) is dict and all(type(k) is str and REF.fullmatch(k) is not None and stack(v) for k,v in value['stacks'].items()),'retained Stack identities and raw fields')
 for key,size in [('main',36),('armor',4),('craft',4),('result',1),('cached',45)]:
  require(type(value[key]) is list and len(value[key])==size,'exact '+key+' length')
  require(all(ref(v) and (v is None or v in value['stacks']) for v in value[key]),'actual '+key+' aliases')
 require(ref(value['cursor']) and (value['cursor'] is None or value['cursor'] in value['stacks']),'actual carried Stack alias')
 return value

def validate(values):
 require(type(values) is list and values,'nonempty ordered cases')
 seen=set()
 for row in values:
  require(type(row) is dict and set(row)==FIELDS,'exact raw row schema')
  require(type(row['profile']) is str and row['profile'] and row['profile'] not in seen,'unique profile')
  seen.add(row['profile'])
  require(row['operation'] in ('click','close') and type(row['operation']) is str and row['exception'] in ('none','java.lang.IndexOutOfBoundsException'),'source operation and exception')
  snapshot(row['before']);snapshot(row['after'])
  require(ref(row['return']) and (row['return'] is None or row['return'] in row['after']['stacks']),'actual returned Stack clone/identity')
  require(type(row['events']) is list and type(row['drops']) is list,'ordered source callback/publication lists')
  for drop in row['drops']:
   require(type(drop) is dict and set(drop)==DROP and ref(drop['ref']) and drop['ref'] is not None and drop['ref'] in row['after']['stacks'] and stack(drop['value']),'exact Item publication and Stack identity schema')
   require(integer(drop['pickup-delay']) and type(drop['world-member']) is bool and integer(drop['chunk-members']),'typed publication state')
   require(all(type(drop[k]) is list and len(drop[k])==3 and all(type(v) is str and re.fullmatch('[0-9a-f]{16}',v) is not None for v in drop[k]) for k in ('position','velocity')),'raw64 published vectors')
  for event in row['events']:
   require(type(event) is dict and set(event)=={'event','index','ref'} and event['event']=='drop' and integer(event['index']) and ref(event['ref']),'typed publication callback')
  E.audit(row)
 return values

def rows(text):
 return validate([json.loads(line.removeprefix('CONTAINER_INTERACTION '),object_pairs_hook=unique)
                  for line in text.splitlines() if line.startswith('CONTAINER_INTERACTION ')])

def projection(values):
 # Explicit declared selection only after own-stream audits; raw entropy remains retained.
 validate(values)
 return [{**{key:row[key] for key in ('profile','operation','exception','before','after','return','events')},
          'drops':[{key:drop[key] for key in ('ref','value','pickup-delay','world-member','chunk-members','position')} for drop in row['drops']]}
         for row in values]

def compare(actual,expected):
 a,b=projection(actual),projection(expected)
 require(len(a)==len(b),'exact case count')
 for index,(first,second) in enumerate(zip(a,b)):
  require(first==second,'exact deterministic state/aliases/return/callbacks at row '+str(index)+' '+second['profile'])

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--stdout',type=Path,required=True)
 parser.add_argument('--fixture',type=Path,default=HERE/'finite.json')
 args=parser.parse_args()
 expected=json.loads(args.fixture.read_text(encoding='utf-8'),object_pairs_hook=unique)['rows']
 compare(rows(args.stdout.read_text(encoding='utf-8')),expected)
 print('Container interaction: '+str(len(expected))+' exact selected original cases')
