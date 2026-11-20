"""Validate retained unchanged-original Hoe/Seeds input, state and source observations."""
import copy
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
COUNT = 118
FROZEN = {'FarmingObservation.java': '7319ed14daac3358367c28d4eb4caa11faf209f6e8f4fbc88466b0d4f24c03c6', 'FarmingJdi.java': '5d186a4ab65c939324394a6c7c07993d7f7015b35e867614904eefec43884b07', 'profiles.tsv': '69d6832e6d1dbeaa57a8026028ea3769db94e3ddfb1fc6ee20484d91cd2f903c', 'declared-inputs.json': '63651b8a7c615d0654376652e39b12f110131cc58a5adffad852276865ee3970', 'source-pins.json': '87cc31e336c57ade85ea12e76665dd2883107f5894817def2345c5dea8ca02c5', 'capture_reference.py': '444eea8af0c442e7da507ebd027989057e44b6095f0a97b996228af0a7b045e7'}
CAPTURED = {'selected.txt': '9411c4b2fe542f897f42a709f8d7ab8b3f67bdb5063d1dd59b29b8208d615716', 'events.jsonl': '7b581d689f6f67173ee2427e2ab18d94f7a78bc4654c75a0422421a06de6ae60', 'live-code.tsv': 'd8179e90f3396fb5cb6b1eb419bb32666d04fff6597760c1a141a7cf5fe97096'}
COLUMNS = 'kind via item count damage damage_mode face x y z ground above remote nulls farmland_alias sound_mode cell_fail crop_id dirt_id'.split()
SITE = re.compile(r'^([A-Za-z0-9_/$]+)\.([^.(]+)(\(.*\).+)@(-?[0-9]+)$')


def require(value, message):
    if not value:
        raise ValueError(message)


def integer(value, low=-(1 << 31), high=(1 << 31) - 1):
    return type(value) is int and low <= value <= high


def site(value):
    require(type(value) is str, 'typed source site')
    match = SITE.fullmatch(value)
    require(match is not None, 'symbolic source site')
    return '.'.join(match.group(1, 2)) + match[3], int(match[4])


def inputs(directory):
    declared = json.loads((directory / 'declared-inputs.json').read_text(encoding='utf-8'))
    rows = []
    for line in (directory / 'profiles.tsv').read_text(encoding='ascii').splitlines():
        fields = line.split('\t')
        require(len(fields) == 20 and re.fullmatch('[a-z0-9_]+', fields[0]), 'closed profile row')
        require(all(re.fullmatch('-?[0-9]+', x) for x in fields[1:]), 'signed input transport')
        values = list(map(int, fields[1:]))
        require(all(integer(v) for v in values), 'signed Java input range')
        kind, via, item, count, damage, mode, face, x, y, z, ground, above, remote, nulls, alias, sound, cell, crop, dirt = values
        require(kind in (0, 1) and via in (0, 1) and item in (range(290, 295) if kind == 0 else (295,)) and
                mode in range(4) and remote in (0, 1) and nulls >= 0 and nulls & ~71 == 0 and
                alias == 0 and sound in range(5) and cell in (0, 1) and 0 <= ground <= 255 and
                0 <= above <= 255 and crop in (0, 59) and dirt in (0, 3) and
                (not nulls & 2 or via == 0), 'closed original operand matrix')
        rows.append({'name': fields[0], 'values': values})
    require(len(rows) == COUNT and len({r['name'] for r in rows}) == COUNT and
            sum(r['values'][1] == 1 for r in rows) == 57 and declared['format'] == 1 and
            declared['profiles'] == COUNT and declared['columns'] == COLUMNS and declared['rows'] == rows,
            'complete declared inputs and distinct direct/wrapper inventory')
    return rows


