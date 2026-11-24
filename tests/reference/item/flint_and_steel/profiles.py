"""Predeclared unchanged-original YE, IZ and OB method inputs."""


def i32(value):
    return (value + (1 << 31)) % (1 << 32) - (1 << 31)


def shifted(xyz,face):
    out=list(xyz)
    if face==0:out[1]=i32(out[1]-1)
    if face==1:out[1]=i32(out[1]+1)
    if face==2:out[2]=i32(out[2]-1)
    if face==3:out[2]=i32(out[2]+1)
    if face==4:out[0]=i32(out[0]-1)
    if face==5:out[0]=i32(out[0]+1)
    return out


def prepared_cell(point):
    x,y,z=point
    return 0<=x<32 and 0<=z<32 and 0<=y<128


def declared():
    result=[]

    def add(name,operation='direct',occupied=False,unsupported=False,**changes):
        p=dict(mode='flint-'+name,operation=operation,xyz=[15,64,15],face=1,
               stack_id=259,count=1,damage=0,animation=5,world_seed=1,item_seed=2,
               remote=False,null_world=False,null_player=False,null_stack=False,
               null_item_random=False,null_fire=False,null_provider=False,null_access=False,
               null_stats_writer=False,null_use_stats=False,null_break_stats=False,
               null_item_table=False,item_random_world=False,sound_action='none',cell_error_at=0,
               range_error_at=0,sound_error_at=0,cells=[],heights=[])
        p.update(changes)
        if 'cells' not in changes and not unsupported:
            target=shifted(p['xyz'],p['face']);floor=[target[0],target[1]-1,target[2]]
            cells={tuple(point):[1,0] for point in (p['xyz'],floor) if prepared_cell(point)}
            if occupied and prepared_cell(target):cells[tuple(target)]=[1,7]
            p['cells']=[[*point,*value] for point,value in sorted(cells.items())]
        result.append(p)

    for face in range(6):add('face-'+str(face),face=face)
    for op in ('wrapper','ob'):add(op,op)
    for face in (-1,6,2147483647):add('face-unchanged-'+str(face),face=face)
    for op in ('direct','wrapper','ob'):
        add(op+'-occupied',op,occupied=True)
        add(op+'-unsupported',op,unsupported=True)
    frame_x=[[14,64,15,49,0],[14,65,15,49,0],[14,66,15,49,0],
             [15,63,15,49,0],[15,67,15,49,0],[16,63,15,49,0],
             [16,67,15,49,0],[17,64,15,49,0],[17,65,15,49,0],[17,66,15,49,0]]
    frame_z=[[15,63,15,49,0],[15,63,16,49,0],[15,64,14,49,0],
             [15,64,17,49,0],[15,65,14,49,0],[15,65,17,49,0],
             [15,66,14,49,0],[15,66,17,49,0],[15,67,15,49,0],[15,67,16,49,0]]
    for op in ('direct','wrapper','ob'):
        add(op+'-portal-x',op,xyz=[15,63,15],cells=frame_x)
        add(op+'-portal-z',op,xyz=[15,63,15],cells=frame_z)
    add('portal-cell-error-third',xyz=[15,63,15],cells=frame_x,cell_error_at=3)
    add('portal-range-error-second',xyz=[15,63,15],cells=frame_z,range_error_at=2)
    add('remote-supported',remote=True)
    add('remote-unsupported',unsupported=True,remote=True)
    add('wrapper-remote','wrapper',remote=True)
    for name,xyz,face in (
        ('x-max',[2147483647,64,15],5),('x-min',[-2147483648,64,15],4),
        ('y-max',[15,2147483647,15],1),('y-min',[15,-2147483648,15],0),
        ('z-max',[15,64,2147483647],3),('z-min',[15,64,-2147483648],2)):
        add('wrap-'+name,xyz=xyz,face=face)
    for y in (-2,-1,127):add('vertical-'+str(y),xyz=[15,y,15])
    for name,xyz in (('x-in',[31999999,64,15]),('x-out',[32000000,64,15]),
                     ('z-in',[15,64,32000000]),('z-out',[15,64,32000001]),
                     ('x-low-in',[-32000000,64,15]),('x-low-out',[-32000001,64,15])):
        add('provider-bound-'+name,xyz=xyz,face=6,null_provider=True)
    for op in ('direct','wrapper','ob'):
        add(op+'-world-null',op,null_world=True)
        add(op+'-stack-null',op,null_stack=True)
        add(op+'-provider-null',op,null_provider=True)
    add('player-null',null_player=True)
    add('player-null-break',null_player=True,damage=64)
    add('wrapper-player-null','wrapper',null_player=True)
    add('wrapper-player-null-break','wrapper',null_player=True,damage=64)
    add('ob-player-null','ob',null_player=True)
    add('random-null',null_item_random=True)
    add('wrapper-random-null','wrapper',null_item_random=True)
    add('fire-null',null_fire=True)
    add('wrapper-fire-null','wrapper',null_fire=True)
    add('random-before-fire-null',null_item_random=True,null_fire=True)
    add('occupied-skips-random-fire',occupied=True,null_item_random=True,null_fire=True)
    add('access-null',null_access=True)
    add('occupied-skips-access',occupied=True,null_access=True)
    add('writer-null-unused',null_stats_writer=True)
    add('writer-null-break',null_stats_writer=True,damage=64)
    add('wrapper-writer-null','wrapper',null_stats_writer=True)
    add('wrapper-writer-null-break','wrapper',null_stats_writer=True,damage=64)
    add('ob-writer-null','ob',null_stats_writer=True)
    add('wrapper-use-array-null','wrapper',null_use_stats=True)
    add('break-array-null',null_break_stats=True,damage=64)
    add('wrapper-use-array-before-player-null','wrapper',null_use_stats=True,null_player=True)
    add('item-table-null',null_item_table=True)
    add('wrapper-item-table-null','wrapper',null_item_table=True)
    for op in ('direct','wrapper','ob'):
        for kind in ('sound','range','cell'):
            add(op+'-'+kind+'-error',op,**{kind+'_error_at':1})
    for action in ('fire-null','fire-id-zero','item-random-world','stack-stick',
                   'stats-writer-null','use-array-null','break-array-null'):
        add('sound-'+action,'wrapper',sound_action=action,
            damage=64 if action in ('stats-writer-null','break-array-null') else 0)
    for name,count,damage in (('damage63',1,63),('break64',1,64),
                              ('damage-overflow',1,2147483647),('zero-count',0,0),
                              ('zero-count-break',0,64),('negative-count-break',-2,64),
                              ('two-count-break',2,64),('negative-damage',1,-2)):
        add(name,count=count,damage=damage)
    add('wrapper-break64','wrapper',damage=64)
    add('ob-break64','ob',damage=64)
    add('direct-nondamageable-stack',stack_id=4)
    add('initial-item-random-world',item_random_world=True)
    add('sound-target-stone','wrapper',sound_action='target-stone')
    return result
