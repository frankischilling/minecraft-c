"""Validate real predicate outputs, read-only order and exact JDK Random replay."""
import json
import re
import struct
from pathlib import Path

MASK=(1<<48)-1
MULT=0x5deece66d

def need(ok,message):
    if not ok:raise ValueError(message)

def rows(path):
    need(path.stat().st_size<2097152,'bounded observation transport')
    return [json.loads(line)for line in path.read_text().splitlines()]

def next_int(seed,bound):
    count=0
    while True:
        seed=(seed*MULT+11)&MASK;count+=1;bits=seed>>17
        if bound&(bound-1)==0:return seed,(bound*bits)>>31,count
        value=bits%bound
        if bits-value+bound-1<1<<31:return seed,value,count

def i32(value):return((value+(1<<31))%(1<<32))-(1<<31)
def i64(value):return((value+(1<<63))%(1<<64))-(1<<63)

def validate(directory,pins):
    directory=Path(directory)
    normal=rows(directory/'normal/observations.ndjson')
    debug=rows(directory/'debugger/target-observations.ndjson')
    need(normal==debug,'ordinary/read-only-JDI eligibility state exactness')
    elig=[r for r in normal if r['event']=='eligibility'];chunks=[r for r in normal if r['event']=='chunk_random']
    need(len(elig)==59 and len(chunks)==30,'finite predicate/Chunk row count')
    records=rows(directory/'debugger/checkpoints.ndjson')
    code=[r for r in records if r['event']=='code-pin']
    for pin in code:
        method=next(m for m in pins['classes'][pin['class']]['methods']if m['name']==pin['method']and m['descriptor']==pin['descriptor'])
        need((pin['bytes'],pin['sha256'])==(method['code_bytes'],method['code_sha256']),'actual live JVM source bytecodes')
    need(len(code)==19,'all19 live source method pins')
    rng=[r for r in records if r['event']=='random'];points=[r for r in records if r['event']=='checkpoint']
    exceptions=[r for r in records if r['event']=='source_exception']
    for index,row in enumerate(elig):
        need(row['index']==index,'predicate identity')
        cells=row['state_row'].split();need(len(cells)==13 and cells[1]==str(index),'predicate row schema')
        need(cells[6]==f'{(89^MULT)&MASK:012x}','World Random untouched')
        seed=(row['own_seed_input']^MULT)&MASK
        case_points=[r for r in points if r['case']==index]
        locations=[(r['location']['class'],r['location']['method'],r['location']['descriptor'],r['location']['pc'])for r in case_points]
        def reached(owner,method,descriptor):return any(x[:3]==(owner,method,descriptor)for x in locations)
        for draw in [r for r in rng if r['case']==index and r['own']]:
            seed,value,_=next_int(seed,draw['bound'])
            need(draw['result']==value and draw['seed']==f'{seed:012x}','actual own RNG replay')
        need(cells[5]==f'{seed:012x}','final actual own Random state')
        if row['controlled_virtual_callee']:
            need(row['failure']=='java.lang.IllegalStateException' and cells[4]=='-1','explicit controlled callee failure')
            need(int(cells[7])==(10 if row['scenario']==2 else 11),'source thunder temporary/restore prefix')
        elif row['kind']!='uw':
            expected=1 if row['scenario']in(2,6,7,8)else 0
            need(int(cells[4])==expected,'genuine GZ predicate outcome')
            need(int(cells[7])==(11 if row['scenario']==8 else 0),'genuine GZ source f restoration')
            scene=row['scenario']
            need(reached('fd','n','(III)I')==(scene!=0),'saved Sky short circuit before block-light')
            need(reached('ls','d','()Z')==(scene not in(0,1)),'light short circuit before LS')
            need(reached('fd','a','(Lsn;Leq;)Ljava/util/List;')==(scene not in(0,1,5,10)),'no-Entity-collision gate order')
            need(reached('fd','b','(Leq;)Z')==(scene not in(0,1,3,5,10)),'solid collision gate before liquid')
            need(reached('gz','a','(III)F')==(scene in(2,6,7,8,9)),'late virtual GZ path-weight dispatch')
            if scene in(8,9):
                temp=[p for p in case_points if p['location']['class']=='gz'and p['location']['pc']==94]
                restored=[p for p in case_points if p['location']['class']=='gz'and p['location']['pc']==115]
                need(len(temp)==len(restored)==1 and temp[0]['world_f']==10 and restored[0]['world_f']==(11 if scene==8 else 0),'actual thunder write/restore boundaries')
        else:
            expected=1 if row['scenario']in(3,4,7,8,9)else -1 if row['scenario']in(10,11)else 0
            need(int(cells[4])==expected,'genuine Slime predicate outcome')
            if row['scenario']==12:
                draws=[r for r in rng if r['case']==index and not r['own']]
                initial=((186643064605892^MULT)&MASK)
                after,value,count=next_int(initial,10)
                need(count==2 and len(draws)==1 and draws[0]['result']==value and draws[0]['seed']==f'{after:012x}','actual UW seeded-Random rejection')
    for index,row in enumerate(chunks):
        cells=row['row'].split();need(len(cells)==10 and cells[1]==str(index),'Chunk Random row schema')
        blank,x,z=int(cells[2]),int(cells[3]),int(cells[4]);salt=int(cells[5],16);world=int(cells[6],16)
        if world>>63:world-=1<<64
        seed=i64(world+i32(i32(x*x)*4987142)+i32(x*5947611)+i64(i32(z*z)*4392871)+i32(z*389711))^salt
        state=(seed^MULT)&MASK
        need(cells[7]==f'{state:012x}','actual Chunk width-sensitive seed')
        after,value,count=next_int(state,10)
        need(cells[8:]==[str(value),f'{after:012x}'],'actual fresh Chunk RNG draw')
        draws=[r for r in rng if r['case']==1000+index]
        need(len(draws)==1 and not draws[0]['own'] and draws[0]['seed']==f'{after:012x}'and draws[0]['result']==value,'read-only actual Chunk draw')
        if index>=28:need(count==2,'actual seeded-Random rejection path')
    animals=[r for r in normal if r['event']=='animal'];water=[r for r in normal if r['event']=='water'];paths=[r for r in normal if r['event']=='path_weight']
    need(len(animals)==50 and len(water)==9 and len(paths)==6,'bounded animal/water/component cases')
    for index,row in enumerate(animals):
        scene=index%10;kind=['wh','dl','bx','ww','gi'][index//10];cells=row['state_row'].split()
        need(row['index']==index and row['kind']==kind and row['scenario']==scene and row['genuine_world']and row['failure']is None,'genuine animal profile identity')
        need(len(cells)==13 and cells[:4]==['ANIMAL_STATE',str(index),kind,str(scene)],'animal row schema')
        need(cells[4]==str(int(scene in(2,6,7,8,9)))and cells[5:8]==[f'{MULT:012x}',f'{89^MULT:012x}',str(15 if scene==9 else 0)],'actual BG outcome/Random/light subtraction')
        case=[r for r in points if r['case']==4000+index]
        def reached(owner,name,descriptor):return any(p['location']['class']==owner and p['location']['method']==name and p['location']['descriptor']==descriptor for p in case)
        need(reached('bg','d','()Z')and reached('fd','m','(III)I')==(scene!=0),'actual BG grass/plant-light order')
        need(reached('ls','d','()Z')==(scene not in(0,1)),'BG threshold before II/LS')
        need(reached('fd','a','(Lsn;Leq;)Ljava/util/List;')==(scene not in(0,1,5)),'BG no-Entity gate')
        need(reached('fd','b','(Leq;)Z')==(scene not in(0,1,3,5)),'BG solid before liquid')
        need(reached('bg','a','(III)F')==(scene in(2,6,7,8,9))and not reached('fd','c','(III)F'),'actual virtual BG grass weight after LS')
        need(not any(r['case']==4000+index for r in rng),'BG consumes no Random draws')
    for index,row in enumerate(water):
        cells=row['state_row'].split();need(row['index']==row['scenario']==index and row['kind']=='xt'and row['genuine_world']and row['failure']is None,'genuine Squid profile')
        need(len(cells)==13 and cells[:4]==['WATER_STATE',str(index),'xt',str(index)],'water row schema')
        need(cells[4]==str(int(index not in(4,7)))and cells[5:10]==[f'{MULT:012x}',f'{89^MULT:012x}','0','0','0'],'AR outcome/state without land/liquid/height gates')
        case=[r for r in points if r['case']==5000+index]
        need(any(p['location']['class']=='ar'for p in case)and any(p['location']['class']=='fd'and p['location']['method']=='a'and p['location']['descriptor']=='(Leq;)Z'for p in case),'actual AR no-Entity wrapper')
        need(not any(p['location']['class']in('ls','ii','bg')or p['location']['class']=='fd'and(p['location']['method'],p['location']['descriptor'])in[('a','(Lsn;Leq;)Ljava/util/List;'),('b','(Leq;)Z'),('m','(III)I'),('n','(III)I'),('c','(III)F')]for p in case),'AR ignores all extra land/light/liquid/collision gates')
        need(not any(r['case']==5000+index for r in rng),'AR consumes no Random')
    for index,row in enumerate(paths):
        cells=row['row'].split();need(cells[:4]==['PATH_WEIGHT',str(index),'wh',str(index)],'direct BG component identity')
        brightness=struct.unpack('>f',bytes.fromhex(row['brightness_bits']))[0]
        expected='41200000'if index==0 else struct.pack('>f',brightness-.5).hex()
        need(cells[4]==expected,'actual BG grass/fallback weight with genuine World brightness')
    failures=[r for r in normal if r['event']=='chunk_failure']
    need(len(failures)==4 and all(r['failure']=='java.lang.NullPointerException'for r in failures),'genuine Chunk World/Info null failures')
    for row in failures:
        found=[r for r in exceptions if r['case']==2000+row['index']]
        need(len(found)==1,'one actual JVM leaf failure point')
        where=found[0]['location']
        need(where['class']==('fd'if row['mode']==1 else 'li'if row['blank']else'lm')and where['pc']==(4 if row['mode']==1 else 8),'exact null World/Info original failure PC')
    for mode,filename in [('normal','target.stdout'),('debugger','target.stdout')]:
        lines=(directory/mode/filename).read_text().splitlines()
        need([line for line in lines if line.startswith('CASE ')]==[f"CASE {r['index']} {r['kind']} {r['scenario']}"for r in elig],'all source predicate completion receipts')
        need([line for line in lines if line.startswith('PASSIVE ')]==[f"PASSIVE {r['event']} {r['index']} {r['kind']} {r['scenario']}"for r in animals+water],'all actual animal/water receipts')
        need([line for line in lines if not line.startswith(('CASE ','PASSIVE ','BIND '))]==['16 achievements','151 recipes'],'actual original preparation banners')
        bindings=[line.split()for line in lines if line.startswith('BIND ')]
        need(len(bindings)==len(pins['classes']),'all original runtime bindings')
        for _,name,size,digest in bindings:
            expected=pins['classes'][name.replace('.','/')]
            need((int(size),digest)==(expected['class_bytes'],expected['class_sha256']),'actual class resource/origin')
        log=directory/mode/('class-init.log'if mode=='normal'else'target-class-init.log')
        initialized=set(re.findall(r"\bInitializing '([^']+)'",log.read_text()))
        need(initialized&set(pins['jar_class_names'])==set(pins['original_initialized_classes']),'exact original initialized closure')
        need(not {'net/minecraft/client/Minecraft','px','yo','vy'}&initialized and not any(n.startswith(('org/lwjgl/','paulscode/'))for n in initialized),'no device/client bootstrap')
    result={'state_rows':[r['state_row']for r in elig],'chunk_rows':[r['row']for r in chunks],
            'animal_rows':[r['state_row']for r in animals],'water_rows':[r['state_row']for r in water],'path_rows':[r['row']for r in paths],
            'original_rows':normal,'checkpoints':points,'random_events':rng,'exceptions':exceptions}
    thunder=[r['row']for r in normal if r['event']=='thunder']
    need(thunder==['THUNDER 0 3f666666 0','THUNDER 1 3f666667 1','THUNDER 2 7fc00000 0'],'actual FD.B double threshold/NaN')
    result['thunder_rows']=thunder
    (directory/'observations.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n')
    return {'genuine_hostile_predicates':57,'genuine_animal_predicates':50,'genuine_water_predicates':9,'animal_path_components':6,'controlled_failure_prefixes':2,'chunk_random_leaves':30,'chunk_failure_prefixes':4,'thunder_threshold_leaves':3,'live_method_pins':len(code),'checkpoints':len(points),'random_returns':len(rng),'actual_exceptions':len(exceptions),'original_initialized_classes':len(pins['original_initialized_classes'])}