def observations(directory, profiles):
    lines = (directory / 'selected.txt').read_text(encoding='ascii').splitlines()
    require(len(lines) == COUNT + 2 and lines[-1] ==
            'FARMING_COMPLETE rows=118 game-code-replacements=0 jdi-source-writes=0' and
            re.fullmatch(r'FARMING_GUARD source network=0 native=0 execs=0 denied-writes=0 world-writes=[0-9]+', lines[-2]),
            'complete guarded ordinary observation')
    rows = []
    fields = {'index', 'name', 'values', 'maximum', 'result', 'exception', 'stack_before', 'stack_after',
              'center_before', 'center_after', 'above_before', 'above_after', 'remote_after',
              'sounds', 'notifications', 'changes', 'used_delta', 'broken_delta'}
    for i, line in enumerate(lines[:-2]):
        require(line.startswith('FARMING\t'), 'ordinary source row tag')
        row = json.loads(line.split('\t', 1)[1])
        require(type(row) is dict and row.keys() == fields and type(row['index']) is int and
                row['index'] == i and row['name'] == profiles[i]['name'] and
                row['values'] == profiles[i]['values'] and
                all(integer(v) for v in row['values']), 'actual original input/observation binding')
        require(row['result'] in ('true', 'false', 'UNRETURNED') and row['exception'] in
                ('NONE', 'java.lang.NullPointerException', 'java.lang.IllegalStateException') and
                (row['result'] == 'UNRETURNED') == (row['exception'] != 'NONE') and
                integer(row['maximum'], 0) and type(row['remote_after']) is bool,
                'typed ordinary result/failure')
        for key in ('stack_before', 'stack_after'):
            require(type(row[key]) is list and len(row[key]) == 4 and all(integer(v) for v in row[key]),
                    'complete signed Stack projection')
        for key in ('center_before', 'center_after', 'above_before', 'above_after'):
            require(type(row[key]) is list and len(row[key]) == 2 and integer(row[key][0], 0, 255) and
                    integer(row[key][1], 0, 15), 'actual World cell projection')
        require(integer(row['used_delta']) and integer(row['broken_delta']) and
                type(row['sounds']) is list and len(row['sounds']) <= 1 and
                type(row['notifications']) is list and type(row['changes']) is list, 'source effects containers')
        for sound in row['sounds']:
            require(type(sound) is list and len(sound) == 6 and type(sound[0]) is str and
                    all(type(v) is str and re.fullmatch('[0-9a-f]{16}', v) for v in sound[1:4]) and
                    all(type(v) is str and re.fullmatch('[0-9a-f]{8}', v) for v in sound[4:]), 'exact sound argument bits')
        seen = set()
        for change in row['changes']:
            require(type(change) is list and len(change) == 5 and all(integer(v, 0) for v in change), 'typed sparse actual array change')
            chunk, array, index, old, new = change
            require(chunk < 4 and array < 5 and index < (32768 if array == 0 else 256 if array == 4 else 16384) and
                    old <= 255 and new <= 255 and old != new and (chunk, array, index) not in seen,
                    'bounded unique original array changes')
            seen.add((chunk, array, index))
        rows.append(row)
    return rows


