"""Authored data-only validation; no VM, raw-tree or archive dependency."""
import hashlib,json,re
from pathlib import Path

def require(value,message):
    if not value:raise ValueError(message)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def pairs(items):
    result={}
    for key,value in items:require(key not in result,'duplicate JSON key');result[key]=value
    return result
def loads(text):
    def invalid(x):raise ValueError('nonfinite JSON token '+x)
    return json.loads(text,object_pairs_hook=pairs,parse_constant=invalid)
def load(path):return loads(Path(path).read_text(encoding='utf-8'))
def exact(a,b):return type(a) is type(b) and (a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a) if type(a) is dict else len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b)) if type(a) is list else a==b)
def keys(value,names,label):require(type(value) is dict and set(value)==set(names),label)
def clean(value):
    if type(value) is dict:return {k:clean(v) for k,v in value.items() if k!='raw_object_id'}
    if type(value) is list:return [clean(v) for v in value]
    return value
def inventory(directory):return {p.relative_to(directory).as_posix():digest(p) for p in sorted(directory.rglob('*')) if p.is_file()}
def preservation(raw):
    a,b=raw['before'],raw['after']
    allowed={'sn.aM','sn.aN','sn.aO','sn.aP','sn.aQ','sn.aR','sn.bg','sn.bh','sn.bf','gs.u','gs.a','gs.v','gs.w','gs.y','gs.b'}
    require(a['actor_fields'].keys()==b['actor_fields'].keys(),'same complete Actor field inventory')
    changed={k for k in a['actor_fields'] if not exact(a['actor_fields'][k],b['actor_fields'][k])}
    require(changed<=allowed,'unselected Actor mutation '+str(changed-allowed))
    changed={k for k in a['world_fields'] if not exact(a['world_fields'][k],b['world_fields'][k])}
    require(changed<={'fd.J'} and exact(a['provider_fields'],b['provider_fields']),'unselected World/provider mutation')
    require(exact(a['random'],b['random']),'within-call actual Random preservation')
    for phase in (a,b):
        require(phase['selected']['world_players']==phase['selected']['world_entities']==1,'genuine published World membership')
        require(phase['selected']['player_list_members']==[{'class':'dc','actor':True}] and phase['selected']['player_list_same'] is True,'actual captured List identity/membership')
        require(phase['selected']['client_null'] is True and phase['selected']['bed_static_same'] is True,'actual NULL client and final static identity')
    require(raw['worker_alive'] is True and raw['worker_returned'] is False,'unreturned genuine worker')
