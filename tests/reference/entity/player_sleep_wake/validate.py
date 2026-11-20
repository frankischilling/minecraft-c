"""Strict portable component values, original-only effects and raw projection support."""
import hashlib
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
def require(v,m):
    if not v:raise ValueError(m)
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def unique(pairs):
    out={}
    for k,v in pairs:
        require(k not in out,'duplicate key '+k);out[k]=v
    return out
def bad_constant(x):raise ValueError('non-JSON numeric scalar '+x)
def loads(s):return json.loads(s,object_pairs_hook=unique,parse_constant=bad_constant)
def load(p):return loads(Path(p).read_text(encoding='utf-8'))
def exact(a,b):
    if type(a) is not type(b):return False
    if type(a) is dict:return a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a)
    if type(a) is list:return len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b))
    return a==b
def keys(v,k,m):require(type(v) is dict and set(v)==set(k),m)
def integer(v,lo=-(1<<31),hi=(1<<31)-1):return type(v) is int and lo<=v<=hi
def profiles(directory=HERE):
    directory=Path(directory)
    rows=load(directory/'profiles.json');lines=(directory/'profiles.tsv').read_text(encoding='ascii').splitlines();require(len(rows)==len(lines)==76,'matrix extent');names=set()
    for i,(r,line) in enumerate(zip(rows,lines)):
        keys(r,('name','values'),'profile schema');require(type(r['name']) is str and r['name'] not in names,'unique profile');names.add(r['name']);v=r['values']
        require(type(v) is list and len(v)==5 and all(integer(x) for x in v) and v[0]==i and v[1] in (0,1) and 0<=v[2]<4 and 0<=v[3]<8 and 0<=v[4]<=(4 if v[1]==0 else 7) and line==' '.join(map(str,v)),'typed exact matrix transport')
    require(sum(r['values'][1]==0 for r in rows)==17 and sum(r['values'][1]==1 for r in rows)==59,'sleep/wake inventory')
    return rows
def expected_fault(p):return (p[1]==0 and p[4]==4) or (p[1]==1 and p[4] in (5,6,7))
ALLOWED_ACTOR={'sn.aM','sn.aN','sn.aO','sn.aP','sn.aQ','sn.aR','sn.bg','sn.bh','sn.bf','gs.u','gs.a','gs.v','gs.w','gs.y','gs.b'}
def raw_rows(text,count=76):
    lines=text.splitlines();require(lines and lines[-1]==f'BED_NIGHT_COMPLETE cases={count} network=0 native=0 writes=0 exec=0 ticks=0 GUI=0','guarded source completion')
    rows=[loads(s[len('BED_NIGHT '):]) for s in lines if s.startswith('BED_NIGHT ')];require(len(rows)==count,'all original rows')
    declared=profiles()
    for i,r in enumerate(rows):
        keys(r,('schema','case','input','result','exception','before','after'),'raw exact row');p=declared[i]['values'];require(r['schema']=='BED_NIGHT_RAW_V1' and integer(r['case']) and r['case']==i and exact(r['input'],p),'raw input/order/types')
        require(r['exception']==('java/lang/NullPointerException' if expected_fault(p) else 'NONE'),'only declared actual source faults')
        for phase in ('before','after'):
            s=r[phase];keys(s,('selected','actor_fields','world_fields','provider_fields','bounds_fields','old_bounds_fields','coordinate_fields','random'),'complete raw snapshot schema')
            require(type(s['actor_fields']) is dict and type(s['world_fields']) is dict and type(s['selected']) is dict,'typed raw field snapshots')
        changed={k for k,v in r['before']['actor_fields'].items() if not exact(v,r['after']['actor_fields'][k])}
        require(changed<=ALLOWED_ACTOR,'unselected Actor mutation: '+str(changed-ALLOWED_ACTOR))
        changed_world={k for k,v in r['before']['world_fields'].items() if not exact(v,r['after']['world_fields'][k])}
        require(changed_world<={'fd.J'} and exact(r['before']['provider_fields'],r['after']['provider_fields']) and exact(r['before']['random'],r['after']['random']),'only selected World J change, actual provider/Random preservation')
        require(r['result'] is None if p[1] or expected_fault(p) else exact(r['result'],{'name':'OK','ordinal':0,'current_a':True}),'actual sleep result, wake void or failure')
        for phase in ('before','after'):
            s=r[phase]['selected'];require(s['world_players']==s['world_entities']==(2 if p[1]==0 and p[4]==1 else 1) and type(s['world_J']) is bool,'genuine published memberships/J')
            require(s['private_bN_same'] is True,'unreached private coordinate unchanged')
        require(r['after']['selected']['Q_same'] is True if p[1]==0 or expected_fault(p) or not(p[3]&1) else True,'Q preserved in source paths not reaching setter')
    return rows
