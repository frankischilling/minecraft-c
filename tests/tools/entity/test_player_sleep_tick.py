"""Compare original GS parent prefixes with a separately scoped actual C continuation."""
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
sys.dont_write_bytecode=True
TREE=Path(__file__).resolve().parents[3]
ROOT=next(p for p in Path(__file__).resolve().parents if
          (p/'tests/reference/client/controller/capture.py').is_file())
PACKET=TREE/'tests/reference/entity/player_sleep_tick'
PLANES={'blocks':32768,'metadata':16384,'sky':16384,'block_light':16384,'height':256}
STATE_KEYS=('position','motion','dimensions','sleep_offsets','sleeping','timer','v','Q',
    'private_bN','bounds','old_bounds','world_J','remote','world_players','world_entities',
    'other_sleeping','world_same','bounds_same','v_same','Q_same','Q_v_same','Q_before_v_same',
    'private_bN_same','head','planes','sky_darkness','bed_static_id','bed_static_same',
    'client_null','player_list_same','player_list_members')
STABLE=('sleeping','timer','v','Q','v_same','Q_same','Q_v_same','Q_before_v_same')
ROW_KEYS=('case','input','before','after','diagnostic','random_before','random_after',
    'unselected_actor_preserved','unselected_world_preserved')


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def pairs(entries):
    result = {}
    for key, value in entries:
        require(key not in result, 'duplicate native JSON key')
        result[key] = value
    return result


def loads(text):
    def invalid(value):
        raise AssertionError('nonfinite native JSON token: ' + value)
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid)


def keys(value, wanted, label):
    require(type(value) is dict and set(value) == set(wanted), label + ' closed keys')


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def sequence(value, length, predicate, label):
    require(type(value) is list and len(value) == length and all(predicate(x) for x in value), label)


def words(value, length, width, label):
    sequence(value, length, lambda x: type(x) is str and
             re.fullmatch('[0-9a-f]{' + str(width) + '}', x) is not None, label)


def plane_digest(runs, length):
    require(type(runs) is list and len(runs) <= length, 'bounded native plane runs')
    plane = bytearray(length)
    previous_end = 0
    previous_value = None
    for run in runs:
        sequence(run, 3, lambda x: type(x) is int, 'native plane run triple')
        start, count, value = run
        require(0 <= start < length and 1 <= count <= length - start and
                1 <= value <= 255 and start >= previous_end,
                'native nonzero ordered plane run extent')
        require(start != previous_end or value != previous_value,
                'native plane runs must be maximal')
        plane[start:start + count] = bytes([value]) * count
        previous_end, previous_value = start + count, value
    return hashlib.sha256(plane).hexdigest()


def state(value):
    keys(value, STATE_KEYS, 'native selected state')
    words(value['position'], 3, 16, 'native position raw doubles')
    words(value['motion'], 3, 16, 'native motion raw doubles')
    words(value['dimensions'], 3, 8, 'native dimensions raw floats')
    words(value['sleep_offsets'], 2, 8, 'native sleep offsets raw floats')
    for key in ('sleeping', 'world_J', 'remote', 'world_same', 'bounds_same',
                'v_same', 'Q_same', 'Q_v_same', 'Q_before_v_same', 'private_bN_same'):
        require(type(value[key]) is bool, 'native Boolean: ' + key)
    require(integer(value['timer'], -(2**31), 2**31 - 1), 'native signed timer')
    for key in ('v', 'Q', 'private_bN'):
        if value[key] is not None:
            sequence(value[key], 3, lambda x: integer(x, -(2**31), 2**31 - 1),
                     'native nullable coordinates: ' + key)
    if value['bounds'] is not None:
        words(value['bounds'], 6, 16, 'native nullable bounds')
    words(value['old_bounds'], 6, 16, 'native rooted original bounds')
    require(integer(value['world_players'], 1, 2) and integer(value['world_entities'], 1, 2),
            'native actual World list counts')
    require(value['other_sleeping'] is None or type(value['other_sleeping']) is bool,
            'native optional other Player sleeping')
    sequence(value['head'], 2, lambda x: type(x) is int, 'native head ID/metadata')
    require(0 <= value['head'][0] <= 255 and 0 <= value['head'][1] <= 15, 'native head values')
    keys(value['planes'], PLANES, 'native full Chunk planes')
    require(integer(value['sky_darkness'], -(2**31), 2**31-1) and
            integer(value['bed_static_id'], 0, 255), 'native World/static integers')
    for name in ('bed_static_same', 'client_null', 'player_list_same'):
        require(type(value[name]) is bool, 'native actual identity/null Boolean')
    require(type(value['player_list_members']) is list and
            value['player_list_members'] == [{'class': 'dc', 'actor': True}] and
            type(value['player_list_members'][0]['actor']) is bool,
            'native actual Player-list membership')
    result = copy.deepcopy(value)
    result['planes'] = {name: plane_digest(value['planes'][name], length)
                        for name, length in PLANES.items()}
    return result


