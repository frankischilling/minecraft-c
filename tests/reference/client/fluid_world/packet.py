"""Validate exact fluid source records, input cells and actual read-only effects."""
import collections
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
FROZEN = {'FluidObservation.java': '697f93c0b3c0deca0a5bdcdd3721e743c5147015e7b6f6e89fb1471e187a2b25', 'FluidJdi.java': 'ad8f1d481ed7e523949c877c843f85de59e906e66c75a6e83952560eaecba151', 'profiles.txt': '64aa19377f56891512c0192891a6f2bfabc4b5625e19c3dd282abdf1c7d5f5aa', 'profiles-0.txt': '6b2e763b4bcf63fc1dbea40dedf473fe40c946411d7a7165253d805add0de4c4', 'profiles-1.txt': '4ce9187a699a97f10c392944385c3035ba187de6c10496b1044706eb7cb81105', 'profiles-2.txt': '5f557347ef3103aaba81c2cd1f7cae7cb200f901efe6e91f2bc15c472598235c', 'declared-inputs.json': '0ac7b509ec1e83ad10ecf5eeafe7ac6f002c0bdf9eab7a0a77ed95d69d8159cf', 'source-pins.json': 'b411fa6170b9521a51c16fafb684b3a879e8b96a149d26120f4426df4b170007', 'capture_reference.py': 'e8c1e7a244d4812a87d9dd4d5b6236dbb5402639be855d7338d416d683e02eeb'}
CAPTURED = {'selected-0.txt': 'd5a4a7e2cbd2de801d78c86610e277141300a414696c6301ccd3b009cc395b7b', 'events-0.jsonl': 'd5e82f9c5e851b240a2064d83947def66236ae3cacc4da68390f029f4eebff3b', 'live-code-0.tsv': 'f64dc2c90f14480143a18ff3582ac3d4c9316d1d73804413c6201d6fe0724caa', 'selected-1.txt': '4d8b46aa88a0607c3e472f8c347adf11976fb8e7efb201123761c3533a047113', 'events-1.jsonl': 'ceecaca8dea3cca1de15b6d220fdbbcf6d7a741b5af96b50bcfbfaac2c69dce3', 'live-code-1.tsv': 'f64dc2c90f14480143a18ff3582ac3d4c9316d1d73804413c6201d6fe0724caa', 'selected-2.txt': 'ea94215fb330ad3059f4ab4770833f710057624f7b29ef59ff6580ba0ccd543a', 'events-2.jsonl': '4b4ba5ee5de7ddc9cf5c37aa154ed706c5af6974b177e67dd35a27d138352cc4', 'live-code-2.tsv': 'a109d73837c2acb9955a78f868a06b78c7b6a9de76829a6fc7ee2bf31cfddae9'}
NW_KEYS = set('A B h i j k l m n o p q r s t u v w x z'.split())
CV_KEYS = set('A B C D E F G H I J K L M N O P Q R S T U V W X Y Z aa ab ac ad ae af ag ah ai aj ak al am b d e f g h i j k l m n o p q r s t u v w x y z'.split())
COHORTS = ((0, 35), (35, 70), (70, 104))
XYZ = ((8, 64, 8), (8, 64, 8), (3, 65, 3))
TRANSLATIONS = ((-.25, .5, -.75), (-8, -64, -8), (0, -64, 0))
BOXES = ((0, 0, 0, 1, 1, 1), (.125, .25, .375, .875, .75, .625))


def require(value, label):
    if not value:
        raise ValueError(label)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for name, value in pairs:
        require(name not in result, 'duplicate JSON field: ' + name)
        result[name] = value
    return result


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def lines(path):
    raw = Path(path).read_bytes()
    require(raw.endswith(b'\n') and b'\r' not in raw, 'ASCII LF transport')
    return raw.decode('ascii').splitlines()


def integer(text):
    require(type(text) is str and re.fullmatch(r'-?(?:0|[1-9][0-9]*)', text), 'typed decimal integer')
    value = int(text)
    require(-2**31 <= value < 2**31, 'Java int range')
    return value


def primitive(text):
    out = {}
    for part in text.split(','):
        name, value = part.split('=', 1)
        require(name not in out, 'unique complete primitive fields')
        out[name] = value
    return out


