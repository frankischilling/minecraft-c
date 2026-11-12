"""Java-free publication audit for composed original Workbench closes."""
import base64
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re

HERE=Path(__file__).resolve().parent
SPEC=importlib.util.spec_from_file_location('auto_close_captured_audit',HERE/'audit.py');A=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(A)
P=A.P
require=P.require
unique=P.unique

def load(name):return json.loads((HERE/name).read_text(encoding='utf-8'),object_pairs_hook=unique)

def typed_rows(rows,cases):
 P.validate(rows,cases)
 for row,case in zip(rows,cases):
  require(type(row['return']) is bool if case['operation']=='predicate' else row['return'] is None,'exact return primitive type')
  for which in ('before','after'):
   snap=row[which];require(type(snap['selected']) is int and type(snap['world-items']) is int,'exact integral state types')
   require(all(type(k) is str and re.fullmatch('[0-9]+',k) and type(v) is int for k,v in snap['stats'].items()),'exact statistic IDs/counts')
   for field in ('inventory','craft','result','cached'):
    require(all(v is None or type(v) is str and v in snap['stacks'] for v in snap[field]),'actual retained Stack references')
   player=snap['player'];doubles=['sn.'+n for n in ('aM','aN','aO','aP','aQ','aR')]+['gs.'+n for n in ('o','p','q','r','s','t')]
   require(set(player)==set(doubles+['sn.bt','ls.Y','sn.aS','sn.aT']),'exact selected Player scalar extent')
   require(all(type(player[k]) is str and re.fullmatch('[0-9a-f]{16}',player[k]) and math.isfinite(P.E.double(player[k])) for k in doubles),'actual finite raw binary64 words')
   require(type(player['sn.bt']) is int and type(player['ls.Y']) is int and all(type(player[k]) is str and re.fullmatch('[0-9a-f]{8}',player[k]) for k in ('sn.aS','sn.aT')),'actual tick/health/orientation types')
  initial=row['before']['player']
  require([initial['sn.'+k] for k in ('aM','aN','aO')]==[P.E.dbits(float(x)) for x in case['position']],'declared actual source position setter input')
  require(all(initial['sn.'+k]=='0000000000000000' for k in ('aP','aQ','aR')) and initial['sn.bt']==0 and initial['ls.Y']==20 and initial['sn.aS']==initial['sn.aT']=='00000000','untouched constructor velocity/tick/health and declared orientation')
  for drop in row['drops']:
   require(type(drop['pickup-delay']) is int and type(drop['chunk-members']) is int and type(drop['world-member']) is bool,'actual publication primitive types')
 return rows

def trace_identities(rows,events):
 for index,row in enumerate(rows):
  group=[e for e in events if e['index']==index];state=group[0]['state']
  for event in group:
   if event['event']=='phase':
    other=event['state'];require(all(other[k]==state[k] for k in ('player','default','screen','it','player-random-id')),'actual same owners retained through source phases')
   if event['event']!='entry':continue
   method=event['method'];receiver=event['receiver'];owner=method.split('.',1)[0]
   if owner in ('gs','ls','sn','dc'):require(receiver=={'class':'dc','id':state['player']},'actual canonical Player receiver')
   if owner in ('oo','id'):require(receiver=={'class':'oo','id':state['screen']},'actual old OO receiver')
   if owner=='it':require(receiver=={'class':'it','id':state['it']},'actual active/captured IT receiver')
   if owner=='aa':require(receiver=={'class':'aa','id':state['default']},'actual constructor AA receiver after GS.r')
   if method=='dw.a(Lgs;)V':
    expected={'class':'aa','id':state['default']} if event['caller']['method']=='aa.a(Lgs;)V' else {'class':'it','id':state['it']}
    require(receiver==expected,'independent inherited cursor-close receivers')
  close=row['after']['app-screen-null']
  if close:
   reads=[e['arguments'] for e in group if e['event']=='entry' and e['method']=='mq.f_(I)Liz;' and e['caller']=={'method':'it.a(Lgs;)V','pc':29}]
   require(reads==[[i] for i in range(9)],'all nine actual ascending IT reads; no setter-clear replay')

