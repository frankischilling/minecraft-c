"""Audit actual cold failure/print milestones and fresh genuine-frame warm returns."""
import hashlib
import json
import struct

JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
def require(value,text):
 if not value:raise ValueError('Player preview observation: '+text)
def unique(pairs):
 result=dict(pairs);require(len(result)==len(pairs),'duplicate JSON field');return result
def tagged(text,prefix):return [json.loads(line.removeprefix(prefix+' '),object_pairs_hook=unique) for line in text.splitlines() if line.startswith(prefix+' ')]
def f(value):return struct.pack('>f',value).hex()
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def parse(text):
 return {key:tagged(text,'PLAYER_PREVIEW_'+marker) for key,marker in [('device','DEVICE'),('owners','OWNERS'),('inputs','INPUT'),('exits','CALL_EXIT'),('returns','RETURN'),('complete','COMPLETE'),('frame-input','FRAME_INPUT'),('frame-return','FRAME_RETURN')]}

def audit_mode(mode,ordinary,observed,events,ordinary_exit,observed_exit,ordinary_stderr,observed_stderr):
 first,second=parse(ordinary),parse(observed)
 require(ordinary_exit==observed_exit and ordinary_stderr==observed_stderr,'independent process exit and exact original stderr bytes agree')
 require(first==second,'independent actual constructor/input/GL/return observations agree')
 require(len(first['device'])==len(first['owners'])==1,'actual context and constructor receipts')
 device=first['device'][0];owners=first['owners'][0]
 require(device['display-created'] is True and device['pbuffer'] is False and [device['width'],device['height']]==[854,480],'real Display context')
 require(owners['app']=='kq' and owners['screen']=='ue' and owners['renderer']=='ds' and owners['input']=='lr' and owners['input-settings-same'] is True and owners['input-key-count']==10 and owners['source-sneaking-before'] is False,'genuine constructor graph and source LR attachment')
 require(all(owners[key] is True for key in ('engine-null','camera-null','settings-null','font-null')) and owners['view-player-same'] is False and owners['world-null'] is False and owners['world-same'] is True,'actual source World setter and remaining TH owner fields cold before any frame')
 require(len(first['inputs'])==len(first['exits']),'every attempted source call has caller-finally observation')
 for before,after in zip(first['inputs'],first['exits']):
  require(before['profile']==after['profile'] and before['kind']==after['kind'] and type(after['returned']) is bool,'actual call boundary/result attribution')
  require(before['partial']==f(.375) and before['mouse']==[[-30,-40],[-60,-70],[-90,-100],[-120,-130]][before['profile']],'declared call arguments')
  require(after['gl-error']==0,'caller-finally GL observation reports no GL error')
 if mode=='cold':
  require(first['inputs'] and first['inputs'][0]['profile']==0 and not first['frame-input'],'cold call occurs before any genuine TH.frame')
  source_caught=[e for e in events if e['event']=='exception' and e['source-GV-caught']]
  require(source_caught,'actual source GV catch observed')
  for event in source_caught:
   require(event['throw']=={'class':'bw','method':'a','descriptor':'(Ljava/lang/String;Ljava/lang/String;)Z','pc':11,'line':36,'file':'SourceFile'} and event['catch']=={'class':'gv','method':'a','descriptor':'(Lls;DDDFF)V','pc':743,'line':116,'file':'SourceFile'} and event['throwable']['class']=='java.lang.NullPointerException','actual downloaded texture Engine NPE and source handler')
   profile=event['profile'];identity=event['throwable']['id']
   for kind in ('jdk-entry','jdk-return'):
    require(any(e['event']==kind and e['profile']==profile and e['method']=='java.lang.Throwable.printStackTrace()V' and e['origin']=='source-GV747' and e['throwable']['id']==identity for e in events),'same actual caught Throwable enters and returns from source printStackTrace')
   require(any(e['event']=='phase' and e['profile']==profile and e['source']['class']=='gv' and e['source']['pc']==750 and e['caught']['id']==identity and e['caught']['stack-trace'] for e in events),'actual print return joins GV suffix with materialized same Throwable trace')
  if ordinary_exit==1:require(first['exits'][-1]['returned'] is False and not first['complete'],'actual later source escape is not relabeled completed UE')
  else:require(ordinary_exit==0 and len(first['complete'])==1,'actual cold target completion')
 else:
  require(mode=='warm' and ordinary_exit==0 and [r['profile'] for r in first['inputs']]==[2,3] and all(r['returned'] is True for r in first['exits']) and len(first['complete'])==1,'two actual fresh-context warm source returns')
  require(len(first['frame-input'])==len(first['frame-return'])==1 and not any(e['event']=='exception' for e in events),'one genuine frame and no warm source exception')
  require(all(not first['frame-return'][0]['pose']['manager-'+name+'-null'] for name in ('engine','world','camera','settings','font')),'actual frame publishes genuine owners')
  for before,after in zip(first['inputs'],first['returns']):
   require(all(before['pose'][key]==after['pose'][key] for key in ('H','aS','aT')) and after['pose']['bE']==f(0) and after['pose']['manager-i']==f(180),'actual warm UE pose restore/reset')
   require(all(before['gl'][key]==after['gl'][key] for key in ('modelview','projection','modelview-depth','projection-depth','matrix-mode')) and after['gl-error']==0,'actual warm GL continuation')
  for profile in (2,3):
   phases=[e['source']['pc'] for e in events if e['event']=='phase' and e['profile']==profile and e['source']['class']=='ue']
   require(phases==[0,347,350,361,397,400,403,408],'actual warm UE preview and restoration continuation PCs')
   parts={e['receiver'] for e in events if e['event']=='return' and e['profile']==profile and e['method']=='ps.a(F)V' and e['primary-part']}
   require(len(parts)==7,'all seven actual warm primary model Parts return')
 return {'mode':mode,'source_target_exit':ordinary_exit,'attempted_profiles':[r['profile'] for r in first['inputs']],'returned_profiles':[r['profile'] for r in first['exits'] if r['returned']],
  'source_caught_exceptions':sum(e['event']=='exception' and e['source-GV-caught'] for e in events),'first':first}