def dword(value):
    return struct.pack('>d', value).hex()


def fword(value):
    return struct.pack('>f', value).hex()


def double(word):
    require(type(word) is str and re.fullmatch('[0-9a-f]{16}', word), 'raw binary64')
    return struct.unpack('>d', bytes.fromhex(word))[0]


def raw_nw(state):
    require(set(state) == NW_KEYS, 'complete NW primitive state')
    for name in 'i j s t u'.split():
        double(state[name])
    for name in 'l m n q w x'.split():
        require(type(state[name]) is bool, 'typed NW boolean')
    for name in NW_KEYS - set('i j s t u l m n q w x'.split()):
        require(type(state[name]) is int, 'typed NW integer')
    require(state['B'] == 2097152 and state['r'] == 7 and state['w'] is True, 'genuine NW backing and one begun batch')
    return {k: str(v).lower() if type(v) is bool else str(v) for k, v in state.items()}


def profiles(directory=HERE):
    out = []
    for text in lines(Path(directory) / 'profiles.txt'):
        row = text.split(' ')
        require(len(row) == 14 and re.fullmatch('[a-z0-9-]+', row[0]), 'name plus thirteen integer operands')
        p = list(map(integer, row[1:]))
        require(p[0] in (8, 9, 10, 11) and 0 <= p[1] < 16 and 0 <= p[2] <= 15 and
                p[3] in range(3) and p[4] in (0, 1) and p[7] in (0, 1) and p[8] in range(3) and
                p[9] in (0, 1) and p[10] in range(3) and p[11:] == [0, 0], 'closed declared input domain')
        out.append({'index': len(out), 'name': row[0], 'values': p})
    require(len(out) == 104 and len({p['name'] for p in out}) == 104, '104 unique profiles')
    for c, (a, b) in enumerate(COHORTS):
        require(lines(Path(directory) / ('profiles-'+str(c)+'.txt')) == lines(Path(directory) / 'profiles.txt')[a:b], 'exact frozen cohort boundaries')
    return out


def expected_cells(p):
    x, y, z = XYZ[p[10]]
    cells = {(x+dx, y+dy, z+dz): [0, 0, 15 if p[3] == 1 else 0, 15 if p[3] == 2 else 0]
             for dx in range(-2, 3) for dy in range(-2, 3) for dz in range(-2, 3)}
    def put(dx, dy, dz, block, meta=0):
        cells[x+dx, y+dy, z+dz][:2] = [block, meta]
    block, meta, layout = p[:3]
    if layout in range(1, 8):
        for dx in range(-1, 2):
            for dy in range(-1, 2):
                for dz in range(-1, 2):
                    put(dx, dy, dz, block, meta)
        if layout > 1:
            for a in range(-1, 2):
                for b in range(-1, 2):
                    put(a, 1 if layout == 2 else -1, b, 0) if layout < 4 else (
                        put(a, b, -1 if layout == 4 else 1, 0) if layout < 6 else
                        put(-1 if layout == 6 else 1, b, a, 0))
    elif layout not in (10, 13):
        for dx in range(-1, 2):
            for dz in range(-1, 2):
                put(dx, 0, dz, block, meta)
    put(0, 0, 0, block, meta)
    extras = {
        8: [(-1, 0, -1, block, 0), (-1, 0, 0, block, 1), (0, 0, -1, block, 7), (1, 0, 1, block, 8)],
        9: [(-1, 1, -1, block, meta)], 10: [(0, -1, 0, 12, 0)],
        11: [(-1, 0, 0, 79, 0), (1, 0, 0, 0, 0)],
        12: [(-1, 0, 0, block, 1), (1, 0, 0, block, 7)],
        13: [(1, -1, 0, block, 2)], 14: [(0, 0, -1, 1, 0), (0, -1, -1, 1, 0)],
        15: [(1, 0, 0, block, 3), (0, 0, 1, block, 6), (0, 1, -1, 1, 0)]}
    for cell in extras.get(layout, []):
        put(*cell)
    return cells


