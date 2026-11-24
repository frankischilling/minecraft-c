"""Independently audit untouched constructor entropy and source arithmetic."""
import math
import struct

MASK=(1<<48)-1
MULTIPLIER=0x5deece66d
INVERSE=pow(MULTIPLIER,-1,1<<48)


def require(value,message):
    if not value:
        raise ValueError(message)


def f32(value):
    return struct.unpack('>f',struct.pack('>f',value))[0]


def word(value,single=False):
    return struct.pack('>f' if single else '>d',f32(value) if single else value).hex()


def number(value,single=False):
    return struct.unpack('>f' if single else '>d',bytes.fromhex(value))[0]


def reverse(state,count):
    for _ in range(count):
        state=((state-11)*INVERSE)&MASK
    return state


def stream(state,count,double):
    values=[];seeds=[]
    for _ in range(count):
        state=(state*MULTIPLIER+11)&MASK
        if double:
            high=state>>(48-26)
            state=(state*MULTIPLIER+11)&MASK
            value=((high<<27)+(state>>(48-27)))/(1<<53)
        else:
            value=(state>>(48-24))/(1<<24)
        values.append(word(value,not double));seeds.append(f'{state:012x}')
    return state,values,seeds


def counts(kind):
    require(kind in ('redstone_default','redstone_negative_zero','redstone_scaled','redstone_large','redstone_zero','redstone_negative'),'declared IM constructor kind')
    return (10,4)


def formula(state,kind,math_words,own_words):
    require((len(math_words),len(own_words))==counts(kind),'source-pinned entropy counts')
    m=[number(v) for v in math_words];r=[number(v,True) for v in own_words]
    require(all(0<=v<1 for v in m+r),'finite actual Random return ranges')
    inherited=[f32(f32(draw*2-1)*f32(.4)) for draw in m[:3]]
    length=f32(math.sqrt((inherited[0]*inherited[0]+inherited[1]*inherited[1])+inherited[2]*inherited[2]))
    speed=f32(f32((m[3]+m[4])+1)*f32(.15))
    inherited=[((v/length)*speed)*f32(.4) for v in inherited]
    inherited[1]+=f32(.1)
    require(state['velocity']==[word(v*f32(.1)) for v in inherited],'XW and IM final velocity widths/order')
    require(state['c']==word(f32(r[0]*3),True) and state['d']==word(f32(r[1]*3),True),'XW actual private jitter fields')
    scale=f32(f32(f32(r[2]*f32(.5))+f32(.5))*2)
    inherited_lifetime=int(f32(4/f32(f32(r[3]*f32(.9))+f32(.1))))
    require(4<=inherited_lifetime<=40,'XW overwritten lifetime consumes fourth Float')
    scalar={'redstone_large':2.5,'redstone_zero':0,'redstone_negative':-.5}.get(kind,1)
    colors={'redstone_default':(0,0,0),'redstone_negative_zero':(-0.0,0,0),
            'redstone_zero':(0,1,0),'redstone_negative':(-.5,.25,1)}.get(kind,(1,.5,.25))
    red,green,blue=colors
    if red==0:red=1
    factor=f32(f32(f32(m[5])*f32(.4))+f32(.6))
    rgb=[word(f32(f32(f32(f32(v*f32(.2))+f32(.8))*color)*factor),True)
         for v,color in zip(m[6:9],(red,green,blue))]
    scale=f32(f32(scale*f32(.75))*scalar)
    lifetime=int(f32(f32(int(8/(m[9]*.8+.2)))*scalar))
    require(state['texture']==0 and state['no_clip'] is False,'IM inherited texture and constructor no-clip store')
    require(state['position']==[word(v) for v in (8.5,64.5,8.5)],'genuine constructor published input position')
    require(state['scale']==state['base_scale']==word(scale,True),'IM original/mutable scale stores')
    require(state['rgb']==rgb and state['age']==0 and state['lifetime']==lifetime,'IM color/age/lifetime widths/order')


def ordinary(rows):
    require([r['profile'] for r in rows]==list(range(-1,36)),'warm plus36 measured constructors')
    totals=[0,0];previous=None
    for row in rows:
        mc,oc=counts(row['kind'])
        if previous is not None:
            require(row['math_before']==previous,'shared Math joins across constructors and components')
        initial=int(row['math_before'],16) if row['math_before'] is not None else reverse(int(row['math_after'],16),mc*2)
        after,m,_=stream(initial,mc,True)
        require(after==int(row['math_after'],16),'actual shared-Math progress')
        initial=reverse(int(row['own_after'],16),oc)
        after,r,_=stream(initial,oc,False)
        require(after==int(row['own_after'],16),'inferred private initial progression')
        formula(row['state'],row['kind'],m,r)
        totals[0]+=mc;totals[1]+=oc;previous=row['math_after']
    return {'constructors':37,'shared_double_returns':totals[0],'private_float_returns_inferred':totals[1],
            'private_initial':'inferred by reversing four pinned Float calls; not directly observed',
            'warm_shared_initial':'inferred by reversing ten pinned double calls; other36 directly observed pre/post',
            'entropy_rebinding_or_supplied_samples':False}
