"""Independent finite YE/IZ/OB interpreter over authored World mutation rules."""
import importlib.util
from pathlib import Path


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value)
    return value


F=module('flint_shared_world_model',Path(__file__).parents[2]/'world/fire/model.py')
D=module('flint_declared_operands',Path(__file__).with_name('profiles.py'))
RandomModel,f32,d,f,i32,Fault=F.RandomModel,F.f32,F.d,F.f,F.i32,F.Fault
FLINT_FIELDS=dict(bf=259,bg=1,a=64,bh=5,bi=False,bj=False,bk=None,bl='item.flintAndSteel')


def domain(at):
    x,y,z=at
    return -32000000<=x<32000000 and -32000000<=z<=32000000 and 0<=y<128


def common(p):
    return dict(p,xyz=D.shifted(p['xyz'],p['face']),entry_id=51,source_id=1,
                random_seed=p['world_seed'],rain=False,biome='i',random_player=False,
                prequeue_delay=-1,g=17,cooldown=6001,added_error_at=0,
                null_encouragement=False,null_flammability=False)


class FlintModel(F.FireModel):
    def __init__(self,p,prepared):
        super().__init__(common(p),prepared)
        self.original=p;self.clicked=tuple(p['xyz'])
        self.item=RandomModel(p['item_seed'])
        self.alias=p['item_random_world'];self.item_null=p['null_item_random']
        self.fire=None if p['null_fire'] else 51
        self.item_table_null=p['null_item_table']
        self.use_null=p['null_use_stats'];self.break_null=p['null_break_stats']
        self.writer_null=p['null_stats_writer']
        self.total=dict(prepared['stats']['total']);self.unsent=dict(prepared['stats']['unsent']);self.dirty=prepared['stats']['dirty']
        self.stack=None if p['null_stack'] else {k:p[q] for k,q in (('id','stack_id'),('count','count'),('damage','damage'),('animation','animation'))}
        self.controller=dict(c=-1,d=-1,e=-1,f=f(0),g=f(0),h=f(0),i=0) if p['operation']=='ob' else None
        self.progress=dict(ye_entered=False,ye_pc=0,sound_entered=False,sound_returned=False,
                           setter_entered=False,setter_returned=False,setter_value=False,
                           durability_reached=False,durability_entered=False,durability_returned=False,
                           wrapper_entered=False,wrapper_pc=0,wrapper_body_returned=False,use_stat_returned=False,
                           ob_pc=0,activation_entered=False,activation_returned=False,activated=False,
                           item_entered=False,item_returned=False)
        value=self.player(self.next_id);half=f32(f32(.6)/2);bottom=64-f32(1.62)
        value['position']=[d(v) for v in (8.5,64,8.5)]
        value['bounds']=[d(v) for v in (8.5-half,bottom,8.5-half,8.5+half,bottom+f32(1.8),8.5+half)]
        self.players=[value];self.next_id+=1;self.sinit=True

    def active_item(self):return None if self.item_null else self.random if self.alias else self.item
    def stats(self):return dict(total=[list(v) for v in sorted(self.total.items())],unsent=[list(v) for v in sorted(self.unsent.items())],dirty=self.dirty,writer_null=self.writer_null)
    def stack_state(self):return None if self.stack is None else dict(self.stack)
    def cell(self,at):
        if not domain(at):return [0,0]
        if (at[0]>>4,at[2]>>4) not in F.footprint(self.p):return None
        return list(self.cells.get(at,[0,0]))

    def snapshot(self):
        value=super().snapshot();value.pop('fire_arrays');value.pop('target')
        value.update(clicked=self.cell(self.clicked),target_xyz=list(self.target),target=self.cell(self.target),
                     item_random=None if self.active_item() is None else self.active_item().observation(),
                     prepared_item_random=self.item.observation(),item_random_alias_world=self.alias and not self.item_null,
                     remote=self.p['remote'],provider_null=self.p['null_provider'],world_access_null=self.p['null_access'],
                     item_table_null=self.item_table_null,use_stats_null=self.use_null,break_stats_null=self.break_null,
                     fire=self.fire,flint_fields=dict(FLINT_FIELDS),stack=self.stack_state(),stats=self.stats(),
                     controller=None if self.controller is None else dict(self.controller))
        return value

    def npe(self,message,stack,owner,method,descriptor,pc):
        failure=Fault('java/lang/NullPointerException',message,stack)
        failure.origin=dict(**{'class':owner},method=method,descriptor=descriptor,pc=pc)
        raise failure

    def getter(self,at,tail):
        if not domain(at):return 0
        if self.p['null_provider']:
            self.npe('Cannot invoke "cl.b(int, int)" because "this.v" is null',['fd.c','fd.a',*tail],
                     'fd','c','(II)Llm;',6)
        value=self.cell(at)
        if value is None:raise ValueError('undeclared actual World acquisition')
        return value[0]

    def set(self,at,block,tail,metadata=None):
        if not domain(at):return False
        return super().set(at,block,tail,metadata)

    def added(self,at,tail):
        if not self.p['remote']:super().added(at,tail)

    def neighbor(self,at,tail):
        if not self.p['remote']:super().neighbor(at,tail)

    def emit(self,kind,tail,**payload):
        if self.p['null_access']:
            descriptor='(DDDLjava/lang/String;FF)V' if kind=='sound' else '(IIIIII)V' if kind=='range' else '(III)V'
            method='a' if kind=='sound' else 'b' if kind=='range' else 'j'
            self.npe('Cannot invoke "java.util.List.size()" because "this.u" is null',tail,'fd',method,descriptor,9)
        active=self.active_item()
        row=dict(kind=kind,clicked=self.cell(self.clicked),target_xyz=list(self.target),target=self.cell(self.target),
                 world_o=self.world_o,world_random=self.random.observation(),math_random=self.math.observation(),
                 item_random=None if active is None else active.observation(),prepared_item_random=self.item.observation(),
                 item_random_alias_world=self.alias and not self.item_null,stack=self.stack_state(),fire=self.fire,stats=self.stats(),
                 item_table_null=self.item_table_null,use_stats_null=self.use_null,break_stats_null=self.break_null,
                 source_stack=tail,**payload)
        self.events.append(row);self.counts[kind]+=1
        if kind=='sound':self.sound_action(tail)
        if kind!='sky' and self.counts[kind]==self.p[kind+'_error_at']:
            raise Fault('java/lang/IllegalStateException','Flint and Steel '+kind+' observer interruption',tail)

    def sound_action(self,tail):
        action=self.p['sound_action']
        if action=='fire-null':self.fire=None
        elif action=='fire-id-zero':self.fire=0
        elif action=='item-random-world':self.alias=True;self.item_null=False
        elif action=='stack-stick':self.stack['id']=280
        elif action=='stats-writer-null':self.writer_null=True
        elif action=='use-array-null':self.use_null=True
        elif action=='break-array-null':self.break_null=True
        elif action=='target-stone':self.set(self.target,1,['FlintAndSteelObservation.soundAction','FlintAndSteelObservation$Observer.a',*tail])
        elif action!='none':raise ValueError('declared sound action')

    def statistic(self,number,tail):
        if self.writer_null:
            self.npe('Cannot invoke "xi.a(vr, int)" because "this.b.I" is null',['dc.a',*tail],
                     'dc','a','(Lvr;I)V',90)
        self.total[number]=i32(self.total.get(number,0)+1)
        self.unsent[number]=i32(self.unsent.get(number,0)+1);self.dirty=True

    def durability(self,tail):
        call=['iz.a',*tail]
        if self.item_table_null:
            self.npe('Cannot load from object array because "gm.c" is null',['iz.e',*call],
                     'iz','e','()Z',7)
        maximum=64 if self.stack['id']==259 else 0
        if maximum<=0:return
        self.stack['damage']=i32(self.stack['damage']+1)
        if self.stack['damage']>maximum:
            if not self.p['null_player']:
                if self.break_null:
                    self.npe('Cannot load from object array because "jl.F" is null',call,'iz','a','(ILsn;)V',47)
                self.statistic(16973824+self.stack['id'],call)
            self.stack['count']=i32(self.stack['count']-1)
            if self.stack['count']<0:self.stack['count']=0
            self.stack['damage']=0

    def ye(self,tail):
        call=['ye.a',*tail]
        self.progress.update(ye_entered=True,ye_pc=60)
        if self.p['null_world']:
            self.npe('Cannot invoke "fd.a(int, int, int)" because "<parameter3>" is null',call,
                     'ye','a','(Liz;Lgs;Lfd;IIII)Z',60)
        current=self.getter(self.target,call)
        if current==0:
            self.progress['ye_pc']=98
            active=self.active_item()
            if active is None:
                self.npe('Cannot invoke "java.util.Random.nextFloat()" because "ye.b" is null',call,
                         'ye','a','(Liz;Lgs;Lfd;IIII)Z',98)
            pitch=f32(f32((active.bits(24)/(1<<24))*f32(.4))+f32(.8))
            self.progress.update(ye_pc=107,sound_entered=True)
            self.emit('sound',['fd.a',*call],name='fire.ignite',position=[d(v+.5) for v in self.target],volume=f(1),pitch=f(pitch))
            self.progress.update(ye_pc=120,sound_returned=True)
            if self.fire is None:self.npe('Cannot read field "bn" because "uu.as" is null',call,'ye','a','(Liz;Lgs;Lfd;IIII)Z',120)
            self.progress.update(ye_pc=123,setter_entered=True)
            self.progress['setter_value']=self.set(self.target,self.fire,call)
            self.progress['setter_returned']=True
        self.progress.update(ye_pc=130,durability_reached=True)
        if self.stack is None:self.npe('Cannot invoke "iz.a(int, sn)" because "<parameter1>" is null',call,'ye','a','(Liz;Lgs;Lfd;IIII)Z',130)
        self.progress['durability_entered']=True
        self.durability(call)
        self.progress.update(ye_pc=134,durability_returned=True)
        return True

    def wrapper(self,tail):
        call=['iz.a',*tail]
        if self.stack is None:
            failure=Fault('java/lang/NullPointerException','Cannot invoke "iz.a(gs, fd, int, int, int, int)" because "FlintAndSteelObservation.stack" is null',[])
            failure.invocation_null=True
            raise failure
        self.progress.update(wrapper_entered=True,wrapper_pc=1)
        if self.item_table_null:self.npe('Cannot load from object array because "gm.c" is null',['iz.a',*call],'iz','a','()Lgm;',7)
        self.progress['wrapper_pc']=14
        result=self.ye(call)
        self.progress['wrapper_body_returned']=True
        if result:
            self.progress['wrapper_pc']=32
            if self.use_null:self.npe('Cannot load from object array because "jl.E" is null',call,'iz','a','(Lgs;Lfd;IIII)Z',32)
            self.progress['wrapper_pc']=34
            if self.p['null_player']:self.npe('Cannot invoke "gs.a(vr, int)" because "<parameter1>" is null',call,'iz','a','(Lgs;Lfd;IIII)Z',34)
            self.statistic(16908288+self.stack['id'],call)
            self.progress['use_stat_returned']=True
        self.progress['wrapper_pc']=39
        return result

    def invoke(self):
        operation=self.p['operation']
        if operation=='direct':self.result=self.ye([])
        elif operation=='wrapper':self.result=self.wrapper([])
        elif operation=='ob':
            self.progress['ob_pc']=7
            if self.p['null_world']:self.npe('Cannot invoke "fd.a(int, int, int)" because "<parameter2>" is null',['ob.a'],'ob','a','(Lgs;Lfd;Liz;IIII)Z',7)
            clicked=self.getter(self.clicked,['ob.a'])
            if clicked>0:self.progress.update(ob_pc=31,activation_entered=True,activation_returned=True)
            if self.stack is None:self.progress['ob_pc']=44;self.result=False
            else:
                self.progress.update(ob_pc=56,item_entered=True)
                self.result=self.wrapper(['ob.a'])
                self.progress.update(ob_pc=59,item_returned=True)
        else:raise ValueError('declared Flint operation')