def source_bindings(text,pins):
    got=[s for s in text.splitlines() if s.startswith('CLASS ')]
    expected=['CLASS '+n+' '+str(v['class_bytes'])+' '+v['class_sha256'] for n,v in pins['classes'].items()]
    require(got==expected,'actual original class origin/bytes')
def strip_raw(value):
    # Only explicit VM object IDs are nonportable. Symbolic identity relations,
    # every scalar/store/call and every selected field word remain unchanged.
    if type(value) is dict:return {k:strip_raw(v) for k,v in value.items() if k!='raw_object_id'}
    if type(value) is list:return [strip_raw(v) for v in value]
    return value
def journal(text,count=76):
    records=[loads(s) for s in text.splitlines()];require(all(type(r) is dict and r.get('event') in ('code','journal') for r in records),'JDI record kinds')
    rows=[r for r in records if r['event']=='journal'];code=[r for r in records if r['event']=='code'];require(len(rows)==count,'all ordered journals');pins=load(HERE/'source-pins.json')['classes'];seen=set()
    for c in code:
        keys(c,('event','class','method','descriptor','bytes','sha256'),'live Code schema');k=(c['class'],c['method'],c['descriptor']);require(k not in seen,'unique Code');seen.add(k)
        matches=[m for m in pins[c['class']]['methods'] if (m['name'],m['descriptor'])==k[1:]]
        require(len(matches)==1 and integer(c['bytes'],1) and c['bytes']==matches[0]['code_length'] and c['sha256']==matches[0]['code_sha256'],'actual original live Code pin')
    for i,r in enumerate(rows):
        keys(r,('event','case','cw_before_initialized','cw_after_initialized','calls','events'),'journal schema')
        require(integer(r['case']) and r['case']==i and type(r['cw_before_initialized']) is bool and type(r['cw_after_initialized']) is bool,'journal scalar types/order')
        require(type(r['calls']) is list and 1<=len(r['calls'])<=256 and type(r['events']) is list and len(r['events'])<=2048,'passive extent')
        for j,c in enumerate(r['calls']):
            keys(c,('id','caller','site','receiver','arguments','entered','returned','completed','exception','result'),'call schema')
            require(integer(c['id']) and c['id']==j and integer(c['entered'],0,2047) and type(c['completed']) is bool and type(c['arguments']) is list,'call scalar types')
            require((integer(c['returned'],0,2047) and c['returned']>c['entered'] and c['exception'] is None) if c['completed'] else (c['returned'] is None and c['exception']=='java/lang/NullPointerException'),'normal/exceptional exact call completion')
        for tick,e in enumerate(r['events']):
            require(type(e) is dict and integer(e['tick']) and e['tick']==tick and e['kind'] in ('ENTER','RETURN','STORE','EXCEPTION'),'total ordered passive timeline')
            if e['kind'] in ('ENTER','RETURN'):require(integer(e['call'],0,len(r['calls'])-1),'timeline call identity')
        root=r['calls'][0];require(root['site']['class']=='gs' and root['site']['method'] in ('a','b'),'actual direct selected GS root')
    return strip_raw(rows),code
def project(raw,js,code):
    result=[]
    for r,j,p in zip(raw,js,profiles()):
        result.append({'case':r['case'],'name':p['name'],'input':r['input'],'result':r['result'],'exception':r['exception'],
            'before':r['before']['selected'],'after':r['after']['selected'],'unselected_actor_preserved':True,'unselected_world_preserved':True,
            'random_before':r['before']['random'],'random_after':r['after']['random'],'journal':j})
    return {'schema':'BED_NIGHT_COMPONENT_FINITE_V1','rows':result,'live_code':code}