def random_state(value):
    require(type(value) is list and len(value) == 2, 'native actor/World Random pair')
    for row in value:
        require(type(row) is list and len(row) == 3 and type(row[1]) is bool,
                'native Random state shape')
        words([row[0], row[2]], 2, 16, 'native Random raw words')
        require(int(row[0], 16) < 2**48, 'native Java Random48 state')


def diagnostic(value):
    keys(value, ('status','status_code','source_class','source_method','player_pc','living_pc',
                 'local_pc','bed_pc','wake'), 'native typed tick diagnostic')
    require(value['status'] in ('OK','STATE_ERROR','OTHER_FAILURE'), 'actual native status category')
    require(integer(value['status_code'],0,65535),'exact actual native enum value')
    for name in ('source_class','source_method'):
        require(value[name] is None or type(value[name]) is str and
                re.fullmatch('[A-Za-z0-9_();/]+',value[name]) is not None,
                'native structured source method')
    for name in ('player_pc','living_pc','local_pc','bed_pc'):
        require(integer(value[name],0,65535), 'native structured PC')
    wake=value['wake']
    if wake is not None:
        keys(wake,('source_null','source_exception','native_policy','exception_class','site','child'),
             'actual retained wake diagnostic')
        for name in ('source_null','source_exception','native_policy'):
            require(type(wake[name]) is bool,'actual typed wake classification')
        require(wake['exception_class'] is None or
                wake['exception_class']=='java/lang/NullPointerException','actual typed child exception')
        for name in ('site','child'):
            site=wake[name]
            if site is not None:
                keys(site,('class','method','descriptor','pc'),'actual source child site')
                require(all(type(site[k]) is str and re.fullmatch('[A-Za-z0-9_();/]+',site[k])
                            for k in ('class','method','descriptor')) and
                        integer(site['pc'],0,65535),'typed child site contents')


def native_rows(stdout):
    require(type(stdout) is str and len(stdout.encode())<=4*1024*1024,'bounded native transport')
    lines=stdout.splitlines()
    footer=re.fullmatch(r'PASS: ([1-9][0-9]*) Player sleep parent component checks',
                        lines[-1] if lines else '')
    require(footer is not None,'completed native transport')
    rows=[]
    initializer=[]
    for line in lines[:-1]:
        if line in ('16 achievements','151 recipes'):
            initializer.append(line)
            require(len(initializer)<=144,'bounded genuine initializer output')
            continue
        require(line.startswith('SLEEP_TICK ') and len(line)<=128*1024,'single complete native JSON row')
        row=loads(line[len('SLEEP_TICK '):])
        keys(row,ROW_KEYS,'native parent row')
        number=len(rows)
        require(number<36 and integer(row['case'],0,35) and row['case']==number,'complete ordered native rows')
        sequence(row['input'],8,lambda x: integer(x,-(2**31),2**31-1),'declared native profile')
        require(row['input'][0]==number and 0<=row['input'][1]<4 and
                all(n in (0,1) for n in row['input'][2:6]) and 0<=row['input'][7]<=7,
                'native declared input domains')
        row['before']=state(row['before']);row['after']=state(row['after'])
        diagnostic(row['diagnostic'])
        random_state(row['random_before']);random_state(row['random_after'])
        for name in ('unselected_actor_preserved','unselected_world_preserved'):
            require(row[name] is None or type(row[name]) is bool,'explicit nullable preservation scope')
        rows.append(row)
    require(len(rows)==36,'all36 native parent profiles')
    return rows,int(footer[1]),initializer