def validate_journal(journal,profile,pinned):
    p=profile['values'];number=p[0]
    require(journal['case']==number and journal['root_returned'] is False and journal['terminal_resumed'] is False,'actual terminal prefix identity')
    worker=journal['source_worker']
    require(worker['name']=='original-gs-parent-'+str(number) and worker['class']=='PlayerSleepParentObservation$SourceWorker' and worker['daemon'] is True and worker['suspended'] is True and worker['suspend_count']>=1,'actual blocked worker identity')
    require(journal['source_receiver']['class']=='dc' and journal['source_receiver']['identity']=='actor','terminal actual source receiver')
    calls=journal['calls'];events=journal['events']
    require(0<len(calls)<=512 and 0<len(events)<=4096,'bounded actual selected trace')
    require([e['tick'] for e in events]==list(range(len(events))),'total source-event order')
    root=calls[0];require(root['site']=={'class':'gs','method':'w_','descriptor':'()V','pc':0} and root['caller']['class']=='PlayerSleepParentObservation$SourceWorker' and root['caller']['method']=='run' and root['receiver']['identity']=='actor' and root['arguments']==[] and root['completed'] is False,'ordinary genuine root and helper caller')
    require(journal['stack'][-1]['class']=='PlayerSleepParentObservation$SourceWorker' and any(s['class']=='gs' and s['method']=='w_' for s in journal['stack']),'actual original blocked stack')
    reached={c['site']['class']+'.'+c['site']['method']+c['site']['descriptor'] for c in calls}
    require('ls.w_()V' not in reached and 'dc.o()V' not in reached,'continuation111 never entered')
    expected_fault=None
    if p[5] and p[7]&2:expected_fault=('gs','w_',36)
    elif p[5] and not p[4] and p[7]&1:expected_fault=('gs','am',8)
    elif p[5] and not p[4] and p[2] and not p[3] and p[7]&4:expected_fault=('sn','e',86)
    terminal=journal['terminal_site']
    if expected_fault:
        require(journal['outcome']=='SOURCE_EXCEPTION' and journal['exception']=='java/lang/NullPointerException' and (terminal['class'],terminal['method'],terminal['pc'])==expected_fault,'actual declared first null prefix')
        require(events[-1]['kind']=='EXCEPTION' and events[-1]['site']==terminal,'exception is actual last source event')
    else:
        require(journal['outcome']=='PREFIX111' and journal['exception'] is None and terminal=={'class':'gs','method':'w_','descriptor':'()V','pc':111},'actual continuation111')
        require(events[-1]=={'kind':'PREFIX','site':terminal,'tick':len(events)-1},'actual final prefix event')
        require(all(c['completed'] is True for c in calls[1:]),'all reached children returned before111')
    wakes=[c for c in calls if c['site']['class']=='gs' and c['site']['method']=='a' and c['site']['descriptor']=='(ZZZ)V']
    should_wake=bool(p[5] and not p[4] and not(p[7]&3) and (p[3] or p[2]))
    require(len(wakes)==int(should_wake),'one reached actual parent wake or ordinary bypass')
    if wakes:
        missing=bool(p[3]);wake=wakes[0]
        require(wake['caller']=={'class':'gs','method':'w_','descriptor':'()V','pc':53 if missing else 73} and exact(wake['arguments'],[missing,True,not missing]),'actual reached caller and exact wake operands')
        require(wake['completed'] is (expected_fault is None),'actual reached wake return/failure')
        if missing:require('fd.f()Z' not in reached,'missing-Bed arm precedes daytime')
    if p[4] and p[5]:require('gs.am()Z' not in reached and not wakes,'actual remote bypass')
    for call in calls:
        site=call['site'];owner=site['class'];require(owner in pinned,'actual pinned call owner '+owner)
        require(any(m['name']==site['method'] and m['descriptor']==site['descriptor'] for m in pinned[owner]['methods']),'actual pinned method identity')
    return clean(journal)

STATE_KEYS=('position', 'motion', 'dimensions', 'sleep_offsets', 'sleeping', 'timer', 'v', 'Q', 'private_bN', 'bounds', 'old_bounds', 'world_J', 'remote', 'world_players', 'world_entities', 'other_sleeping', 'world_same', 'bounds_same', 'v_same', 'Q_same', 'Q_v_same', 'Q_before_v_same', 'private_bN_same', 'head', 'planes', 'sky_darkness', 'bed_static_id', 'bed_static_same', 'client_null', 'player_list_same', 'player_list_members')
def integer(value,low=-(2**31),high=2**31-1):return type(value) is int and low<=value<=high
def word(value,length):return type(value) is str and re.fullmatch('[0-9a-f]{'+str(length)+'}',value) is not None
def words(value,count,length):require(type(value) is list and len(value)==count and all(word(v,length) for v in value),'raw source floating words')
def state(value):
    keys(value,STATE_KEYS,'closed actual selected state')
    for name in ('position','motion'):words(value[name],3,16)
    words(value['dimensions'],3,8);words(value['sleep_offsets'],2,8)
    for name in ('bounds','old_bounds'):
        if value[name] is not None:words(value[name],6,16)
    for name in ('v','Q','private_bN'):
        x=value[name];require(x is None or type(x) is list and len(x)==3 and all(integer(i) for i in x),'actual nullable coordinate')
    for name in ('sleeping','world_J','remote','world_same','bounds_same','v_same','Q_same','Q_v_same','Q_before_v_same','private_bN_same','bed_static_same','client_null','player_list_same'):require(type(value[name]) is bool,'typed actual source Boolean')
    require(integer(value['timer']) and integer(value['sky_darkness']) and integer(value['bed_static_id']) and integer(value['world_players'],1,1) and integer(value['world_entities'],1,1),'actual source integers and publication counts')
    require(value['other_sleeping'] is None,'single actual Player matrix')
    require(type(value['head']) is list and len(value['head'])==2 and integer(value['head'][0],0,255) and integer(value['head'][1],0,15),'actual head ID/nibble')
    keys(value['planes'],('blocks','metadata','sky','block_light','height'),'all complete plane hashes')
    require(all(word(s,64) for s in value['planes'].values()),'actual SHA256 plane words')
    require(value['player_list_members']==[{'class':'dc','actor':True}],'actual published class/membership')