def pool_state(state):
    require(set(state) == {'cursor', 'size', 'class', 'entries'} and
            type(state['cursor']) is int and type(state['size']) is int and
            0 <= state['cursor'] <= state['size'] and state['class'] == 'java.util.ArrayList' and
            len(state['entries']) == state['size'], 'actual typed Vector pool state')
    identities = []
    for entry in state['entries']:
        require(type(entry) is list and len(entry) == 4 and re.fullmatch('v[0-9]+', entry[0]), 'actual Vector identity and components')
        for value in entry[1:]:
            double(value)
        identities.append(entry[0])
    require(len(set(identities)) == len(identities), 'distinct original pool allocations')
    return state


def validate_rows(selected, selected_profiles, cohort):
    registry, light, static = {}, None, None
    records = collections.defaultdict(dict)
    guards = []
    for text in selected:
        row = text.split('\t')
        tag = row[0]
        if tag == 'FLUID_REGISTRY':
            require(len(row) == 9, 'complete registry record')
            id = integer(row[1]);require(id not in registry, 'unique registry slot')
            registry[id] = row
        elif tag == 'FLUID_LIGHT':
            require(light is None and len(row) == 4 and row[1] == '3', 'actual FD.f and Dimension light table')
            light = row[3].split(',');require(len(light) == 16 and all(re.fullmatch('[0-9a-f]{8}', v) for v in light), 'sixteen actual float light entries')
        elif tag == 'FLUID_STATIC':
            require(static is None and len(row) == 5 and row[1:4] == ['cohort='+str(cohort), 'NW.b=true', 'NW.c=false'] and row[4].startswith('pool='), 'actual NW static flags')
            static = pool_state(json.loads(row[4][5:], object_pairs_hook=unique))
        elif tag in ('FLUID', 'FLUID_INPUT', 'FLUID_CELLS', 'FLUID_POOL', 'FLUID_BACKING'):
            index = integer(row[1]);require(tag not in records[index], 'one actual row per role/profile')
            records[index][tag] = row
        else:
            require(text.startswith(('FLUID_GUARD ', 'FLUID_COMPLETE ')), 'known selected source record')
            guards.append(text)
    require(set(registry) == {0, 1, 8, 9, 10, 11, 12, 79} and light is not None and static is not None,
            'complete actual registered input identities')
    require(set(records) == {p['index'] for p in selected_profiles}, 'complete cohort observation indices')
    out, end, untouched_total, buffer_state, previous_pool = [], 0, 0, None, static
    for profile in selected_profiles:
        index, p = profile['index'], profile['values'];record = records[index]
        require(set(record) == {'FLUID', 'FLUID_INPUT', 'FLUID_CELLS', 'FLUID_POOL', 'FLUID_BACKING'}, 'complete source role rows')
        inp, row, cells, pool, backing = [record[k] for k in ('FLUID_INPUT','FLUID','FLUID_CELLS','FLUID_POOL','FLUID_BACKING')]
        reg = registry[p[0]];original_bm = integer(reg[4]);entered_bm = (original_bm+p[6]+2**31) % 2**32-2**31
        require(len(inp) == 9 and inp == ['FLUID_INPUT', str(index), reg[2], str(p[0]), str(original_bm), str(entered_bm), ','.join(map(str,p)), ','.join(map(str,XYZ[p[10]])), reg[7]], 'actual registered inputs, pass, placement and thirteen operands')
        require(reg[2] == ('om' if p[0] % 2 == 0 else 'nx') and reg[3] == str(p[0]) and reg[6] == '4' and reg[7] == ('1' if p[0] < 10 else '0') and reg[8] == 'false', 'actual RP registration/type/pass/opacity')
        require(len(row) == 16 and row[:3] == ['FLUID',str(index),profile['name']], 'sixteen-field whole fluid row')
        failed, hidden = p[8] != 0, p[2] == 1
        require(row[3:5] == ['UNRETURNED' if failed else 'false' if hidden else 'true','NULL' if failed else 'NONE'], 'whole-method return and reached null prefix')
        before, after, capacity = map(integer,row[5:8]);delta = after-before
        require(before == end and capacity == 2097152 and delta >= 0 and delta % 48 == 0 and delta <= 288, 'one uninterrupted bounded whole-quad batch')
        if failed or hidden:require(delta == 0,'culled/null produces no vertices')
        if profile['name'].startswith('base-'):require(delta == 96,'homogeneous exposed top and bottom')
        if profile['name'].startswith('only-'):require(delta == 48,'one isolated face')
        if profile['name'] == 'live-bm-override-bounds-success':require(delta == 288,'renderAll emits six visible-branch faces')
        require(row[8] in ('0','1') and row[9].split(',') == [dword(v) for v in TRANSLATIONS[p[10]]], 'actual LM flag and declared genuine translation setter')
        bounds = [dword(v) for v in BOXES[p[7]]];require(row[10].split(',') == bounds,'declared raw input bounds')
        expected_bounds = bounds.copy()
        if not failed and not hidden:expected_bounds[1],expected_bounds[4] = dword(0),dword(1)
        require(row[11].split(',') == expected_bounds,'actual bounds0/1 success stores and failure retention')
        cv, old, new = map(primitive,row[12:15]);require(set(cv) == CV_KEYS and set(old) == set(new) == NW_KEYS,'all CV/NW primitive fields')
        require(cv['f'] == str(bool(p[4])).lower() and cv['d'] == str(p[5]),'actual renderAll/override inputs')
        if p[9]:require(cv['m'] == cv['e'] == 'true' and all(cv[n] == '2' for n in 'ghijkl'),'authored retained CV scratch')
        else:require(cv['m'] == cv['e'] == 'false' and all(cv[n] == '0' for n in 'ghijkl'),'genuine default CV scratch')
        require(old['o'] == str(before) and new['o'] == str(after) and int(new['h'])-int(old['h']) == delta//8 and int(new['p'])-int(old['p']) == delta//12,'source input/packed vertex counts')
        if failed or hidden:require(old == new,'zero-write branch retains complete NW primitive state')
        words = row[15].split(',');require(len(words) == max(delta,100) and all(re.fullmatch('[0-9a-f]{8}',w) for w in words),'every active word and observed own inactive tail')
        require(len(cells) == 4,'actual finite cell inputs and full-array hashes')
        actual_cells = {}
        for item in cells[2].split(';'):
            v = list(map(integer,item.split(',')));require(len(v) == 7 and tuple(v[:3]) not in actual_cells,'typed unique actual input cell')
            actual_cells[tuple(v[:3])] = v[3:]
        require(len(actual_cells) == 125 and actual_cells == expected_cells(p),'actual entered cells match declared finite layout')
        require(len(cells[3].split(';')) == 4 and all(len(v.split(',')) == 4 and all(-2**31 <= integer(q) < 2**31 for q in v.split(',')) for v in cells[3].split(';')),'actual four Chunk Block/meta/sky/block array fingerprints')
        require(len(pool) == 4,'actual before/after pool observations')
        pool_before,pool_after = [pool_state(json.loads(v,object_pairs_hook=unique)) for v in pool[2:]]
        require(pool_before == previous_pool and pool_after['cursor'] >= pool_before['cursor'] and pool_after['size'] >= pool_before['size'] and pool_before['entries'][:pool_before['cursor']] == pool_after['entries'][:pool_before['cursor']],'retained actual pool history and completed old entries')
        if failed or hidden:require(pool_before == pool_after,'early branch bypasses genuine Vector pool')
        require(len(backing) == 6 and backing[:3] == ['FLUID_BACKING',str(index),str(capacity-delta)],'complete own-prior inactive backing audit')
        integer(backing[3]);integer(backing[4])
        if not delta:require(backing[3] == backing[4],'zero-write full backing unchanged')
        if buffer_state is None:buffer_state = backing[5]
        require(backing[5] == buffer_state,'actual NIO identities/cursors retained')
        out.append({'profile':profile,'raw':'\t'.join(row),'input':'\t'.join(inp),'cells_raw':'\t'.join(cells),'cells':actual_cells,'registry':registry,'lights':light,'before':old,'after':new,'words':words,'delta':delta,'return':None if failed else not hidden,'pool_before':pool_before,'pool_after':pool_after})
        end,previous_pool = after,pool_after;untouched_total += capacity-delta
    require(guards == ['FLUID_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=3','FLUID_GUARD controls network=2 native=1 denied-writes=1 execs=1','FLUID_COMPLETE cohort='+str(cohort)+' rows='+str(len(out))+' first='+str(selected_profiles[0]['index'])+' own-unchanged-words='+str(untouched_total)+' genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0'],'complete actual calls, noGL/noRAW/own-backing and denial guards')
    return out


def validate_events(events,observed,pins,live):
    methods={o+'.'+m['name']+m['descriptor']:m for o,c in pins['classes'].items() for m in c['methods']}
    live_methods={}
    for line in live:
        p=line.split('\t');require(len(p) in (5,6),'typed live method record');tag,owner,name,desc=p[:4];key=owner+'.'+name+desc
        require(key in methods and key not in live_methods,'unique pinned live original/JDK method');m=methods[key]
        if tag=='FW_LIVE_CODE':require(len(p)==6 and m.get('code_bytes')==integer(p[4]) and m.get('code_sha256')==p[5],'actual live Code fingerprint')
        else:require(tag=='FW_LIVE_NATIVE' and len(p)==5 and m['access']==integer(p[4]) and m['access']&256 and 'code_bytes' not in m,'actual native JDK flags without invented Code')
        live_methods[key]=m
    rows={r['profile']['index']:r for r in observed};stack=[];entered={};current_profile=None;next_call=0;ended=set();counts=collections.Counter();stores=collections.defaultdict(list);source_vertices=collections.defaultdict(list);height_queries=collections.Counter();flow_neighbors=collections.Counter();flow_last={};side_lights=collections.Counter()
    def at(location):
        key,pc=location.rsplit('@',1);pc=int(pc);require(key in live_methods and (-1==pc if live_methods[key]['access']&256 else 0<=pc<live_methods[key]['code_bytes']),'actual live method/PC');return key,pc
    for seq,event in enumerate(events):
        require(type(event['seq']) is int and event['seq']==seq and type(event['profile']) is int and event['profile'] in rows,'contiguous typed actual event sequence')
        pidx=event['profile'];row=rows[pidx];p=row['profile']['values'];kind=event['event']
        if kind=='profile-start':
            require(current_profile is None and not stack and pidx not in ended,'ordered observer profile boundary');current_profile=pidx
            require(event['pool']==row['pool_before'] and raw_nw(event['nw'])==row['before'],'actual entered pool/NW state');continue
        require(current_profile==pidx,'no cross-profile source events')
        if kind=='profile-end':
            require(not stack and event['pool']==row['pool_after'] and raw_nw(event['nw'])==row['after'],'complete returned/failed profile pool/NW state');ended.add(pidx);current_profile=None;continue
        if kind=='entry':
            d=event['data'];key,pc=at(d['at']);require(type(d['call']) is int and d['call']==next_call and pc in (0,-1),'ordered actual method entry');next_call+=1;counts[key]+=1
            native=bool(live_methods[key]['access']&256);require(type(d['native']) is bool and d['native']==native,'typed actual native method flag')
            if native:require(d['args'] is None and type(d.get('caller_args')) is list and (not stack or d['caller_args']==entered[stack[-1]][0]['args']),'native arguments unavailable; actual caller operands retained')
            else:require(type(d['args']) is list,'typed source arguments')
            if key=='cv.k(Luu;III)Z':require(not stack and d['args']==[None if p[8]==2 else 'block:'+str(p[0]),*XYZ[p[10]]] and raw_nw(d['nw'])==row['before'],'actual whole CV.k root/arguments/state')
            parent,parent_key=entered[stack[-1]] if stack else (None,None)
            caller_key,caller_pc=d['caller'].rsplit('@',1);caller_pc=int(caller_pc)
            if key=='cv.a(IIILln;)F':
                base=XYZ[p[10]];dx,dz={268:(0,0),282:(0,1),298:(1,1),312:(1,0)}.get(caller_pc,(99,99))
                require(caller_key=='cv.k(Luu;III)Z' and d['args']==[base[0]+dx,base[1],base[2]+dz,row['registry'][p[0]][5]],'ordered corner-height caller/arguments')
            if key=='rp.c(Lxp;III)I' and parent_key=='rp.e(Lxp;III)Lbt;':
                base=parent['args'][1:4]
                if caller_pc==14:expected=base
                elif caller_pc==81:
                    n=flow_neighbors[parent['call']];require(n<4,'bounded four-neighbor flow loop');dx,dz=((-1,0),(0,-1),(1,0),(0,1))[n];expected=[base[0]+dx,base[1],base[2]+dz];flow_neighbors[parent['call']]+=1;flow_last[parent['call']]=expected
                else:
                    require(caller_pc==119 and parent['call'] in flow_last,'reached below-neighbor flow contribution');last=flow_last[parent['call']];expected=[last[0],last[1]-1,last[2]]
                require(d['args']==['ref:observedCache',*expected],'ordered source flow-decay coordinates')
            if key=='rp.d(Lxp;IIII)Z' and parent_key=='rp.e(Lxp;III)Lbt;':
                offsets={256:(0,0,-1,2),279:(0,0,1,3),302:(-1,0,0,4),325:(1,0,0,5),350:(0,1,-1,2),375:(0,1,1,3),400:(-1,1,0,4),425:(1,1,0,5)}
                require(caller_pc in offsets,'actual falling-wall source PC');dx,dy,dz,side=offsets[caller_pc];x,y,z=parent['args'][1:4];require(d['args']==['ref:observedCache',x+dx,y+dy,z+dz,side],'actual falling-wall coordinates/side')
            if key.startswith('ew.'):
                if parent_key=='cv.a(IIILln;)F':
                    x,y,z=parent['args'][:3];i=height_queries[parent['call']]
                    if caller_pc==48:require(i<4,'bounded original height samples');height_queries[parent['call']]+=1
                    else:i-=1
                    require(i>=0 and caller_pc in (48,70,94) and d['args'][:3]==[x-(i&1),y+(1 if caller_pc==48 else 0),z-((i>>1)&1)],'ordered source upper/same-level height query')
                elif parent_key in ('rp.b(Lxp;IIII)Z','rp.c(Lxp;III)I','rp.d(Lxp;IIII)Z','uu.b(Lxp;IIII)Z','uu.d(Lxp;IIII)Z'):
                    require(d['args'][:3]==parent['args'][1:4],'actual source Block/Material query coordinates')
                elif parent_key=='rp.d(Lxp;III)F':
                    x,y,z=parent['args'][1:4];require(caller_pc in (5,19) and d['args'][:3]==[x,y+(1 if caller_pc==19 else 0),z],'actual fluid brightness current/above query')
                elif parent_key and parent_key.startswith('ew.'):
                    require(d['args'][:3]==parent['args'][:3],'actual nested EW coordinate forwarding')
                elif parent_key=='cv.k(Luu;III)Z':
                    require(caller_pc==254 and d['args'][:3]==list(XYZ[p[10]]),'actual center metadata query')
                elif parent_key=='rp.e(Lxp;III)Lbt;':
                    require(caller_pc in (98,229),'actual flow direct query PC');expected=flow_last.get(parent['call']) if caller_pc==98 else parent['args'][1:4];require(d['args'][:3]==expected,'actual flow selected-neighbor/center query')
                else:require(False,'reviewed source query parent')
                require(d['receiver']=='ref:observedCache' and len(d['args'])>=3 and all(type(q) is int for q in d['args'][:3]) and tuple(d['args'][:3]) in row['cells'],'actual genuine EW query and finite coordinates')
            if key.startswith('nw.'):
                require(d['receiver']=='ref:observedTess' and 'nw' in d,'actual singleton NW and prefix')
                raw_nw(d['nw'])
                if key=='nw.a(DDD)V':
                    require(len(d['args'])==3 and d['nw']['l'] and d['nw']['m'] and not d['nw']['n'],'actual packed vertex flags/operands')
                    xyz=[fword(double(a)+double(d['nw'][n])) for a,n in zip(d['args'],'stu')];uv=[fword(double(d['nw'][n])) for n in 'ij'];source_vertices[pidx].append(xyz+uv+[f'{d["nw"]["k"]&0xffffffff:08x}'])
            if key.startswith('bt.'):
                require(type(d['pool']['cursor']) is int and type(d['pool']['size']) is int,'actual Vector pool state at call')
            entered[d['call']]=(d,key);stack.append(d['call'])
        elif kind=='return':
            d=event['data'];key,pc=at(d['at']);require(stack and d['call']==stack[-1],'properly nested actual source return');entry,method=entered[stack.pop()];require(method==key,'entered/returned source method identity');result=d['result']
            if key.startswith('ew.'):
                args=entry['args'];cell=row['cells'][tuple(args[:3])]
                if key=='ew.a(III)I':require(type(result) is int and result==cell[0],'actual Block query result')
                elif key=='ew.e(III)I':require(type(result) is int and result==cell[1],'actual nibble query result')
                elif key=='ew.f(III)Lln;':require(result==row['registry'][cell[0]][5],'actual Material query identity/result')
                elif key=='ew.g(III)Z':require(type(result) is bool and result==(row['registry'][cell[0]][8]=='true'),'actual opacity query result')
                elif key in ('ew.d(III)I','ew.a(IIIZ)I'):require(type(result) is int and result==max(cell[2]-3,cell[3],0),'actual saved sky/block light query result')
                elif key in ('ew.c(III)F','ew.a(IIII)F'):
                    level=max(cell[2]-3,cell[3],0);level=max(level,args[3]) if len(args)==4 else level;require(result==row['lights'][level],'actual Dimension light-table result')
                else:require(False,'unreviewed reached EW method')
            elif key=='nw.a(DD)V':require(result=='void' and d['nw']==dict(entry['nw'],i=entry['args'][0],j=entry['args'][1],m=True),'actual UV stores and retained NW state')
            elif key=='nw.a(DDD)V':
                n=3 if entry['nw']['p']%4==3 else 1;require(result=='void' and d['nw']['o']==entry['nw']['o']+8*n and d['nw']['h']==entry['nw']['h']+n and d['nw']['p']==entry['nw']['p']+1,'actual quad expansion/index stores')
            elif key=='cv.k(Luu;III)Z':require(not stack and result is row['return'] and raw_nw(d['nw'])==row['after'],'actual whole CV.k return/prefix')
        elif kind=='field-read':
            key,pc=at(event['at']);require(stack and event['field'] in ('nw.a','uu.B','uu.D','ln.g','ln.h','ln.s'),'actual selected static dependency read')
            if event['field']=='nw.a':require(event['value']=='ref:observedTess','actual singleton capture')
            if event['field'] in ('uu.B','uu.D'):require(event['value']=='block:'+('8' if event['field']=='uu.B' else '10'),'unchanged actual final flow Block')
        elif kind=='field-store':
            key,pc=at(event['at']);require(stack,'reached actual field store');stores[pidx].append(event)
            if event['field']=='bt.e':require(key=='bt.b(DDD)Lbt;' and pc==38 and type(event['before']) is int and event['after']==event['before']+1,'actual ordered Vector pool cursor increment')
            else:require(event['field'] in ('uu.bt','uu.bw') and key=='cv.k(Luu;III)Z' and pc==(1233 if event['field']=='uu.bt' else 1239) and event['receiver']=='block:'+str(p[0]) and event['after']==dword(0 if event['field']=='uu.bt' else 1),'actual successful bounds store/source PC')
        elif kind=='exception':
            key,pc=at(event['at']);require(stack and p[8] and event['type']=='java.lang.NullPointerException' and raw_nw(event['nw'])==row['after'] and event['pool']==row['pool_after'],'actual reached source NPE and prefix')
            require((key,pc)==(('cv.k(Luu;III)Z',14) if p[8]==2 else ('rp.b(Lxp;IIII)Z',5)),'actual null Block/cache failure PC')
            require(event['frames'][0]==event['at'] and any(f.startswith('cv.k(Luu;III)Z@') for f in event['frames']) and event['catch'].startswith('FluidObservation.main([Ljava/lang/String;)V@'),'actual source frames and authored catcher')
            stack.clear()
        else:require(False,'known actual event kind')
    require(not stack and current_profile is None and ended==set(rows) and counts['cv.k(Luu;III)Z']==len(rows),'complete source cohort event coverage')
    for index,row in rows.items():
        p=row['profile']['values'];bound_stores=[s for s in stores[index] if s['field'].startswith('uu.')];pool_stores=[s for s in stores[index] if s['field']=='bt.e']
        require(len(bound_stores)==(0 if row['return'] is not True else 2),'source bounds store reachability')
        require(len(pool_stores)==row['pool_after']['cursor']-row['pool_before']['cursor'],'actual pool progression equals source stores')
        vertices=source_vertices[index];require(len(vertices)==row['delta']//12,'actual source vertex count')
        active=[]
        for i in range(0,len(vertices),4):
            require(len(vertices[i:i+4])==4,'complete source quad');active.extend(w for q in (0,1,2,0,2,3) for w in vertices[i+q]+['00000000','00000000'])
        require(active==row['words'][:row['delta']],'all active raw packed words match actual source NW arguments/state')
    return {'events':len(events),'entries':next_call,'queries':sum(v for k,v in counts.items() if k.startswith('ew.')),'methods':len(live_methods),'counts':dict(counts)}


def read_packet(directory=HERE):
    directory=Path(directory);provenance=load(directory/'provenance.json')
    require(type(provenance['profiles']) is int and provenance['profiles']==104 and provenance['client_jar_sha256']==JAR,'exact original authority/profile count')
    require(provenance['original_inputs_unchanged'] is True and provenance['original_helpers_unchanged'] is True and provenance['source_writes_from_jdi']==0,'unchanged original/read-only observation')
    require(FROZEN and provenance['frozen_authored_files_sha256']==FROZEN and provenance['files_sha256']=={**FROZEN,**CAPTURED},'exact frozen source/product inventory')
    for name,pin in provenance['files_sha256'].items():require(digest(directory/name)==pin,'changed captured source/product: '+name)
    ps=profiles(directory);pins=load(directory/'source-pins.json');cohorts=[]
    for c,(a,b) in enumerate(COHORTS):
        observed=validate_rows(lines(directory/('selected-'+str(c)+'.txt')),ps[a:b],c)
        events=[json.loads(line,object_pairs_hook=unique) for line in lines(directory/('events-'+str(c)+'.jsonl'))]
        live=lines(directory/('live-code-'+str(c)+'.tsv'));metrics=validate_events(events,observed,pins,live)
        receipt=provenance['cohorts'][c]
        counts={'index':c,'profiles':b-a,'first_global_index':a,'ordinary_fresh_vms':2,
                'read_only_jdi_fresh_vms':1,'events':metrics['events'],'queries':metrics['queries'],
                'field_reads':sum(e['event']=='field-read' for e in events),
                'field_stores':sum(e['event']=='field-store' for e in events),
                'source_exceptions':sum(e['event']=='exception' for e in events),
                'live_methods':len(live)}
        require(all(type(receipt.get(k)) is int and receipt[k]==v for k,v in counts.items()),
                'typed complete original cohort receipt')
        require(type(receipt['stdout_sha256']) is list and len(receipt['stdout_sha256'])==3 and
                len(set(receipt['stdout_sha256']))==1 and
                re.fullmatch('[0-9a-f]{64}',receipt['stdout_sha256'][0]) and
                type(receipt['stdout_bytes']) is list and len(receipt['stdout_bytes'])==3 and
                all(type(n) is int and n>(directory/('selected-'+str(c)+'.txt')).stat().st_size
                    for n in receipt['stdout_bytes']) and len(set(receipt['stdout_bytes']))==1 and
                receipt['world_unchanged_except_source_session_lock'] is True,
                'three identical complete original streams and unchanged private World')
        cohorts.append({'index':c,'observed':observed,'events':events,'live':live,'metrics':metrics})
    totals={'total_events':sum(c['metrics']['events'] for c in cohorts),
            'total_query_entries':sum(c['metrics']['queries'] for c in cohorts),
            'total_source_exceptions':sum(e['event']=='exception' for c in cohorts for e in c['events']),
            'total_active_words':sum(r['delta'] for c in cohorts for r in c['observed']),
            'own_unchanged_words_each_complete_104_profile_set':
                sum(2097152-r['delta'] for c in cohorts for r in c['observed'])}
    require(all(type(provenance.get(k)) is int and provenance[k]==v for k,v in totals.items()),
            'typed complete original aggregate receipt')
    return {'cohorts':cohorts,'profiles':ps,'pins':pins,'provenance':provenance,'rows':[r['raw'] for c in cohorts for r in c['observed']],'inputs':[r['input'] for c in cohorts for r in c['observed']]}
