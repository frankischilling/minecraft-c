"""Audit each process's real source Player stream and actual published drops."""
from decimal import Decimal, localcontext
import math
import re
import struct

MASK=(1<<48)-1
POSITION=['4021000000000000','404fe8f5c2700000','4021000000000000']

def require(value,text):
 if not value: raise ValueError('Workbench entropy: '+text)

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
  else: raise ValueError('Workbench entropy: Taylor truncation bound')
  nearest=float(total);rounded=Decimal.from_float(nearest)
  require(abs(rounded-total)>Decimal('1e-95'),'unambiguous binary64 bracketing at declared Decimal bound')
  low,high=(nearest,math.nextafter(nearest,math.inf)) if rounded<total else (math.nextafter(nearest,-math.inf),nearest)
  require(word in (dbits(low),dbits(high)),'Math return must bracket independently evaluated true trig result')

def audit(row):
 seed=state(row['player-random-before'])
 state(row['player-random-after']);state(row['world-random-before']);state(row['world-random-after'])
 require(row['world-random-before']==row['world-random-after'],'World stream unchanged')
 for index,drop in enumerate(row['drops']):
  random=[]
  for unused in range(4):
   seed=(seed*0x5deece66d+11)&MASK
   random.append((seed>>24)/(1<<24))
  angle=f32(f32(random[0]*f32(math.pi))*2.)
  scale=f32(f32(.02)*random[1])
  trig=drop['trig']
  require(type(trig) is dict and set(trig)=={'angle','cos','sin'} and trig['angle']==fbits(angle),
          'actual trig source-leaf receipt with exact reconstructed argument')
  cosine,sine=double(trig['cos']),double(trig['sin'])
  require(all(math.isfinite(value) and -1<=value<=1 for value in (cosine,sine)),'finite actual trig returns')
  trig_bracket(angle,trig['cos'],True);trig_bracket(angle,trig['sin'],False)
  # Explicit per-process Math leaves; every subsequent raw64 arithmetic word
  # must match exactly. No payload normalization or host-libm replacement.
  expected=[dbits(cosine*scale),dbits(float(f32(.1))+float(f32(f32(random[2]-random[3])*f32(.1)))),
            dbits(float(f32(.3))+sine*scale)]
  require(drop['velocity']==expected,'exact source GS false-branch velocity from own four nextFloat draws')
  require(drop['position']==POSITION and drop['pickup-delay']==40 and drop['world-member'] is True and drop['chunk-members']==1,
          'actual deterministic Item position/delay/World and Chunk publication')
  state(drop['private-random'])
 require(row['player-random-after']==f'{seed:016x}','exact Player seed progression')
 require(row['events']==[{'event':'drop','index':i,'ref':d['ref']} for i,d in enumerate(row['drops'])],
         'ordered actual publications, no invented listener callback')
 return {'profile':row['profile'],'drops':len(row['drops']),'player-float-draws':4*len(row['drops'])}

def constructors(rows,observations):
 require(len(rows)==len(observations),'one read-only audit per actual call')
 result=[]
 for index,(row,observation) in enumerate(zip(rows,observations)):
  require(observation['index']==index and set(observation)=={'index','calls','constructors'},'ordered read-only call audit')
  require(len(observation['constructors'])==len(row['drops']),'all actual Item constructors retained')
  for drop,ctor in zip(row['drops'],observation['constructors']):
   require(set(ctor)=={'math-values','phase','yaw','position','velocity','private-random'},'exact raw constructor schema')
   require(len(ctor['math-values'])==4,'four source shared Math returns')
   values=[double(word) for word in ctor['math-values']]
   require(all(math.isfinite(value) and 0<=value<1 for value in values),'unchanged independent shared Math entropy')
   require(ctor['phase']==fbits(f32(values[0]*math.pi*2.)) and ctor['yaw']==fbits(f32(values[1]*360.)),
           'actual constructor phase/yaw formulas')
   expected=[dbits(float(f32(values[2]*float(f32(.2))-float(f32(.1))))),dbits(float(f32(.2))),
             dbits(float(f32(values[3]*float(f32(.2))-float(f32(.1)))))]
   require(ctor['velocity']==expected and ctor['position']==POSITION,'actual constructor position and independent shared-Math velocity formulas')
   require(ctor['private-random']==drop['private-random'],'Item-private stream remains unchanged between construction and publication')
  result.append({'profile':row['profile'],'calls':len(observation['calls']),'constructors':len(observation['constructors'])})
 return result
