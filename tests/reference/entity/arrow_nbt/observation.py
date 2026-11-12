"""Closed Arrow NBT observations and explicit source/native representation bounds."""
import copy
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREFIX = 'ARROW_NBT '
_spec = importlib.util.spec_from_file_location('arrow_nbt_constructor_observation', HERE.parent / 'arrow_construction/observation.py')
C = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(C)
need, exact, loads, word, stream = C.need, C.exact, C.loads, C.word, C.stream
OWN = ('xTile', 'yTile', 'zTile', 'inTile', 'inData', 'shake', 'inGround', 'player')
BASE = ('Pos', 'Motion', 'Rotation', 'FallDistance', 'Fire', 'Air', 'OnGround')
ROOTS = (('sl', 'a', '(Lnu;)V', 101), ('sl', 'b', '(Lnu;)V', 95),
         ('sn', 'e', '(Lnu;)V', 314), ('sn', 'd', '(Lnu;)V', 148),
         ('sn', 'c', '(Lnu;)Z', 31), ('jc', 'a', '(Lnu;Lfd;)Lsn;', 103))
FAILURES = {**dict.fromkeys(range(7, 15), 'java/lang/ClassCastException'),
            15: 'java/lang/NullPointerException', 25: 'java/lang/ClassCastException',
            26: 'java/lang/IndexOutOfBoundsException', 27: 'java/lang/IndexOutOfBoundsException',
            28: 'java/lang/ClassCastException', 29: 'java/lang/NullPointerException',
            30: 'java/lang/ClassCastException', 31: 'java/lang/ClassCastException',
            42: 'java/lang/ClassCastException', **dict.fromkeys(range(44, 48), 'java/lang/NullPointerException')}