def validate(profiles, records, events, pins, live):
    require(len(profiles) == len(records) == COUNT and pins['client_jar_sha256'] == JAR,
            'original packet inventory and archive identity')
    methods = {owner + '.' + m['name'] + m['descriptor']: m
               for owner, c in pins['classes'].items() for m in c['methods']}
    pinned = set()
    for line in live:
        parts = line.split('\t')
        require(len(parts) == 6 and parts[0] == 'FU_LIVE_CODE', 'typed live Code row')
        _, owner, name, descriptor, size, code = parts
        key = owner + '.' + name + descriptor
        require(key not in pinned and key in methods and str(methods[key]['code_bytes']) == size and
                methods[key]['code_sha256'] == code, 'live reached Code equals original archive')
        pinned.add(key)
    require(bool(events) and bool(pinned), 'actual read-only source observations')
    grouped = [[] for _ in records]
    entries, returned = {}, set()
    for seq, e in enumerate(events):
        require(type(e) is dict and type(e.get('seq')) is int and e['seq'] == seq and
                integer(e.get('profile'), 0, COUNT - 1), 'complete ordered observation journal')
        grouped[e['profile']].append(e)
        event = e.get('event')
        require(event in ('entry', 'return', 'field-read', 'field-write', 'exception'), 'closed JDI event kind')
        key, pc = site(e.get('at'))
        if not key.startswith('FarmingObservation$Observer.'):
            require(key in pinned and 0 <= pc < methods[key]['code_bytes'], 'actual pinned source event site')
        if event == 'entry':
            require(integer(e.get('call'), 0) and e['call'] == len(entries) and e['call'] not in entries and
                    pc == 0 and type(e.get('args')) is list, 'unique ordered source method entry')
            site(e.get('caller'))
            entries[e['call']] = e
        elif event == 'return':
            require(integer(e.get('call'), 0) and e['call'] in entries and e['call'] not in returned and
                    entries[e['call']]['profile'] == e['profile'] and site(entries[e['call']]['at'])[0] == key,
                    'observed return belongs to exact source entry')
            returned.add(e['call'])
        elif event == 'field-read':
            require(e.get('field') in ('uu.v', 'uu.w', 'uu.aB', 'uu.bn', 'uu.by', 'fd.B', 'rk.a'),
                    'reached actual original field read')
        elif event == 'field-write':
            require(e.get('field') in ('iz.a', 'iz.d') and integer(e.get('before')) and integer(e.get('after')),
                    'actual signed Stack field store')
        else:
            require(e.get('type') in ('java.lang.NullPointerException', 'java.lang.IllegalStateException') and
                    type(e.get('frames')) is list and e['frames'], 'actual source failure frames')
            for frame in e['frames']:
                frame_key, frame_pc = site(frame)
                require(frame_key in pinned and 0 <= frame_pc < methods[frame_key]['code_bytes'], 'pinned failure frame')
    for i, (profile, row, trace) in enumerate(zip(profiles, records, grouped)):
        require(row['index'] == i and row['name'] == profile['name'] and row['values'] == profile['values'] and trace,
                'one complete original trace per executed row')
        p = profile['values']
        body = ('la' if p[0] == 0 else 'rk') + '.a(Liz;Lgs;Lfd;IIII)Z'
        root = 'iz.a(Lgs;Lfd;IIII)Z' if p[1] else body
        starts = [e for e in trace if e['event'] == 'entry' and site(e['at'])[0] == root]
        bodies = [e for e in trace if e['event'] == 'entry' and site(e['at'])[0] == body]
        require(len(starts) == len(bodies) == 1, 'one actual root and actual Item body')
        stack_ref = None if p[13] & 2 else 'ref:observedStack'
        player_ref = None if p[13] & 4 else 'ref:observedPlayer'
        world_ref = None if p[13] & 1 else 'ref:observedWorld'
        require(bodies[0]['receiver'] == 'ref:observedItem' and bodies[0]['args'] ==
                [stack_ref, player_ref, world_ref, *p[7:10], p[6]], 'actual Item receiver and all executed operands')
        if p[1]:
            require(starts[0]['receiver'] == 'ref:observedStack' and starts[0]['args'] ==
                    [player_ref, world_ref, *p[7:10], p[6]], 'actual wrapper receiver and all executed operands')
        damage = row['maximum'] if p[5] == 1 else row['maximum'] - 1 if p[5] == 2 else row['maximum'] + 1 if p[5] == 3 else p[4]
        require(row['stack_before'] == [p[3], 5, p[2], damage], 'declared actual constructed Stack fields')
        require(site(bodies[0]['caller']) == ('iz.a(Lgs;Lfd;IIII)Z', 14) if p[1] else
                site(bodies[0]['caller'])[0].startswith('FarmingObservation.main'), 'distinct direct/wrapper invocation')
        ends = [e for e in trace if e['event'] == 'return' and e['call'] == starts[0]['call']]
        failures = [e for e in trace if e['event'] == 'exception']
        if row['exception'] == 'NONE':
            require(len(ends) == 1 and type(ends[0].get('value')) is bool and
                    str(ends[0]['value']).lower() == row['result'] and not failures, 'ordinary return equals actual JDI return')
        else:
            require(not ends and len(failures) == 1 and failures[0]['type'] == row['exception'],
                    'ordinary failure equals actual source exception prefix')
        sounds = [e for e in trace if e['event'] == 'entry' and site(e['caller']) == (body, 116)] if p[0] == 0 else []
        require(len(sounds) == len(row['sounds']), 'all original sound calls observed')
        if sounds:
            args = sounds[0]['args']
            require(site(sounds[0]['at'])[0] == 'fd.a(DDDLjava/lang/String;FF)V' and len(args) == 6 and
                    [args[3], *args[:3], *args[4:]] == row['sounds'][0], 'source sound arguments equal actual observer bits')
        setters = [e for e in trace if e['event'] == 'entry' and site(e['at'])[0] == 'fd.f(IIII)Z' and
                   site(e['caller']) == (body, 140 if p[0] == 0 else 59)]
        if p[0] == 1 and p[6] != 1:
            require(row['result'] == 'false' and not [e for e in trace if e['event'] == 'entry' and
                    site(e['at'])[0].startswith('fd.')], 'non-UP Seeds skips all World reads')
        if profile['name'].startswith(('seeds_same_air_false_setter', 'seeds_y127_false_setter', 'hoe_sound_mutates_captured_id')):
            require(len(setters) == 1 and any(e['event'] == 'return' and e['call'] == setters[0]['call'] and
                    e.get('value') is False for e in trace) and row['result'] == 'true', 'actual false setter return is ignored')
        if p[1] and row['result'] == 'true':
            require(row['used_delta'] == 1 and any(e['event'] == 'entry' and site(e['caller']) == (root, 34)
                    for e in trace), 'true original wrapper reaches real Player used statistic')
        elif not p[1]:
            require(row['used_delta'] == 0, 'direct Item body does not invent wrapper statistics')
    return grouped


