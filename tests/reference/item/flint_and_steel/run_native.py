"""Pair real native Flint and Steel lifecycle/ticks with the closed unchanged-original packet."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
import packet as W

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
P = W.P


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


N = module('flint_native_plane_helpers', ROOT / 'tests/reference/entity/remaining_plants_wake/run_native.py')
MODES = tuple(profile['mode'] for profile in W.profiles())
STATE_KEYS = {'phase','clicked','target_xyz','target','world_o','world_random','math_random','item_random',
 'prepared_item_random','item_random_alias_world','stack','fire','stats','item_table_null','use_stats_null',
 'break_stats_null','time','sky_seen','remote','provider_null','world_access_null','scheduled','lighting',
 'controller','flint_fields','entities','entity_class','entity_count','player_count','players','random_work','weather','chunk_planes'}
EVENT_COMMON = {'kind','clicked','target_xyz','target','world_o','world_random','math_random','item_random',
 'prepared_item_random','item_random_alias_world','stack','fire','stats','item_table_null','use_stats_null','break_stats_null'}
OUTCOME_KEYS = {'mode','input','prepared_class','raw','returned','result','exception','native_policy',
 'source_exception','source_error','query_provider_null','site','child','progress'}
TNT_KEYS = {'class','id','position','motion','previous','dimensions','offset','bounds','fuse','prevent_spawning','removed','on_ground','world_same'}
PLAYER_KEYS = {'class','id','position','motion','dimensions','offset','bounds','removed','on_ground','health','sleeping'}
SCOPE = ('Selected unchanged-original YE/IZ/OB calls, complete prepared Chunk planes, callback order, World/Math/Item '
 'Random including the abandoned prepared Item owner, Stack/Player/statistic maps and native cleanup. '
 'Prepared qy/SN class state and statistic mappings are explicit inputs. App/statistics constructors, native '
 'statistics files, Java map iteration, unused Entity entropy, natural encounters, graphics and FPS remain unpaired. '
 'Only the actual NULL IZ receiver control pairs NPE and zero effects without an Original JAR frame/PC claim; '
 'its native facade IZ1 location is a separate native invocation boundary.')


def require(condition, message):
    P.require(condition, 'Flint and Steel native: ' + message)


def integer(value):
    return type(value) is int


def vector(value, length, predicate=integer):
    return type(value) is list and len(value) == length and all(predicate(entry) for entry in value)


def word(value, width):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(width) + '}', value) is not None


def profile_for(mode):
    require(type(mode) is str and mode in MODES, 'declared mode')
    return next(profile for profile in W.profiles() if profile['mode'] == mode)


def random_state(value):
    require(vector(value, 3, lambda _: True) and word(value[0], 16) and int(value[0], 16) < 1 << 48 and
            type(value[1]) is bool and word(value[2], 16), 'complete typed Random state')


def class_state(value):
    require(type(value) is dict and set(value) == {'initialized', 'next_id'} and
            type(value['initialized']) is bool and integer(value['next_id']), 'actual Entity class state')


def stat_pairs(values):
    require(type(values) is list and len(values) <= 512 and all(vector(entry,2) for entry in values) and
            all(values[i-1][0] < values[i][0] for i in range(1,len(values))), 'complete sorted actual statistic mapping')


def stats(value, prepared=False):
    require(type(value) is dict and set(value) == ({'total','unsent','dirty'} if prepared else {'total','unsent','dirty','writer_null'}) and
            type(value['dirty']) is bool and (prepared or type(value['writer_null']) is bool), 'closed actual statistic map state')
    stat_pairs(value['total']); stat_pairs(value['unsent'])


def prepared_class(value):
    require(type(value) is dict and set(value) == {'scheduled','entity','stats'}, 'closed explicit prepared classes/maps')
    class_state(value['entity']); stats(value['stats'],True)
    q=value['scheduled']
    require(type(q) is dict and set(q)=={'initialized','sequence'} and type(q['initialized']) is bool and word(q['sequence'],16),
            'actual explicit prepared scheduled class')


def entity(value, player=False):
    require(type(value) is dict and set(value) == (PLAYER_KEYS if player else TNT_KEYS) and
            value['class'] == ('dc' if player else 'qw') and integer(value['id']), 'closed actual Entity fields')
    for key in ('position', 'motion') + (() if player else ('previous',)):
        require(vector(value[key], 3, lambda entry: word(entry, 16)), 'actual Entity double vector ' + key)
    require(vector(value['dimensions'], 2, lambda entry: word(entry, 8)) and word(value['offset'], 8) and
            vector(value['bounds'], 6, lambda entry: word(entry, 16)), 'actual Entity dimensions/bounds')
    for key in ('removed', 'on_ground') + (('sleeping',) if player else ('prevent_spawning', 'world_same')):
        require(type(value[key]) is bool, 'actual Entity boolean ' + key)
    require(integer(value['health'] if player else value['fuse']), 'actual concrete Entity scalar')


def entities(values):
    require(type(values) is list and len(values) <= 16, 'bounded actual TNT list')
    for value in values:
        entity(value)
    require(len({value['id'] for value in values}) == len(values), 'distinct published TNT identities')


def scheduled(value):
    require(type(value) is dict and set(value) == {'initialized', 'sequence', 'ordered', 'members'} and
            type(value['initialized']) is bool and word(value['sequence'], 16), 'complete actual scheduled state')
    for key in ('ordered', 'members'):
        require(type(value[key]) is list and len(value[key]) <= 4096, 'bounded actual queue')
        for entry in value[key]:
            require(type(entry) is dict and set(entry) == {'xyz', 'block', 'time', 'ordinal'} and
                    vector(entry['xyz'], 3) and integer(entry['block']) and word(entry['time'], 16) and
                    word(entry['ordinal'], 16), 'actual scheduled record')


def lighting(value):
    require(type(value) is dict and set(value) == {'depth', 'regions'} and integer(value['depth']) and
            type(value['regions']) is list and len(value['regions']) <= 4096, 'complete actual lighting queue')
    for region in value['regions']:
        require(type(region) is dict and set(region) == {'kind', 'bounds'} and region['kind'] in ('Sky', 'Block') and
                vector(region['bounds'], 6), 'ordered actual lighting region')




def cell(value):
    require(value is None or (vector(value,2) and 0<=value[0]<=255 and 0<=value[1]<=15), 'actual known cell or unloaded unknown')


def common(value):
    cell(value['clicked']);cell(value['target'])
    require(vector(value['target_xyz'],3) and type(value['world_o']) is bool, 'actual target coordinates/World flag')
    for key in ('world_random','math_random','prepared_item_random'): random_state(value[key])
    if value['item_random'] is not None: random_state(value['item_random'])
    for key in ('item_random_alias_world','item_table_null','use_stats_null','break_stats_null'):
        require(type(value[key]) is bool, 'actual source reference flag '+key)
    require(not value['item_random_alias_world'] or P.exact(value['item_random'],value['world_random']), 'actual aliased Random contents')
    require(value['fire'] is None or integer(value['fire']), 'actual nullable live Fire bn')
    stack=value['stack']
    require(stack is None or (type(stack) is dict and set(stack)=={'id','count','damage','animation'} and
            all(integer(entry) for entry in stack.values())), 'complete nullable actual Stack fields')
    stats(value['stats'])


def state(value,phase,profile):
    require(type(value) is dict and set(value)==STATE_KEYS and value['phase']==phase, 'closed ordered '+phase+' state')
    common(value)
    require(word(value['time'],16) and all(type(value[k]) is bool for k in ('sky_seen','remote','provider_null','world_access_null')), 'actual World state')
    scheduled(value['scheduled']);lighting(value['lighting']);class_state(value['entity_class']);entities(value['entities'])
    fields=value['flint_fields']
    require(type(fields) is dict and set(fields)=={'bf','bg','a','bh','bi','bj','bk','bl'} and
            all(integer(fields[k]) for k in ('bf','bg','a','bh')) and all(type(fields[k]) is bool for k in ('bi','bj')) and
            (fields['bk'] is None or integer(fields['bk'])) and (fields['bl'] is None or type(fields['bl']) is str), 'all eight actual Item fields')
    controller=value['controller']
    require((controller is not None) is (profile['operation']=='ob'), 'actual OS controller inventory')
    if controller is not None:
        require(type(controller) is dict and set(controller)=={'c','d','e','f','g','h','i'} and
                all(integer(controller[k]) for k in ('c','d','e','i')) and all(word(controller[k],8) for k in ('f','g','h')), 'actual OS fields')
    require(type(value['players']) is list and len(value['players'])==1 and value['entity_count']==1 and value['player_count']==1 and
            integer(value['entity_count']) and integer(value['player_count']) and value['entities']==[], 'genuine sole prepared Player membership')
    entity(value['players'][0],True)
    weather=value['weather']
    require(type(weather) is dict and set(weather)=={'rain_previous','rain_current','thunder_previous','thunder_current','rain_time','thunder_time','raining','thundering'} and
            all(word(weather[k],8) for k in ('rain_previous','rain_current','thunder_previous','thunder_current')) and
            all(integer(weather[k]) for k in ('rain_time','thunder_time')) and all(type(weather[k]) is bool for k in ('raining','thundering')), 'complete weather')
    work=value['random_work']
    require(type(work) is dict and set(work)=={'g','cooldown','candidates'} and integer(work['g']) and integer(work['cooldown']) and work['candidates']==[], 'actual idle random-work fields')
    chunks=value['chunk_planes'];footprint=[[x,z] for x in range(2) for z in range(2)]
    require(type(chunks) is list and len(chunks)==4, 'complete four-Chunk footprint')
    for chunk,xyz in zip(chunks,footprint):
        require(type(chunk) is dict and set(chunk)=={'xyz','planes'} and P.exact(chunk['xyz'],xyz) and
                type(chunk['planes']) is dict and set(chunk['planes'])==set(N.PLANES), 'ordered full Chunk planes')
        chunk['planes']={name:N.plane(chunk['planes'][name],length) for name,length in N.PLANES.items()}


def event(value):
    require(type(value) is dict and value.get('kind') in ('cell','range','sound','sky'), 'actual callback kind')
    kind=value['kind'];extra={'xyz','cell'} if kind=='cell' else {'xyz'} if kind=='range' else {'name','position','volume','pitch'} if kind=='sound' else set()
    require(set(value)==EVENT_COMMON|extra, 'closed complete callback fields')
    common(value)
    if kind in ('cell','range'):
        require(vector(value['xyz'],3 if kind=='cell' else 6), 'actual callback coordinates')
        if kind=='cell': cell(value['cell']);require(value['cell'] is not None,'actual notified cell in prepared footprint')
    elif kind=='sound':
        require(type(value['name']) is str and 0<len(value['name'])<64 and vector(value['position'],3,lambda v:word(v,16)) and
                word(value['volume'],8) and word(value['pitch'],8), 'actual sound arguments')


def site(value):
    require(type(value) is dict and set(value)=={'class','method','pc'} and integer(value['pc']) and value['pc']>=0 and
            all(value[k] is None or type(value[k]) is str for k in ('class','method')), 'actual typed source site')


def mutation(value):
    require(type(value) is dict and set(value)=={'status','access_fault','access_pc'} and all(integer(v) for v in value.values()) and
            0<=value['status']<=9 and 0<=value['access_fault']<=2 and value['access_pc']>=0, 'actual selected World child')


def progress(value,profile):
    require(type(value) is dict and set(value)=={'item','ob'}, 'closed actual caller progress')
    item=value['item']
    if item is not None:
        require(type(item) is dict and set(item)=={'pc','target_xyz','item_id','returned','body_result','sound_entered','sound_returned',
            'setter_entered','setter_returned','setter_value','durability_entered','durability_returned','used_stat_returned','query_provider_null',
            'sound','setter','site','child'} and integer(item['pc']) and item['pc']>=0 and vector(item['target_xyz'],3) and
            (item['item_id'] is None or integer(item['item_id'])), 'actual retained Item call')
        for key in ('returned','body_result','sound_entered','sound_returned','setter_entered','setter_returned','setter_value',
                    'durability_entered','durability_returned','used_stat_returned','query_provider_null'):
            require(type(item[key]) is bool, 'actual Item lifecycle '+key)
        site(item['site']);site(item['child'])
        for key,entered in (('sound','sound_entered'),('setter','setter_entered')):
            require((item[key] is not None) is item[entered], 'exact entered World child inventory')
            if item[key] is not None: mutation(item[key])
    ob=value['ob']
    require((ob is not None) is (profile['operation']=='ob'), 'actual OB call inventory')
    if ob is not None:
        require(type(ob) is dict and set(ob)=={'pc','activation_entered','activation_returned','activated','item_entered','item_returned','zero_slot_cleared','selected_slot_stored'} and
                integer(ob['pc']) and ob['pc']>=0 and all(type(v) is bool for k,v in ob.items() if k!='pc'), 'actual OB source progress')


def receipt_expected(profile):
    # An ordinal operand does not prove the callback is reached. This sealed
    # independent Source model is qualified against every actual Original row.
    expected=W.model(profile)
    return expected['exception']=='java/lang/IllegalStateException'


def receipt(value):
    keys = {'busy_before', 'before_holds', 'after_busy_holds', 'after_call_holds', 'retire',
            'same_identity', 'selected_source', 'exception', 'attempts'}
    require(type(value) is dict and set(value) == keys and
            all(integer(value[key]) for key in keys - {'same_identity', 'selected_source', 'exception'}) and
            P.exact([value[key] for key in ('busy_before', 'before_holds', 'after_busy_holds', 'after_call_holds', 'retire', 'attempts')],
                    [9, 2, 2, 1, 0, 2]) and value['same_identity'] is True and value['selected_source'] is True and
            type(value['exception']) is str, 'actual retained producer identity, BUSY retirement and one checked final consumption')



def parse(stdout,mode):
    profile=profile_for(mode)
    require(type(stdout) is str and stdout.endswith('\n'), 'complete decoded stdout')
    lines=stdout.splitlines()
    for startup in ('16 achievements','151 recipes'):
        if lines and lines[0]==startup: lines.pop(0)
    lifetime=None
    if len(lines)>=3 and lines[-3].startswith('FLINT_RECEIPT '):
        lifetime=P.loads(lines.pop(-3).removeprefix('FLINT_RECEIPT '));receipt(lifetime)
    require(len(lines)>=6 and re.fullmatch(r'PASS: [1-9][0-9]* Flint and Steel checks;mode='+re.escape(mode),lines[-1]), 'exact checked completion footer')
    require(lines[0].startswith('FLINT_STATE ') and all(lines[i].startswith('FLINT_STATE ') for i in (-4,-3,-2)) and
            lines[-5].startswith('FLINT_OUTCOME '), 'closed ordered before/outcome/retirement/cleanup protocol')
    events=[]
    for line in lines[1:-5]:
        require(line.startswith('FLINT_EVENT '), 'only complete declared callback records')
        entry=P.loads(line.removeprefix('FLINT_EVENT '));event(entry);events.append(entry)
    require(len(events)<=4096, 'bounded complete callback inventory')
    snapshots=[P.loads(lines[i].removeprefix('FLINT_STATE ')) for i in (0,-4,-3,-2)]
    for value,phase in zip(snapshots,('before','after','retired','cleanup')):state(value,phase,profile)
    steady=[{k:v for k,v in value.items() if k!='phase'} for value in snapshots[1:]]
    require(all(P.exact(value,steady[0]) for value in steady[1:]), 'retirement/cleanup preserve every selected source effect')
    outcome=P.loads(lines[-5].removeprefix('FLINT_OUTCOME '))
    require(type(outcome) is dict and set(outcome)==OUTCOME_KEYS and outcome['mode']==mode and
            P.exact(outcome['input'],profile) and integer(outcome['raw']) and 0<=outcome['raw']<=14 and
            type(outcome['returned']) is bool and (outcome['result'] is None or type(outcome['result']) is bool) and
            (outcome['exception'] is None or type(outcome['exception']) is str), 'closed exact inputs and typed outcome')
    for key in ('native_policy','source_exception','source_error','query_provider_null'):
        require(type(outcome[key]) is bool, 'actual enclosing category/witness '+key)
    prepared_class(outcome['prepared_class']);site(outcome['site']);site(outcome['child']);progress(outcome['progress'],profile)
    require((outcome['raw']==0) is outcome['returned'] and
            outcome['native_policy'] is (not outcome['returned'] and outcome['exception'] is None) and
            outcome['source_exception'] is (outcome['exception'] is not None) and outcome['source_error'] is False and
            (not outcome['returned'] or outcome['exception'] is None), 'actual ordinary/source/native category remains independent')
    require(lifetime is None if outcome['native_policy'] else
            (lifetime is not None) is receipt_expected(profile),
            'actual native frontier has no producer; reached Source failure has its exact owned receipt inventory')
    if lifetime is not None:
        require(not outcome['returned'] and lifetime['exception']==outcome['exception'], 'owned receipt preserves actual enclosing source category')
    return dict(before=snapshots[0],after=snapshots[1],events=events,outcome=outcome,receipt=lifetime)


def selected_state(value):return {k:copy.deepcopy(value[k]) for k in STATE_KEYS-{'phase'}}
def scalar_count(value):
    if type(value) is dict:return sum(scalar_count(v) for v in value.values())
    if type(value) is list:return sum(scalar_count(v) for v in value)
    return 1

def exception_class(original):return None if original['exception']=='NONE' else original['exception']


def validate(actual,mode,originals):
    original=originals[mode];profile=profile_for(mode);outcome=actual['outcome']
    expected=W.model(profile,original['prepared_class']);facts=expected['progress'];exception=exception_class(original)
    require(P.exact(outcome['prepared_class'],original['prepared_class']), 'exact explicit original prepared classes and statistic mappings')
    for phase in ('before','after'):
        require(P.exact(selected_state(actual[phase]),selected_state(original[phase])), 'complete selected original '+phase+' state and five planes')
    events=[{k:copy.deepcopy(v) for k,v in entry.items() if k!='stack_trace'} for entry in original['events']]
    require(P.exact(actual['events'],events), 'complete ordered callback values, all Random streams, actual Stack and statistic snapshots')
    require(outcome['returned'] is (exception is None) and outcome['exception']==exception and outcome['native_policy'] is False and
            P.exact(outcome['result'],original['result']), 'actual original return/category without a native frontier')
    boundary=expected['invocation_null']
    if boundary:
        require(mode=='flint-wrapper-stack-null' and profile['operation']=='wrapper' and profile['null_stack'] is True and
                exception=='java/lang/NullPointerException' and expected['exception_origin'] is None and
                expected['exception_source_stack']==[], 'only the sealed actual NULL IZ invocation has an unpaired native boundary PC')
    elif exception is not None:
        frames={(entry['class'],entry['method']) for entry in original['exception_stack']
                if entry['class']!='FlintAndSteelObservation' and not entry['class'].startswith('FlintAndSteelObservation$')}
        current=outcome['site']
        require(current['class'] is not None and current['method'] is not None and
                (current['class'],current['method'].split('(')[0]) in frames, 'reported source caller exists in actual Original failure frames')
    origin=expected['exception_origin']
    provider=origin is not None and P.exact([origin['class'],origin['method'],origin['pc']],['fd','c',6])
    require(outcome['query_provider_null'] is provider, 'actual positive provider-null witness, never inferred from current World fields')
    if provider:
        require(P.exact(outcome['child'],dict(**{'class':'fd'},method='c',pc=6)), 'same actual ordinary getter supplies fd.c6 source cause')
    node=outcome['progress']['item'];ob=outcome['progress']['ob']
    item_present=profile['operation']!='ob' or facts['item_entered']
    require((node is not None) is item_present, 'exact reached Item-call inventory')
    if node is not None:
        if boundary: expected_pc=1 # Native facade boundary only; no Original IZ1 claim.
        elif profile['operation']=='direct' or (facts['ye_entered'] and not facts['wrapper_body_returned']):expected_pc=facts['ye_pc']
        else:expected_pc=facts['wrapper_pc']
        target=W.D.shifted(profile['xyz'],profile['face']) if facts['ye_entered'] else profile['xyz']
        require(node['pc']==expected_pc and P.exact(node['target_xyz'],target) and
                node['item_id']==(259 if facts['ye_entered'] else None), 'actual entered receiver/coordinate/program counter scope')
        result_body=facts['durability_returned'] if profile['operation']=='direct' else facts['wrapper_body_returned']
        checks=dict(returned=exception is None,body_result=result_body,
                    sound_entered=facts['sound_entered'],sound_returned=facts['sound_returned'],
                    setter_entered=facts['setter_entered'],setter_returned=facts['setter_returned'],setter_value=facts['setter_value'],
                    durability_entered=facts['durability_entered'],durability_returned=facts['durability_returned'],
                    used_stat_returned=facts['use_stat_returned'],query_provider_null=provider)
        require(all(node[k] is v for k,v in checks.items()), 'independent source lifecycle and ordinary setter-false continuation')
        for key,returned_key in (('sound','sound_returned'),('setter','setter_returned')):
            view=node[key]
            if view is None:continue
            if facts[returned_key]:expected_view=dict(status=0,access_fault=0,access_pc=0)
            elif exception=='java/lang/IllegalStateException':
                callback=original['events'][-1]['kind'];expected_view=dict(status=3,access_fault=0,access_pc={'sound':41,'cell':34,'range':40}[callback])
            else:
                require(key=='sound' and profile['null_access'], 'declared selected non-callback World failure')
                expected_view=dict(status=5,access_fault=1,access_pc=9)
            require(P.exact(view,expected_view), 'actual selected World child status and source loop instruction')
    if ob is not None:
        require(ob['pc']==facts['ob_pc'] and all(ob[k] is facts[k] for k in
                ('activation_entered','activation_returned','activated','item_entered','item_returned')) and
                ob['zero_slot_cleared'] is False and ob['selected_slot_stored'] is False, 'actual OB stages; no in-air inventory fallback or zero-count clear')
    count=scalar_count(selected_state(actual['before']))+scalar_count(selected_state(actual['after']))+scalar_count(events)+3
    return dict(matched=True,compared_scalars=count,receipt_lifetime_checked=actual['receipt'] is not None,
                invocation_boundary_scope=boundary)


def records(values):return ';'.join(','.join(str(v) for v in entry) for entry in values) or '-'

def native_arguments(executable,profile,work,prepared):
    prepared_class(prepared)
    require(P.exact(prepared['stats']['total'],prepared['stats']['unsent']), 'actual prepared natural no-service maps agree for supported increment preparation')
    return [str(executable),profile['mode'],str(work).encode('utf-8').hex(),profile['operation'],
        *(str(v) for v in profile['xyz']),*(str(profile[k]) for k in ('face','stack_id','count','damage','animation','world_seed','item_seed')),
        *(str(int(profile[k])) for k in ('remote','null_world','null_player','null_stack','null_item_random','null_fire','null_provider',
         'null_access','null_stats_writer','null_use_stats','null_break_stats','null_item_table','item_random_world')),
        profile['sound_action'],*(str(profile[k]) for k in ('cell_error_at','range_error_at','sound_error_at')),
        records(profile['cells']),records(profile['heights']),prepared['scheduled']['sequence'],str(int(prepared['scheduled']['initialized'])),
        str(int(prepared['entity']['initialized'])),str(prepared['entity']['next_id']),records(prepared['stats']['total']),str(int(prepared['stats']['dirty'])),'none']

command=native_arguments


def digests(paths):
    return {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    originals = {row['mode']: row for row in W.read_reference()['rows']}
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='flint-and-steel-', dir=args.work_dir.resolve()))
    capture = module('flint_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
    inputs = list(dict.fromkeys([executable, Path(__file__).resolve(),
              *(HERE / name for name in W.SOURCES), *(Path(path) for path in W.SHARED.values()),
              HERE / 'finite.json', HERE / 'provenance.json',
              ROOT / 'tests/item/use/flint_and_steel_reference.c', ROOT / 'tests/world/runtime/test_redstone_ore.c', ROOT / 'tests/world/runtime/test_fire.c',
              ROOT / 'tests/world/runtime/test_fluids.c',
              ROOT / 'tests/reference/entity/remaining_plants_wake/run_native.py',
              ROOT / 'tests/reference/client/controller/capture.py']))
    before = digests(inputs)
    report = {'schema': 'ITEM_FLINT_AND_STEEL_NATIVE_V1', 'status': 'FAIL', 'scope': SCOPE, 'input_before_sha256': before, 'cases': []}
    try:
        for profile in W.profiles():
            mode = profile['mode']
            work = output / mode
            work.mkdir()
            receipt = capture.capture(native_arguments(executable, profile, work, originals[mode]['prepared_class']),
                                      work, 'native', 60, os.environ.copy())
            row = {'mode': mode, 'native': receipt}
            report['cases'].append(row)
            capture.require_success(receipt, empty_stderr=True)
            row['comparison'] = validate(parse((work / receipt['stdout_file']).read_text(encoding='utf-8'), mode), mode, originals)
        report.update(status='PASS_SELECTED_COMPONENTS_ONLY', original_pairs=len(MODES), native_controls=0,
                      retained_receipt_controls=sum(row['comparison']['receipt_lifetime_checked'] for row in report['cases']),
                      original_scalar_comparisons=sum(row['comparison']['compared_scalars'] for row in report['cases']))
    finally:
        report['input_after_sha256'] = digests(inputs)
        if before != report['input_after_sha256']: report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        require(before == report['input_after_sha256'], 'native replay inputs unchanged')
    print('Flint and Steel:', len(MODES), 'original pairs;', report['retained_receipt_controls'], 'retained receipt controls;',
          report['original_scalar_comparisons'], 'selected original scalars;', output / 'report.json')


if __name__ == '__main__':
    main()