def model(profile,prepared_class=None):
    if prepared_class is None:prepared_class=dict(scheduled=dict(initialized=False,sequence='0'*16),entity=dict(initialized=False,next_id=0),stats=dict(total=[],unsent=[],dirty=False))
    m=FlintModel(profile,prepared_class);before=m.snapshot()
    m.before_cells={k:list(v) for k,v in m.cells.items()};m.before_heights=dict(m.heights)
    try:m.invoke()
    except Fault as failure:m.failure=failure
    return dict(before=before,after=m.snapshot(),events=m.events,result=m.result if m.failure is None else None,
                exception='NONE' if m.failure is None else m.failure.exception,
                exception_message=None if m.failure is None else m.failure.message,
                exception_source_stack=[] if m.failure is None else m.failure.stack,
                exception_origin=None if m.failure is None else getattr(m.failure,'origin',None),
                invocation_null=False if m.failure is None else getattr(m.failure,'invocation_null',False),
                cells=m.cells,heights=m.heights,sky=m.sky,before_cells=m.before_cells,before_heights=m.before_heights,
                world_draws=m.random.draws,math_draws=m.math.draws,item_draws=m.item.draws,progress=dict(m.progress))


def plane_arrays(profile,after,expected=None):return F.plane_arrays(common(profile),after,model(profile) if expected is None else expected)
def planes(profile,after,expected=None):return F.planes(common(profile),after,model(profile) if expected is None else expected)
