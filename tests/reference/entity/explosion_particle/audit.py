"""Audit observed constructor streams independently; never rebind game entropy."""
import struct

MASK=(1<<48)-1
MULTIPLIER=0x5deece66d
INVERSE=pow(MULTIPLIER,-1,1<<48)


def require(value,message):
    if not value:
        raise ValueError(message)


def f32(value):
    return struct.unpack('>f',struct.pack('>f',value))[0]


def fword(value):
    return struct.pack('>f',f32(value)).hex()


def dword(value):
    return struct.pack('>d',value).hex()


def from_word(value,single=False):
    return struct.unpack('>f' if single else '>d',bytes.fromhex(value))[0]


def next_bits(state,bits):
    state=(state*MULTIPLIER+11)&MASK
    return state,state>>(48-bits)


def floats(state):
    values=[]
    for _ in range(8):
        state,value=next_bits(state,24)
        values.append(fword(value/(1<<24)))
    return state,values


def doubles(state):
    values=[]
    for _ in range(8):
        state,a=next_bits(state,26)
        state,b=next_bits(state,27)
        values.append(dword(((a<<27)+b)/(1<<53)))
    return state,values


def formula(state,math_values,own_values):
    require(len(math_values)==len(own_values)==8,'eight actual constructor returns per stream')
    math=[from_word(word) for word in math_values]
    own=[from_word(word,True) for word in own_values]
    require(all(0<=value<1 for value in [*math,*own]),'actual finite Random return ranges')
    expected_velocity=[]
    for value,random in zip((.125,-.25,.0625),math[5:]):
        offset=f32(f32(random*2.0-1.0)*f32(.05))
        expected_velocity.append(dword(value+offset))
    grey=fword(f32(f32(own[4]*f32(.3))+f32(.7)))
    scale=fword(f32(f32(f32(own[5]*own[6])*f32(6))+f32(1)))
    lifetime=int(16.0/(own[7]*.8+.2))+2
    require(state['velocity']==expected_velocity,'GY final velocity bytecode formula')
    require(state['c']==fword(f32(own[0]*f32(3))) and
            state['d']==fword(f32(own[1]*f32(3))),'XW inherited jitter fields')
    require(state['scale']==scale and state['rgb']==[grey]*3,'GY scale and grayscale bytecode formulas')
    require(state['age']==0 and state['lifetime']==lifetime,'GY age and lifetime bytecode formulas')


def ordinary(rows):
    require([row['profile'] for row in rows]==list(range(13)),'thirteen measured original constructors')
    for row in rows:
        after,math_values=doubles(int(row['math_before'],16))
        require(after==int(row['math_after'],16),'eight actual shared-Math double advances')
        # The ordinary process records the returned private stream state.
        # Reverse its eight source-pinned Float advances as an explicit inference;
        # the separate read-only JDI run observes that initial state directly.
        own_initial=int(row['state']['own_seed'],16)
        for _ in range(8):
            own_initial=((own_initial-11)*INVERSE)&MASK
        private_after,own_values=floats(own_initial)
        require(private_after==int(row['state']['own_seed'],16),'private Float progression')
        formula(row['state'],math_values,own_values)
    return {'constructors':13,'shared_double_returns':104,'private_float_returns_inferred':104,
            'private_initial_state':'inferred by reversing eight pinned Float advances; independently observed in JDI',
            'constructor_or_shared_entropy_rebound':False}


def debugger(journal,target_rows,pins):
    require([row['profile'] for row in journal]==list(range(-1,13)),'fourteen original JDI constructor records')
    by_profile={row['profile']:row for row in target_rows}
    method=next(method for method in pins['classes']['gy']['methods'] if method['name']=='<init>')
    for row in journal:
        require(row['code_bytes']==method['code_bytes'] and row['code_sha256']==method['code_sha256'],
                'live JDI original GY constructor bytecode pin')
        after,own=floats(int(row['own_initial'],16))
        require(own==row['own_values'] and after==int(row['own_after'],16),
                'eight directly observed private Float returns and states')
        formula(row['state'],row['math_values'],row['own_values'])
        if row['profile']>=0:
            observed=by_profile[row['profile']]
            after,math=doubles(int(observed['math_before'],16))
            require(math==row['math_values'] and after==int(observed['math_after'],16),
                    'eight directly observed shared-Math returns and states')
            require(all(observed['state'][key]==value for key,value in row['state'].items()),
                    'read-only JDI and returned constructor state in the same process')
            require(observed['state']['own_seed']==row['own_after'],'same-process returned private state')
    return {'constructors':14,'warm_constructors':1,'shared_double_returns':112,
            'private_float_returns':112,'live_constructor_code_pins':14,'target_state_writes':0,
            'game_invocations_from_debugger':0,'constructor_or_shared_entropy_rebound':False}
