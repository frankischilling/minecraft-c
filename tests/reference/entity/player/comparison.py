"""Compare untouched original CPU observations with the concrete C owner.

The application Math streams are independent. Three final random Living fields
are excluded only from same-constructor comparison and checked separately using
production scalar formulas on four independently recovered original Math draws.
No draw tape enters a native constructor. Object IDs/allocator identities and
the ordinary default-Random seed are not cross-process equivalence claims.
"""
import json,struct
from support import require
from pathlib import Path

def normalized(value):
    if isinstance(value,dict) and 'class' in value:
        return {'utf16':value['utf16']} if 'utf16' in value else {'nonnull':True}
    return value
def bits_f(value):return {'F':struct.pack('>f',value).hex()}
def f32(value):return struct.unpack('>f',struct.pack('>f',value))[0]
def compare(original,native,prefix=None):
    originals=[json.loads(x) for x in original.read_text().splitlines()]
    natives=[json.loads(x) for x in native.read_text().splitlines()]
    originals=[x for x in originals if x['event']=='case']
    require(len(originals)==len(natives)==8, 'exact eight native/original cases')
    counts={'cases':8,'ready_cases':0,'constructor_fields':0,'bounds_fields':0,'slot_cells':0,'alias_checks':0,'formula_replay_fields':0,'source_null_failures':0,'actual_failed_receiver_fields':0,'actual_failed_receiver_bounds':0}
    prefixes={} if prefix is None else {c['case']:c for c in json.loads(prefix.read_text())['cases']}
    def check(a,b,label):require(a==b, str((label,a,b)))
    for original,native in zip(originals,natives):
        case=original['case'];check(case,native['case'],'case')
        if original['outcome']!='success':
            check(native['status'],10,('source null status',case))
            check(native['failure_pc'],68 if original['null_world'] else 103,('source null pc',case))
            counts['source_null_failures']+=1
            if prefix is not None:
                failed=[c for c in prefixes[case]['checkpoints'] if c['event']=='exception']
                check(len(failed),1,(case,'one actual failed receiver'))
                receiver=failed[0]
                check(receiver['location']['pc'],native['failure_pc'],(case,'actual failed receiver pc'))
                for name,value in receiver['fields'].items():
                    if name=='sn.aD':continue
                    check(normalized(value),native['fields'][name],(case,'failed prefix',name));counts['actual_failed_receiver_fields']+=1
                for name,value in receiver['bounds'].items():
                    check(value,native['bounds'][name],(case,'failed bounds',name));counts['actual_failed_receiver_bounds']+=1
            continue
        counts['ready_cases']+=1;check(native['status'],0,('ready',case))
        a=original['player']['fields'];b=native['fields'];check(set(a),set(b),('full field closure',case))
        for name in a:
            if name in {'sn.aD','ls.aj','ls.G','ls.F'}:continue
            check(normalized(a[name]),b[name],(case,name));counts['constructor_fields']+=1
        for name in native['bounds']:
            check(original['player']['bounds'][name],native['bounds'][name],(case,name));counts['bounds_fields']+=1
        for name,value in native['aliases'].items():
            check(original['player'][name],value,(case,name));counts['alias_checks']+=1
        slots=original['player']['container']['slots'];check(len(slots),len(native['slots']),(case,'slots'))
        for left,right in zip(slots,native['slots']):
            check(left,right,(case,'slot',left['index']));counts['slot_cells']+=len(left)
        for name in ['ls.aj','ls.G','ls.F']:
            check(a[name],native['formula_replay'][name],(case,'observed formula',name));counts['formula_replay_fields']+=1
        d=[struct.unpack('>d',bytes.fromhex(x))[0] for x in original['draws']]
        check(bits_f((d[3]*3.1415927410125732)*2.0),native['formula_replay']['initial_yaw'],(case,'initial yaw formula'))
        counts['formula_replay_fields']+=1
    return counts
