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
    return (6,10) if kind=='flame' else (7,4)


def formula(state,kind,math_words,own_words):
    require((len(math_words),len(own_words))==counts(kind),'source-pinned entropy counts')
    m=[number(v) for v in math_words];r=[number(v,True) for v in own_words]
    require(all(0<=v<1 for v in m+r),'finite actual Random return ranges')
    inputs=(.125,-.25,.0625)
    # XW uses five shared doubles, four private floats and a float sqrt result.
    inherited=[(0 if kind!='flame' else v)+f32(f32(draw*2-1)*f32(.4))
               for v,draw in zip(inputs,m)]
    length=f32(math.sqrt((inherited[0]*inherited[0]+inherited[1]*inherited[1])+inherited[2]*inherited[2]))
    speed=f32(f32((m[3]+m[4])+1)*f32(.15))
    inherited=[((v/length)*speed)*f32(.4) for v in inherited]
    inherited[1]+=f32(.1)
    factor=f32(.01) if kind=='flame' else f32(.1)
    velocity=[word(v*factor+input) for v,input in zip(inherited,inputs)]
    require(state['velocity']==velocity,'XW and subtype final velocity widths/order')
    require(state['c']==word(f32(r[0]*3),True) and state['d']==word(f32(r[1]*3),True),'XW actual private jitter fields')
    scale=f32(f32(f32(r[2]*f32(.5))+f32(.5))*2)
    # r[3] initializes inherited lifetime even though the subtype overwrites it.
    inherited_lifetime=int(f32(4/f32(f32(r[3]*f32(.9))+f32(.1))))
    require(4<=inherited_lifetime<=40,'XW overwritten lifetime consumes its fourth Float')
    if kind=='flame':
        rgb=[word(1,True)]*3;lifetime=int(8/(m[5]*.8+.2))+4
        # Six additional private Float returns alter only constructor locals.
        # Their consumption is checked independently of the published position.
        require(state['texture']==48 and state['no_clip'] is True,'QU texture and no-clip constructor stores')
    else:
        rgb=[word(m[5]*f32(.3),True)]*3
        scale=f32(f32(scale*f32(.75))*{'large_smoke':2.5,'smoke_zero':0,'smoke_negative':-.5}.get(kind,1))
        lifetime=int(f32(f32(int(8/(m[6]*.8+.2)))*{'large_smoke':2.5,'smoke_zero':0,'smoke_negative':-.5}.get(kind,1)))
        require(state['texture']==0 and state['no_clip'] is False,'XN inherited texture and constructor no-clip store')
    require(state['position']==[word(v) for v in (8.5,64.5,8.5)],'QU local jitter never republishes position')
    require(state['scale']==state['base_scale']==word(scale,True),'actual subtype and mutable inherited scale')
    require(state['rgb']==rgb and state['age']==0 and state['lifetime']==lifetime,'subtype color/age/lifetime bytecode formulas')


def ordinary(rows):
    require([r['profile'] for r in rows]==list(range(-1,36)),'warm plus36 measured constructors')
    totals=[0,0]
    for row in rows:
        mc,oc=counts(row['kind'])
        # Warm initializes Math in actual source order; only its final state is read.
        initial=int(row['math_before'],16) if row['math_before'] is not None else reverse(int(row['math_after'],16),mc*2)
        after,m,_=stream(initial,mc,True)
        require(after==int(row['math_after'],16),'actual shared-Math progress')
        initial=reverse(int(row['own_after'],16),oc)
        after,r,_=stream(initial,oc,False)
        require(after==int(row['own_after'],16),'inferred private initial state progression')
        formula(row['state'],row['kind'],m,r)
        totals[0]+=mc;totals[1]+=oc
    return {'constructors':37,'shared_double_returns':totals[0],'private_float_returns_inferred':totals[1],
            'private_initial':'reverse pinned Float count; separate JDI directly reads first state',
            'warm_shared_initial':'reverse pinned double count; other36 read before/after directly',
            'entropy_rebinding_or_supplied_samples':False}


def debugger(journal,target,pins):
    require([r['profile'] for r in journal]==list(range(-1,36)),'37 actual JDI constructor roots')
    require(len(target)==37,'37 same-VM returned constructor records')
    totals=[0,0];methods=0
    for row,returned in zip(journal,target):
        require(row['profile']==returned['profile'] and row['kind']==returned['kind'],'same-VM constructor identity')
        mc,oc=counts(row['kind'])
        after,r,seeds=stream(int(row['own_initial'],16),oc,False)
        require(r==row['own_values'] and seeds==row['own_after_each'] and after==int(row['own_after'],16),'directly observed private seed/Float returns')
        require(row['own_after']==returned['own_after'],'same-VM private returned state')
        initial=int(returned['math_before'],16) if returned['math_before'] else reverse(int(returned['math_after'],16),mc*2)
        after,m,seeds=stream(initial,mc,True)
        require(m==row['math_values'] and seeds==row['math_after_each'] and after==int(returned['math_after'],16),'directly observed Math values/seed progress')
        require(row['state']==returned['state'],'read-only JDI matches actual returned state')
        formula(row['state'],row['kind'],m,r)
        expected=[('qu','(Lfd;DDDDDD)V')] if row['kind']=='flame' else ([('xn','(Lfd;DDDDDD)V')] if row['kind']=='smoke_default' else [])+[('xn','(Lfd;DDDDDDF)V')]
        expected += [('xw','(Lfd;DDDDDD)V'),('sn','(Lfd;)V')]
        require([(v['class'],v['descriptor']) for v in row['methods']]==expected,'actual delegation and real ancestor constructor order')
        for pin in row['methods']:
            expected_pin=next(v for v in pins['classes'][pin['class']]['methods'] if v['name']==pin['name'] and v['descriptor']==pin['descriptor'])
            require(pin['bytes']==expected_pin['code_bytes'] and pin['sha256']==expected_pin['code_sha256'],'live original constructor Code pin')
            methods+=1
        totals[0]+=mc;totals[1]+=oc
    return {'constructors':37,'shared_double_returns':totals[0],'private_float_returns':totals[1],
            'live_constructor_method_observations':methods,'target_state_writes':0,'debugger_game_invocations':0,
            'entropy_rebinding_or_supplied_samples':False}