FIELDS={'position':(3,16),'motion':(3,16),'dimensions':(3,8),'sleep_offsets':(2,8),'bounds':(6,16),'old_bounds':(6,16)}
SELECTED={'position','motion','dimensions','sleep_offsets','sleeping','timer','v','Q','private_bN','bounds','old_bounds','world_J','remote','world_players','world_entities','other_sleeping','world_same','bounds_same','v_same','Q_same','Q_v_same','Q_before_v_same','private_bN_same','head','planes'}
def word(v,n):return type(v) is str and re.fullmatch('[0-9a-f]{'+str(n)+'}',v) is not None
def selected(v):
    keys(v,SELECTED,'selected field exact inventory')
    for name,(extent,size) in FIELDS.items():
        if name=='bounds' and v[name] is None:continue
        require(type(v[name]) is list and len(v[name])==extent and all(word(x,size) for x in v[name]),'raw field words '+name)
    for name in ('sleeping','world_J','remote','world_same','bounds_same','v_same','Q_same','Q_v_same','Q_before_v_same','private_bN_same'):
        require(type(v[name]) is bool,'boolean source relation '+name)
    require(integer(v['timer']) and integer(v['world_players'],1,2) and integer(v['world_entities'],1,2) and (v['other_sleeping'] is None or type(v['other_sleeping']) is bool),'source timer/list values')
    for name in ('v','Q','private_bN'):
        require(v[name] is None or (type(v[name]) is list and len(v[name])==3 and all(integer(x) for x in v[name])),'coordinate values '+name)
    require(type(v['head']) is list and len(v['head'])==2 and integer(v['head'][0],0,255) and integer(v['head'][1],0,15),'head fields')
    keys(v['planes'],('blocks','metadata','sky','block_light','height'),'complete plane hash set');require(all(word(x,64) for x in v['planes'].values()),'plane hash values')
def typed(v,kind=None,raw=False):
    if kind=='V':require(v is None,'void source result');return
    if kind=='Z':require(type(v) is bool,'boolean source operand');return
    if kind=='I':require(integer(v),'int source operand');return
    if kind in ('F','D'):
        keys(v,(kind,),'FP source scalar shape');require(word(v[kind],8 if kind=='F' else 16),'FP source scalar word');return
    if kind and kind.startswith('L'):
        require(v is None or type(v) is dict,'reference source operand')
    if v is None or type(v) is bool or integer(v):return
    require(type(v) is dict,'source transport value')
    if set(v) in ({'F'},{'D'}):typed(v,next(iter(v)),raw);return
    if set(v)=={'utf16'}:
        require(type(v['utf16']) is list and all(integer(x,0,65535) for x in v['utf16']),'UTF16 source String');return
    allowed=({'class','identity','raw_object_id'},{'class','identity','raw_object_id','static'}) if raw else ({'class','identity'},{'class','identity','static'})
    require(set(v) in allowed,'only declared comparison-only reference fields')
    require(type(v['class']) is str and v['class'] and type(v['identity']) is str and v['identity'] and (not raw or integer(v['raw_object_id'],1,(1<<63)-1)),'actual comparison-only reference identity')
    if 'static' in v:require(v['class']=='cw' and type(v['static']) is str and v['static'] in ('a','b','c','d','e'),'actual enum static identity')
def descriptor(text):
    require(type(text) is str and text.startswith('('),'descriptor');at=1;args=[]
    while text[at]!=')':
        start=at
        if text[at]=='L':at=text.index(';',at)+1
        else:require(text[at] in 'ZIFD','selected primitive descriptor');at+=1
        args.append(text[start:at])
    return args,text[at+1:]
def method_site(v,pins,helper=False):
    keys(v,('class','method','descriptor','pc'),'source site shape');require(all(type(v[k]) is str for k in ('class','method','descriptor')) and integer(v['pc'],0,65535),'source site scalar types')
    owner=v['class'];require(owner in pins,'pinned source declaring class '+owner)
    found=[m for m in pins[owner]['methods'] if m['name']==v['method'] and m['descriptor']==v['descriptor']]
    require(len(found)==1 and v['pc']<found[0]['code_length'],'reached source bytecode range')
    return found[0]