def expected_diagnostic(original):
    """Use original recorded frames, never operation-text exception inference."""
    stack=original['journal']['stack']
    parent=next(s for s in stack if s['class']=='gs' and s['method']=='w_')
    terminal=original['journal']['terminal_site']
    result={'status':'STATE_ERROR','source_class':parent['class'],
            'source_method':parent['method']+parent['descriptor'],
            'player_pc':parent['pc'],'living_pc':0,'local_pc':0,
            'bed_pc':terminal['pc'] if terminal['class']=='gs' and terminal['method']=='am' else 0,
            'wake':None}
    if any(s['class']=='gs' and s['method']=='a' and s['descriptor']=='(ZZZ)V' for s in stack):
        wake=next(s for s in stack if s['class']=='gs' and s['method']=='a' and s['descriptor']=='(ZZZ)V')
        # The successful am getter's final field dereference is source31.
        # Its instructions are pinned in source-pins.json, not an exception class.
        result['bed_pc']=31
        result['wake']={'source_null':True,'source_exception':True,'native_policy':False,
                        'exception_class':original['journal']['exception'],
                        'site':copy.deepcopy(wake),'child':copy.deepcopy(terminal)}
    return result


def projection(actual, original):
    require(actual['case']==original['case'] and actual['input']==original['input'],
            'same declared profile values')
    if original['journal']['outcome']=='PREFIX111':
        require(actual['before']['client_null'] is False and
                original['before']['client_null'] is True,
                'explicit differing App input for real native continuation')
        require(actual['unselected_actor_preserved'] is None and
                actual['unselected_world_preserved'] is None,
                'whole-tick unselected preservation is unqualified')
        require((actual['case']!=7 and actual['diagnostic']['status']=='OK') or
                (actual['case']==7 and actual['diagnostic']['status']!='OK' and
                 actual['diagnostic']['player_pc']==111 and
                 actual['diagnostic']['living_pc']!=0),
                'actual quiet continuation or preserved null-World later failure')
        def eight(value):return {n:copy.deepcopy(value[n]) for n in STABLE}
        return ({'case':actual['case'],'input':actual['input'],
                 'before':eight(actual['before']),'after':eight(actual['after'])},
                {'case':original['case'],'input':original['input'],
                 'before':eight(original['before']),'after':eight(original['after'])})
    require(original['journal']['outcome']=='SOURCE_EXCEPTION' and
            actual['before']['client_null'] is True,'actual pre111 null-App input')
    wanted={n:copy.deepcopy(original[n]) for n in
            ('case','input','before','after','random_before','random_after',
             'unselected_actor_preserved','unselected_world_preserved')}
    wanted['diagnostic']=expected_diagnostic(original)
    observed=copy.deepcopy(actual)
    observed['diagnostic'].pop('status_code')
    return observed,wanted


def compare(actual,expected):
    count=0
    def visit(a,b,path):
        nonlocal count
        require(type(a) is type(b),'comparison type '+path)
        if type(b) is dict:
            require(a.keys()==b.keys(),'comparison keys '+path)
            for key in b:visit(a[key],b[key],path+'/'+key)
        elif type(b) is list:
            require(len(a)==len(b),'comparison length '+path)
            for i,(left,right) in enumerate(zip(a,b)):visit(left,right,path+'/'+str(i))
        else:
            count+=1
            require(a==b,'original scalar differs '+path+': native='+str(a)+' original='+str(b))
    visit(actual,expected,'projection')
    return count


