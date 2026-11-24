"""Independent finite Fire interpreter over declared source fields and cells.

No native implementation or observed result is called or imported here.
"""
import hashlib
import importlib.util
import math
from pathlib import Path
import struct

_spec=importlib.util.spec_from_file_location('fire_independent_hash_order',Path(__file__).with_name('hashset_order.py'))
_order=importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_order)


def f32(value): return struct.unpack('>f', struct.pack('>f', value))[0]
def d(value): return struct.pack('>d', value).hex()
def f(value): return struct.pack('>f', value).hex()
def i32(value): return ((value + (1 << 31)) & 0xffffffff) - (1 << 31)


class RandomModel:
    def __init__(self, seed):
        self.state = (seed ^ 0x5deece66d) & ((1 << 48) - 1)
        self.draws = 0
    def bits(self, count):
        self.state = (self.state * 0x5deece66d + 11) & ((1 << 48) - 1)
        self.draws += 1
        return self.state >> (48 - count)
    def bounded(self, bound):
        if bound <= 0: raise ValueError('declared positive Java Random bound')
        if bound & (bound - 1) == 0: return (bound * self.bits(31)) >> 31
        while True:
            bits = self.bits(31)
            value = bits % bound
            if bits - value + bound - 1 < (1 << 31): return value
    def double(self): return ((self.bits(26) << 27) + self.bits(27)) / (1 << 53)
    def observation(self): return [f'{self.state:016x}', False, '0' * 16]


ENCOURAGEMENT = {5: 5, 85: 5, 53: 5, 17: 5, 18: 30, 47: 30, 46: 15, 31: 60, 35: 30}
FLAMMABILITY = {5: 20, 85: 20, 53: 20, 17: 5, 18: 60, 47: 20, 46: 100, 31: 100, 35: 60}
DIRECTIONS = ((1,0,0),(-1,0,0),(0,-1,0),(0,1,0),(0,0,-1),(0,0,1))
NOTIFY = (DIRECTIONS[1],DIRECTIONS[0],*DIRECTIONS[2:])


def add(at, delta): return tuple(i32(a+b) for a,b in zip(at,delta))


class Fault(Exception):
    def __init__(self, exception, message, stack):
        self.exception, self.message, self.stack = exception, message, stack


def footprint(profile):
    return [(x,z) for x in range(-10,9) for z in range(-7,12)] if profile['random_player'] else [(x,z) for x in range(2) for z in range(2)]


