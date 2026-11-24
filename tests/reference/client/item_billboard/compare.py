"""Strict native-entered/original-completed Item billboard word comparison."""
import json
import re
ORDER=[(profile,copy) for profile in range(4) for copy in range(profile+1)]
FIELDS={'profile','copy','icon','uv','prior','words','state','completed-draws'}
STATE={'vertices','source-vertices','words','mode','building','color','texture','normal','translation'}
def require(value,text):
 if not value:raise ValueError('Item billboard comparison: '+text)
def unique(pairs):
 value=dict(pairs);require(len(value)==len(pairs),'duplicate JSON field');return value
def active_indices(row):return [8*v+s for v in range(6) for s in ([0,1,2,3,4,6] if v in (0,1,2,5) else [0,1,2,3,4])]
def audit_inactive(row):
 active=set(active_indices(row))
 for i,(word,prior) in enumerate(zip(row['words'],row['prior'])):
  if i not in active:require(word==prior,'actual same-index inactive backing at '+str(i))
 return {'active':34,'inactive':14}
def validate(rows,completed):
 require(type(rows) is list and len(rows)==10 and all(type(r) is dict and set(r)==FIELDS for r in rows),'exact ten-row schema')
 require([(r['profile'],r['copy']) for r in rows]==ORDER,'actual profile/copy order')
 for row in rows:
  require(all(type(row[k]) is int for k in ('profile','copy','icon','completed-draws')) and row['icon']==53 and row['completed-draws']==completed,'typed Source selection and original-completed/native-entered scope')
  for name in ('words','prior'):require(type(row[name]) is list and len(row[name])==48 and all(type(w) is str and re.fullmatch('[0-9a-f]{8}',w) for w in row[name]),'exact48 lowercase raw32 '+name)
  require(type(row['uv']) is list and row['uv']==['3ea00000','3ec00000','3e400000','3e800000'],'actual Stick53 u0/u1/v0/v1 source floats')
  state=row['state'];require(type(state) is dict and set(state)==STATE,'exact Source state fields')
  require(all(type(state[k]) is int for k in ('vertices','source-vertices','words','mode')) and [state[k] for k in ('vertices','source-vertices','words','mode')]==[6,4,48,7],'actual converted/source counts and mode')
  require(all(type(state[k]) is bool for k in ('building','color','texture','normal')) and state['building'] is True and state['color'] is False and state['texture'] is True and state['normal'] is True,'actual entered normal/UV-only builder')
  require(type(state['translation']) is list and state['translation']==['0000000000000000']*3,'actual unchanged Source NW translations')
  require(row['uv']==[row['words'][3],row['words'][11],row['words'][20],row['words'][4]],'actual parent UV values agree with emitted words')
  audit_inactive(row)
 return rows
def rows(text):return validate([json.loads(line.removeprefix('ITEM_BILLBOARD_GEOMETRY '),object_pairs_hook=unique) for line in text.splitlines() if line.startswith('ITEM_BILLBOARD_GEOMETRY ')],0)
def projection(values,completed):
 validate(values,completed)
 return [{**{k:row[k] for k in ('profile','copy','icon','uv','state')},'active':[row['words'][i] for i in active_indices(row)]} for row in values]
def compare(actual,expected):require(projection(actual,0)==projection(expected,1),'exact Source local XYZ/UV/normal words and flags/counts; each own inactive backing')