def read_packet(directory=HERE):
    directory = Path(directory)
    for name, expected in {**FROZEN, **CAPTURED}.items():
        require(hashlib.sha256((directory / name).read_bytes()).hexdigest() == expected, 'retained original product pin: ' + name)
    provenance = json.loads((directory / 'provenance.json').read_text(encoding='utf-8'))
    require(provenance['client_jar_sha256'] == JAR and provenance['profiles'] == COUNT and
            provenance['ordinary_fresh_vms'] == 2 and provenance['read_only_jdi_fresh_vms'] == 1 and
            provenance['inputs_unchanged'] is True and provenance['helpers_unchanged'] is True and
            provenance['source_writes_from_jdi'] == 0 and provenance['target_invocations_from_jdi'] == 0 and
            provenance['frozen_authored_files_sha256'] == FROZEN and provenance['captured_files_sha256'] == CAPTURED,
            'unchanged original capture provenance')
    profiles = inputs(directory)
    rows = observations(directory, profiles)
    pins = json.loads((directory / 'source-pins.json').read_text(encoding='utf-8'))
    events = [json.loads(line) for line in (directory / 'events.jsonl').read_text(encoding='ascii').splitlines()]
    live = (directory / 'live-code.tsv').read_text(encoding='ascii').splitlines()
    grouped = validate(profiles, rows, events, pins, live)
    require(provenance['events'] == len(events) and provenance['live_methods'] == len(live) and
            provenance['exceptions'] == sum(r['exception'] != 'NONE' for r in rows), 'complete captured observation counts')
    return [{'profile': copy.deepcopy(p), 'observed': copy.deepcopy(r), 'events': copy.deepcopy(e)}
            for p, r, e in zip(profiles, rows, grouped)]