def controls(original):
    """Authored parser/coverage negatives; never native execution evidence."""
    rejected=0
    def refuse(callback):
        nonlocal rejected
        try:callback()
        except (AssertionError,ValueError,KeyError,TypeError,IndexError):rejected+=1
        else:raise AssertionError('malformed authored transport admitted')
    for length in PLANES.values():
        for runs in ([[0,0,1]],[[length,1,1]],[[0,length+1,1]],[[0,1,0]],
                     [[0,2,1],[1,1,2]],[[0,1,1],[1,1,1]],[[False,1,1]],None):
            refuse(lambda r=runs,n=length:plane_digest(r,n))
        require(plane_digest([[0,2,7],[3,length-3,255]],length)==
                hashlib.sha256(b'\x07\x07\x00'+bytes([255])*(length-3)).hexdigest(),
                'actual lossless nonzero plane protocol')
    for value in ('{"case":0,"case":1}','{"timer":NaN}','{"timer":Infinity}'):
        refuse(lambda text=value:loads(text))
    # Build parser-control rows from the independent original data only. The
    # real driver never substitutes these authored positives for binary output.
    authored=[]
    for row in original['rows']:
        item={n:copy.deepcopy(row[n]) for n in ROW_KEYS if n!='diagnostic'}
        for phase in ('before','after'):
            item[phase]['planes']={name:[] for name in PLANES}
        if row['journal']['outcome']=='PREFIX111':
            item['before']['client_null']=False;item['after']['client_null']=False
            item['diagnostic']={'status':'OK','status_code':0,'source_class':None,'source_method':None,
                                'player_pc':0,'living_pc':0,'local_pc':0,'bed_pc':0,'wake':None}
            if row['case']==7:
                item['diagnostic'].update(status='OTHER_FAILURE',status_code=1,
                                          player_pc=111,living_pc=1)
            item['unselected_actor_preserved']=None;item['unselected_world_preserved']=None
        else:
            item['diagnostic']=expected_diagnostic(row)
            item['diagnostic']['status_code']=1 # Authored parser scalar; never native evidence.
        authored.append(item)
    def encode(rows):
        return ''.join('SLEEP_TICK '+json.dumps(r,separators=(',',':'))+'\n' for r in rows)+\
               'PASS: 1 Player sleep parent component checks\n'
    native_rows(encode(authored))
    refuse(lambda:native_rows(encode(authored[:-1])))
    refuse(lambda:native_rows(encode(authored)+ 'extra\n'))
    for number,row in enumerate(authored):
        for key in ROW_KEYS:
            changed=copy.deepcopy(authored);changed[number].pop(key)
            refuse(lambda rows=changed:native_rows(encode(rows)))
        for phase in ('before','after'):
            for key in STATE_KEYS:
                changed=copy.deepcopy(authored);changed[number][phase].pop(key)
                refuse(lambda rows=changed:native_rows(encode(rows)))
    # Success projection must ignore explicitly unqualified after111 fields,
    # while every one of the eight covered properties remains strict.
    for row in original['rows']:
        if row['journal']['outcome']!='PREFIX111':continue
        candidate=copy.deepcopy(authored[row['case']])
        for phase in ('before','after'):
            candidate[phase]=copy.deepcopy(row[phase]);candidate[phase]['client_null']=False
        left,right=projection(candidate,row);compare(left,right)
        candidate['after']['private_bN']=None
        candidate['after']['position']=['0000000000000000']*3
        left,right=projection(candidate,row);compare(left,right)
        if row['case']==7:
            broken=copy.deepcopy(candidate)
            broken['diagnostic']['status']='OK'
            refuse(lambda a=broken,b=row:projection(a,b))
            broken=copy.deepcopy(candidate)
            broken['diagnostic']['living_pc']=0
            refuse(lambda a=broken,b=row:projection(a,b))
        for field in STABLE:
            broken=copy.deepcopy(left)
            value=broken['after'][field]
            broken['after'][field]=not value if type(value) is bool else None if value is not None else False
            refuse(lambda a=broken,b=right:compare(a,b))
    return rejected


