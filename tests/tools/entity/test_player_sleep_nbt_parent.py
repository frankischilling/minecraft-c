"""Compare genuine native NBT parents with the sealed forty-row original packet."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = next(p for p in Path(__file__).resolve().parents if
            (p / 'tests/reference/client/statistics/support.py').is_file())
PLANES = {'blocks': 32768, 'metadata': 16384, 'sky': 16384, 'block_light': 16384, 'height': 256}
STATE_KEYS = ('position','motion','history','base_flags','dimensions','angles','sleep_offsets',
    'living','dimension','sleeping','timer','score','v','Q','private_bN','bounds','old_bounds',
    'world_J','world_same','bounds_same','v_same','Q_same','Q_v_same','private_bN_same',
    'world_players','world_entities','player_list_same','player_list_members','client_null',
    'main_same','armor_same','inventory_selected','inventory_dirty','info_same',
    'info_player_present','info_player_input','main','armor','head','planes')
SNAPSHOT_KEYS = ('selected','actor_fields','world_fields','provider_fields','info_fields',
    'inventory_fields','old_slot_value_view','rooted_entry_coordinates','input_tree','random')
ROW_KEYS = ('case','name','input','entry','before','after','diagnostic','retired','cleanup')
DIAGNOSTIC_KEYS = ('nbt_status','nbt_code','field','base_pc','living_pc','player_pc','local_pc',
                   'owned_work','sleep','fd')
BOOL_FIELDS = ('sleeping','world_J','world_same','bounds_same','v_same','Q_same','Q_v_same',
    'private_bN_same','player_list_same','client_null','main_same','armor_same','inventory_dirty',
    'info_same','info_player_present','info_player_input')
AUTHORED_BEFORE = ('position','world_same','bounds_same','Q','private_bN','score','world_players',
    'world_entities','player_list_members','client_null','main_same','armor_same',
    'inventory_selected','inventory_dirty','info_same','info_player_present','info_player_input',
    'head','planes')
MAX_BYTES = 16 * 1024 * 1024
# NATIVE_SCHEMA_BEGIN
NATIVE_SCHEMA = {'actor': {'SN.id': 'integer', 'SN.private_b': 'integer', 'SN.bt': 'integer', 'SN.bu': 'integer', 'SN.bv': 'integer', 'SN.bw': 'integer', 'SN.by': 'integer', 'SN.bz': 'integer', 'SN.bG': 'integer', 'SN.bH': 'integer', 'SN.bI': 'integer', 'SN.client_only.bJ': 'integer', 'SN.client_only.bK': 'integer', 'SN.client_only.bL': 'integer', 'SN.aE': 'double', 'SN.aJ': 'double', 'SN.aK': 'double', 'SN.aL': 'double', 'SN.x': 'double', 'SN.y': 'double', 'SN.z': 'double', 'SN.velocity_x': 'double', 'SN.velocity_y': 'double', 'SN.velocity_z': 'double', 'SN.bl': 'double', 'SN.bm': 'double', 'SN.bn': 'double', 'SN.private_d': 'double', 'SN.private_e': 'double', 'SN.yaw': 'float', 'SN.aT': 'float', 'SN.aU': 'float', 'SN.aV': 'float', 'SN.offset': 'float', 'SN.width': 'float', 'SN.height': 'float', 'SN.bi': 'float', 'SN.bj': 'float', 'SN.bk': 'float', 'SN.y_size': 'float', 'SN.bp': 'float', 'SN.br': 'float', 'SN.bE': 'float', 'SN.aF': 'boolean', 'SN.aX': 'boolean', 'SN.aY': 'boolean', 'SN.aZ': 'boolean', 'SN.ba': 'boolean', 'SN.bb': 'boolean', 'SN.bc': 'boolean', 'SN.bd': 'boolean', 'SN.be': 'boolean', 'SN.bq': 'boolean', 'SN.bx': 'boolean', 'SN.private_c': 'boolean', 'SN.bC': 'boolean', 'SN.bF': 'boolean', 'SN.bM': 'boolean', 'SN.aG': 'pointer', 'SN.aH': 'pointer', 'SN.world': 'pointer', 'SN.client_only.bA': 'pointer', 'SN.client_only.bB': 'pointer', 'LS.E': 'integer', 'LS.T': 'integer', 'LS.Y': 'integer', 'LS.Z': 'integer', 'LS.private_a': 'integer', 'LS.aa': 'integer', 'LS.ab': 'integer', 'LS.ad': 'integer', 'LS.ae': 'integer', 'LS.ai': 'integer', 'LS.an': 'integer', 'LS.au': 'integer', 'LS.av': 'integer', 'LS.aC': 'integer', 'LS.F': 'float', 'LS.G': 'float', 'LS.H': 'float', 'LS.I': 'float', 'LS.J': 'float', 'LS.K': 'float', 'LS.L': 'float', 'LS.M': 'float', 'LS.Q': 'float', 'LS.S': 'float', 'LS.U': 'float', 'LS.W': 'float', 'LS.X': 'float', 'LS.ac': 'float', 'LS.af': 'float', 'LS.ag': 'float', 'LS.aj': 'float', 'LS.ak': 'float', 'LS.al': 'float', 'LS.am': 'float', 'LS.at': 'float', 'LS.aw': 'float', 'LS.ax': 'float', 'LS.ay': 'float', 'LS.aA': 'float', 'LS.aB': 'float', 'LS.ao': 'double', 'LS.ap': 'double', 'LS.aq': 'double', 'LS.ar': 'double', 'LS.as': 'double', 'LS.N': 'boolean', 'LS.P': 'boolean', 'LS.V': 'boolean', 'LS.ah': 'boolean', 'LS.az': 'boolean', 'LS.O': 'pointer', 'LS.R': 'pointer', 'LS.private_b': 'pointer', 'GS.f': 'integer', 'GS.g': 'integer', 'GS.k': 'integer', 'GS.m': 'integer', 'GS.private_a': 'integer', 'GS.z': 'integer', 'GS.private_bO': 'integer', 'GS.h': 'float', 'GS.i': 'float', 'GS.w': 'float', 'GS.x': 'float', 'GS.y': 'float', 'GS.B': 'float', 'GS.C': 'float', 'GS.o': 'double', 'GS.p': 'double', 'GS.q': 'double', 'GS.r': 'double', 'GS.s': 'double', 'GS.t': 'double', 'GS.j': 'boolean', 'GS.u': 'boolean', 'GS.A': 'boolean', 'GS.c': 'pointer', 'GS.d': 'pointer', 'GS.e': 'pointer', 'GS.l': 'pointer', 'GS.n': 'pointer', 'GS.v': 'pointer', 'GS.private_b': 'pointer', 'GS.private_bN': 'pointer', 'GS.D': 'pointer', 'DC.input': 'pointer', 'DC.minecraft': 'pointer', 'DC.filter0': 'pointer', 'DC.filter1': 'pointer', 'DC.filter2': 'pointer', 'DC.skin_url': 'pointer', 'native.bounds': 'pointer', 'native.random': 'pointer', 'native.watcher': 'pointer', 'native.class_literal': 'pointer', 'native.phase': 'integer'}, 'world': {'FD.a': 'boolean', 'FD.o': 'boolean', 'FD.s': 'boolean', 'FD.y': 'boolean', 'FD.J': 'boolean', 'FD.L': 'boolean', 'FD.N': 'boolean', 'FD.O': 'boolean', 'FD.B': 'boolean', 'FD.H': 'integer', 'FD.f': 'integer', 'FD.g': 'integer', 'FD.h': 'integer', 'FD.m': 'integer', 'FD.n': 'integer', 'FD.I': 'integer', 'FD.p': 'integer', 'FD.q': 'integer', 'FD.M': 'integer', 'FD.Q': 'integer', 'FD.i': 'float', 'FD.j': 'float', 'FD.k': 'float', 'FD.l': 'float', 'FD.C': 'pointer', 'FD.b': 'pointer', 'FD.D': 'pointer', 'FD.E': 'pointer', 'FD.F': 'pointer', 'FD.c': 'pointer', 'FD.G': 'pointer', 'FD.d': 'pointer', 'FD.e': 'pointer', 'FD.r': 'pointer', 'FD.t': 'pointer', 'FD.u': 'pointer', 'FD.v': 'pointer', 'FD.w': 'pointer', 'FD.x': 'pointer', 'FD.z': 'pointer', 'FD.K': 'pointer', 'FD.P': 'pointer', 'FD.R': 'pointer'}, 'provider': {'XA.a': 'pointer', 'XA.b': 'pointer', 'XA.f': 'pointer', 'XA.h': 'pointer', 'XA.c': 'boolean', 'XA.d': 'boolean', 'XA.e': 'boolean', 'XA.g': 'integer'}, 'info': {'Info.seed': 'integer', 'Info.spawn_x': 'integer', 'Info.spawn_y': 'integer', 'Info.spawn_z': 'integer', 'Info.time': 'integer', 'Info.last_played': 'integer', 'Info.size_on_disk': 'integer', 'Info.player_dimension': 'integer', 'Info.version': 'integer', 'Info.rain_time': 'integer', 'Info.thunder_time': 'integer', 'Info.raining': 'boolean', 'Info.thundering': 'boolean', 'Info.name': 'pointer', 'Info.player': 'pointer'}, 'inventory': {'Inventory.main': 'pointer', 'Inventory.armor': 'pointer', 'Inventory.owner': 'pointer', 'Inventory.cursor': 'pointer', 'Inventory.selected': 'integer', 'Inventory.dirty': 'boolean'}}
ALLOWED_WRITES = {'actor': ['GS.g', 'GS.m', 'GS.private_a', 'GS.private_b', 'GS.u', 'GS.v', 'LS.Y', 'LS.aa', 'LS.ad', 'LS.ae', 'SN.aJ', 'SN.aK', 'SN.aL', 'SN.aT', 'SN.aU', 'SN.aV', 'SN.aX', 'SN.bF', 'SN.bG', 'SN.bH', 'SN.bI', 'SN.bk', 'SN.bl', 'SN.bm', 'SN.bn', 'SN.bv', 'SN.bz', 'SN.height', 'SN.offset', 'SN.velocity_x', 'SN.velocity_y', 'SN.velocity_z', 'SN.width', 'SN.x', 'SN.y', 'SN.yaw', 'SN.z'], 'world': ['FD.J'], 'provider': [], 'info': ['Info.player'], 'inventory': ['Inventory.main', 'Inventory.armor']}
# NATIVE_SCHEMA_END

def require(value, message):
    if not value:
        raise ValueError(message)

def native_failure_details(output, receipt, returncode):
    """Keep the failed process visible while complete files remain retained."""
    output=Path(output)
    code=returncode if returncode is not None else receipt.get('exit') if type(receipt) is dict else None
    lines=['Player sleep NBT native failure', 'native exit: '+str(code),
           'retained report: '+str(output/'report.json'),
           'retained capture: '+str(output/'native.capture.json'),
           'retained stdout: '+str(output/'native.stdout'),
           'retained stderr: '+str(output/'native.stderr')]
    text=json.dumps(receipt,sort_keys=True)
    lines.append('capture receipt: '+text[:16384])
    if len(text)>16384:
        lines.append('capture receipt excerpt truncated; complete receipt is retained in report.json')
    try:
        path=output/'native.stderr'
        size=path.stat().st_size
        with path.open('rb') as source: raw=source.read(8193)
        excerpt=raw[:8192].decode('utf-8','backslashreplace')
        lines.append('native stderr ('+str(size)+' bytes):\n'+excerpt[:16384])
        if len(raw)>8192 or len(excerpt)>16384:
            lines.append('stderr excerpt truncated; complete bytes are retained in native.stderr')
    except OSError as error:
        lines.append('native stderr unavailable: '+str(error)[:1024])
    return '\n'.join(lines)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, Path(path))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def pairs(entries):
    result = {}
    for key, value in entries:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result

def loads(text):
    def invalid(token):
        raise ValueError('nonfinite JSON token: ' + token)
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid)

def exact(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return a.keys() == b.keys() and all(exact(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
    return a == b

def keys(value, wanted, label):
    require(type(value) is dict and set(value) == set(wanted), label + ' closed keys')

def integer(value, low=-(2**31), high=2**31-1):
    return type(value) is int and low <= value <= high

def word(value, width):
    return type(value) is str and re.fullmatch('[0-9a-f]{'+str(width)+'}', value) is not None

def words(value, length, width):
    return type(value) is list and len(value) == length and all(word(v, width) for v in value)

def coordinates(value):
    return value is None or type(value) is list and len(value) == 3 and all(integer(v) for v in value)

def plane_hash(value, length):
    require(type(value) is list and len(value) <= length, 'bounded plane runs')
    data = bytearray(length)
    previous_end, previous_value = 0, None
    for run in value:
        require(type(run) is list and len(run) == 3 and all(type(v) is int for v in run),
                'plane run triple')
        start, count, byte = run
        require(0 <= start < length and 1 <= count <= length-start and 1 <= byte <= 255 and
                start >= previous_end and (start != previous_end or byte != previous_value),
                'ordered maximal nonzero plane run extent')
        data[start:start+count] = bytes([byte])*count
        previous_end, previous_value = start+count, byte
    return hashlib.sha256(data).hexdigest()

def stack_slots(value, count, native=True):
    require(type(value) is list and len(value) == count, 'complete Stack slot vector')
    for stack in value:
        if stack is None:
            continue
        keys(stack, ('identity','fields'), 'actual Stack')
        require(word(stack['identity'], 16) and int(stack['identity'],16)!=0 if native else
                integer(stack['identity'], 1, 2**31-1), 'actual scoped Stack identity')
        keys(stack['fields'], ('iz.a','iz.b','iz.c','iz.d'), 'actual Stack source fields')
        require(all(integer(v) for v in stack['fields'].values()), 'signed Stack fields')

def selected(value):
    keys(value, STATE_KEYS, 'native selected snapshot')
    for key in ('position','motion'):
        require(words(value[key], 3, 16), key + ' raw double words')
    require(type(value['history']) is list and len(value['history']) == 6 and
            all(type(v) is dict and set(v) == {'D'} and word(v['D'],16) for v in value['history']),
            'six raw historical doubles')
    keys(value['base_flags'], ('bk','bv','bz','aX'), 'base flags')
    require(type(value['base_flags']['bk']) is dict and set(value['base_flags']['bk']) == {'F'} and
            word(value['base_flags']['bk']['F'], 8) and integer(value['base_flags']['bv']) and
            integer(value['base_flags']['bz']) and type(value['base_flags']['aX']) is bool,
            'typed base flags')
    for name, count in (('dimensions',3),('angles',4),('sleep_offsets',2)):
        require(words(value[name],count,8), name+' raw float words')
    require(type(value['living']) is list and len(value['living']) == 4 and
            all(integer(v) for v in value['living']), 'Living signed fields')
    for name in ('dimension','timer','score','inventory_selected'):
        require(integer(value[name]), 'signed selected field: '+name)
    require(all(type(value[n]) is bool for n in BOOL_FIELDS), 'typed selected Booleans')
    require(all(coordinates(value[n]) for n in ('v','Q','private_bN')), 'nullable BR values')
    require((value['bounds'] is None or words(value['bounds'],6,16)) and
            words(value['old_bounds'],6,16), 'actual permanent bounds words')
    require(integer(value['world_players'],0,1) and integer(value['world_entities'],0,1),
            'bounded actual World list counts')
    members=value['player_list_members']
    require(type(members) is list and len(members) == value['world_players'], 'actual Player membership')
    for member in members:
        keys(member, ('class','actor'), 'actual Player-list occurrence')
        require(member['class']=='dc' and member['actor'] is True, 'genuine actual Player member')
    stack_slots(value['main'],36); stack_slots(value['armor'],4)
    require(type(value['head']) is list and len(value['head'])==2 and
            integer(value['head'][0],0,255) and integer(value['head'][1],0,15), 'actual head ID/metadata')
    keys(value['planes'],PLANES,'five complete planes')
    result=dict(value)
    result['planes']={name:plane_hash(value['planes'][name],length) for name,length in PLANES.items()}
    return result

def raw_fields(value, group):
    keys(value,NATIVE_SCHEMA[group],'all inspected native '+group+' cells')
    for name,kind in NATIVE_SCHEMA[group].items():
        cell=value[name]
        if kind=='integer':
            width=64 if name in ('FD.H','FD.I','Info.seed','Info.time','Info.last_played',
                                'Info.size_on_disk') else 8 if name=='GS.f' else 32
            require(integer(cell,-(2**(width-1)),2**(width-1)-1), 'raw signed integer '+name)
        elif kind=='boolean':
            require(type(cell) is bool,'raw Boolean '+name)
        elif kind=='pointer':
            require(cell is None or type(cell) is dict and set(cell)=={'native_identity'} and
                    word(cell['native_identity'],16) and int(cell['native_identity'],16)!=0,
                    'scoped native identity '+name)
        else:
            tag,width=('F',8) if kind=='float' else ('D',16)
            require(type(cell) is dict and set(cell)=={tag} and word(cell[tag],width),
                    'raw floating word '+name)

def snapshot(value):
    keys(value,SNAPSHOT_KEYS,'complete native snapshot')
    result=dict(value);result['selected']=selected(value['selected'])
    for group in NATIVE_SCHEMA:
        raw_fields(value[group+'_fields'],group)
    stack_slots(value['old_slot_value_view'],40)
    require(type(value['rooted_entry_coordinates']) is list and
            len(value['rooted_entry_coordinates'])==3 and
            all(coordinates(v) for v in value['rooted_entry_coordinates']), 'rooted entry BR values')
    require(type(value['input_tree']) is dict,'actual typed input tree')
    random=value['random']
    require(type(random) is list and len(random)==2,'Actor/World Random pair')
    for r in random:
        require(type(r) is list and len(r)==3 and word(r[0],16) and int(r[0],16)<2**48 and
                type(r[1]) is bool and word(r[2],16), 'actual Java Random fields')
    return result

def source_site(value):
    if value is None:
        return
    keys(value,('class','method','descriptor','pc'),'actual typed source site')
    require(all(type(value[n]) is str and 0<len(value[n])<=128 and
                re.fullmatch('[A-Za-z0-9_();/]+',value[n]) is not None
                for n in ('class','method','descriptor')) and integer(value['pc'],0,65535),
            'actual source site members')

def diagnostic(value):
    keys(value,DIAGNOSTIC_KEYS,'native typed diagnostic')
    require(type(value['nbt_status']) is str and len(value['nbt_status'])<80 and
            integer(value['nbt_code'],0,65535), 'native status value/string')
    require(value['field'] is None or type(value['field']) is str and
            re.fullmatch('[A-Za-z]+',value['field']) is not None, 'static diagnostic field')
    require(all(integer(value[k],0,65535) for k in ('base_pc','living_pc','player_pc','local_pc')) and
            type(value['owned_work']) is bool,'native parent sites and owner presence')
    sleep=value['sleep']
    if sleep is not None:
        keys(sleep,('status','source_null','source_exception','source_error','native_policy',
                    'exception_class','site','child'),'retained typed SleepCall cause')
        require(integer(sleep['status'],0,65535) and
                all(type(sleep[k]) is bool for k in ('source_null','source_exception','source_error','native_policy')),
                'SleepCall typed classification')
        require(sleep['exception_class'] is None or sleep['exception_class']=='java/lang/NullPointerException',
                'actual typed matrix SleepCall exception')
        source_site(sleep['site']);source_site(sleep['child'])
    fd=value['fd']
    if fd is not None:
        keys(fd,('status','pc','saved_data_present','nbt_returned','info_cleared','publication_returned',
                 'source_returned','source_exception','source_error','native_policy','catch_pc'),'actual FD result')
        require(integer(fd['status'],0,65535) and integer(fd['pc'],0,65535) and
                integer(fd['catch_pc'],0,65535) and
                all(type(fd[k]) is bool for k in fd if k not in ('status','pc','catch_pc')),
                'typed FD receipt')

def native_rows(payload):
    require(type(payload) is str and len(payload.encode('utf-8'))<=MAX_BYTES,'bounded native payload')
    lines=payload.splitlines()
    footer=re.fullmatch(r'PLAYER_NBT_SLEEP_MATRIX_COMPLETE 40 rows ([1-9][0-9]*) checks',
                        lines[-1] if lines else '')
    require(footer is not None and len(lines)>=2 and
            lines[-2]=='Player active tick: '+footer[1]+' checks','complete native/common fixture markers')
    rows=[];initializer=[]
    for line in lines[:-2]:
        if line in ('16 achievements','151 recipes'):
            initializer.append(line);require(len(initializer)<=82,'bounded actual initializer output')
            continue
        require(line.startswith('NBT_SLEEP_NATIVE ') and len(line)<=512*1024,'complete native row protocol')
        row=loads(line[len('NBT_SLEEP_NATIVE '):]);keys(row,ROW_KEYS,'native matrix row')
        require(integer(row['case'],0,39) and row['case']==len(rows) and
                type(row['name']) is str and re.fullmatch('[a-z0-9_]+',row['name']) is not None and
                row['entry'] in ('SN_READ','FD_ATTACH'),'ordered profile/entry identity')
        values=row['input']
        require(type(values) is list and len(values)==26 and values[1]==row['name'] and
                values[0]==row['case'] and all(integer(v) for i,v in enumerate(values) if i!=1),
                'complete consumed 26-column input')
        for phase in ('before','after','retired'):
            row[phase]=snapshot(row[phase])
        diagnostic(row['diagnostic'])
        keys(row['cleanup'],('refusal_attempted','refused','views_cleared','first_cause_preserved',
                             'entry_counts_balanced'),'checked native retirement result')
        require(all(type(v) is bool for v in row['cleanup'].values()),'typed cleanup flags')
        rows.append(row)
    require(len(rows)==40,'exact complete forty-row matrix')
    return rows,int(footer[1]),initializer

def normalize_slots(slots, identities):
    result=[]
    for stack in slots:
        if stack is None:
            result.append(None);continue
        identity=stack['identity']
        if identity not in identities:
            identities[identity]=len(identities)+1
        result.append({'identity':identities[identity],'fields':dict(stack['fields'])})
    return result

def compare_value(a,b,path='$'):
    require(type(a) is type(b),'type differs at '+path)
    if type(a) is dict:
        require(a.keys()==b.keys(),'keys differ at '+path)
        return sum(compare_value(a[k],b[k],path+'.'+k) for k in a)
    if type(a) is list:
        require(len(a)==len(b),'length differs at '+path)
        return sum(compare_value(x,y,path+'['+str(i)+']') for i,(x,y) in enumerate(zip(a,b)))
    require(a==b,'value differs at '+path+': '+repr(a)+' != '+repr(b))
    return 1

def methods(journal, owner, name, descriptor):
    return [c for c in journal['calls'] if c['site']['class']==owner and
            c['site']['method']==name and c['site']['descriptor']==descriptor]

def compare_diagnostic(native, original, declaration):
    """Pair actual active parent frames; do not synthesize Java leaf receipts."""
    journal=original['journal'];d=native['diagnostic'];count=0
    exceptions=[e for e in journal['events'] if e['kind']=='EXCEPTION']
    reached_wake=methods(journal,'gs','a','(ZZZ)V')
    require(d['owned_work'] is bool(exceptions and reached_wake),
            'original reached wake plus interrupted parent requires retained child/entry Registry owner')
    if not exceptions:
        require(journal['root_returned'] is True and d['nbt_code']==0 and d['nbt_status']=='ok' and
                d['sleep'] is None and d['owned_work'] is False,'actual normal native parent return')
    else:
        require(len(exceptions)==1 and journal['root_returned'] is False,'one original first abrupt completion')
        failure=exceptions[0]
        for owner,key in (('sn','base_pc'),('ls','living_pc'),('gs','player_pc'),('dc','local_pc')):
            frame=next((s for s in failure['stack'] if s['class']==owner and
                        s['descriptor']=='(Lnu;)V'),None)
            if frame is not None:
                require(d[key]==frame['pc'],'exact active '+owner+' parent source site');count+=1
            elif not methods(journal,owner,'e' if owner=='sn' else 'a','(Lnu;)V'):
                require(d[key]==0,'unentered native parent site is neutral')
        wake=next((s for s in failure['stack'] if s['class']=='gs' and
                   s['method']=='a' and s['descriptor']=='(ZZZ)V'),None)
        if wake is not None:
            typed=d['sleep']
            require(typed is not None and typed['source_null'] is True and
                    typed['source_exception'] is True and typed['source_error'] is False and
                    typed['native_policy'] is False and typed['exception_class']==failure['class'] and
                    exact(typed['site'],wake) and d['owned_work'] is True and
                    d['nbt_code']==2 and d['nbt_status']=='null source reference',
                    'actual retained wake first source cause')
            # NULL World invocation at48 enters no lower source receiver.
            require(typed['child'] is None,'no invented entered child on NULL invocation');count+=1
        elif failure['class']=='java/lang/ClassCastException':
            require(d['sleep'] is None and d['nbt_code']==3 and
                    d['nbt_status']=='wrong source tag type','actual native wrong-tag category')
            getters=[c for c in journal['calls'] if c['site']['class']=='nu' and
                c['site']['method']==failure['site']['method'] and
                c['site']['descriptor']==failure['site']['descriptor'] and
                c['exception']==failure['class']]
            require(bool(getters),'actual throwing named getter')
            getter=getters[-1];args=getter['arguments']
            require(len(args)==1 and type(args[0]) is dict and 'utf16' in args[0],
                    'actual named getter argument')
            name=''.join(chr(c) for c in args[0]['utf16'])
            require(d['field']==name and name in declaration['input_tree']['values'],
                    'actual wrong named field')
            wanted={'B':1,'S':2,'I':3,'Lsp;':9}.get(getter['site']['descriptor'].split(')')[-1])
            require(wanted is not None and declaration['input_tree']['values'][name]['kind']!=wanted,
                    'declared malformed getter input, not operation-text exception inference');count+=1
        else:
            # Generic NBT exposes its actual null-bounds check at294; it has
            # no typed SN.e(DDD)86 producer. The original leaf stays unpaired.
            require(failure['class']=='java/lang/NullPointerException' and
                    failure['site']=={'class':'sn','method':'e','descriptor':'(DDD)V','pc':86} and
                    d['nbt_code']==2 and d['nbt_status']=='null source reference' and
                    d['base_pc']==294 and d['field']=='Pos' and d['player_pc']==0 and
                    d['local_pc']==0 and d['sleep'] is None and d['owned_work'] is False and
                    native['before']['selected']['bounds'] is None and
                    not methods(journal,'gs','a','(ZZZ)V'),'native actual294 null-bounds observation limit')
    fd=d['fd']
    if original['entry']=='FD_ATTACH':
        readers=methods(journal,'sn','e','(Lnu;)V')
        clears=[c for c in methods(journal,'ei','a','(Lnu;)V') if c['arguments']==[None] and c['completed']]
        publication=methods(journal,'fd','b','(Lsn;)Z')
        require(fd is not None and fd['status']==0 and fd['pc']==91 and
                fd['source_returned'] is journal['root_returned'] and
                fd['saved_data_present'] is bool(declaration['control']['info_tag']) and
                fd['nbt_returned'] is bool(readers and readers[0]['completed']) and
                fd['info_cleared'] is bool(clears) and
                fd['publication_returned'] is bool(publication and publication[0]['completed']) and
                all(fd[k] is False for k in ('source_exception','source_error','native_policy')) and
                fd['catch_pc']==0,'real FD child/clear/publication/return extent')
        count+=5
    else:
        require(fd is None,'direct reader has no fabricated FD result')
    return count

def compare_rows(rows, original, declarations, witnesses):
    originals=witnesses['observations'][0]['rows']
    require(len(rows)==len(original['rows'])==len(declarations)==len(originals)==40,'complete original/native cohort')
    scalar_count=site_count=preserved_count=0
    for n,(native,expected,declared,witness) in enumerate(zip(rows,original['rows'],declarations,originals)):
        require(native['case']==expected['case']==declared['case']==witness['case']==n and
                native['name']==expected['name']==declared['name'] and
                native['entry']==expected['entry']==declared['entry'],'same declared row')
        columns=['case','name','entry','direction','bed','sleeping_kind','sleeping_value','timer_kind',
            'timer_value','health','spawn_mask','spawn_x_kind','spawn_x','spawn_y_kind','spawn_y',
            'spawn_z_kind','spawn_z','score_kind','score','old_q','world_null','bounds_null','remote',
            'inventory_kind','info_tag','published_sleeper']
        require(exact(native['input'],[declared['control'][k] for k in columns]),'exact consumed before-input columns')
        before,after,retired=native['before'],native['after'],native['retired']
        require(before['actor_fields']['native.phase']==2 and after['actor_fields']['native.phase']==2,
                'actual full constructor READY operand, no native phase rewrite')
        require(before['world_fields']['FD.B'] is bool(declared['control']['remote']) and
                before['info_fields']['Info.seed']==declared['fixture']['world_constructor']['seed'] and
                [before['info_fields']['Info.spawn_'+axis] for axis in ('x','y','z')]==
                    declared['fixture']['world_spawn'], 'declared actual World inputs')
        require(exact(before['input_tree'],declared['input_tree']) and
                exact(before['input_tree'],after['input_tree']) and exact(after,retired),
                'immutable actual typed input and checked-retirement source preservation')
        require(exact(before['random'],after['random']) and
                exact(before['old_slot_value_view'],after['old_slot_value_view']) and
                exact(before['rooted_entry_coordinates'],after['rooted_entry_coordinates']),
                'within-call actual Random/retained Stack/entry BR preservation')
        for group in NATIVE_SCHEMA:
            a,b=before[group+'_fields'],after[group+'_fields']
            unchanged=[k for k in a if k not in ALLOWED_WRITES[group]]
            require(all(exact(a[k],b[k]) for k in unchanged),'native source-unselected '+group+' preservation')
            preserved_count+=len(unchanged)
        if 'history' in expected['within_call_only']:
            require(exact(before['selected']['history'],after['selected']['history']),
                    'no-reader history within-call preservation')
        for name in AUTHORED_BEFORE:
            scalar_count+=compare_value(before['selected'][name],witness['before']['selected'][name],
                                        '$.case'+str(n)+'.authored_before.'+name)
        if declared['control']['published_sleeper']:
            require(before['selected']['sleeping'] is True and before['selected']['world_J'] is True and
                    before['selected']['v']==[8,64,8],'genuine pre-root sleep/publication effects')
        actual_ids={};original_ids={}
        for key in ('main','armor'):
            scalar_count+=compare_value(normalize_slots(before['selected'][key],actual_ids),
                normalize_slots(witness['before']['selected'][key],original_ids),
                '$.case'+str(n)+'.entry_slots.'+key)
        observed=dict(after['selected']);wanted=dict(expected['after'])
        for key in ('main','armor'):
            observed[key]=normalize_slots(observed[key],actual_ids)
            wanted[key]=normalize_slots(wanted[key],original_ids)
        for key in expected['within_call_only']:
            observed.pop(key)
        scalar_count+=compare_value(observed,wanted,'$.case'+str(n)+'.selected_after')
        old_original=witness['after']['old_main']+witness['after']['old_armor']
        scalar_count+=compare_value(normalize_slots(after['old_slot_value_view'],actual_ids),
            normalize_slots(old_original,original_ids),'$.case'+str(n)+'.retained_old_stack_values')
        cleanup=native['cleanup'];d=native['diagnostic']
        require(cleanup['refusal_attempted'] is d['owned_work'] and
                cleanup['refused'] is d['owned_work'] and
                all(cleanup[k] is True for k in ('views_cleared','first_cause_preserved','entry_counts_balanced')),
                'actual noncopyable error retirement and repair')
        site_count+=compare_diagnostic(native,expected,declared)
    return {'scalar_comparisons':scalar_count,'active_source_site_comparisons':site_count,
            'native_within_call_unselected_cells':preserved_count}

# Native transport mutations are authored tests, never execution observations.
_CONTROL_DELETE = object()

def _control_change(value, path, replacement):
    """Copy only one dictionary/list route; every baseline sibling stays shared."""
    require(bool(path), 'nonempty authored mutation path')
    head,*tail=path
    result=dict(value) if type(value) is dict else list(value)
    if tail:
        result[head]=_control_change(value[head],tail,replacement)
    elif replacement is _CONTROL_DELETE:
        del result[head]
    else:
        result[head]=replacement
    return result

def _control_other(value):
    if type(value) is bool:
        return not value
    if type(value) is int:
        return 1 if value==0 else 0
    if type(value) is str:
        return value[:-1]+('1' if value[-1:]!='1' else '0')
    if type(value) is list and value:
        return [_control_other(value[0]),*value[1:]]
    if type(value) is dict and value:
        first=next(iter(value))
        return {**value,first:_control_other(value[first])}
    raise ValueError('mutation requires an explicit nullable/empty replacement')

def native_controls(payload, original, declarations, witnesses):
    """Exercise the unchanged full parser/comparator on one immutable real sample.

    A supplied positive sample does not itself qualify native execution here.
    Every changed payload is an authored corruption, including metamorphic
    positive checks that document the deliberately bounded comparison scope.
    """
    started=time.perf_counter();deadline=started+90
    baseline_hash=hashlib.sha256(payload.encode('utf-8')).hexdigest()
    baseline,checks,initializer=native_rows(payload)
    baseline_result=compare_rows(baseline,original,declarations,witnesses)
    lines=tuple(payload.splitlines(keepends=True))
    positions=tuple(i for i,line in enumerate(lines) if line.startswith('NBT_SLEEP_NATIVE '))
    raw=tuple(loads(lines[i][len('NBT_SLEEP_NATIVE '):]) for i in positions)
    raw_seal=tuple(json.dumps(row,separators=(',',':'),ensure_ascii=True) for row in raw)
    # Cache only immutable sample strings in this one control invocation. No
    # source packet, public admission or changed row object is cached/reused.
    surrounds={}
    receipts=[];labels=set()

    def encode(row_number, changed):
        if row_number not in surrounds:
            at=positions[row_number]
            surrounds[row_number]=(''.join(lines[:at]),''.join(lines[at+1:]))
        prefix,suffix=surrounds[row_number]
        return prefix+'NBT_SLEEP_NATIVE '+json.dumps(changed,separators=(',',':'),ensure_ascii=True)+'\n'+suffix

    def verify(label, candidate, reject=True):
        require(time.perf_counter()<deadline,'native mutation control deadline')
        require(label not in labels,'unique control label');labels.add(label)
        try:
            rows,_,_=native_rows(candidate)
            compare_rows(rows,original,declarations,witnesses)
        except ValueError as error:
            require(reject,'positive authored scope control rejected: '+label+': '+str(error))
            receipts.append({'label':label,'expected':'REJECT','actual':'REJECT',
                             'exception':type(error).__name__,'message':str(error)})
            return
        require(not reject,'authored native corruption was accepted: '+label)
        receipts.append({'label':label,'expected':'ACCEPT','actual':'ACCEPT'})

    def mutation(label, number, changes, reject=True):
        changed=raw[number]
        for path,value in changes:
            changed=_control_change(changed,path,value)
        verify(label,encode(number,changed),reject)

    def after_changes(path, value):
        return [(('after',)+path,value),(('retired',)+path,value)]

    verify('protocol/missing-completion',''.join(lines[:-1]))
    verify('protocol/missing-common-footer',''.join(lines[:-2])+lines[-1])
    verify('protocol/count-disagreement',''.join(lines[:-1])+lines[-1].replace(str(checks)+' checks',str(checks+1)+' checks'))
    verify('protocol/extra-tail',payload+'unexpected\n')
    verify('protocol/unknown-prefix','unexpected\n'+payload)
    verify('protocol/missing-row',''.join(line for i,line in enumerate(lines) if i!=positions[-1]))
    verify('protocol/duplicate-row',''.join(lines[:positions[-1]])+lines[positions[-2]]+''.join(lines[positions[-1]:]))
    swapped=list(lines);swapped[positions[0]],swapped[positions[1]]=swapped[positions[1]],swapped[positions[0]]
    verify('protocol/swapped-rows',''.join(swapped))
    first=positions[0]
    changed=list(lines);changed[first]=lines[first].replace('{','{"case":0,',1)
    verify('protocol/duplicate-JSON-key',''.join(changed))
    changed=list(lines);changed[first]=lines[first].rstrip('\r\n')[:-1]+'\n'
    verify('protocol/truncated-JSON-row',''.join(changed))
    changed=list(lines);changed[first]=lines[first].replace('"case":0','"case":NaN',1)
    verify('protocol/nonfinite-JSON',''.join(changed))

    for key in ROW_KEYS:
        mutation('row/missing/'+key,0,[((key,),_CONTROL_DELETE)])
    mutation('row/unknown-key',0,[(('unknown',),0)])
    for phase in ('before','after','retired'):
        for key in SNAPSHOT_KEYS:
            mutation('snapshot/'+phase+'/missing/'+key,0,[((phase,key),_CONTROL_DELETE)])
        mutation('snapshot/'+phase+'/unknown-key',0,[((phase,'unknown'),0)])
    for key in STATE_KEYS:
        mutation('selected/missing/'+key,0,[(('after','selected',key),_CONTROL_DELETE)])
    for group,schema in NATIVE_SCHEMA.items():
        # One actual field per representation and group suffices to exercise
        # each closed schema/type route without cloning the full schema suite.
        representatives={}
        for name,kind in schema.items():
            representatives.setdefault(kind,name)
        for kind,name in representatives.items():
            mutation('raw/'+group+'/missing/'+name,0,[(('after',group+'_fields',name),_CONTROL_DELETE)])
            invalid=0 if kind in ('boolean','pointer','float','double') else True
            mutation('raw/'+group+'/wrong-type/'+name,0,[(('after',group+'_fields',name),invalid)])
        mutation('raw/'+group+'/unknown-key',0,[(('after',group+'_fields','unknown'),None)])

    typed=[('position',(0,),0),('motion',(0,),False),('history',(0,'D'),False),
        ('base_flags',('aX',),0),('base_flags',('bv',),True),('base_flags',('bk','F'),0),
        ('dimensions',(0,),0),('angles',(0,),None),('sleep_offsets',(0,),'x'),
        ('living',(0,),True),('dimension',(),True),('timer',(),2**31),('score',(),False),
        ('v',(0,),False),('Q',(),[]),('bounds',(0,),0),('world_players',(),True),
        ('head',(1,),16),('main',(0,'identity'),'0000000000000000')]
    for key,tail,value in typed:
        mutation('selected/type/'+key+'/'+str(tail),0,[(('after','selected',key)+tail,value)])
    for key in ('source_null','source_exception','source_error','native_policy'):
        mutation('sleep/type/'+key,28,[(('diagnostic','sleep',key),0)])

    # Every compared selected field gets a valid-shaped changed after value.
    # Mirror it into retired so the original comparison, rather than merely
    # the cleanup invariant, must notice the fabricated selected observation.
    for key in STATE_KEYS:
        value=raw[0]['after']['selected'][key]
        if key=='planes':
            continue
        if key=='world_players':
            changes=after_changes(('selected',key),1)+after_changes(('selected','player_list_members'),[{'class':'dc','actor':True}])
        elif key=='player_list_members':
            changes=after_changes(('selected',key),[{'class':'dc','actor':True}])+after_changes(('selected','world_players'),1)
        elif key=='main':
            changes=after_changes(('selected','main',0,'fields','iz.a'),4)
        elif key=='armor':
            changes=after_changes(('selected','armor',0),raw[0]['before']['selected']['armor'][0])
        else:
            changes=after_changes(('selected',key),_control_other(value))
        mutation('comparison/selected/'+key,0,changes)

    for name,length in PLANES.items():
        runs=raw[0]['after']['selected']['planes'][name]
        mutation('plane/'+name+'/out-of-range',0,[(('after','selected','planes',name),[[length,1,1]])])
        mutation('plane/'+name+'/zero-byte',0,[(('after','selected','planes',name),[[0,1,0]])])
        mutation('plane/'+name+'/zero-count',0,[(('after','selected','planes',name),[[0,0,1]])])
        mutation('plane/'+name+'/overlap',0,[(('after','selected','planes',name),[[0,2,1],[1,1,2]])])
        mutation('plane/'+name+'/nonmaximal',0,[(('after','selected','planes',name),[[0,1,1],[1,1,1]])])
        alternate=([list(runs[0]),*runs[1:]] if runs else [[0,1,1]])
        if runs:alternate[0][2]=1 if alternate[0][2]!=1 else 2
        mutation('plane/'+name+'/different-valid-content',0,after_changes(('selected','planes',name),alternate))

    mutation('input/consumed-column',0,[(('input',22),1)])
    mutation('input/immutable-but-not-declared',0,[((phase,'input_tree','values','Sleeping','value'),0)
             for phase in ('before','after','retired')])
    mutation('input/entry-pose',0,[(('before','selected','position',0),'4022000000000000')])
    mutation('input/remote-World-B',0,[((phase,'world_fields','FD.B'),True) for phase in ('before','after','retired')])
    mutation('input/seed',0,[((phase,'info_fields','Info.seed'),5144) for phase in ('before','after','retired')])
    mutation('input/READY',0,[((phase,'actor_fields','native.phase'),0) for phase in ('before','after','retired')])
    mutation('within-call/Random',0,after_changes(('random',0,0),_control_other(raw[0]['after']['random'][0][0])))
    mutation('within-call/rooted-BR',0,after_changes(('rooted_entry_coordinates',1,0),43))
    mutation('within-call/old-Stack',0,after_changes(('old_slot_value_view',0,'fields','iz.a'),6))
    mutation('within-call/unselected-Actor',0,after_changes(('actor_fields','LS.F'),_control_other(raw[0]['after']['actor_fields']['LS.F'])))
    mutation('within-call/unselected-World',0,after_changes(('world_fields','FD.L'),not raw[0]['after']['world_fields']['FD.L']))
    mutation('within-call/unselected-Provider',0,after_changes(('provider_fields','XA.c'),not raw[0]['after']['provider_fields']['XA.c']))
    mutation('no-reader/history-changed',35,after_changes(('selected','history',0,'D'),'0000000000000001'))

    distinct='00000000000000a1'
    old_identity=raw[0]['before']['selected']['main'][0]['identity']
    changes=[(('before','selected','armor',0,'identity'),distinct)]
    changes += [((phase,'old_slot_value_view',36,'identity'),distinct) for phase in ('before','after','retired')]
    mutation('alias/entry-main-armor-distinct',0,changes)
    mutation('alias/current-reuses-old-Stack',0,after_changes(('selected','main',0,'identity'),old_identity))
    mutation('alias/equal-XYZ-is-not-identity',22,after_changes(('selected','Q_v_same'),True))
    changes=[]
    for phase in ('before','after','retired'):
        changes += [((phase,'selected','armor',0,'identity'),distinct),
                    ((phase,'old_slot_value_view',36,'identity'),distinct)]
    mutation('alias/no-reader-current-main-armor',35,changes)

    for number,source in enumerate(original['rows']):
        failures=[e for e in source['journal']['events'] if e['kind']=='EXCEPTION']
        if not failures:
            continue
        for frame in failures[0]['stack']:
            key={'sn':'base_pc','gs':'player_pc','dc':'local_pc','ls':'living_pc'}.get(frame['class'])
            if key is not None and frame['descriptor']=='(Lnu;)V':
                mutation('cause/'+str(number)+'/'+key,number,[(('diagnostic',key),frame['pc']+1)])
        mutation('cause/'+str(number)+'/status',number,[(('diagnostic','nbt_code'),0)])
        if raw[number]['diagnostic']['owned_work']:
            mutation('cause/'+str(number)+'/missing-owner',number,
                     [(('diagnostic','owned_work'),False),(('cleanup','refusal_attempted'),False),
                      (('cleanup','refused'),False)])
    for key in ('source_null','source_exception','source_error','native_policy'):
        mutation('cause/typed-wake/'+key,28,[(('diagnostic','sleep',key),not raw[28]['diagnostic']['sleep'][key])])
    mutation('cause/typed-wake/exception',28,[(('diagnostic','sleep','exception_class'),None)])
    for key in ('class','method','descriptor','pc'):
        value=raw[28]['diagnostic']['sleep']['site'][key]
        mutation('cause/typed-wake/site-'+key,28,[(('diagnostic','sleep','site',key),value+1 if type(value) is int else 'x')])
    mutation('cause/typed-wake/invented-child86',28,[(('diagnostic','sleep','child'),
             {'class':'sn','method':'e','descriptor':'(DDD)V','pc':86})])
    mutation('cause/bounds-null/fake-wake',29,[(('diagnostic','owned_work'),True),
             (('cleanup','refusal_attempted'),True),(('cleanup','refused'),True)])
    for number in (34,35,36,37):
        fd=raw[number]['diagnostic']['fd']
        for key,value in fd.items():
            mutation('FD/'+str(number)+'/'+key,number,[(('diagnostic','fd',key),
                     not value if type(value) is bool else value+1)])
    for key in raw[0]['cleanup']:
        mutation('cleanup/'+key,0,[(('cleanup',key),not raw[0]['cleanup'][key])])

    # Deliberately accepted parser/comparator transformations explain the scope:
    # random/constructor cells are not cross-VM answers, and identity numbers
    # represent alias relations rather than native or Java absolute addresses.
    for actor in (0,1):
        mutation('scope/Random-'+str(actor),0,[((phase,'random',actor),
            ['0000000000000001',True,'8000000000000000']) for phase in ('before','after','retired')],False)
    mutation('scope/unselected-constructor-F',0,[((phase,'actor_fields','LS.F'),{'F':'7fc12345'})
             for phase in ('before','after','retired')],False)
    mutation('scope/no-reader-history',35,[((phase,'selected','history',0,'D'),'0000000000000001')
             for phase in ('before','after','retired')],False)
    changes=[];identities={}
    for phase in ('before','after','retired'):
        for key,length in (('main',36),('armor',4),('old_slot_value_view',40)):
            path=(phase,'selected',key) if key!='old_slot_value_view' else (phase,key)
            slots=raw[0][phase]['selected'][key] if key!='old_slot_value_view' else raw[0][phase][key]
            for at,stack in enumerate(slots):
                if stack is None:continue
                old=stack['identity']
                identities.setdefault(old,format(0xa00+len(identities),'016x'))
                changes.append((path+(at,'identity'),identities[old]))
    mutation('scope/consistent-Stack-identity-renumber',0,changes,False)

    require(tuple(json.dumps(row,separators=(',',':'),ensure_ascii=True) for row in raw)==raw_seal and
            hashlib.sha256(payload.encode('utf-8')).hexdigest()==baseline_hash,
            'golden native graph/payload changed during path-copy mutations')
    final,final_checks,final_initializer=native_rows(payload)
    require(exact(compare_rows(final,original,declarations,witnesses),baseline_result) and
            final_checks==checks and exact(final_initializer,initializer),'unchanged full baseline still passes')
    return {'schema':'PLAYER_SLEEP_NBT_NATIVE_MUTATION_CONTROLS_V1',
            'scope':'authored transport/comparison mutations over an immutable supplied sample; no native execution credit',
            'native_processes_launched':0,'native_execution_qualified_by_this_mode':False,
            'sample_sha256':baseline_hash,'sample_checks':checks,'sample_comparison':baseline_result,
            'rejections':sum(r['actual']=='REJECT' for r in receipts),
            'positive_scope_controls':sum(r['actual']=='ACCEPT' for r in receipts),
            'controls':receipts,'elapsed_seconds':time.perf_counter()-started}

def run_native_controls(args, packet, reference, original, declarations, witnesses):
    require(args.executable is None and args.work_dir is None and args.native_sample is not None,
            'pure controls take --native-sample and no executable/work directory')
    sample=args.native_sample.resolve(strict=True)
    inputs={Path(__file__).resolve(),Path(sys.executable).resolve(),sample,
            args.source_root.resolve()/'tests/tools/entity/test_player_sleep_nbt_parent.py'}
    inputs.update(p for p in reference.iterdir() if p.is_file())
    for name in ('report.json','native.capture.json'):
        if (sample.parent/name).is_file():inputs.add(sample.parent/name)
    before={str(p):digest(p) for p in sorted(inputs)}
    report={'status':'FAIL','inputs_before_sha256':before,'python':sys.version,'host':sys.platform}
    started=time.perf_counter();cpu=time.process_time()
    try:
        payload=sample.read_bytes().decode('utf-8','strict')
        report.update(native_controls(payload,original,declarations,witnesses))
        final_original,final_witnesses,final_declarations=packet._read(reference)
        require(exact(final_original,original) and exact(final_witnesses,witnesses) and
                exact(final_declarations,declarations),'strict original graph changed during authored controls')
        report['status']='PASS'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)}
        raise
    finally:
        after={str(p):digest(p) for p in sorted(inputs)}
        report.update(inputs_after_sha256=after,wall_seconds=time.perf_counter()-started,
                      cpu_seconds=time.process_time()-cpu)
        if after!=before:report['status']='FAIL'
        if args.control_report is not None:
            output=args.control_report.resolve()
            require(output not in inputs and not output.is_relative_to(reference) and
                    output.parent!=sample.parent,'control report must not alter the protected packet/sample directory')
            output.parent.mkdir(parents=True,exist_ok=True)
            output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    require(report['status']=='PASS','pure control input integrity')
    print('Player sleeping NBT native transport: '+str(report['rejections'])+
          ' authored rejections; '+str(report['positive_scope_controls'])+
          ' positive scope controls; native processes launched=0')
    return 0


def source_paths(root, executable, packet):
    paths={executable,Path(__file__).resolve(),Path(sys.executable).resolve(),
           root/'CMakeLists.txt',root/'.clang-format'}
    for folder in ('src','include','cmake','tests'):
        paths.update(p for p in (root/folder).rglob('*') if p.is_file() and
            p.suffix in ('.c','.h','.inc','.cmake','.py','.json','.java','.txt','.tsv'))
    paths.update(p for p in packet.iterdir() if p.is_file())
    paths.update(executable.parent.glob('*.dll'))
    paths.update(executable.parent.glob('*.so'))
    paths.update(executable.parent.glob('*.so.*'))
    return sorted(paths)

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path)
    parser.add_argument('--work-dir',type=Path)
    parser.add_argument('--source-root',type=Path,default=ROOT)
    parser.add_argument('--reference-dir',type=Path,default=ROOT/'tests/reference/entity/player_sleep_nbt_parent')
    parser.add_argument('--controls',action='store_true')
    parser.add_argument('--native-sample',type=Path)
    parser.add_argument('--control-report',type=Path)
    args=parser.parse_args(argv)
    root=args.source_root.resolve();reference=args.reference_dir.resolve()
    packet=module('player_sleep_nbt_strict_original',reference/'packet.py')
    # This is the packet's common full-admission entry: it checks all proof,
    # file inventories, pins, declarations, finite journals and both witnesses.
    # Its public convenience readers each repeat this same complete operation.
    original,witnesses,declarations=packet._read(reference)
    require(original['schema']=='PLAYER_SLEEP_NBT_MATRIX_V1' and len(original['rows'])==40 and
            witnesses['schema']=='PLAYER_SLEEP_NBT_PARENT_WITNESSES_V1' and
            len(witnesses['observations'])==2,'strict complete original packet/witnesses')
    if args.controls:
        return run_native_controls(args,packet,reference,original,declarations,witnesses)
    require(args.executable is not None and args.work_dir is not None and
            args.native_sample is None and args.control_report is None,
            'native execution requires executable/work directory; sample/report flags are pure-controls only')
    process=module('player_sleep_nbt_bounded_capture',root/'tests/reference/client/statistics/support.py')
    resource=module('player_sleep_nbt_fixture_resources',root/'tests/reference/client/player_statistics/support.py')
    executable=args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='player-sleep-nbt-parent-',dir=args.work_dir.resolve()))
    resources=output/'resources';resource.resources_at(resources)
    paths=source_paths(root,executable,reference)
    before={str(p):digest(p) for p in paths}
    resource_before={p.relative_to(resources).as_posix():digest(p) for p in sorted(resources.rglob('*')) if p.is_file()}
    report={'status':'FAIL','scope':__doc__,'inputs_before_sha256':before,
        'resources_before_sha256':resource_before,'original_rows':0,'expected_original_rows':40,
        'original_journals_validated':40,'source_original_selected_after_fields':{'ordinary':40,'no_reader':39},
        'no_reader_history_cross_vm':False,'constructor_entropy_cross_vm':False,
        'source_journal_equals_native_execution_trace':False,'original_initializer_execution_extent':'UNOBSERVED_UNQUALIFIED',
        'diagnostic_scope':'actual native active parent positions and retained typed wake; ordinary wrong-tag pairing is limited to declared named-getter key/type controls, not native Java Throwable or NU execution',
        'old_native_array_lifetime':'ends on replacement; entry slot-value view and actual Stack owners retained',
        'unpaired_original_leaf':[{'class':'sn','method':'e','descriptor':'(DDD)V','pc':86,
            'native_observation':'typed NBT null source reference at SN294/fieldPos; no typed native child86'}],
        'binary_input_scope':'executable, executable-adjacent DLL/shared objects, current Python executable; no OS loader-image equality claim',
        'GUI':False,'loaded_restart':False,'natural_sleep_progression':False}
    result=None
    try:
        command=[str(executable),'player-nbt-sleep-matrix',str(output/'worlds').encode().hex(),
                 str(resources).encode().hex(),str((reference/'profiles.tsv').resolve())]
        environment=os.environ.copy();environment['ALSOFT_DRIVERS']='null'
        result=process.run_bounded(command,output,'native',timeout=180,limit=MAX_BYTES,env=environment)
        receipt=result.capture_report;report['capture']=receipt
        require(result.returncode==0 and result.stderr==b'' and receipt['exit']==0 and
                receipt['reader_errors']=={} and receipt['pipe_capture_complete'] is True and
                receipt['reaped'] is True and receipt['timed_out'] is False and
                receipt['output_overflow'] is False and receipt['stdout_bytes']==len(result.stdout) and
                receipt['stderr_bytes']==0,'complete successful native process/stream inventory')
        rows,checks,initializer=native_rows(result.stdout.decode('utf-8','strict'))
        report.update(compare_rows(rows,original,declarations,witnesses))
        report.update(original_rows=len(rows),native_checks=checks,native_initializer_lines=initializer)
        final_original,final_witnesses,final_declarations=packet._read(reference)
        require(exact(final_original,original) and exact(final_witnesses,witnesses) and
                exact(final_declarations,declarations), 'strict original packet changed during replay')
        report['status']='PASS'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)}
        if getattr(error,'capture_report',None) is not None:
            report['capture']=error.capture_report
        try:
            print(native_failure_details(output,report.get('capture'),
                                         getattr(result,'returncode',None)),file=sys.stderr)
        except BaseException as diagnostic_error:
            report['failure_diagnostic_error']={'type':type(diagnostic_error).__name__,
                                               'message':str(diagnostic_error)}
        raise
    finally:
        after={str(p):digest(p) for p in source_paths(root,executable,reference)}
        resource_after={p.relative_to(resources).as_posix():digest(p) for p in sorted(resources.rglob('*')) if p.is_file()}
        report.update(inputs_after_sha256=after,resources_after_sha256=resource_after,
                      python=sys.version,host=sys.platform)
        if after!=before or resource_after!=resource_before:report['status']='FAIL'
        report['products']={p.name:digest(p) for p in sorted(output.iterdir()) if p.is_file()}
        (output/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    require(report['status']=='PASS','native inputs changed: '+str(output/'report.json'))
    print('Player sleeping NBT parents: '+str(report['scalar_comparisons'])+
          ' original scalar comparisons; report='+str(output/'report.json'))
    return 0

if __name__=='__main__':
    raise SystemExit(main())
