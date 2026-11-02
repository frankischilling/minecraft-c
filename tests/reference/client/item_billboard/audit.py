"""Audit each real Item/BB stream and actual returned local NW batches."""
import hashlib
import json
import math
import struct
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
MASK=(1<<48)-1
COUNTS=[1,2,6,21]
COPIES=[1,2,3,4]
def require(value,text):
 if not value:raise ValueError('Original Item billboard: '+text)
def unique(pairs):
 result=dict(pairs);require(len(result)==len(pairs),'duplicate JSON field');return result
def tagged(text,prefix):return [json.loads(line.removeprefix(prefix+' '),object_pairs_hook=unique) for line in text.splitlines() if line.startswith(prefix+' ')]
def f32(value):return struct.unpack('>f',struct.pack('>f',value))[0]
def f(value):return struct.pack('>f',value).hex()
def d(value):return struct.pack('>d',value).hex()
def floatword(word):return struct.unpack('>f',bytes.fromhex(word))[0]
def doubleword(word):return struct.unpack('>d',bytes.fromhex(word))[0]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def advance(seed,n):
 values=[]
 for unused in range(n):seed=(seed*0x5deece66d+11)&MASK;values.append((seed>>24)/(1<<24))
 return seed,values

def target(text):
 values={name:tagged(text,'ITEM_BILLBOARD_'+name.upper()) for name in ('device','owners','initial','input','return','complete')}
 require(all(len(values[k])==1 for k in ('device','owners','initial','complete')),'actual context and complete constructor receipts')
 owners=values['owners'][0];device=values['device'][0]
 require(device['display-created'] is True and device['pbuffer'] is False and [device['width'],device['height']]==[854,480],'real Display, no Pbuffer')
 require(all(owners[k]==v for k,v in {'app':'kq','world':'fd','player':'dc','engine':'ji','packs':'ik','font':'sj','manager':'th','renderer':'bb'}.items()),'genuine complete owners')
 require(all(owners[k] is True for k in ('renderer-manager-same','engine-same','world-same','camera-same','settings-same','font-same','canonical-renderer')),'actual TH.frame and canonical renderer relationships')
 require(owners['downloads-before']==0,'no prepared downloader')
 require([r['profile'] for r in values['input']]==[0,1,2,3] and [r['profile'] for r in values['return']]==[0,1,2,3],'all four actual parent calls return')
 for profile,(before,after) in enumerate(zip(values['input'],values['return'])):
  require(before['render-position']==[d(1.25),d(2.5),d(-3.75)] and before['yaw']==f(35) and before['partial']==f(.375),'declared original call inputs')
  first,last=before['state'],after['state']
  require(first['stack']==[280,COUNTS[profile],0,0] and first['item-class']=='hl' and first['stack-class']=='iz' and first['stack-same'] is True and first['world-same'] is True and first['world-member'] is False,'actual constructor Item/Stack graph, no World publication claim')
  for key in ('item-class','stack-class','stack','stack-same','world-same','world-member','item','entity','item-random','manager-yaw'):require(first[key]==last[key],'actual Source draw preserves '+key)
  require(first['item']['b']==0 and first['item']['c']==0 and first['item']['f']==5 and 0<=floatword(first['item']['d'])<2*math.pi,'actual unchanged constructor age/delay/health and independent phase')
  expected,unused=advance((187^0x5deece66d)&MASK,3*(COPIES[profile]-1))
  require(last['renderer-random']==f'{expected:016x}','actual BB source reseed187 plus copy-draw progression')
  if profile:require(first['renderer-random']==values['return'][profile-1]['state']['renderer-random'],'same actual renderer Random owner persists between profiles')
  require(after['gl-error']==0 and after['downloads-after']==0 and after['gl']['rescale-normal'] is False,'actual BB final GL continuation')
  require(all(before['gl'][k]==after['gl'][k] for k in ('modelview','projection','modelview-depth','projection-depth','matrix-mode')),'actual balanced parent matrices/stacks')
 complete=values['complete'][0]
 require(complete['profiles']==4 and complete['source-constructors']==4 and complete['actual-TH-frame-calls']==1 and complete['network']==0 and complete['random-rebinding'] is False and complete['game-substitution'] is False and all(complete[k]==0 for k in ('raw-allocations','nw-clears','player-field-repairs','world-field-repairs','item-field-repairs','helper-random-writes')),'bounded genuine constructor scene without repairs')
 return values

def audit_constructor(event):
 words=event['math-values'];require(len(words)==4,'four actual unchanged HL constructor Math returns')
 values=[doubleword(word) for word in words];require(all(0<=value<1 and math.isfinite(value) for value in values),'independent shared Math entropy')
 require(event['phase']==f(f32(values[0]*math.pi*2)) and event['yaw']==f(f32(values[1]*360)),'actual constructor phase/yaw formulas')
 expected=[d(float(f32(values[2]*float(f32(.2))-float(f32(.1))))),d(float(f32(.2))),d(float(f32(values[3]*float(f32(.2))-float(f32(.1)))))]
 require(event['velocity']==expected and event['position']==[d(8.5),d(64),d(8.5)],'actual full constructor position/velocity formulas')