def audit_journal(row,pins,raw=False):
    keys(row,('event','case','cw_before_initialized','cw_after_initialized','calls','events'),'journal exact shape')
    require(row['event']=='journal' and integer(row['case'],0,75) and type(row['cw_before_initialized']) is bool and type(row['cw_after_initialized']) is bool,'journal scalar types')
    require(type(row['calls']) is list and type(row['events']) is list and 0<len(row['calls'])<=256 and len(row['events'])<=2048,'passive extent')
    for i,c in enumerate(row['calls']):
        keys(c,('id','caller','site','receiver','arguments','entered','returned','completed','exception','result'),'source call shape')
        require(integer(c['id']) and c['id']==i and integer(c['entered'],0,2047) and type(c['completed']) is bool,'source call identity/tick')
        method_site(c['site'],pins);method_site(c['caller'],pins)
        require(c['site']['pc']==0,'actual method entry zero');typed(c['receiver'],raw=raw)
        args,result=descriptor(c['site']['descriptor']);require(type(c['arguments']) is list and len(c['arguments'])==len(args),'actual argument arity')
        for value,kind in zip(c['arguments'],args):typed(value,kind,raw)
        if c['completed']:
            require(integer(c['returned'],c['entered']+1,2047) and c['exception'] is None,'actual returned call');typed(c['result'],result,raw)
        else:require(c['returned'] is None and c['result'] is None and c['exception']=='java/lang/NullPointerException','actual failed call prefix')
    stack=[];entered=set();returned=set();faults=[]
    for tick,e in enumerate(row['events']):
        require(type(e) is dict and integer(e.get('tick')) and e['tick']==tick and e.get('kind') in ('ENTER','RETURN','STORE','EXCEPTION'),'total ordered source events')
        if e['kind'] in ('ENTER','RETURN'):
            keys(e,('kind','call','tick'),'call-event shape');require(integer(e['call'],0,len(row['calls'])-1),'call-event identity');c=row['calls'][e['call']]
            if e['kind']=='ENTER':require(e['call'] not in entered and c['entered']==tick,'unique exact source entry');entered.add(e['call']);stack.append(e['call'])
            else:require(stack and stack.pop()==e['call'] and e['call'] not in returned and c['returned']==tick and c['completed'],'nested exact source return');returned.add(e['call'])
        elif e['kind']=='STORE':
            keys(e,('kind','site','owner','field','receiver','before','value','tick'),'field-store shape');method_site(e['site'],pins);typed(e['receiver'],raw=raw)
            require(type(e['owner']) is str and e['owner'] in pins and type(e['field']) is str,'actual field owner/name')
            f=[f for f in pins[e['owner']]['fields'] if f['name']==e['field']];require(len(f)==1,'actual pinned field');typed(e['before'],f[0]['descriptor'],raw);typed(e['value'],f[0]['descriptor'],raw)
        else:
            keys(e,('kind','site','class','tick'),'exception shape');method_site(e['site'],pins);require(e['class']=='java/lang/NullPointerException','actual source null exception');faults.append(e)
    require(entered==set(range(len(row['calls']))) and returned=={c['id'] for c in row['calls'] if c['completed']} and set(stack)=={c['id'] for c in row['calls'] if not c['completed']},'all complete and failed calls retained')
    require(len(faults)==int(expected_fault(profiles()[row['case']]['values'])),'declared fault count')
    return strip_raw(row)
