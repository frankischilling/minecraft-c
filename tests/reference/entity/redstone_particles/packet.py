"""Strict finite IM constructor streams and unchanged original CPU component records."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
PINS={'.gitattributes': 'db9d3e5de890fe4e5d613aff34301f0453008914226a23a11630a3dd51ca933d', 'audit.py': 'c4fc7cfda7b51cde86b2f6e30bfde83e5accfc4796de22dadfddc2b8f10cb4e1', 'capture_reference.py': '07a8ad84f676fc7b7d10d09094d6f22b5a28c25f829beb175363de67ef7ac18b', 'compare.py': 'dd05614a35d19d266d7ad8bfbd423eb1dd89ae75f166bb8dae40a52935bbfa5a', 'declared-inputs.json': 'a4a864f331c4eed54a3a2e323a6933bb998513e430804910b9bafc3336023fd3', 'ordinary-0.components.json': '90a234eaa6cbd03253549c047322d42d33c81297749453e2f9de8854a6a116bc', 'ordinary-0.constructors.json': '0a6b5e4c9ffa6098cb6b84ec25c4aece254d860b128c00ac1f4e78aef6f75929', 'ordinary-0.stdout': '34c89d7cc2eea6629e2a4694ae87c536d371656790b048b40c492ec62383d9a9', 'ordinary-1.components.json': '90a234eaa6cbd03253549c047322d42d33c81297749453e2f9de8854a6a116bc', 'ordinary-1.constructors.json': 'd7f120f8706afa194f59427121b0c6f8a51e5a20b590596403d8f8f8c6e7c439', 'ordinary-1.stdout': 'c678d5627364577aa232e9f46725e380282f7b08d32aac7a5ae922142c4b8cd1', 'profiles.tsv': 'a2c91c68f8db47318004894d2dc62051750ca93559645f13bacc3d91c7dd0b00', 'provenance.json': '3ce2cc3ea85c8a2085de3a3df891d7fcc4c13405437a97c1b95a072851509af5', 'README.md': 'b26a2eb7cd5b1451b111124b27be6f710a01b0eef3fae0b4898ac33207ca060a', 'RedstoneParticleObservation.java': '17764c48fab82c26aee5e5b4f148ee24ee1e4a5d3c0ef9c0eca1bf45e2a3c664', 'source-code.json': 'a50668a1932e330c4d8a55021b3243b294f0305adb80b726419654847ab9d607'}
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'

def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
A=module('redstone_packet_audit',HERE/'audit.py')
P=module('redstone_packet_compare',HERE/'compare.py')
require=A.require
STATE={'position','previous','velocity','bounds','size','y_size','texture','c','d','age','lifetime','scale','gravity','rgb','base_scale','dead','ground','no_clip','collision'}
COMPONENT={'profile','name','kind','operation','exception','before','after','brightness','nw','packed'}
GUARD='REDSTONE_PARTICLE_GUARD source network=0 native=0 write=0 exec=0'
COMPLETE='REDSTONE_PARTICLE_COMPLETE constructors=37 measured=36 components=30 own-unchanged-words=25165440 network-controls=2 native-controls=1 write-controls=1 exec-controls=1'

def digest(data):return hashlib.sha256(data).hexdigest()
def state_schema(s):
 require(type(s) is dict and set(s)==STATE,'complete particle state schema')
 for key,count in (('position',3),('previous',3),('velocity',3),('bounds',6)):
  require(type(s[key]) is list and len(s[key])==count and all(type(v) is str and re.fullmatch('[0-9a-f]{16}',v) for v in s[key]),'double state vector '+key)
 for key,count in (('size',3),('rgb',3)):
  require(type(s[key]) is list and len(s[key])==count and all(type(v) is str and re.fullmatch('[0-9a-f]{8}',v) for v in s[key]),'float state vector '+key)
 for key in ('y_size','c','d','scale','gravity','base_scale'):require(type(s[key]) is str and re.fullmatch('[0-9a-f]{8}',s[key]),'float state scalar '+key)
 for key in ('texture','age','lifetime'):require(type(s[key]) is int and -(1<<31)<=s[key]<(1<<31),'int state scalar '+key)
 for key in ('dead','ground','no_clip'):require(type(s[key]) is bool,'boolean state scalar '+key)
 require(type(s['collision']) is list and len(s['collision'])==6 and all(type(v) is bool for v in s['collision']),'collision flags')

def constructor_fields(s):
 state_schema(s)
 require(s['previous']==['0000000000000000']*3 and s['size']==['3dcccccd','3e4ccccd','3e4ccccd'],'original XW size and SN previous defaults')
 require(s['bounds']==['4020ccccccc00000','4050199999980000','4020ccccccc00000','4021333333400000','4050266666680000','4021333333400000'],'actual source setPosition bounds from input coordinates and widened float dimensions')
 require(s['y_size']==s['gravity']=='00000000' and s['dead'] is False and s['ground'] is False and s['collision']==[False,False,False,False,False,True],'complete remaining original constructor defaults')

def parse_profiles(data):
 lines=data.decode('ascii').splitlines();require(data==('\n'.join(lines)+'\n').encode('ascii'),'canonical profile LF')
 rows=[line.split('\t') for line in lines]
 require(len(rows)==36 and all(len(r)==14 for r in rows) and len({r[0] for r in rows})==36,'full declared profile inventory')
 return rows

def qualify(directory=None):
 base=HERE if directory is None else Path(directory)
 require(bool(PINS),'frozen public artifact inventory')
 for name,pin in PINS.items():require(digest((base/name).read_bytes())==pin,'artifact pin '+name)
 profiles=parse_profiles((base/'profiles.tsv').read_bytes());declared=json.loads((base/'declared-inputs.json').read_text(encoding='utf-8'))
 require(declared['profiles']==profiles,'all constructor/component authored inputs declared')
 source=json.loads((base/'source-code.json').read_text(encoding='utf-8'));require(source['client_jar_sha256']==JAR and set(source['classes'])=={'im','xw','sn','nw','fd','eq','in','lm','ok','wy','xa'},'unchanged source authority and class inventory')
 methods={(m['name'],m['descriptor']):m for m in source['classes']['im']['methods']}
 require({k:v['code_bytes'] for k,v in methods.items()}=={('<init>','(Lfd;DDDFFF)V'):18,('<init>','(Lfd;DDDFFFF)V'):203,('a','(Lnw;FFFFFF)V'):65,('w_','()V'):185},'four exact IM Code body lengths')
 receipts=json.loads((base/'provenance.json').read_text(encoding='utf-8'));require(receipts['status']=='PASS' and receipts['ordinary_fresh_vms']==2 and receipts['original_game_class_replacements']==0,'successful original ordinary VM capture')
 require(receipts['inputs_unchanged'] and receipts['inputs_before']==receipts['inputs_after'],'exact frozen original/external input continuity')
 require(receipts['helpers_unchanged'] and receipts['helpers_before']==receipts['helpers_after'],'exact compiled authored helper continuity')
 for name in ('RedstoneParticleObservation.java','profiles.tsv','declared-inputs.json','capture_reference.py','audit.py','compare.py'):
  require(receipts['inputs_before']['authored/'+name]==digest((base/name).read_bytes()),'actual executed producer '+name)
 components=[];audits=[]
 for process in range(2):
  raw=(base/f'ordinary-{process}.stdout').read_bytes();lines=raw.decode('ascii').splitlines()
  require(raw==('\r\n'.join(lines)+'\r\n').encode('ascii') and len(lines)==71,'full exact original CRLF transport')
  require([(i,line) for i,line in enumerate(lines) if not line.startswith('REDSTONE_PARTICLE')]==[(32,'16 achievements'),(33,'151 recipes')] and lines[-2:]==[GUARD,COMPLETE],'actual bootstrap/guards/terminal')
  require(receipts['runs'][process]['returncode']==0 and receipts['runs'][process]['stdout_sha256']==digest(raw),'actual original process exit and transport hash')
  text='\n'.join(lines)+'\n';ctors=P.rows(text,'REDSTONE_PARTICLE_CONSTRUCTOR');rows=P.components(text)
  require(json.loads((base/f'ordinary-{process}.constructors.json').read_text(encoding='utf-8'))==ctors,'every original constructor retained')
  require(json.loads((base/f'ordinary-{process}.components.json').read_text(encoding='utf-8'))==rows,'every original component retained')
  expected=['REDSTONE_PARTICLE_CONSTRUCTOR']
  for profile in profiles:expected.extend(['REDSTONE_PARTICLE_CONSTRUCTOR']+([] if profile[2]=='constructor' else ['REDSTONE_PARTICLE_COMPONENT']))
  require([line.split(' ',1)[0] for line in lines[:-2] if line.startswith('REDSTONE_PARTICLE')]==expected,'exact original profile/component order, no hidden or omitted rows')
  for row in ctors:constructor_fields(row['state'])
  audits.append(A.ordinary(ctors))
  for row in rows:
   require(type(row) is dict and set(row)==COMPONENT,'complete original component schema')
   profile=profiles[row['profile']]
   require([row['name'],row['kind'],row['operation']]==profile[:3],'actual component input identity')
   state_schema(row['before']);state_schema(row['after'])
  components.append(rows)
 P.compare(*components)
 require(receipts['logical_components_identical'] and receipts['own_constructor_audits']==audits,'independent own-stream audits and exact semantic equality scope')
 return {'ordinary_vms':2,'constructors_per_vm':37,'components_per_vm':30,'own_constructor_audits':audits,'inactive_nw_words_per_vm':25165440,'game_code_replacements':0,'gl_calls':0}

def reference_components(directory=None):
 qualify(directory);base=HERE if directory is None else Path(directory)
 return json.loads((base/'ordinary-0.components.json').read_text(encoding='utf-8'))

if __name__=='__main__':print('Original Redstone particle packet PASS:',qualify())
