"""Audit each process's real source Player stream and actual published drops."""
from decimal import Decimal, localcontext
import math
import re
import struct

MASK=(1<<48)-1

def require(value,text):
 if not value: raise ValueError('Auto-close entropy: '+text)

def state(value):
 require(type(value) is str and re.fullmatch('[0-9a-f]{16}',value) is not None and int(value,16)<=MASK,'actual48-bit seed')
 return int(value,16)

def f32(value): return struct.unpack('>f',struct.pack('>f',value))[0]
def fbits(value): return struct.pack('>f',value).hex()
def dbits(value): return struct.pack('>d',value).hex()
def double(value):
 require(type(value) is str and re.fullmatch('[0-9a-f]{16}',value) is not None,'raw64 double')
 return struct.unpack('>d',bytes.fromhex(value))[0]

def trig_bracket(angle, word, cosine):
 # Authored Taylor evaluation, not the host libm. Domain is the actual f32
 # source angle [0,2*pi], so 110 Decimal digits/180 terms bound truncation and
 # rounding far below any relevant binary64 spacing. Refuse ambiguous edges.
 value=double(word)
 require(math.isfinite(value),'finite recorded Math return')
 if angle==0:
  require(word==dbits(1. if cosine else 0.),'exact Math signed-zero/one cases')
  return
 with localcontext() as context:
  context.prec=110
  x=Decimal.from_float(angle)
  require(0<x<7,'declared finite source angle range')
  term=Decimal(1) if cosine else x
  total=term
  for n in range(1,181):
   first=2*n-1 if cosine else 2*n
   term=-term*x*x/Decimal(first*(first+1))
   total+=term
   if abs(term)<Decimal('1e-100'):break
  else: raise ValueError('Auto-close entropy: Taylor truncation bound')
  nearest=float(total);rounded=Decimal.from_float(nearest)
  require(abs(rounded-total)>Decimal('1e-95'),'unambiguous binary64 bracketing at declared Decimal bound')
  low,high=(nearest,math.nextafter(nearest,math.inf)) if rounded<total else (math.nextafter(nearest,-math.inf),nearest)
  require(word in (dbits(low),dbits(high)),'Math return must bracket independently evaluated true trig result')

def next_state(seed):return (seed*0x5deece66d+11)&MASK

def bounded_int(seed,bound):
 require(type(bound) is int and bound>0,'positive actual nextInt bound')
 while True:
  seed=next_state(seed);bits=seed>>17;value=bits%bound
  if bits-value+(bound-1)<(1<<31):return seed,value

def audit(row):
 seed=state(row['player-random-before'])
 require(row['world-random-before']==row['world-random-after'],'own World stream unchanged')
 state(row['world-random-before'])
 if row['operation']=='gs-tick':seed,_=bounded_int(seed,1000)
 for drop in row['drops']:
  require(state(drop['player-random-before'])==seed,'drop begins at actual post-prefix/previous-drop stream')
  values=[]
  for unused in range(4):seed=next_state(seed);values.append((seed>>24)/(1<<24))
  require(state(drop['player-random-after'])==seed,'four actual Player nextFloat draws per drop')
  angle=f32(f32(values[0]*f32(math.pi))*2.);scale=f32(f32(.02)*values[1]);trig=drop['trig']
  require(set(trig)=={'angle','cos','sin'} and trig['angle']==fbits(angle),'own reconstructed source perturbation angle')
  trig_bracket(angle,trig['cos'],True);trig_bracket(angle,trig['sin'],False)
  expected=[dbits(double(trig['cos'])*scale),dbits(float(f32(.1))+float(f32(f32(values[2]-values[3])*f32(.1)))),dbits(float(f32(.3))+double(trig['sin'])*scale)]
  require(drop['velocity']==expected,'exact own source false-branch drop arithmetic at declared yaw/pitch0')
  pose=row['after']['player'];position=[pose['sn.aM'],dbits(double(pose['sn.aN'])-float(f32(.3))+float(f32(.12))),pose['sn.aO']]
  require(drop['position']==position,'actual drop position from own live Player')
  require(drop['pickup-delay']==40 and drop['world-member'] is True and drop['chunk-members']==1,'actual publication/delay')
  state(drop['private-random'])
 require(state(row['player-random-after'])==seed,'exact own prefix/drop/suffix stream continuation')

def constructors(row,events):
 starts=[e for e in events if e['event']=='entry' and e['method']=='hl.<init>(Lfd;DDDLiz;)V']
 ends=[e for e in events if e['event']=='return' and e['method']=='hl.<init>(Lfd;DDDLiz;)V']
 require(len(starts)==len(ends)==len(row['drops']),'every actual Item constructor retained')
 for start,end,drop in zip(starts,ends,row['drops']):
  left=events.index(start);right=events.index(end);require(left<right,'constructor event order')
  maths=[double(e['value']) for e in events[left:right] if e['event']=='return' and e['method']=='java.lang.Math.random()D']
  require(len(maths)==4 and all(math.isfinite(v) and 0<=v<1 for v in maths),'four independent actual shared Math returns')
  ctor=end['constructor'];require(ctor['phase']==fbits(f32(maths[0]*math.pi*2.)) and ctor['yaw']==fbits(f32(maths[1]*360.)),'actual constructor independent phase/yaw formulas')
  expected=[dbits(float(f32(maths[2]*float(f32(.2))-float(f32(.1))))),dbits(float(f32(.2))),dbits(float(f32(maths[3]*float(f32(.2))-float(f32(.1)))))]
  require(ctor['velocity']==expected and ctor['position']==drop['position'],'actual independent constructor position/velocity')
  require(ctor['private-random']==drop['private-random'],'Item-private stream unchanged through source publication')
  require(start['arguments'][1:4]==drop['position'],'actual constructor arguments equal published position')
 # Native Math.cos/sin produce no JDI method-entry events on this JVM. The
 # observer's reconstructed argument/pure JDK leaves and independent Decimal
 # bracket audit above qualify the final source arithmetic, not live native
 # return-value interception. Actual raw source velocities remain unchanged.

def random_trace(row,events):
 begin=events[0]['state'];identity=begin['player-random-id'];seed=state(row['player-random-before'])
 require(begin['player-random']==row['player-random-before'],'read-only Player Random start identity/seed')
 entries=[e for e in events if e['event']=='entry' and e['method'].startswith('java.util.Random.') and e['receiver']['id']==identity]
 require([e['method'] for e in entries]==(['java.util.Random.nextInt(I)I'] if row['operation']=='gs-tick' else [])+['java.util.Random.nextFloat()F']*(4*len(row['drops'])),'exact actual source Player private draw sequence')
 for entry in entries:
  require(state(entry['seed'])==seed,'actual read-only private draw entry seed')
  returned=next(e for e in events[events.index(entry)+1:] if e['event']=='return' and e['method']==entry['method'] and e['receiver']==entry['receiver'])
  if entry['method']=='java.util.Random.nextInt(I)I':
   require(entry['arguments']==[1000],'actual source nextInt bound');seed,result=bounded_int(seed,1000)
  else:seed=next_state(seed);result=fbits((seed>>24)/(1<<24))
  require(returned['value']==result and state(returned['seed'])==seed,'actual read-only private return value/seed')
 require(events[-1]['state']['player-random']==row['player-random-after']==f'{seed:016x}','actual unchanged private Random identity and final continuation')