def calls(row,owner,name,desc=None,pc=None):return [c for c in row['calls'] if c['site']['class']==owner and c['site']['method']==name and (desc is None or c['site']['descriptor']==desc) and (pc is None or c['caller']['pc']==pc)]
def stores(row,owner,field,pc=None):return [e for e in row['events'] if e['kind']=='STORE' and e['owner']==owner and e['field']==field and (pc is None or e['site']['pc']==pc)]
def effect_order(raw,j):
    p=raw['input'];kind,direction,flags,scenario=p[1:];a=raw['before']['selected'];b=raw['after']['selected'];selected(a);selected(b)
    wrapper=calls(j,'ls','b','(FF)V');resize=calls(j,'sn','b','(FF)V');require(len(wrapper)==len(resize)==1 and wrapper[0]['caller']['pc']==(119 if kind==0 else 5) and resize[0]['caller']['class']=='ls' and resize[0]['caller']['pc']==3,'actual GS -> LS -> SN resize chain')
    dims=['3e4ccccd','3e4ccccd','3e4ccccd'] if kind==0 else ['3f19999a','3fe66666','3fcf5c29']
    require(b['dimensions']==dims,'actual source dimensions/offset')
    width=stores(j,'sn','bg',2);height=stores(j,'sn','bh',7);require(len(width)==len(height)==1 and resize[0]['entered']<width[0]['tick']<height[0]['tick']<resize[0]['returned']<wrapper[0]['returned'],'source two size stores, without invented bounds update')
    if kind==0:
        require(len(calls(j,'fd','i','(III)Z',135))==1 and calls(j,'fd','i','(III)Z',135)[0]['result'] is True and len(calls(j,'gs','e','(I)V',228))==1,'present loaded Chunk and actual orientation child')
        position=calls(j,'sn','e','(DDD)V',250);require(len(position)==1,'actual oriented position call')
        require(b['Q_same'] is True and b['private_bN_same'] is True,'sleep does not set saved spawn')
        if scenario==4:require(b['sleeping'] is False and b['timer']==17 and b['v_same'] is True and b['bounds'] is None and not calls(j,'fd','y'),'sleep failure before u/timer/v and World y')
        else:
            require(b['sleeping'] is True and b['timer']==0 and b['v']==[8,64,8] and b['v_same'] is False and b['motion']==['0000000000000000']*3,'complete sleep state/new coordinate/zero motion')
            require(b['world_J'] is (scenario not in (1,2)) and len(calls(j,'fd','y','()V',331))==int(scenario!=2),'mixed/remote genuine World J behavior')
            born=calls(j,'br','<init>','(III)V',296);require(len(born)==1 and stores(j,'gs','u',280)[0]['tick']<born[0]['entered'] and stores(j,'gs','a',285)[0]['tick']<born[0]['entered'] and born[0]['returned']<stores(j,'gs','v',299)[0]['tick'],'new v source allocation/publication order')
            q=stores(j,'sn','aQ',307)[0]['tick'];r=stores(j,'sn','aR',311)[0]['tick'];x=stores(j,'sn','aP',314)[0]['tick'];require(q<r<x,'actual Y/Z/X zero-motion order')
    else:
        require(len(calls(j,'gs','E','()V',9))==1,'actual wake offset wrapper')
        if scenario in (5,6,7):
            require(b['timer']==37 and b['Q_same'] is True and not calls(j,'gs','a','(Lbr;)V'),'wake failure keeps timer/Q and exact prior effects')
            require(b['sleeping'] is (scenario!=6),'wake null World after null-v u store is a distinct prefix')
        else:
            require(b['sleeping'] is False and b['timer']==(0 if flags&4 else 100),'actual wake flags/timer')
            require(len(calls(j,'fd','y','()V',201))==int(bool(flags&2) and scenario!=4),'flag2 actual World y gate')
            setters=calls(j,'gs','a','(Lbr;)V',231);require(len(setters)==int(bool(flags&1)),'flag3 actual late saved-spawn setter')
            if flags&1:
                require(b['Q']==b['v'] and (b['Q_v_same'] is (b['v'] is None)) and b['Q_same'] is False,'Q copy/null is a source identity operation')
                require(stores(j,'gs','a',210 if flags&4 else 219)[0]['tick']<setters[0]['entered'],'timer before late Q child')
            else:require(b['Q_same'] is True,'Q not reached')
        bed=scenario not in (1,2,5,6)
        require(len(calls(j,'ve','a','(Lfd;IIIZ)V',80))==int(bed) and len(calls(j,'ve','f','(Lfd;IIII)Lbr;',103))==int(bed),'only matching Bed clears/searches')
        if bed:
            clear=calls(j,'ve','a','(Lfd;IIIZ)V',80)[0];search=calls(j,'ve','f','(Lfd;IIII)Lbr;',103)[0];require(clear['returned']<search['entered'] and b['head']==[26,8|direction],'occupied clear completes before exit search')
        if scenario==3:require(search['result'] is None and len(calls(j,'br','<init>','(III)V',134))==1,'observed blocked exit fallback, not invented success')
        if scenario in (1,2,5,6):require(not calls(j,'sn','e','(DDD)V') and b['bounds']==a['bounds'],'absent/null branches do not reposition/rebuild bounds')
    positions=calls(j,'sn','e','(DDD)V')
    if positions:
        require(len(stores(j,'sn','aM',2))==len(stores(j,'sn','aN',7))==len(stores(j,'sn','aO',13))==1,'all source position stores retained')
        require(stores(j,'sn','aM',2)[0]['tick']<stores(j,'sn','aN',7)[0]['tick']<stores(j,'sn','aO',13)[0]['tick'],'source X/Y/Z position order')