def transports(finite,observed):
 raw={}
 require(set(observed['transport'])=={'original.stdout','original.stderr','debugger/target.stdout','debugger/target.stderr','debugger/journal.ndjson'},'complete retained raw streams')
 for name,item in observed['transport'].items():
  require(type(item) is dict and set(item)=={'bytes','sha256','base64'},'raw stream receipt schema')
  data=base64.b64decode(item['base64'],validate=True);require(type(item['bytes']) is int and len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==item['sha256'],'exact raw stream bytes/hash')
  raw[name]=data.decode('utf-8')
 require(P.rows(raw['original.stdout'])==finite and P.rows(raw['debugger/target.stdout'])==observed['rows'],'published independent raw rows are preserved exactly')
 require([json.loads(line,object_pairs_hook=unique) for line in raw['debugger/journal.ndjson'].splitlines()]==observed['events'],'read-only raw event transport preserved exactly')
 for name in ('original.stderr','debugger/target.stderr'):
  lines=raw[name].splitlines();require(len(lines)==4 and all(line.startswith('WARNING:') for line in lines) and any('System::setSecurityManager' in line for line in lines),'only declared JVM deprecation stderr, no caught source exception')
 for name in ('original.stdout','debugger/target.stdout'):
  lines=raw[name].splitlines();marker='WORKBENCH_AUTO_CLOSE_COMPLETE '
  require(lines[-1].startswith(marker),'actual target final completion marker')
  complete=json.loads(lines[-1][len(marker):],object_pairs_hook=unique)
  require(complete['rows']==7 and complete['network']==0 and complete['display-created'] is True and complete['raw-allocations']==0 and complete['game-substitution'] is False and complete['random-rebinding'] is False and complete['invocation-repairs']==0 and complete['source-input-attachments']==7,'actual bounded source process completion')
  devices=[json.loads(line.split(' ',1)[1],object_pairs_hook=unique) for line in lines if line.startswith('WORKBENCH_AUTO_CLOSE_DEVICE ')]
  require(len(devices)==1 and devices[0]['display-created'] is True and devices[0]['pbuffer'] is False,'actual original Display context, no Pbuffer conflation')

def audit_public():
 result=A.audit_public();finite=load('finite.json');observed=load('observations.json');cases=load('declared-inputs.json')['cases']
 typed_rows(finite['rows'],cases);typed_rows(observed['rows'],cases);trace_identities(observed['rows'],observed['events']);transports(finite['rows'],observed)
 provenance=load('provenance.json')
 for name,value in provenance['publication_helpers'].items():require(A.digest(HERE/name)==value,'unchanged publication validator '+name)
 require(provenance['status']=='PASS' and provenance['scope']['source-calls-per-process']==7 and provenance['scope']['composed-gs-ticks-per-process']==4 and provenance['scope']['whole-world-ticks']==0,'bounded source call scope')
 require(len(provenance['verified_inputs'])==112 and all(type(row) is dict and set(row)=={'path','before','after'} and row['before']==row['after'] and re.fullmatch('[0-9a-f]{64}',row['before']) for row in provenance['verified_inputs']),'all original/JDK/library/native/helper inputs unchanged')
 require(provenance['capture_helpers']==provenance['public_helpers'],'published captured helper bytes equal pre-execution pins')
 require(set(provenance['compiled_helpers'])=={'WorkbenchAutoCloseTarget.class','WorkbenchAutoCloseTarget$1.class','WorkbenchAutoCloseTarget$Observer.class','WorkbenchAutoCloseJdi.class','WorkbenchAutoCloseJdi$Drain.class'},'only five authored helper classes compiled, no game-class substitution')
 require(len(provenance['failed_cohorts'])==2 and [f['classification'] for f in provenance['failed_cohorts']]==['observer-focus-recursion-assertion','archive-class-name-lookup'],'retained failed observer/pin cohorts')
 result.update({'live-Code-pins':len(observed['live_Code']),'source-class-inventories':len(load('source-pins.json')['classes']),'events':len(observed['events'])})
 return result

audit=audit_public
if __name__=='__main__':print(json.dumps(audit_public(),indent=2))