class FireModel:
    def __init__(self, profile, prepared_class):
        self.p = profile
        self.target = tuple(profile['xyz'])
        self.cells = {tuple(c[:3]):list(c[3:]) for c in profile['cells']}
        self.heights = {(x,z):height for x,z,height in profile['heights']}
        self.sky = {}
        self.random, self.math = RandomModel(profile['random_seed']), RandomModel(0)
        self.events, self.regions, self.entities = [], [], []
        self.counts = dict(cell=0,range=0,added=0,sound=0,sky=0)
        self.world_o = False
        self.sky_seen = False
        self.time = 0
        self.seq = int(prepared_class['scheduled']['sequence'],16)
        self.qinit = prepared_class['scheduled']['initialized']
        self.next_id = prepared_class['entity']['next_id']
        self.sinit = prepared_class['entity']['initialized']
        self.players = []
        if profile['random_player']:
            self.players.append(self.player(self.next_id))
            self.next_id += 1
            self.sinit = True
        self.ordered = []
        self.g, self.cooldown = profile['g'], profile['cooldown']
        self.candidates = []
        self.block_ticks = 0
        self.failure = None
        self.result = None
        self.rain_time,self.thunder_time = 20,30
        if profile['prequeue_delay'] >= 0:
            self.schedule(self.target,profile['entry_id'],profile['prequeue_delay'])

    def cell(self,at): return list(self.cells.get(at,[0,0])) if 0 <= at[1] < 128 else [0,0]
    def getter(self,at,tail):
        if self.p['null_provider']:
            raise Fault('java/lang/NullPointerException','Cannot invoke "cl.b(int, int)" because "this.v" is null',['fd.c','fd.a',*tail])
        return self.cell(at)[0]
    def source_null(self,array,tail):
        raise Fault('java/lang/NullPointerException','Cannot load from int array because "this.'+array+'" is null',tail)
    def encouragement(self,at,tail,maximum=0,direct=False):
        block = self.getter(at,['yq.c' if direct else 'yq.f',*tail])
        if self.p['null_encouragement']: self.source_null('a',['yq.c' if direct else 'yq.f',*tail])
        return max(maximum,ENCOURAGEMENT.get(block,0))
    def flammable(self,at,tail): return self.encouragement(at,tail,direct=True)>0
    def support(self,at,tail):
        block = self.getter(at,['fd.h',*tail])
        return block in (1,2,3,5,17,18,35,46,47,49,58,87)
    def neighbors_flammable(self,at,tail):
        return any(self.flammable(add(at,delta),['yq.h',*tail]) for delta in DIRECTIONS)
    def can_place(self,at,tail):
        return self.support(add(at,(0,-1,0)),['yq.a',*tail]) or self.neighbors_flammable(at,['yq.a',*tail])

    def emit(self,kind,tail,**payload):
        row=dict(kind=kind,target=self.cell(self.target),world_o=self.world_o,
                 world_random=self.random.observation(),math_random=self.math.observation(),
                 entities=[dict(e) for e in self.entities],**payload)
        row['source_stack'] = tail
        self.events.append(row)
        self.counts[kind] += 1
        if kind != 'sky' and self.counts[kind] == self.p[kind+'_error_at']:
            raise Fault('java/lang/IllegalStateException','Fire '+kind+' observer interruption',tail)

    def schedule(self,at,block,delay):
        self.qinit = True
        record=dict(xyz=list(at),block=block,time=f'{self.time+delay:016x}',ordinal=f'{self.seq:016x}')
        self.seq += 1
        # All prepared coordinates have the complete nonloading radius8.
        if not any(r['xyz']==record['xyz'] and r['block']==block for r in self.ordered):
            self.ordered.append(record)
            self.ordered.sort(key=lambda r:(int(r['time'],16),int(r['ordinal'],16)))

    def light(self,kind,bounds):
        for region in reversed(self.regions[-5:]):
            if region['kind'] != kind: continue
            old = region['bounds']
            if all(bounds[i]>=old[i] for i in range(3)) and all(bounds[i]<=old[i] for i in range(3,6)): return
            if not(all(bounds[i]>=old[i]-1 for i in range(3)) and all(bounds[i]<=old[i]+1 for i in range(3,6))): continue
            merged=[min(bounds[i],old[i]) if i<3 else max(bounds[i],old[i]) for i in range(6)]
            def volume(b):return (b[3]-b[0])*(b[4]-b[1])*(b[5]-b[2])
            if volume(merged)-volume(old)<=2:
                region['bounds']=merged
                return
        self.regions.append(dict(kind=kind,bounds=list(bounds)))

    def opacity(self,at):
        block=self.cell(at)[0]
        return 0 if block in (0,31,51,85,90) else 1 if block==18 else 3 if block in (8,9) else 255
    def update_light(self,at,tail,metadata):
        x,y,z=at
        old=self.heights.get((x,z),0)
        if self.opacity(at)>0 and y>=old or self.opacity(at)==0 and y==old-1:
            height=max(y+1 if self.opacity(at)>0 else y,old)
            while height>0 and self.opacity((x,height-1,z))==0:height-=1
            if height!=old:
                # LM.g uses local X/Z in its range call, even for other Chunks.
                self.emit('range',['fd.b','fd.h','lm.g','lm.a','fd.a' if metadata else 'fd.c',*tail],
                          xyz=[x&15,min(height,old),z&15,x&15,max(height,old),z&15])
                self.heights[x,z]=height
                if height<old:
                    for level in range(height,old):self.sky[x,level,z]=15
                else:
                    self.light('Sky',[x,old,z,x,height,z])
                    for level in range(old,height):self.sky[x,level,z]=0
                value,level=15,height
                while level>0 and value>0:
                    level-=1
                    value=max(0,value-max(1,self.opacity((x,level,z))))
                    self.sky[x,level,z]=value
                while level>0 and self.opacity((x,level-1,z))==0:level-=1
                if level!=height:self.light('Sky',[x-1,level,z-1,x+1,height,z+1])
        self.light('Sky',[x,y,z,x,y,z]);self.light('Block',[x,y,z,x,y,z])
        current=self.heights.get((x,z),0)
        for dx,dz in ((-1,0),(1,0),(0,-1),(0,1)):
            adjacent=self.heights.get((x+dx,z+dz),0)
            if adjacent!=current:self.light('Sky',[x+dx,min(adjacent,current),z+dz,x+dx,max(adjacent,current),z+dz])

    def set(self,at,block,tail,metadata=None):
        old,oldmeta=self.cell(at)
        if old==block and (metadata is None or oldmeta==metadata):return False
        wrapper='fd.f' if metadata is None else 'fd.b'
        call=[wrapper,*tail]
        self.cells[at]=[block,0 if metadata is None else metadata]
        if old in (17,18):
            radius=4 if old==17 else 1
            for point,(kind,data) in list(self.cells.items()):
                if kind==18 and all(abs(a-b)<=radius for a,b in zip(at,point)):
                    self.cells[point]=[18,data|8]
        self.update_light(at,call,metadata is not None)
        added_tail=['lm.a','fd.a' if metadata is not None else 'fd.c',*call]
        if block==51:self.added(at,added_tail)
        self.emit('cell',['fd.j','fd.g',*call],xyz=list(at),cell=self.cell(at))
        if not self.world_o:
            for delta in NOTIFY:
                point=add(at,delta);kind=self.cell(point)[0]
                next_tail=['fd.l','fd.i','fd.g',*call]
                if kind==51:self.neighbor(point,next_tail)
                elif kind==90:self.portal_neighbor(point,next_tail)
                elif kind==31:
                    if self.cell(add(point,(0,-1,0)))[0] not in (2,3):raise ValueError('declared TallGrass must survive its real neighbor call')
        return True

    def portal(self,at,tail):
        x,y,z=at
        dx=int(self.cell((x-1,y,z))[0]==49 or self.cell((x+1,y,z))[0]==49)
        dz=int(self.cell((x,y,z-1))[0]==49 or self.cell((x,y,z+1))[0]==49)
        if dx==dz:return False
        if self.cell((x-dx,y,z-dz))[0]==0:x,z=x-dx,z-dz
        for u in range(-1,3):
            for v in range(-1,4):
                edge=u in (-1,2) or v in (-1,3)
                if u in (-1,2) and v in (-1,3):continue
                kind=self.cell((x+dx*u,y+v,z+dz*u))[0]
                if edge and kind!=49 or not edge and kind not in (0,51):return False
        self.world_o=True
        for u in range(2):
            for v in range(3):self.set((x+dx*u,y+v,z+dz*u),90,['ak.a_',*tail])
        self.world_o=False
        return True

    def portal_neighbor(self,at,tail):
        x,y,z=at
        dx,dz=0,1
        if self.cell((x-1,y,z))[0]==90 or self.cell((x+1,y,z))[0]==90:dx,dz=1,0
        low=y
        while self.cell((x,low-1,z))[0]==90:low-=1
        if self.cell((x,low-1,z))[0]!=49:self.set(at,0,['ak.b',*tail]);return
        height=1
        while height<4 and self.cell((x,low+height,z))[0]==90:height+=1
        if height!=3 or self.cell((x,low+height,z))[0]!=49:self.set(at,0,['ak.b',*tail]);return
        horizontal=self.cell((x-1,y,z))[0]==90 or self.cell((x+1,y,z))[0]==90
        vertical=self.cell((x,y,z-1))[0]==90 or self.cell((x,y,z+1))[0]==90
        if horizontal and vertical:self.set(at,0,['ak.b',*tail]);return
        a,b=self.cell((x+dx,y,z+dz))[0],self.cell((x-dx,y,z-dz))[0]
        if not (a==49 and b==90 or b==49 and a==90):self.set(at,0,['ak.b',*tail])

    def added(self,at,tail):
        call=['yq.c',*tail]
        if self.getter(add(at,(0,-1,0)),call)==49 and self.portal(at,call):return
        if not self.support(add(at,(0,-1,0)),call) and not self.neighbors_flammable(at,call):self.set(at,0,call);return
        self.schedule(at,51,40)
    def neighbor(self,at,tail):
        call=['yq.b',*tail]
        if not self.support(add(at,(0,-1,0)),call) and not self.neighbors_flammable(at,call):self.set(at,0,call)
    def raining_at(self,at):
        if not self.p['rain']:return False
        x,y,z=at
        if y<self.heights.get((x,z),0):return False
        top=0
        for level in range(127,-1,-1):
            if self.cell((x,level,z))[0] not in (0,31,51,85,90):top=level+1;break
        if top>y:return False
        return self.p['biome']=='i'

    def burn(self,at,bound,age,tail):
        call=['yq.a',*tail]
        block=self.getter(at,call)
        if self.p['null_flammability']:self.source_null('b',call)
        if self.random.bounded(bound)>=FLAMMABILITY.get(block,0):return
        tnt=self.getter(at,call)==46
        if self.random.bounded(age+10)<5 and not self.raining_at(at):
            data=min(15,age+self.random.bounded(5)//4)
            self.set(at,51,call,data)
        else:self.set(at,0,call)
        if tnt:self.prime(at,call)

    def tick(self,at,tail):
        call=['yq.a',*tail]
        eternal=self.getter(add(at,(0,-1,0)),call)==87
        if not self.can_place(at,call):self.set(at,0,call)
        if not eternal and self.p['rain'] and any(self.raining_at(add(at,v)) for v in ((0,0,0),(-1,0,0),(1,0,0),(0,0,-1),(0,0,1))):
            self.set(at,0,call);return
        age=self.cell(at)[1]
        if age<15:self.cells[at]=[self.cell(at)[0],age+self.random.bounded(3)//2]
        self.schedule(at,51,40)
        if not eternal and not self.neighbors_flammable(at,call):
            if not self.support(add(at,(0,-1,0)),call) or age>3:self.set(at,0,call)
            return
        if not eternal and not self.flammable(add(at,(0,-1,0)),call) and age==15 and self.random.bounded(4)==0:
            self.set(at,0,call);return
        for delta,bound in zip(DIRECTIONS,(300,300,250,250,300,300)):self.burn(add(at,delta),bound,age,call)
        x,y,z=at
        for nx in range(x-1,x+2):
            for nz in range(z-1,z+2):
                for ny in range(y-1,y+5):
                    point=(nx,ny,nz)
                    if point==at:continue
                    maximum=0
                    if self.cell(point)[0]!=0:continue
                    for delta in DIRECTIONS:maximum=self.encouragement(add(point,delta),['yq.i',*call],maximum)
                    threshold=(maximum+40)//(age+30)
                    if maximum<=0 or threshold<=0:continue
                    bound=100+max(0,ny-(y+1))*100
                    if self.random.bounded(bound)>threshold:continue
                    if (self.p['rain'] and self.raining_at(point) or self.raining_at((nx-1,ny,z)) or
                        self.raining_at((nx+1,ny,nz)) or self.raining_at((nx,ny,nz-1)) or self.raining_at((nx,ny,nz+1))):continue
                    data=min(15,age+self.random.bounded(5)//4)
                    self.set(point,51,call,data)

    def prime(self,at,tail):
        angle=f32(self.math.double()*3.1415927410125732*2.0)
        radians=f32(f32(angle*f32(math.pi))/f32(180))
        index=int(f32(radians*f32(10430.378)))&65535
        cos_index=int(f32(f32(radians*f32(10430.378))+16384.0))&65535
        sine=f32(math.sin(index*math.pi*2/65536))
        cosine=f32(math.sin(cos_index*math.pi*2/65536))
        position=[float(f32(f32(v)+.5)) for v in at]
        width=f32(.98);offset=f32(width/2)
        x,y,z=position;bottom=y-offset
        entity={'class':'qw','id':self.next_id,'position':[d(v) for v in position],
                'motion':[d(f32(-sine*f32(.02))),d(.20000000298023224),d(f32(-cosine*f32(.02)))],
                'previous':[d(v) for v in position],'dimensions':[f(width),f(width)],'offset':f(offset),
                'bounds':[d(v) for v in (x-offset,bottom,z-offset,x+offset,bottom+width,z+offset)],
                'removed':False,'on_ground':False,'world_same':True,'fuse':80,'prevent_spawning':True}
        self.next_id+=1;self.sinit=True;self.entities.append(entity)
        self.emit('added',['fd.c','fd.b','ah.c',*tail],entity=dict(entity))
        self.emit('sound',['fd.a','ah.c',*tail],name='random.fuse',position=[d(x),d(y-offset),d(z)],volume=f(1),pitch=f(1))

    def player(self,number):
        x,y,z=-.5,64.,32.5;width=f32(.6);height=f32(1.8);offset=f32(1.62);half=f32(width/2)
        bottom=y-offset
        return {'class':'dc','id':number,'position':[d(v) for v in (x,y,z)],'motion':['0'*16]*3,
                'dimensions':[f(width),f(height)],'offset':f(offset),
                'bounds':[d(v) for v in (x-half,bottom,z-half,x+half,bottom+height,z+half)],
                'removed':False,'on_ground':False,'health':20,'sleeping':False}

    def lava(self,at,tail):
        call=['nx.a',*tail]
        count=self.random.bounded(3)
        for _ in range(count):
            at=add(at,(self.random.bounded(3)-1,1,self.random.bounded(3)-1))
            block=self.cell(at)[0]
            if block==0:
                # Material.e differs from Fire encouragement, e.g CraftingTable.
                if any(self.cell(add(at,v))[0] in (5,17,47,53,54,58,64,65,85) for v in ((-1,0,0),(1,0,0),(0,0,-1),(0,0,1),(0,-1,0),(0,1,0))):
                    self.set(at,51,call);return
            elif block not in (31,51,90):return

    def lcg(self):
        self.g=i32(self.g*3+1013904223)
        return self.g>>2

    def random_work(self):
        # The one declared Player has this genuine 19x19 candidate domain.
        # No ambient query is reached with its positive prepared cooldown.
        self.candidates=_order.random_chunks()
        if self.cooldown<=0:raise ValueError('declared random-work cooldown')
        self.cooldown-=1
        for cx,cz in self.candidates:
            self.random.bounded(100000)  # Dry weather stops the lightning branch.
            if self.random.bounded(16)==0:self.lcg()  # Actual Plains.c() is false.
            for _ in range(80):
                value=self.lcg()
                point=(cx*16+(value&15),(value>>16)&127,cz*16+((value>>8)&15))
                block=self.cell(point)[0]
                if block==51:
                    self.block_ticks+=1
                    self.tick(point,['fd.n','fd.l'])
                elif block not in (0,1,87):raise ValueError('undeclared random Block tick')

    def snapshot(self):
        members=sorted(self.ordered,key=lambda r:(r['xyz'][1]+r['xyz'][0]*1024+r['xyz'][2]*1024*1024)&0xffffffff)
        return dict(target=self.cell(self.target),world_random=self.random.observation(),math_random=self.math.observation(),
                    time=f'{self.time:016x}',world_o=self.world_o,sky_seen=self.sky_seen,
                    scheduled=dict(initialized=self.qinit,sequence=f'{self.seq:016x}',ordered=[dict(r) for r in self.ordered],members=[dict(r) for r in members]),
                    entity_class=dict(initialized=self.sinit,next_id=self.next_id),lighting=dict(depth=0,regions=[dict(r) for r in self.regions]),
                    fire_arrays=dict(encouragement=None if self.p['null_encouragement'] else [ENCOURAGEMENT.get(i,0) for i in range(256)],
                                     flammability=None if self.p['null_flammability'] else [FLAMMABILITY.get(i,0) for i in range(256)]),
                    entities=[dict(e) for e in self.entities],players=self.players,entity_count=len(self.entities)+len(self.players),player_count=len(self.players),
                    weather=dict(rain_previous=f(1 if self.p['rain'] else 0),rain_current=f(1 if self.p['rain'] else 0),
                                 thunder_previous=f(0),thunder_current=f(0),rain_time=self.rain_time,thunder_time=self.thunder_time,
                                 raining=self.p['rain'],thundering=False),
                    random_work=dict(g=self.g,cooldown=self.cooldown,candidates=self.candidates))


def model(profile, prepared_class=None):
    if prepared_class is None:prepared_class={'scheduled':{'initialized':False,'sequence':'0'*16},'entity':{'initialized':False,'next_id':0}}
    m=FireModel(profile,prepared_class)
    before=m.snapshot()
    m.before_cells={k:list(v) for k,v in m.cells.items()}
    m.before_heights=dict(m.heights)
    try:
        operation=profile['operation']
        if operation=='added':m.result=m.set(m.target,profile['entry_id'],[])
        elif operation=='neighbor':m.neighbor(m.target,[])
        elif operation in ('tick','lava'):
            (m.lava if operation=='lava' else m.tick)(m.target,[])
        elif operation=='world-tick':
            m.rain_time-=1;m.thunder_time-=1;m.time=1
            while m.ordered and int(m.ordered[0]['time'],16)<=m.time:
                record=m.ordered.pop(0);point=tuple(record['xyz'])
                if m.cell(point)[0]==record['block']:
                    (m.lava if record['block']==11 else m.tick)(point,['fd.a','fd.l'])
            if profile['random_player']:m.random_work()
            elif m.cooldown>0:m.cooldown-=1
        else:raise ValueError('declared Fire operation')
    except Fault as fault:m.failure=fault
    origin=None
    if m.failure is not None and m.failure.exception=='java/lang/NullPointerException':
        owner,method=m.failure.stack[0].split('.')
        descriptor,pc=(('(II)Llm;',6) if owner=='fd' else
                       ('(Lxp;III)Z',14) if method=='c' else ('(Lfd;IIIILjava/util/Random;I)V',12))
        origin={'class':owner,'method':method,'descriptor':descriptor,'pc':pc}
    return dict(before=before,after=m.snapshot(),events=m.events,result=m.result if m.failure is None else None,
                exception='NONE' if m.failure is None else m.failure.exception,
                exception_message=None if m.failure is None else m.failure.message,
                exception_source_stack=[] if m.failure is None else m.failure.stack,exception_origin=origin,
                cells=m.cells,heights=m.heights,sky=m.sky,before_cells=m.before_cells,before_heights=m.before_heights,
                world_draws=m.random.draws,math_draws=m.math.draws,block_ticks=m.block_ticks)


def plane_arrays(profile,after,expected=None):
    expected=model(profile) if expected is None else expected
    cells=expected['cells' if after else 'before_cells']
    heights=expected['heights' if after else 'before_heights']
    result=[]
    def index(x,y,z):return ((x&15)<<11)|((z&15)<<7)|y
    def nibble(data,i,value):
        shift=(i&1)*4;data[i>>1]=(data[i>>1]&~(15<<shift))|(value<<shift)
    for cx,cz in footprint(profile):
        values=dict(blocks=bytearray(32768),metadata=bytearray(16384),sky=bytearray(16384),block_light=bytes(16384),height=bytearray(256))
        for (x,y,z),(block,meta) in cells.items():
            if (x>>4,z>>4)!=(cx,cz):continue
            i=index(x,y,z);values['blocks'][i]=block;nibble(values['metadata'],i,meta)
        for (x,z),height in heights.items():
            if (x>>4,z>>4)==(cx,cz):values['height'][((z&15)<<4)|(x&15)]=height
        if after:
            for (x,y,z),value in expected['sky'].items():
                if (x>>4,z>>4)==(cx,cz):nibble(values['sky'],index(x,y,z),value)
        result.append(dict(xyz=[cx,cz],planes=values))
    return result


def planes(profile,after,expected=None):
    return [dict(xyz=r['xyz'],planes={k:hashlib.sha256(v).hexdigest() for k,v in r['planes'].items()}) for r in plane_arrays(profile,after,expected)]