def code_records(records,pins):
    require(type(records) is list and len(records)==26,'exact reached live Code inventory')
    seen=set()
    for c in records:
        keys(c,('event','class','method','descriptor','bytes','sha256'),'live Code schema')
        require(c['event']=='code' and type(c['class']) is str and c['class'] in pins and type(c['method']) is str and type(c['descriptor']) is str,'actual live method')
        identity=(c['class'],c['method'],c['descriptor']);require(identity not in seen,'unique live method');seen.add(identity)
        m=[m for m in pins[c['class']]['methods'] if (m['name'],m['descriptor'])==identity[1:]]
        require(len(m)==1 and integer(c['bytes'],1) and c['bytes']==m[0]['code_length'] and c['sha256']==m[0]['code_sha256'],'actual pinned live Code')

def random_state(value):
    require(type(value) is list and len(value)==2,'Actor and World Random')
    for state in value:
        require(type(state) is list and len(state)==3 and word(state[0],16) and type(state[1]) is bool and word(state[2],16),'exact seed/Gaussian flag/cached word')

def validate_canonical(value,directory=HERE):
    directory=Path(directory)
    keys(value,('schema','rows','live_code'),'closed canonical finite schema')
    require(value['schema']=='BED_NIGHT_COMPONENT_FINITE_V1' and type(value['rows']) is list and len(value['rows'])==76,'all selected original components')
    pins=load(directory/'source-pins.json')['classes'];pinned=dict(pins)
    pinned.update(load(directory/'helper-pins.json')['classes'])
    code_records(value['live_code'],pins)
    declared=profiles(directory);call_count=event_count=0;faults={0:('sn','e',86),73:('gs','a',48),74:('gs','a',187),75:('sn','e',86)}
    for i,(row,profile) in enumerate(zip(value['rows'],declared)):
        keys(row,('case','name','input','result','exception','before','after','unselected_actor_preserved','unselected_world_preserved','random_before','random_after','journal'),'closed component row')
        require(integer(row['case']) and row['case']==i and row['name']==profile['name'] and exact(row['input'],profile['values']),'typed original profile order')
        p=row['input'];require(row['exception']==('java/lang/NullPointerException' if expected_fault(p) else 'NONE'),'declared source completion')
        if p[1] or expected_fault(p):require(row['result'] is None,'void or source-failed result')
        else:require(exact(row['result'],{'name':'OK','ordinal':0,'current_a':True}),'actual CW.a identity/name/ordinal')
        require(row['unselected_actor_preserved'] is True and row['unselected_world_preserved'] is True,'within-call preservation attestations')
        random_state(row['random_before']);random_state(row['random_after']);require(exact(row['random_before'],row['random_after']),'actual Random preservation')
        for phase in ('before','after'):
            selected(row[phase]);s=row[phase]
            require(s['world_players']==s['world_entities']==(2 if p[1]==0 and p[4]==1 else 1) and s['private_bN_same'] is True,'genuinely published graph and unreached private coordinate')
        j=row['journal'];audit_journal(j,pinned)
        require(j['case']==i and j['cw_before_initialized'] is (i>1) and j['cw_after_initialized'] is (i>0),'observed lazy enum initialization')
        root=j['calls'][0];require(root['site']['class']=='gs' and root['site']['method']==('b' if p[1]==0 else 'a') and root['site']['descriptor']==('(III)Lcw;' if p[1]==0 else '(ZZZ)V') and root['receiver']=={'class':'dc','identity':'actor'},'actual selected root')
        args=[8,64,8] if p[1]==0 else [bool(p[3]&4),bool(p[3]&2),bool(p[3]&1)]
        require(exact(root['arguments'],args) and root['completed'] is (not expected_fault(p)),'actual root operands/completion')
        reached=[e for e in j['events'] if e['kind']=='EXCEPTION']
        if i in faults:
            fault=reached[0]['site'];require((fault['class'],fault['method'],fault['pc'])==faults[i],'exact original failure prefix site')
        effect_order({'input':p,'before':{'selected':row['before']},'after':{'selected':row['after']}},j)
        call_count+=len(j['calls']);event_count+=len(j['events'])
    require(call_count==1313 and event_count==3979,'exact selected calls and total source event order')
    return value
