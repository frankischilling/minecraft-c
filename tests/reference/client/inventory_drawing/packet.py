"""Java-free audit of published original Inventory drawing evidence."""
import hashlib
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent

def module(name,file):
 spec=importlib.util.spec_from_file_location(name,HERE/file);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
A=module('inventory_captured_audit','audit.py')
P=module('inventory_quad_compare','compare.py')

def load(name):return json.loads((HERE/name).read_text(encoding='utf-8'),object_pairs_hook=P.unique)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def require(value,text):P.require(value,'packet '+text)

def from_draw(row):
 state=row['state']
 return {'profile':row['profile'],'kind':row['kind'],'words':row['words'],'prior':row['prior'],
  'state':{'vertices':state['nw.h'],'source-vertices':state['nw.p'],'words':state['nw.o'],'mode':state['nw.r'],
    'building':state['nw.w'],'color':state['nw.l'],'texture':state['nw.m'],'normal':state['nw.n'],
    'translation':[state['nw.'+name] for name in ('s','t','u')]},'completed-draws':1}

def audit_public():
 finite=load('finite.json');observed=load('observations.json');pins=load('source-pins.json');provenance=load('provenance.json')
 require(finite['schema']==observed['schema']==provenance['schema']==1 and finite['client_jar_sha256']==pins['client_jar_sha256']==A.JAR,'supplied unchanged original archive')
 result=A.audit({},observed['ordinary_stdout'],observed['jdi_stdout'],observed['events'])
 draws=[row for row in observed['events'] if row['event']=='nw-draw']
 require(finite['rows']==[from_draw(row) for row in draws],'declared selection from actual original draw records, no replacement values')
 P.validate(finite['rows'],1)
 require(sum(len(P.active_indices(row)) for row in finite['rows'])==108,'all108 active source words')
 for key,value in observed['live_Code'].items():
  owner,tail=key.split('.',1);name,descriptor=tail.split('(',1)
  original=next(m for m in pins['classes'][owner]['methods'] if m['name']==name and m['descriptor']=='('+descriptor)
  require(all(original[k]==v for k,v in value.items()),'actual live source Code pin')
 require(len(observed['live_Code'])==14,'all14 live source Codes retained')
 for key in ('captured_helpers','publication_helpers','outputs'):
  for name,expected in provenance[key].items():require(digest(HERE/name)==expected,'unchanged '+key+' '+name)
 for field,payload in [('original_stdout_sha256','ordinary_stdout'),('jdi_stdout_sha256','jdi_stdout'),('original_stderr_sha256','ordinary_stderr'),('jdi_stderr_sha256','jdi_stderr')]:
  require(hashlib.sha256(observed[payload].encode('utf-8')).hexdigest()==provenance[field],'actual stdout byte preservation')
 for key in ('ordinary_stderr','jdi_stderr'):
  lines=observed[key].splitlines()
  require(len(lines)==4 and all(line.startswith('WARNING:') for line in lines) and any('System::setSecurityManager' in line for line in lines),'no caught-renderer/body stderr error')
 require(provenance['failed_prepared_2']['status']=='FAIL' and provenance['failed_prepared_2']['reused'] is False,'retained genuine missing-Input failure')
 return {'profiles':result['profiles'],'original_completed_draws':4,'native_completed_draws_qualified_here':0,
         'raw_words':192,'active_words':108,'own_inactive_words':84,'primary_part_returns':14,'live_Code_pins':14}

audit=audit_public
if __name__=='__main__':print(audit_public())
