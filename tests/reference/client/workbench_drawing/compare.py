"""Strict active-word comparison of genuine native UB prefixes and original draws."""
import argparse
import json
from pathlib import Path
import re

HERE=Path(__file__).resolve().parent
ORDER=[(profile,kind) for profile in (0,1) for kind in ('gradient','tile')]
FIELDS={'profile','kind','words','prior','state','completed-draws'}
STATE={'vertices','source-vertices','words','mode','building','color','texture','normal','translation'}

def require(value,text):
 if not value:raise ValueError('Workbench quad comparison: '+text)

def unique(pairs):
 result=dict(pairs);require(len(result)==len(pairs),'duplicate JSON field');return result

def active_indices(row):
 state=row['state'];result=[]
 for vertex in range(6):
  slots=[0,1,2]
  if state['texture']:slots.extend((3,4))
  if state['color']:slots.append(5)
  # NW's two generated vertices never copy a normal word, even when n=true.
  if state['normal'] and vertex in (0,1,2,5):slots.append(6)
  result.extend(8*vertex+slot for slot in slots)
 return result

def audit_inactive(row):
 active=set(active_indices(row))
 for index,(word,prior) in enumerate(zip(row['words'],row['prior'])):
  if index not in active:require(word==prior,'actual same-index inactive backing at word '+str(index))
 return {'active':len(active),'inactive':48-len(active)}

def validate(values,completed_draws):
 require(type(values) is list and len(values)==4,'exactly four ordered source records')
 require(all(type(row) is dict for row in values),'typed row objects')
 require([(row.get('profile'),row.get('kind')) for row in values]==ORDER,'exact profile/kind order')
 for row in values:
  require(type(row) is dict and set(row)==FIELDS and type(row['profile']) is int and type(row['kind']) is str,'exact typed row schema')
  require(type(row['completed-draws']) is int and row['completed-draws']==completed_draws,'original-completed versus native-entered scope')
  for key in ('words','prior'):
   require(type(row[key]) is list and len(row[key])==48 and all(type(word) is str and re.fullmatch('[0-9a-f]{8}',word) is not None for word in row[key]),'all48 raw32 '+key+' words')
  state=row['state'];require(type(state) is dict and set(state)==STATE,'exact source state selection')
  require(all(type(state[key]) is int for key in ('vertices','source-vertices','words','mode')) and [state[key] for key in ('vertices','source-vertices','words','mode')]==[6,4,48,7],'actual converted/source counts and mode')
  require(all(type(state[key]) is bool for key in ('building','color','texture','normal')) and state['building'] is True and state['normal'] is False,'actual entered builder and normal flags')
  gradient=row['kind']=='gradient'
  require(state['color'] is gradient and state['texture'] is (not gradient),'source-selected active color/texture attributes')
  require(type(state['translation']) is list and state['translation']==['0000000000000000']*3,'actual unchanged source translations')
  audit_inactive(row)
 return values

def rows(text):
 return validate([json.loads(line.removeprefix('WORKBENCH_DRAW_GEOMETRY '),object_pairs_hook=unique)
                  for line in text.splitlines() if line.startswith('WORKBENCH_DRAW_GEOMETRY ')],0)

def projection(values,completed_draws):
 validate(values,completed_draws)
 return [{'profile':row['profile'],'kind':row['kind'],'state':row['state'],
          'indices':active_indices(row),'active':[row['words'][index] for index in active_indices(row)]} for row in values]

def compare(actual,expected):
 first,second=projection(actual,0),projection(expected,1)
 for index,(a,b) in enumerate(zip(first,second)):
  require(a==b,'exact source active words/state at '+str(ORDER[index]))

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--stdout',type=Path,required=True)
 parser.add_argument('--fixture',type=Path,default=HERE/'finite.json');args=parser.parse_args()
 expected=json.loads(args.fixture.read_text(encoding='utf-8'),object_pairs_hook=unique)['rows']
 compare(rows(args.stdout.read_text(encoding='utf-8')),expected)
 print('Workbench quads: four original-completed/native-entered records;108 exact active words; own inactive backing')