def audit(report,ordinary,observed,events):
 first,second=target(ordinary),target(observed)
 require(first['device']==second['device'],'same recorded real backend')
 require(not any(e['event']=='exception' for e in events),'no source or probe exception during constructor/parent calls')
 constructors=[e for e in events if e['event']=='constructor'];require([e['profile'] for e in constructors]==[0,1,2,3],'all four genuine full constructors observed')
 for event in constructors:
  audit_constructor(event);state=second['input'][event['profile']]['state']
  require(event['phase']==state['item']['d'] and event['private-random']==state['item-random'],'same actual constructor state reaches real renderer')
 draws=[e for e in events if e['event']=='nw-draw'];order=[(p,c) for p in range(4) for c in range(COPIES[p])]
 require([(e['profile'],e['copy']) for e in draws]==order,'actual ten copies in declared source order')
 for row in draws:
  state=row['state'];words=row['words'];prior=row['prior']
  require(len(words)==len(prior)==64 and state['nw.h']==6 and state['nw.p']==4 and state['nw.o']==48 and state['nw.r']==7 and state['nw.w'] is True,'actual source quad conversion and own64word backing')
  require(state['nw.n'] is True and state['nw.m'] is True and state['nw.l'] is False and row['icon']==53,'actual normal/texture-only Stick icon53')
  active={8*v+s for v in range(6) for s in ([0,1,2,3,4,6] if v in (0,1,2,5) else [0,1,2,3,4])}
  require(all(words[i]==prior[i] for i in range(64) if i not in active),'own same-index inactive and unwritten tail; no staging')
 for profile in range(4):
  entries=[e for e in events if e['event']=='entry' and e['profile']==profile];returns=[e for e in events if e['event']=='return' and e['profile']==profile]
  require(sum(e['method']=='bb.a(Lhl;DDDFF)V' for e in entries)==sum(e['method']=='bb.a(Lhl;DDDFF)V' for e in returns)==1,'actual BB entry and full return')
  seeds=[e for e in entries if e['method']=='java.util.Random.setSeed(J)V'];require(len(seeds)==1 and seeds[0]['arguments']==[187] and any(f['class']=='bb' and f['descriptor']=='(Lhl;DDDFF)V' and f['pc']==7 for f in seeds[0]['frames']),'actual Source setSeed187 receiver/caller')
  random=[e for e in returns if e['method']=='java.util.Random.nextFloat()F'];expected,values=advance((187^0x5deece66d)&MASK,3*(COPIES[profile]-1))
  require([e['return-value'] for e in random]==[f(v) for v in values],'actual private Random returns, no helper replay')
  offsets=[e for e in events if e['event']=='gl-entry' and e['profile']==profile and e['method']=='org.lwjgl.opengl.GL11.glTranslatef(FFF)V' and any(fr['class']=='bb' and fr['pc']==652 for fr in e['frames'])]
  require(len(offsets)==COPIES[profile]-1,'actual additional-copy translations')
  for index,event in enumerate(offsets):require(event['arguments']==[f(f32(f32(f32(v*2)-1)*f32(.3))) for v in values[3*index:3*index+3]],'actual copy offset float operations and GL arguments')
  sine_entries=[e for e in entries if e['method']=='in.a(F)F'];sine=[e for e in returns if e['method']=='in.a(F)F'];require(len(sine_entries)==len(sine)==1,'actual bob sine-table leaf')
  phase=floatword(second['input'][profile]['state']['item']['d']);argument=f32(f32(f32(.375)/10)+phase)
  require(sine_entries[0]['arguments']==[f(argument)],'actual bob argument from own phase')
  bob=f32(f32(floatword(sine[0]['return-value'])*f32(.1))+f32(.1))
  outer=[e for e in events if e['event']=='gl-entry' and e['profile']==profile and e['method']=='org.lwjgl.opengl.GL11.glTranslatef(FFF)V' and any(fr['class']=='bb' and fr['pc']==124 for fr in e['frames'])]
  require(len(outer)==1 and outer[0]['arguments']==[f(1.25),f(f32(f32(2.5)+bob)),f(-3.75)],'actual unnormalized per-process bob translation')
  require(sum(e['method']=='nw.a()V' for e in returns)==COPIES[profile],'all actual completed NW draws return')
 return {'profiles':4,'source_constructors':4,'selected_draws':10,'raw_words':480,'active_words':340,'inactive_words':140,'unwritten_tail_words':160,'scope':'Genuine original Item280 constructor scene and complete BB parent returns; local NW prefix comparison has separate native scope.','draws':draws}
