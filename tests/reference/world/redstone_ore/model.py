"""Independent Ore body model over fixed inputs and shared authored World rules."""
import importlib.util
from pathlib import Path

_spec=importlib.util.spec_from_file_location('ore_shared_world_model',Path(__file__).parents[1]/'fire/model.py')
F=importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(F)
RandomModel,f32,d,f,Fault=F.RandomModel,F.f32,F.d,F.f,F.Fault


def common(profile):
    return dict(profile,source_id=1,rain=False,biome='i',added_error_at=0,
                null_encouragement=False,null_flammability=False)


class OreModel(F.FireModel):
    def __init__(self,p,prepared):
        super().__init__(common(p),prepared)
        self.counts['particle']=0
        self.stat=prepared.get('item_use_stat',0)
        self.controller=None
        self.held=None
        if p['player']:
            value=self.player(self.next_id)
            half=f32(f32(.6)/2);bottom=64-f32(1.62)
            value['position']=[d(v) for v in (8.5,64,8.5)]
            value['bounds']=[d(v) for v in (8.5-half,bottom,8.5-half,8.5+half,bottom+f32(1.8),8.5+half)]
            self.players.append(value);self.next_id+=1;self.sinit=True
            self.controller=dict(c=-1,d=-1,e=-1,f=f(0),g=f(0),h=f(0),i=0)
            if p['operation']=='right-use':self.held=dict(id=4,count=1,damage=0)

    def snapshot(self):
        value=super().snapshot()
        value.pop('fire_arrays')
        if self.p['null_random']:value['world_random']=None
        value.update(provider_null=self.p['null_provider'],world_access_null=self.p['null_access'],
                     ore_statics=dict(unlit=None if self.p['null_unlit'] else 73,lit=None if self.p['null_lit'] else 74),
                     controller=None if self.controller is None else dict(self.controller),
                     held_stack=None if self.held is None else dict(self.held),item_use_stat=self.stat)
        return value

    def emit(self,kind,tail,**payload):
        if self.p['null_access']:
            raise Fault('java/lang/NullPointerException','Cannot invoke "java.util.List.size()" because "this.u" is null',tail)
        row=dict(kind=kind,target=self.cell(self.target),world_o=self.world_o,
                 world_random=None if self.p['null_random'] else self.random.observation(),
                 math_random=self.math.observation(),entities=[],source_stack=tail,**payload)
        self.events.append(row);self.counts[kind]+=1
        if kind!='sky' and self.counts[kind]==self.p[kind+'_error_at']:
            raise Fault('java/lang/IllegalStateException','Redstone Ore '+kind+' observer interruption',tail)

    def source_null(self,field,tail):
        raise Fault('java/lang/NullPointerException','Cannot read field "bn" because "uu.'+field+'" is null',tail)

    def particle(self,at,tail):
        call=['bs.i',*tail]
        if self.p['null_world']:
            raise Fault('java/lang/NullPointerException','Cannot read field "r" because "<parameter1>" is null',call)
        if self.p['null_random']:
            raise Fault('java/lang/NullPointerException','Cannot invoke "java.util.Random.nextFloat()" because "<local5>" is null',call)
        x,y,z=at
        directions=((0,1,0),(0,-1,0),(0,0,1),(0,0,-1),(1,0,0),(-1,0,0))
        for face,delta in enumerate(directions):
            point=[float(f32(f32(v)+self.random.bits(24)/(1<<24))) for v in at]
            neighbor=F.add(at,delta)
            block=self.getter(neighbor,['fd.g',*call])
            if block not in (1,4,73,74):
                axis=1 if face<2 else 2 if face<4 else 0
                point[axis]=float(at[axis]+(1 if face%2==0 else 0))+(0.0625 if face%2==0 else -0.0625)
            if point[0]<x or point[0]>x+1 or point[1]<0 or point[1]>y+1 or point[2]<z or point[2]>z+1:
                if self.p['null_access']:
                    raise Fault('java/lang/NullPointerException','Cannot invoke "java.util.List.size()" because "this.u" is null',['fd.a',*call])
                self.emit('particle',['fd.a',*call],name='reddust',position=[d(v) for v in point],motion=[d(0)]*3)

    def activate(self,at,tail):
        call=['bs.h',*tail]
        self.particle(at,call)
        if self.p['null_unlit']:self.source_null('aO',call)
        if self.p['entry_id']==73:
            if self.p['null_lit']:self.source_null('aP',call)
            self.set(at,74,call)

    def tick(self,at,tail):
        call=['bs.a',*tail]
        if self.p['null_lit']:self.source_null('aP',call)
        if self.p['entry_id']==74:
            if self.p['null_unlit']:self.source_null('aO',call)
            if self.p['null_world']:
                raise Fault('java/lang/NullPointerException','Cannot invoke "fd.f(int, int, int, int)" because "<parameter1>" is null',call)
            self.set(at,73,call)

    def random_work(self):
        self.candidates=F._order.random_chunks();self.cooldown-=1
        for cx,cz in self.candidates:
            self.random.bounded(100000)
            if self.random.bounded(16)==0:self.lcg()
            for _ in range(80):
                value=self.lcg();at=(cx*16+(value&15),(value>>16)&127,cz*16+((value>>8)&15))
                if self.cell(at)[0]==74:self.block_ticks+=1;self.tick(at,['fd.n','fd.l'])

    def invoke(self):
        op=self.p['operation']
        if op in ('left','walk','right'):
            self.activate(self.target,['bs.a' if op=='right' else 'bs.b'])
            if op=='right':self.result=False
        elif op=='display':
            if self.p['entry_id']==74:self.particle(self.target,['bs.b'])
        elif op=='tick':self.tick(self.target,[])
        elif op=='world-tick':
            self.rain_time-=1;self.thunder_time-=1;self.time=1
            while self.ordered and int(self.ordered[0]['time'],16)<=self.time:
                record=self.ordered.pop(0);at=tuple(record['xyz'])
                if self.cell(at)[0]==record['block']:self.tick(at,['fd.a','fd.l'])
            if self.p['random_player']:self.random_work()
            elif self.cooldown>0:self.cooldown-=1
        elif op=='mining':self.activate(self.target,['bs.b','os.a'])
        elif op=='right-use':
            self.activate(self.target,['bs.a','ob.a'])
            at=F.add(self.target,(0,1,0))
            self.set(at,4,['ck.a','iz.a','ob.a'],metadata=0)
            self.emit('sound',['fd.a','ck.a','iz.a','ob.a'],name='step.stone',position=[d(v+.5) for v in at],volume=f(1),pitch=f(.8))
            self.held['count']-=1;self.stat+=1;self.result=True
        else:raise ValueError('declared Ore operation')