def profiles(directory):
    directory=Path(directory)
    value=load(directory/'profiles.json')
    lines=(directory/'profiles.tsv').read_text(encoding='ascii').splitlines()
    require(type(value) is list and len(value)==len(lines)==36,'all36 declared profiles')
    for number,(row,line) in enumerate(zip(value,lines)):
        keys(row,('name','values'),'closed prepared profile')
        p=row['values']
        require(type(row['name']) is str and row['name'] and type(p) is list and len(p)==8 and all(integer(n) for n in p),'typed prepared inputs')
        require(p[0]==number and 0<=p[1]<4 and all(n in (0,1) for n in p[2:6]) and 0<=p[7]<=7 and line==' '.join(map(str,p)),'profile domain/order/TSV')
    return value

def validate_canonical(value,directory):
    keys(value,('schema','rows','live_code','qualification'),'closed original finite')
    require(value['schema']=='PLAYER_SLEEP_PARENT_PREFIX_FINITE_V1','actual unchanged finite schema')
    require(exact(value['qualification'],{'whole_parent_return':False,'source_boundary':111,'source_frames_resumed':False,'gui':False,'native_implementation':False,'null_final_T_excluded':True}),'no inferred continuation qualification')
    prepared=profiles(directory)
    require(type(value['rows']) is list and len(value['rows'])==36,'complete original rows')
    pinned=dict(load(Path(directory)/'source-pins.json')['classes'])
    pinned.update(load(Path(directory)/'helper-pins.json')['classes'])
    calls=events=prefixes=faults=0
    for number,(row,profile) in enumerate(zip(value['rows'],prepared)):
        keys(row,('case','name','input','before','after','random_before','random_after','unselected_actor_preserved','unselected_world_preserved','journal'),'closed original selected row')
        require(integer(row['case'],0,35) and row['case']==number and row['name']==profile['name'] and exact(row['input'],profile['values']),'actual profile identity')
        require(row['unselected_actor_preserved'] is True and row['unselected_world_preserved'] is True,'within-call original preservation')
        state(row['before']);state(row['after'])
        require(exact(row['random_before'],row['random_after']),'original Random preservation only')
        validate_journal(row['journal'],profile,pinned)
        prefixes+=row['journal']['outcome']=='PREFIX111'
        faults+=row['journal']['outcome']=='SOURCE_EXCEPTION'
        calls+=len(row['journal']['calls']);events+=len(row['journal']['events'])
    require((prefixes,faults,calls,events)==(32,4,1085,3359),'exact actual source trace extent')
    require(type(value['live_code']) is list and len(value['live_code'])==39,'all39 live Code identities')
    seen=set()
    for record in value['live_code']:
        keys(record,('event','class','method','descriptor','bytes','sha256'),'closed actual live Code record')
        identity=(record['class'],record['method'],record['descriptor'])
        require(identity not in seen,'distinct live Code methods');seen.add(identity)
        methods=[m for m in pinned[record['class']]['methods'] if m['name']==record['method'] and m['descriptor']==record['descriptor']]
        require(record['event']=='code' and len(methods)==1 and methods[0]['code_length']==record['bytes'] and methods[0]['code_sha256']==record['sha256'],'actual pinned live Code')
    return value
