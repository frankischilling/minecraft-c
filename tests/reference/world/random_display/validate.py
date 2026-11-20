"""Check genuine source execution, seeded RNG returns, query order and sampler prefix replay."""
import hashlib
import json
from pathlib import Path
import re
import struct

MULT=0x5deece66d
MASK=(1<<48)-1
HERE=Path(__file__).resolve().parent

def need(ok,message):
    if not ok:raise ValueError(message)
def rows(path):
    need(path.stat().st_size<8*1024*1024,'bounded observations')
    return[json.loads(line)for line in path.read_text().splitlines()]
def table(name):return[line.split('\t')for line in(HERE/name).read_text().splitlines()[1:]]
def i32(x):return(x+(1<<31))%(1<<32)-(1<<31)
def fhex(x):return struct.pack('>f',x).hex()

class Random:
    def __init__(self,seed):self.seed=(seed^MULT)&MASK
    def bits(self,n):self.seed=(self.seed*MULT+11)&MASK;return self.seed>>(48-n)
    def integer(self,bound):
        while True:
            bits=self.bits(31);value=(bound*bits)>>31 if bound&(bound-1)==0 else bits%bound
            if bound&(bound-1)==0 or bits-value+bound-1<1<<31:return value
    def floating(self):return self.bits(24)/(1<<24)
    def hex(self):return f'{self.seed:012x}'