def model(profile,prepared_class=None):
    if prepared_class is None:prepared_class=dict(scheduled=dict(initialized=False,sequence='0'*16),entity=dict(initialized=False,next_id=0),item_use_stat=0)
    m=OreModel(profile,prepared_class);before=m.snapshot()
    m.before_cells={k:list(v) for k,v in m.cells.items()};m.before_heights=dict(m.heights)
    try:m.invoke()
    except Fault as failure:m.failure=failure
    origin=None
    if m.failure is not None and m.failure.exception=='java/lang/NullPointerException':
        owner,method=m.failure.stack[0].split('.')
        if owner=='fd':
            descriptor,pc=('(II)Llm;',6) if method=='c' else ('(IIIIII)V',9) if method=='b' else ('(III)V',9) if method=='j' else ('(Ljava/lang/String;DDDDDD)V',9)
        elif method=='i':descriptor,pc='(Lfd;III)V',1 if profile['null_world'] else 25
        elif method=='h':descriptor,pc='(Lfd;III)V',16 if profile['null_unlit'] else 30
        else:descriptor,pc='(Lfd;IIILjava/util/Random;)V',7 if profile['null_lit'] else 21 if profile['null_unlit'] else 24
        origin=dict(**{'class':owner},method=method,descriptor=descriptor,pc=pc)
    return dict(before=before,after=m.snapshot(),events=m.events,result=m.result if m.failure is None else None,
                exception='NONE' if m.failure is None else m.failure.exception,
                exception_message=None if m.failure is None else m.failure.message,
                exception_source_stack=[] if m.failure is None else m.failure.stack,
                exception_origin=origin,cells=m.cells,heights=m.heights,sky=m.sky,
                before_cells=m.before_cells,before_heights=m.before_heights,
                world_draws=m.random.draws,math_draws=m.math.draws,block_ticks=m.block_ticks)


def plane_arrays(profile,after,expected=None):return F.plane_arrays(profile,after,model(profile) if expected is None else expected)
def planes(profile,after,expected=None):return F.planes(profile,after,model(profile) if expected is None else expected)
