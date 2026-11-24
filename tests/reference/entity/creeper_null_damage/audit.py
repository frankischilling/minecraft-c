"""Java-free audit of static Creeper source pins; no damage execution credit."""
import hashlib
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
CLASSES={'gb':'ed05890aa9881dcc52c1d47963e4e69995b265a3db9a07fdd0af2291db4241f7','gz':'1754401116a87cd6237ccf135a4703affee9087d44a856fcd66f099801795088','ii':'ab344d121ef7f2adfb4e7625020bb924d8a7f80a8eebe61d37909916f51b7e7a','ls':'b70fbb1499cd1b37ef27dd5dec7637c7338439ef03fe13012dca93fb2326d078','sn':'b53107a5bdd5ff92637582aaed4fd49366443a11cba96cef0a6ad719e197b7be'}
def require(value,text):
 if not value:raise ValueError('Static Creeper packet: '+text)
def unique(pairs):
 value=dict(pairs);require(len(value)==len(pairs),'duplicate JSON field');return value
def load(name):return json.loads((HERE/name).read_text(encoding='utf8'),object_pairs_hook=unique)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def reference(kind,owner,name,descriptor):return {'kind':kind,'class':owner,'name':name,'descriptor':descriptor}
def validate(pins):
 require(set(pins)=={'schema','scope','runtime_observation','executed_original_damage_calls','jar_bytes','jar_sha256','classes','hierarchy','resolved_declarations','selected_references','selected_instruction_windows'},'exact static schema')
 require(type(pins['schema']) is int and pins['schema']==1 and pins['runtime_observation'] is False and type(pins['executed_original_damage_calls']) is int and pins['executed_original_damage_calls']==0 and pins['scope'].startswith('STATIC ONLY:'),'static-only scope; zero original executed calls')
 require(pins['jar_sha256']==JAR and type(pins['jar_bytes']) is int and pins['jar_bytes']==1465375 and set(pins['classes'])==set(CLASSES),'supplied original archive and actual five-class hierarchy')
 path=['gb','gz','ii','ls','sn'];require(pins['hierarchy']==path,'exact Creeper ancestry')
 for index,name in enumerate(path):
  cls=pins['classes'][name];require(cls['class_sha256']==CLASSES[name] and cls['superclass']==(path[index+1] if index+1<len(path) else 'java/lang/Object'),'actual class identity/superclass')
  inventory=cls['method_inventory'];require(type(inventory) is list and type(cls['method_count']) is int and cls['method_count']==len(inventory),'complete declared method inventory')
  require(all(type(m) is dict and set(m)=={'name','descriptor','access'} and type(m['name']) is str and type(m['descriptor']) is str and type(m['access']) is int for m in inventory),'typed original inventory')
  require(len({(m['name'],m['descriptor']) for m in inventory})==len(inventory),'unique declarations')
  require(hashlib.sha256(json.dumps(inventory,sort_keys=True,separators=(',',':')).encode()).hexdigest()==cls['method_inventory_sha256'],'unchanged complete method inventory')
  for method in cls['selected_methods']:
   require(any(all(row[k]==method[k] for k in ('name','descriptor','access')) for row in inventory),'selected Code belongs to declared method')
 def declaring(name,method,descriptor):
  while name in pins['classes']:
   cls=pins['classes'][name]
   if any((m['name'],m['descriptor'])==(method,descriptor) for m in cls['method_inventory']):return name
   name=cls['superclass']
  raise ValueError('Static Creeper packet: unresolved declaring method')
 expected={'gb.a(Lsn;I)Z':'gz','ii.a(Lsn;I)Z':'ls','gb.j_()Ljava/lang/String;':'gb','gb.i()Ljava/lang/String;':'gb','gb.k()F':'ls','gb.b(Lsn;)V':'gb','gz.b(Lsn;)V':'ls'}
 require(pins['resolved_declarations']==expected,'actual virtual/special declaring owners')
 for key,owner in expected.items():
  prefix,descriptor=key.split('(',1);kind,method=prefix.split('.',1)
  require(declaring(kind,method,'('+descriptor)==owner,'independent resolution through complete II inventory')
 refs=pins['selected_references']
 expected_refs={
 'gz.a(Lsn;I)Z:3':('invokespecial',reference('method','ii','a','(Lsn;I)Z')),
 'ls.U()V:9':('invokespecial',reference('method','sn','U','()V')),
 'ls.U()V:66':('invokevirtual',reference('method','ls','a','(Lsn;I)Z')),
 'sn.U()V:501':('invokevirtual',reference('method','sn','a','(Lsn;I)Z')),
 'ls.a(Lsn;I)Z:284':('invokevirtual',reference('method','ls','i','()Ljava/lang/String;')),
 'ls.a(Lsn;I)Z:288':('invokevirtual',reference('method','ls','k','()F')),
 'ls.a(Lsn;I)Z:311':('invokevirtual',reference('method','fd','a','(Lsn;Ljava/lang/String;FF)V')),
 'ls.a(Lsn;I)Z:316':('invokevirtual',reference('method','ls','b','(Lsn;)V')),
 'ls.a(Lsn;I)Z:332':('invokevirtual',reference('method','ls','j_','()Ljava/lang/String;')),
 'ls.a(Lsn;I)Z:336':('invokevirtual',reference('method','ls','k','()F')),
 'ls.a(Lsn;I)Z:359':('invokevirtual',reference('method','fd','a','(Lsn;Ljava/lang/String;FF)V')),
 'gb.b(Lsn;)V:2':('invokespecial',reference('method','gz','b','(Lsn;)V')),
 'gb.b(Lsn;)V:6':('instanceof',{'kind':'class','name':'fr'}),
 }
 for method,text in [('j_','mob.creeper'),('i','mob.creeperdeath')]:expected_refs['gb.'+method+'()Ljava/lang/String;:0']=('ldc',{'kind':'String','text':text,'utf8_hex':text.encode().hex()})
 require(refs=={key:{'operation':op,'reference':ref} for key,(op,ref) in expected_refs.items()},'exact source calls/sound constants and fatal parent before Skeleton branch')
 windows=pins['selected_instruction_windows']
 require(set(windows)=={'gb.j_()Ljava/lang/String;','gb.i()Ljava/lang/String;','gb.b(Lsn;)V','ls.U()V','ls.k()F'},'exact selected short instruction windows')
 def ops(key):return [(row['pc'],row['opcode'],row['operand_hex']) for row in windows[key]]
 require(ops('gb.j_()Ljava/lang/String;')==[(0,18,'07'),(2,176,'')] and ops('gb.i()Ljava/lang/String;')==[(0,18,'08'),(2,176,'')],'literal load followed by actual return')
 require(ops('gb.b(Lsn;)V')==[(0,42,''),(1,43,''),(2,183,'002f')],'unconditional receiver/attacker parent call first at2')
 require(ops('ls.U()V')==[(63,42,''),(64,1,''),(65,4,''),(66,182,'00fa'),(69,87,'')],'actual NULL attacker and damage1 call/result discard')
 require(ops('ls.k()F')==[(0,12,''),(1,174,'')],'actual inherited volume1 float leaf')
 return {'scope':'STATIC ONLY','classes':5,'selected_methods':sum(len(c['selected_methods']) for c in pins['classes'].values()),'resolved_declarations':7,'selected_references':15,'executed_original_damage_calls':0}

def audit_public():
 result=validate(load('source-pins.json'));provenance=load('provenance.json')
 require(provenance['scope']=='STATIC ONLY' and provenance['executed_original_damage_calls']==0 and provenance['jar_before']==provenance['jar_after']==JAR,'offline source-only provenance')
 for key in ('helpers','outputs'):
  for name,expected in provenance[key].items():require(digest(HERE/name)==expected,'unchanged '+key+' '+name)
 return result

audit=audit_public
if __name__=='__main__':print(audit_public())