def validate(directory,pins):
    directory=Path(directory)
    normal=rows(directory/'normal/target.ndjson');debug=rows(directory/'debugger/target.ndjson')
    need(normal==debug,'ordinary/read-only JDI observations exact')
    need((directory/'normal/target.ndjson').read_bytes()==(directory/'debugger/target.ndjson').read_bytes(),'ordinary/JDI byte identity')
    samplers=[r for r in normal if r['event']=='sampler'];displays=[r for r in normal if r['event']=='display']
    source_samplers,source_displays=table('samplers.tsv'),table('profiles.tsv')
    need(len(samplers)==len(source_samplers)==12 and len(displays)==len(source_displays)==99,'finite authored input profile counts')
    records=rows(directory/'debugger/checkpoints.ndjson')
    code=[r for r in records if r['event']=='code-pin'];entries=[r for r in records if r['event']=='entry'];randoms=[r for r in records if r['event']=='random'];exceptions=[r for r in records if r['event']=='exception']
    summaries={r['case']:r for r in records if r['event']=='summary'}
    need(set(summaries)==set(range(12))|set(range(1000,1099)),'complete actual call lifetime summaries')
    for pin in code:
        m=next(m for m in pins['classes'][pin['class']]['methods']if(m['name'],m['descriptor'])==(pin['method'],pin['descriptor']))
        need((pin['bytes'],pin['sha256'])==(m['code_bytes'],m['code_sha256']),'actual live original Code')
    need(len(code)==24,'all selected live method pins')
    need({r['class']for r in code if r['method']=='b'}=={'uu','ss','bs','rp','vm','db','tc','sm','wo','ak','yq'},'all eleven unchanged display declarations pinned')
    for index,(row,p)in enumerate(zip(samplers,source_samplers)):
        need(row['case']==index and row['input']==p,'sampler identity/declared input')
        cells=row['row'].split();need(len(cells)==11,'sampler transport')
        summary=summaries[index]
        constructor=[r for r in records if r['event']=='local-constructor'and r['case']==index]
        clock=[r for r in records if r['event']=='clock'and r['case']==index]
        need(len(constructor)==1 and len(clock)==2,'one actual default local Random with U/nano evidence')
        clock={r['kind']:int(r['value'],16)for r in clock};need(set(clock)=={'seedUniquifier','nanoTime'},'actual constructor clock pair')
        local=constructor[0];expected=(clock['seedUniquifier']^clock['nanoTime']^MULT)&MASK
        need(not local['is_world_random']and local['seed']==summary['local_seed_initial']==summary['local_seed_final']==f'{expected:012x}','actual distinct local Random and unchanged state through inherited/torch routes')
        original,replacement=Random(0),Random(123);coordinates=[]
        count=0 if index==6 else 1 if index in(7,10)else 2 if index==11 else 1000
        origin=list(map(int,p[3:6]))
        for sample in range(count):
            random=replacement if index==9 and sample>0 else original
            xyz=[]
            for arg in origin:xyz.append(i32(i32(arg+random.integer(16))-random.integer(16)))
            coordinates.append(xyz)
        queries=[r for r in entries if r['case']==index and r['location']['class']=='fd'and r['location']['method']=='a'and r['location']['descriptor']=='(III)I']
        need([r['arguments']for r in queries]==coordinates[:16],'actual first16 acquiring queries/int32 coordinate replay')
        completed=0 if index in(6,7,10)else 1 if index==11 else 1000
        selected=1000 if index in(1,2,3,9)else 1 if index in(7,10,11)else 0
        need((summary['completed_samples'],summary['query_sites'],summary['selected_sites'],summary['world_random_sites'])==(completed,count,selected,1 if index==6 else count*6),'actual complete sampler invocation/tail checkpoints')
        need(cells[4]==('-'if index==6 else replacement.hex()if index==9 else original.hex())and cells[5]==original.hex(),'complete current/captured World Random seed replay')
        need(cells[6:8]==['17','6001'],'sampler g/Q unchanged')
        bodies=[r for r in entries if r['case']==index and r['location']['method']=='b'and r['location']['descriptor']=='(Lfd;IIILjava/util/Random;)V']
        need(all(r['is_sampler_local']and not r['is_current_world_random']and r['argument_seed']==local['seed']for r in bodies),'actual retained local argument across selected display invocations')
        if index in(6,7):need(any(r['case']==index and r['class']=='java.lang.NullPointerException'and r['location']['class']=='fd'and r['location']['method']=='q'and r['location']['pc']==(31 if index==6 else 125)for r in exceptions),'actual sampler null-failure BCI')
        if index==11:need(any(r['case']==11 and r['location']=={'class':'fd','method':'c','descriptor':'(II)Llm;','pc':6}and any(f['class']=='fd'and f['method']=='q'and f['pc']==100 for f in r['source_frames'])for r in exceptions),'actual late provider-null query failure prefix')
    display_event_rows=[];sampler_event_rows=[]
    names={'smoke','flame','reddust','portal','lava','largesmoke','liquid.water','portal.portal','fire.fire'}
    for group in(samplers,displays):
        for row in group:
            for ordinal,event in enumerate(row['events']):
                cells=event.split();need(cells[0]in('DISPLAY_PARTICLE','DISPLAY_SOUND')and cells[1:3]==[str(row['case']),str(ordinal)]and cells[3]in names,'ordered original callback event identity/name')
                widths=[16]*6 if cells[0]=='DISPLAY_PARTICLE'else[16]*3+[8]*2
                need(len(cells)==4+len(widths)and all(re.fullmatch('[0-9a-f]{'+str(width)+'}',v)for v,width in zip(cells[4:],widths)),'original raw IEEE event transport')
                if group is displays:display_event_rows.append(event)
                else:sampler_event_rows.append(event.replace('DISPLAY_','SAMPLER_',1))
    for index,(row,p)in enumerate(zip(displays,source_displays)):
        need(row['case']==index and row['input']==p and not row['argument_is_world_random'],'direct identity/explicit distinct argument')
        cells=row['row'].split();need(len(cells)==12,'direct transport')
        rng={'argument':None if p[4]=='NULL'else Random(int(p[4])),'original':Random(int(p[5])),'replacement':Random(123)}
        for draw in[r for r in randoms if r['case']==1000+index]:
            need(draw['role']in rng and rng[draw['role']]is not None,'actual selected Random ownership')
            random=rng[draw['role']];expected=random.integer(draw['arguments'][0])if draw['method']=='nextInt'else{'F':fhex(random.floating())}
            need(draw['result']==expected and draw['seed']==random.hex(),'actual argument/original/replacement RNG return/state replay')
        need(cells[6]==('-'if rng['argument']is None else rng['argument'].hex())and cells[8]==rng['original'].hex(),'direct argument/captured World final RNG states')
        need(cells[7]==('-'if p[9]=='world_random_null'else rng['replacement'].hex()if p[10]=='swap_random'else rng['original'].hex()),'direct late current World RNG state')
        bodies=[r for r in entries if r['case']==1000+index and r['location']['method']=='b']
        need(bodies and all(r['is_direct_argument']and not r['is_current_world_random']for r in bodies),'actual display/wrapper argument identity')
        need(int(cells[9])+int(cells[10])==int(cells[11])==len(row['events']),'direct ordered event census')
    def direct_entries(i,owner,name,descriptor=None):return[r for r in entries if r['case']==1000+i and r['location']['class']==owner and r['location']['method']==name and(descriptor is None or r['location']['descriptor']==descriptor)]
    def event_count(i):return len(displays[i]['events'])
    need([event_count(i)for i in(6,7,8,10)]==[5,6,0,5],'ore zero-Y quirk/exposure and captured World Random continuation')
    need(len([r for r in randoms if r['case']==1010 and r['role']=='original'])==18,'ore continues captured original Random after synchronous replacement')
    need(not direct_entries(16,'fd','e'),'water nonzero ticket short circuits metadata query')
    need(direct_entries(12,'fd','e')and not direct_entries(21,'fd','g'),'water ticket precedes metadata; covered lava material short circuits opacity')
    need([r['location']['class']for r in direct_entries(4,'ss','b')+direct_entries(4,'vm','b')]==['ss','vm'],'real Stairs virtual wrapper delegate')
    need(not direct_entries(74,'yq','c')and len(direct_entries(75,'yq','c'))==1,'Fire solid and burnable support short circuit distinction')
    need([r['arguments'][1:]for r in direct_entries(81,'yq','c')]==[[8,63,8],[7,64,8],[9,64,8],[8,64,7],[8,64,9],[8,65,8]],'Fire independent face query order')
    need(int(displays[81]['row'].split()[9])==10,'all five Fire faces emit independently')
    need(len(direct_entries(91,'fd','a','(Ljava/lang/String;DDDDDD)V'))==2 and event_count(91)==1,'source emits second Torch particle despite current listener List cleared')
    for index,owner,name,pc in[(3,'ss','b',11),(9,'bs','i',25),(89,'db','b',26),(94,'yq','c',14)]:need(any(r['case']==1000+index and r['class']=='java.lang.NullPointerException'and(r['location']['class'],r['location']['method'],r['location']['pc'])==(owner,name,pc)for r in exceptions),'actual direct source exception BCI')
    for mode in('normal','debugger'):
        actual=set(re.findall(r"Initializing '([^']+)'",(directory/mode/'class-init.log').read_text()))&set(pins['jar_class_names'])
        need(actual==set(pins['initialized_original_classes']),'actual initialized original class closure')
        need(not{'net/minecraft/client/Minecraft','yo','vy','px'}&actual,'excluded device bootstrap initialization')
        bindings=[s.split()for s in(directory/mode/'target.stdout').read_text().splitlines()if s.startswith('BIND ')]
        need(len(bindings)==len(pins['classes']),'all runtime original class bindings')
        for _,name,extent,digest in bindings:need((int(extent),digest)==(pins['classes'][name]['class_bytes'],pins['classes'][name]['class_sha256']),'runtime original bytes and origin')
    observations={'sampler_records':samplers,'display_records':displays,'sampler_rows':[r['row']for r in samplers],'display_rows':[r['row']for r in displays],
                  'sampler_event_rows':sampler_event_rows,'display_event_rows':display_event_rows,'summaries':[summaries[i]for i in sorted(summaries)],
                  'coverage':{'samplers':12,'direct_displays':99,'declaring_bodies':11,'live_method_pins':len(code),'class_pins':len(pins['classes']),'initialized_original_classes':len(pins['initialized_original_classes']),'rng_returns':len(randoms),'query_returns':sum(r['event']=='query-return'for r in records),'source_entries':len(entries),'exception_events':len(exceptions),'local_default_constructor_occurrences':12,'constructor_clock_rows':24},
                  'limits':['Source body unchanged; genuine finite cached World/Chunk fixtures, no generation qualification.','Direct argument Random seeds are declared inputs and distinct from actual FD.q local default entropy.','Default U/nano pairs are read-only observations with independent seed formula replay, never rebound.','Callbacks are authored observation/failure/mutation adapters; native IO Throwable classes pair reached source effects only.','Sampler detailed method arguments cover first16 samples; complete loop counts are actual invocation/tail checkpoints, full RNG states independently replayed.','Whole staticfinal Block.m remains original; exceptional source input mutates/restores only selected m[1].']}
    (directory/'observations.json').write_bytes((json.dumps(observations,indent=2)+'\n').encode())
    return observations['coverage']
