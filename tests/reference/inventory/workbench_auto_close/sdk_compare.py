"""Declared structural comparison across GS-only and whole-World tick scopes."""
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
SPEC=importlib.util.spec_from_file_location('auto_close_sdk_packet',HERE/'packet.py');Q=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(Q)
require=Q.require
MARKER='WORKBENCH_AUTO_CLOSE_STRUCTURE '
FIELDS={'profile','screen-null','active-default','cursor-null','grid0-retained-same','grid0','grid-rest-empty','drop-input-refs','drop-values','drop-stack-same-input'}

def structural_projection(row):
 require(row['profile'] in ('tick-removed','tick-distance') and row['operation']=='gs-tick','only declared composed primary close profiles')
 before,after=row['before'],row['after'];grid=before['craft'][0]
 return {'profile':row['profile'],'screen-null':after['app-screen-null'],'active-default':after['active-default'],'cursor-null':after['cursor'] is None,
  'grid0-retained-same':after['craft'][0]==grid,'grid0':list(after['stacks'][after['craft'][0]]),'grid-rest-empty':after['craft'][1:]==[None]*8,
  'drop-input-refs':[d['ref'] for d in row['drops']],'drop-values':[list(d['value']) for d in row['drops']],
  'drop-stack-same-input':[d['ref']==r for d,r in zip(row['drops'],[before['cursor'],grid])]}

def rows(stdout):
 return [json.loads(line[len(MARKER):],object_pairs_hook=Q.unique) for line in stdout.splitlines() if line.startswith(MARKER)]

def validate(actual):
 require(type(actual) is list and len(actual) in (1,2),'one explicitly selected primary row or both ordered primary rows')
 profiles=[r.get('profile') for r in actual]
 require(profiles==['tick-removed','tick-distance'] if len(actual)==2 else profiles[0] in ('tick-removed','tick-distance'),'declared structural row selection/order')
 for row in actual:
  require(type(row) is dict and set(row)==FIELDS,'exact structural schema')
  require(all(type(row[k]) is bool for k in ('screen-null','active-default','cursor-null','grid0-retained-same','grid-rest-empty')),'typed structural owner/alias booleans')
  require(type(row['grid0']) is list and len(row['grid0'])==4 and all(type(v) is int for v in row['grid0']),'actual grid Stack scalar types')
  require(type(row['drop-input-refs']) is list and len(row['drop-input-refs'])==2 and all(type(v) is str for v in row['drop-input-refs']),'actual ordered input reference labels')
  require(type(row['drop-values']) is list and len(row['drop-values'])==2 and all(type(v) is list and len(v)==4 and all(type(n) is int for n in v) for v in row['drop-values']),'actual ordered drop Stack scalar types')
  require(type(row['drop-stack-same-input']) is list and len(row['drop-stack-same-input'])==2 and all(type(v) is bool for v in row['drop-stack-same-input']),'actual within-process drop Stack aliases')
 return actual

def compare(actual,original):
 Q.typed_rows(original,Q.load('declared-inputs.json')['cases']);validate(actual)
 profiles=[row['profile'] for row in actual]
 expected=[structural_projection(row) for row in original if row['profile'] in profiles]
 require(actual==expected,'exact declared close state, retained grid alias and publication sequence')
 return {'rows':len(actual),'profiles':profiles,'scope':'structural close projection only; independent coordinates, motion, entropy and World Item ticks remain outside this comparison'}