def profiles(directory=HERE):
    rows = [[int(v) for v in line.split()] for line in (Path(directory) / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
    expected = [[i, 0, i, 0, 1] for i in range(7)]
    expected += [[i + 6, 0, 0, i, 1] for i in range(1, 10)]
    expected += [[i + 16, 1, i, 0, 1] for i in range(8)]
    expected += [[i + 24, 2, i, 0, 1] for i in range(10)]
    expected += [[34, 3, 0, 0, 1], [35, 3, 1, 0, 1], [36, 4, 0, 0, 1],
                 [37, 4, 1, 0, 1], [38, 4, 0, 0, 0], [39, 5, 0, 0, 1],
                 [40, 5, 0, 0, 0], [41, 5, 1, 0, 1], [42, 5, 2, 0, 1],
                 [43, 5, 3, 0, 1], [44, 0, 0, 10, 1], [45, 0, 0, 11, 1],
                 [46, 1, 0, 9, 1], [47, 1, 0, 11, 1]]
    need(exact(rows, expected), 'exact declared 44 paired plus four source-only profiles')
    return rows


def snapshot(value):
    if value is None:
        return
    need(type(value) is dict and set(value) == {'entity', 'base', 'bounds_same', 'original_bounds'}, 'closed Arrow NBT snapshot')
    C.snapshot(value['entity'])
    need(value['entity']['class'] == 'sl', 'actual canonical Arrow receiver')
    b = value['base']
    need(type(b) is dict and set(b) == {'fall', 'fire', 'air', 'on_ground', 'y_size'} and
         type(b['fire']) is type(b['air']) is int and type(b['on_ground']) is bool, 'complete typed parent scalar snapshot')
    word(b['fall'], 8); word(b['y_size'], 8)
    need(type(value['bounds_same']) is bool, 'actual permanent bounds identity')
    if value['original_bounds'] is not None:
        C.vector(value['original_bounds'], 6, 16)


def tag(value, depth=0):
    need(depth < 32, 'bounded tag depth')
    if value is None:
        return
    need(type(value) is dict and type(value.get('kind')) is int and 1 <= value['kind'] <= 10 and value['kind'] != 7, 'declared typed tag')
    k = value['kind']
    if k == 10:
        need(set(value) == {'kind', 'map_null', 'values', 'source_order'} and type(value['map_null']) is bool and
             type(value['values']) is dict and type(value['source_order']) is list and len(value['values']) <= 64, 'closed actual Compound Map')
        seen = set()
        for name, item in value['values'].items():
            need(type(name) is str and len(name) <= 256, 'bounded exact Compound name'); tag(item, depth + 1)
        for item in value['source_order']:
            need(type(item) is dict and set(item) == {'key', 'tag'} and type(item['key']) is dict and set(item['key']) == {'utf16'}, 'actual Map traversal entry')
            units = item['key']['utf16']; need(type(units) is list and all(type(x) is int and 0 <= x <= 65535 for x in units), 'actual UTF16 Map key')
            name = ''.join(chr(x) for x in units)
            need(name not in seen and name in value['values'] and exact(item['tag'], value['values'][name]), 'unique complete actual Map traversal')
            seen.add(name)
        need(seen == set(value['values']) and (not value['map_null'] or not seen), 'complete actual Map contents')
    elif k == 9:
        need(set(value) == {'kind', 'element_type', 'values'} and type(value['element_type']) is int and
             0 <= value['element_type'] <= 10 and type(value['values']) is list and len(value['values']) <= 16, 'closed typed source List')
        for item in value['values']:
            tag(item, depth + 1)
    else:
        need(set(value) == {'kind', 'value'}, 'closed scalar tag')
        v = value['value']
        if k <= 3:
            bits = (8, 16, 32)[k - 1]
            need(type(v) is int and -(1 << (bits - 1)) <= v < 1 << (bits - 1), 'signed exact primitive tag')
        elif k in (4, 5, 6):
            key, width = {4: ('J', 16), 5: ('F', 8), 6: ('D', 16)}[k]
            need(type(v) is dict and set(v) == {key}, 'typed raw primitive tag'); word(v[key], width)
        else:
            need(k == 8 and (v is None or type(v) is dict and set(v) == {'utf16'} and type(v['utf16']) is list and
                 all(type(x) is int and 0 <= x <= 65535 for x in v['utf16'])), 'exact String tag payload')


def canonical_tag(value):
    if value is None:
        return None
    result = copy.deepcopy(value)
    if result['kind'] == 10:
        del result['source_order']
        result['values'] = {k: canonical_tag(v) for k, v in result['values'].items()}
    elif result['kind'] == 9:
        result['values'] = [canonical_tag(v) for v in result['values']]
    return result


def context(value):
    need(type(value) is dict and set(value) == {'counter', 'world_random', 'math_random', 'shooter'} and type(value['counter']) is int, 'closed actual source context')
    stream(value['world_random']); stream(value['math_random']); C.snapshot(value['shooter'])


def validate_row(row, profile, native=False):
    required = {'schema', 'case', 'input', 'domain', 'returned', 'exception', 'saved', 'before', 'after',
                'arrow_before', 'arrow_after', 'result', 'tag_before', 'tag_after', 'exception_message', 'exception_frames'}
    need(type(row) is dict and required <= set(row) and not (set(row) - required - {'source_raw', 'native'}), 'closed raw NBT row')
    n, entry = profile[:2]
    need(row['schema'] == 'ARROW_NBT_RAW_V1' and type(row['case']) is int and row['case'] == n and exact(row['input'], profile) and
         row['domain'] == ('paired' if n < 44 else 'source-only'), 'actual row/profile/domain identity')
    need(type(row['returned']) is bool and row['returned'] == (n not in FAILURES) and row['exception'] == FAILURES.get(n, 'NONE'), 'actual complete return or source exception')
    need(row['saved'] is (n != 37) if entry == 4 else row['saved'] is None, 'actual Boolean save result')
    need(row['exception_message'] is None or type(row['exception_message']) is str, 'retained exception message')
    need(type(row['exception_frames']) is list and all(type(x) is str for x in row['exception_frames']), 'retained source exception frames')
    context(row['before']); context(row['after'])
    for name in ('arrow_before', 'arrow_after', 'result'):
        snapshot(row[name])
    tag(row['tag_before']); tag(row['tag_after'])
    before, after = row['before'], row['after']
    need(exact(before['world_random'], after['world_random']) and exact(before['math_random'], after['math_random']) and
         exact(before['shooter'], after['shooter']), 'NBT preserves genuine World/Math streams and shooter')
    need(after['counter'] - before['counter'] == (1 if 39 <= n <= 42 else 0), 'actual factory constructor ID increment')
    need((row['arrow_before'] is None) == (entry == 5) and (row['arrow_after'] is None) == (entry == 5), 'ordinary existing versus factory receiver access')
    need((row['result'] is not None) == (39 <= n <= 41), 'only complete successful factory transfers a returned receiver')
    if entry != 5:
        a, b = [row['arrow_' + side]['entity'] for side in ('before', 'after')]
        need(a['id'] == b['id'] and exact(a['random'], b['random']) and
             all(exact(a['arrow'][k], b['arrow'][k]) for k in ('c', 'j', 'k')), 'existing c/age/actual Random and ID remain unchanged')
        need(row['arrow_after']['bounds_same'], 'same permanent bounds object survives NBT')
        if entry in (1, 3, 4):
            need(exact(row['arrow_before'], row['arrow_after']), 'source writer never mutates its actual Arrow')
        if entry == 0:
            a, b = copy.deepcopy(row['arrow_before']), copy.deepcopy(row['arrow_after'])
            del a['entity']['arrow']; del b['entity']['arrow']
            need(exact(a, b), 'own reader changes only actual own scalar fields')
    if entry in (0, 2, 5):
        need(exact(row['tag_before'], row['tag_after']), 'source read/factory preserves actual input Compound')
    if n == 37:
        need(exact(row['tag_before'], row['tag_after']), 'dead false save leaves destination unchanged')
    if entry in (1, 3, 4) and n < 44 and n != 37:
        values = row['tag_after']['values']
        expected = set(OWN) | ({'id'} if entry == 4 else set()) | (set(BASE) if entry >= 3 else set())
        need(set(values) == expected | (set() if native else {'retained_unknown'}), 'complete generated semantic key inventory')
        if not native:
            need(exact(values['retained_unknown'], row['tag_before']['values']['retained_unknown']), 'retained actual source unknown')


def parse_rows(text, directory=HERE, native=False):
    need(type(text) is str and len(text) <= 16 * 1024 * 1024, 'bounded source/native stdout')
    values = [loads(line[len(PREFIX):]) for line in text.splitlines() if line.startswith(PREFIX)]
    inventory = profiles(directory)[:44] if native else profiles(directory)
    need(len(values) == len(inventory), 'complete declared source/native row inventory')
    for row, profile in zip(values, inventory):
        validate_row(row, profile, native)
    return values


def site(value):
    need(type(value) is dict and set(value) == {'class', 'method', 'descriptor', 'pc'} and
         all(type(value[k]) is str for k in ('class', 'method', 'descriptor')) and type(value['pc']) is int, 'closed actual source site')


def source_call(trace, row):
    need(type(trace) is dict and set(trace) == {'event', 'case', 'entry_arrow', 'exception_arrow', 'return_arrow', 'final_arrow',
         'math_initialized_before', 'math_initialized_after', 'events'} and trace['event'] == 'call' and trace['case'] == row['case'], 'complete passive call journal')
    for name in ('entry_arrow', 'exception_arrow', 'return_arrow', 'final_arrow'):
        snapshot(trace[name])
    need(type(trace['math_initialized_before']) is type(trace['math_initialized_after']) is bool and
         trace['math_initialized_before'] == trace['math_initialized_after'], 'NBT introduces no Math initialization')
    events = trace['events']
    need(type(events) is list and 0 < len(events) < 32768 and [e['sequence'] for e in events] == list(range(len(events))), 'bounded contiguous actual source events')
    entries, returns, errors, stores = [], [], [], []
    for event in events:
        need(type(event) is dict and set(event) == {'sequence', 'value'}, 'closed ordered event wrapper')
        e = event['value']; need(type(e) is dict and e.get('kind') in ('ENTRY', 'RETURN', 'EXCEPTION', 'STORE'), 'actual event kind'); site(e['site'])
        {'ENTRY': entries, 'RETURN': returns, 'EXCEPTION': errors, 'STORE': stores}[e['kind']].append(e)
    entry = row['input'][1]; owner, method, descriptor, return_pc = ROOTS[entry]
    root = lambda e: all(e['site'][k] == v for k, v in zip(('class', 'method', 'descriptor'), (owner, method, descriptor)))
    need(sum(root(e) for e in entries) == 1, 'one complete actual root entry')
    root_returns = [e for e in returns if root(e)]
    need(bool(root_returns) == row['returned'] and len(root_returns) <= 1, 'complete root returns only on actual success')
    if root_returns:
        expected_pc = 17 if row['case'] == 37 else return_pc
        need(root_returns[0]['site']['pc'] == expected_pc, 'actual complete root return PC')
    if not row['returned']:
        need(errors and errors[0]['class'] == row['exception'], 'actual original first thrown exception')
        for frame in errors[0]['stack']:
            site(frame)
        n = row['case']
        expected = ('sl', 'a', 4) if n == 15 else ('nu', 'l', 31) if n == 25 else \
                   ('sn', 'e', {26: 204, 27: 40, 28: 43, 29: 46, 30: 132}[n]) if 26 <= n <= 30 else \
                   ('sl', 'a', 95) if n in (31, 42) else ('nu', 'd', 28) if n == 44 else \
                   ('nu', 'd', 5) if n == 45 else ('sl', 'b', 8) if n == 46 else \
                   ('nu', 'a', 17) if n == 47 else ('sl', 'a', (4, 14, 24, 34, 48, 62, 76, 95)[n - 7])
        need(any((s['class'], s['method'], s['pc']) == expected for s in errors[0]['stack']), 'exact reached source failure boundary')
        if entry == 5:
            need(any(frame['class'] == 'jc' and frame['pc'] == 66 for frame in errors[0]['stack']), 'malformed read is outside factory constructor catch at66')
    if entry != 5:
        need(exact(trace['entry_arrow'], row['arrow_before']) and exact(trace['final_arrow'], row['arrow_after']), 'passive and ordinary snapshots of same existing receiver')
    elif row['result'] is not None:
        need(exact(trace['final_arrow'], row['result']), 'passive actual factory result receiver')
    own_fields = [e for e in stores if e['field'].startswith('sl.')]
    need(not any(e['field'] in ('sl.c', 'sl.j', 'sl.k') for e in own_fields if entry != 5), 'no source shooter/age writes in existing NBT receiver')
    if entry in (0, 2):
        n = row['case']
        count = n - 7 if 7 <= n <= 14 else 0 if n in (15, 25, 26, 27, 28, 29, 30, 44, 45) else 7 if n == 31 else 8
        fields = ('sl.d', 'sl.e', 'sl.f', 'sl.g', 'sl.h', 'sl.b', 'sl.i', 'sl.a')
        pcs = (7, 17, 27, 41, 55, 69, 88, 98)
        need([(e['field'], e['site']['class'], e['site']['method'], e['site']['pc']) for e in own_fields] ==
             [(name, 'sl', 'a', pc) for name, pc in zip(fields[:count], pcs[:count])], 'actual ordered own-reader completed-store prefix')
    elif entry in (1, 3, 4):
        need(not own_fields, 'source writers do not store Arrow fields')
    return {'root': {'class': owner, 'method': method, 'descriptor': descriptor},
            'return_pc': root_returns[0]['site']['pc'] if root_returns else None,
            'first_exception': errors[0]['site'] if errors else None,
            'first_exception_stack': errors[0]['stack'] if errors else [],
            'own_stores': [{'field': e['field'], 'site': e['site'], 'value': e['value']} for e in own_fields]}


def selected_snapshot(value):
    if value is None:
        return None
    result = copy.deepcopy(value)
    del result['entity']['id']; del result['entity']['random']
    return result


def project(row):
    """Select represented values only; keep raw source and entropy separately."""
    need(row['case'] < 44, 'source-only fault is never silently paired')
    entry = row['input'][1]
    result = {k: copy.deepcopy(row[k]) for k in ('case', 'input', 'domain', 'returned', 'exception', 'saved')}
    result.update(counter_increment=row['after']['counter'] - row['before']['counter'],
                  arrow_before=selected_snapshot(row['arrow_before']), arrow_after=selected_snapshot(row['arrow_after']),
                  result=selected_snapshot(row['result']), tag_before=canonical_tag(row['tag_before']), tag_after=canonical_tag(row['tag_after']))
    if entry in (1, 3, 4) and row['case'] != 37:
        # The native API constructs an independent semantic result. The raw
        # Java destination and its unknown retention remain intact above.
        result['tag_after']['values'].pop('retained_unknown', None)
    return result


def compare(expected, actual):
    need(len(expected) == len(actual) == 44, 'complete 44-row represented paired inventory')
    for left, right in zip(expected, actual):
        need(exact(left, right), 'selected Arrow NBT disagreement in case ' + str(left['case']))
