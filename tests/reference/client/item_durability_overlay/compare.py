"""Audit each process's own NW backing and compare explicit active overlay fields."""
import argparse,json,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
PROFILES=(('iron-pristine',267,0,250),('iron-one',267,1,250),('iron-half',267,125,250),('iron-last',267,249,250),('iron-maximum',267,250,250),('iron-over',267,251,250),('diamond-maximum',276,1561,1561),('shovel-one',256,1,250),('shovel-maximum',256,250,250))
KEYS={'scope','profile','id','damage','max','x','y','draws','rectangle_prefixes','damageable','damaged','samples','bar_width','bar_intensity','rectangles'}
RECT={'x','y','width','height','color','before','words','h','p','o','r','l','m','n'}
ACTIVE=(0,1,2,5)
INACTIVE=(3,4,6,7)
def unique(pairs):
 value=dict(pairs)
 if len(value)!=len(pairs):raise ValueError('Duplicate durability JSON field')
 return value
def words(value):
 return type(value)is list and len(value)==48 and all(type(w)is str and re.fullmatch('[0-9a-f]{8}',w)for w in value)
def validate(rows):
 if type(rows)is not list or len(rows)!=9:raise ValueError('Exactly nine ordered durability profiles')
 for r,(name,item,damage,maximum)in zip(rows,PROFILES):
  if type(r)is not dict or set(r)!=KEYS:raise ValueError('Exact durability profile schema')
  if type(r['scope'])is not str or r['scope']not in ('completed-source-draws','entered-rectangle-prefix'):raise ValueError('Declared execution scope')
  if any(type(r[k])is not int for k in ('id','damage','max','x','y','draws','rectangle_prefixes')):raise ValueError('Typed durability inputs/counts')
  if (r['profile'],r['id'],r['damage'],r['max'],r['x'],r['y'])!=(name,item,damage,maximum,7,11):raise ValueError('Declared ordered original inputs')
  if type(r['damageable'])is not bool or type(r['damaged'])is not bool or not r['damageable']or r['damaged']!=(damage>0):raise ValueError('Actual IZ damage predicates')
  n=3 if damage>0 else 0
  if r['draws']!=(n if r['scope']=='completed-source-draws'else 0)or r['rectangle_prefixes']!=n:raise ValueError('Honest actual completed draws versus entered prefixes')
  if type(r['samples'])is not list or len(r['samples'])!=(3 if n else 1):raise ValueError('Exact separate operand samples')
  for s in r['samples']:
   if type(s)is not dict or set(s)!={'damage','maximum'}or type(s['damage'])is not int or type(s['maximum'])is not int or(s['damage'],s['maximum'])!=(damage,maximum):raise ValueError('Actual separately sampled source damage/maximum')
  if n:
   if type(r['bar_width'])is not int or type(r['bar_intensity'])is not int:raise ValueError('Actual Java round/l2i results')
  elif r['bar_width']is not None or r['bar_intensity']is not None:raise ValueError('Skipped overlay arithmetic is absent')
  if type(r['rectangles'])is not list or len(r['rectangles'])!=n:raise ValueError('Three actual rectangle entries')
  for rect in r['rectangles']:
   if type(rect)is not dict or set(rect)!=RECT or any(type(rect[k])is not int for k in ('x','y','width','height','color','h','p','o','r')):raise ValueError('Typed actual rectangle source inputs')
   if(rect['h'],rect['p'],rect['o'],rect['r'])!=(6,4,48,7)or any(type(rect[k])is not bool for k in ('l','m','n'))or(rect['l'],rect['m'],rect['n'])!=(True,False,False):raise ValueError('Actual NW color/UV/normal/mode flags')
   if not words(rect['before'])or not words(rect['words']):raise ValueError('All48 raw prior/current words retained')
   if any(rect['words'][i]!=rect['before'][i]for i in range(48)if i%8 in INACTIVE):raise ValueError('Actual own inactive NW backing must remain untouched')
 return rows
def projection(rows):
 validate(rows)
 out=[]
 for r in rows:
  x={k:v for k,v in r.items()if k not in ('scope','draws','rectangles')}
  x['rectangles']=[{**{k:v for k,v in rect.items()if k not in ('before','words')},'active_words':[w for i,w in enumerate(rect['words'])if i%8 in ACTIVE]}for rect in r['rectangles']]
  out.append(x)
 return out
def rows(text):
 return validate([json.loads(l.removeprefix('ITEM_DURABILITY '),object_pairs_hook=unique)for l in text.splitlines()if l.startswith('ITEM_DURABILITY ')])
def compare(actual,expected):
 if projection(actual)!=projection(expected):raise ValueError('Selected actual source durability operands/rectangle XYZ/color differ')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--stdout',type=Path,required=True);p.add_argument('--fixture',type=Path,default=HERE/'finite.json');a=p.parse_args()
 compare(rows(a.stdout.read_text()),json.loads(a.fixture.read_text(),object_pairs_hook=unique)['rows'])
 print('Nine original durability profiles;24 rectangle prefixes;576 exact active XYZ/color words; independent prior inactive backing audited')