def source_paths(executable):
    paths={executable,Path(__file__).resolve(),ROOT/'CMakeLists.txt',ROOT/'.clang-format',
           TREE/'tests/entity/player/test_sleep_tick.c'}
    for folder in ('src','include','cmake','tests/entity','tests/world/runtime',
                   'tests/reference/client/controller','tests/reference/client/player_statistics',
                   'tests/support'):
        paths.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and
                     p.suffix in ('.c','.h','.inc','.cmake','.py','.json','.java','.txt','.tsv'))
    paths.update(p for p in PACKET.iterdir() if p.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return sorted(paths)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path)
    parser.add_argument('--work-dir',type=Path)
    parser.add_argument('--controls',action='store_true')
    args=parser.parse_args()
    packet=module('sleep_parent_original_packet',PACKET/'packet.py')
    original=packet.read_reference()
    if args.controls:
        print('Player parent native transport:',controls(original),'authored rejection controls')
        return
    require(args.executable is not None and args.work_dir is not None,'executable/work directory required')
    process=module('sleep_parent_bounded_process',ROOT/'tests/reference/client/controller/capture.py')
    support=module('sleep_parent_genuine_resources',ROOT/'tests/reference/client/player_statistics/support.py')
    executable=args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='player-sleep-tick-',dir=args.work_dir.resolve()))
    worlds=output/'worlds'
    support.resources_at(worlds/'resources')
    paths=source_paths(executable)
    before={str(path):process.digest(path) for path in paths}
    fixture_before={p.relative_to(worlds).as_posix():process.digest(p)
                    for p in (worlds/'resources').rglob('*') if p.is_file()}
    report={'status':'FAIL','scope':__doc__,'inputs_before_sha256':before,
            'resources_before_sha256':fixture_before,
            'original_rows':0,'original_expected_rows':36,'original_journals_validated':36,
            'original_source_parent_returns':0,'original_boundary':111,
            'original_continuation_entered':False,'native_original_whole_parent_equality':False,
            'prefix111_rows':32,'prefix111_fields_per_state':8,
            'prefix111_covered_fields':list(STABLE),'prefix111_covered_properties':512,
            'pre111_fault_rows':4,'pre111_fault_fields_per_state':31,
            'pre111_fault_covered_properties':248,
            'prefix111_other_after_fields_qualified':False,
            'prefix111_client_null_input':'original true; native false; differing/unqualified',
            'constructor_entropy_cross_vm_equality':False,'original_draw':False,'loaded_night':False}
    environment=os.environ.copy();environment['ALSOFT_DRIVERS']='null'
    try:
        receipt=process.capture([str(executable),'sleep-tick',str(worlds).encode().hex(),
                                 str((PACKET/'profiles.tsv').resolve())],output,'native',180,
                                environment,output)
        report['native']=receipt
        process.require_success(receipt,empty_stderr=True)
        actual,checks,initializer=native_rows((output/receipt['stdout_file']).read_text(encoding='utf-8'))
        count=0
        for native,expected in zip(actual,original['rows']):
            left,right=projection(native,expected);count+=compare(left,right)
        report['original_rows']=len(actual)
        report['native_ordinary_continuation_rows']=31
        report['native_post111_failure_rows']=[7]
        report['native_continuation_controls_original_qualified']=False
        report['scalar_comparisons']=count;report['native_checks']=checks
        report['native_initializer_lines']=initializer
        report['native_whole_calls_returning_ok']=sum(r['diagnostic']['status']=='OK' for r in actual)
        report['native_other_diagnostics']=[{'case':r['case'],'diagnostic':r['diagnostic']}
                                           for r in actual if r['diagnostic']['status']!='OK']
        require(packet.read_reference()==original,'strict original packet changed during native replay')
        report['status']='PASS'
    finally:
        after={str(path):process.digest(path) for path in source_paths(executable)}
        resources_after={p.relative_to(worlds).as_posix():process.digest(p)
                         for p in (worlds/'resources').rglob('*') if p.is_file()}
        report['inputs_after_sha256']=after;report['resources_after_sha256']=resources_after
        if after!=before or resources_after!=fixture_before:report['status']='FAIL'
        (output/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    require(report['status']=='PASS','native source/resource guards failed: '+str(output/'report.json'))
    print('Player parent prefixes:',report['scalar_comparisons'],'original scalar comparisons; report='+str(output/'report.json'))


if __name__=='__main__':
    main()
