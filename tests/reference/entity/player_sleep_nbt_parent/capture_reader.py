"""Reproduction capture reader for the forty full NBT parent components.

No expected post-state is authored here. The declared before inputs and genuine
call/return/store/throw journal are checked before the two actual projections
are compared. Raw constructor fields remain available in each physical stream.
"""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
CASE_COUNT = 40
MAX_BYTES = 16 * 1024 * 1024
PREFIX = (
    {'text': '16 achievements', 'class': 'ep', 'method': '<clinit>', 'descriptor': '()V',
     'class_sha256': '30daee352142c88d5cdffb12b1a14b410d4baf866e59ac79f60a1b336258bdac',
     'bytes': 455, 'sha256': '1765d4ad71261c2e39ead99c3e6b22a1e509cfe48ff890b3ab3db46783392502',
     'println_pc': 451},
    {'text': '151 recipes', 'class': 'hk', 'method': '<init>', 'descriptor': '()V',
     'class_sha256': 'b5ca5cf35bd1e32b549cfdcebbb6731f69bc3c0361b79443be0271ff3c3444af',
     'bytes': 3238, 'sha256': '7676a3b5efc25c9f8eb7ad0cc65810662be3c05732250fa00c80f411f633bced',
     'println_pc': 3234})
ACTOR_WRITES = frozenset(
    ['gs.' + n for n in ('a', 'b', 'g', 'm', 'u', 'v')]
    + ['ls.' + n for n in ('Y', 'aa', 'ad', 'ae')]
    + ['sn.' + n for n in ('aJ', 'aK', 'aL', 'aM', 'aN', 'aO', 'aP', 'aQ', 'aR',
                           'aS', 'aT', 'aU', 'aV', 'aX', 'bf', 'bg', 'bh', 'bk',
                           'bl', 'bm', 'bn', 'bv', 'bz', 'bF', 'bG', 'bH', 'bI')])


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def loads(text):
    def invalid(value):
        raise ValueError('nonfinite JSON: ' + value)
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid)


def load(path):
    return loads(Path(path).read_text(encoding='utf-8'))


def exact(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return a.keys() == b.keys() and all(exact(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
    return a == b


def clean(value):
    if type(value) is dict:
        return {k: clean(v) for k, v in value.items()
                if k not in ('raw_object_id', 'source_thread_id')}
    if type(value) is list:
        return [clean(v) for v in value]
    return value


def tag_declaration(value):
    require(type(value) is dict, 'actual tag object')
    return {k: ({n: tag_declaration(v) for n, v in part.items()} if k == 'values' and type(part) is dict
                else [tag_declaration(v) for v in part] if k == 'values' else part)
            for k, part in value.items() if k != 'class'}


def profiles():
    data = load(HERE / 'profiles.json')
    require(set(data) == {'schema', 'columns', 'profiles'} and
            data['schema'] == 'PLAYER_SLEEP_NBT_MATRIX_INPUTS_V1', 'profile schema')
    rows = data['profiles']
    require(type(rows) is list and len(rows) == CASE_COUNT, 'fixed profile count')
    lines = (HERE / 'profiles.tsv').read_text(encoding='ascii').splitlines()
    require(len(lines) == CASE_COUNT, 'complete profile TSV')
    for number, (row, line) in enumerate(zip(rows, lines)):
        require(set(row) == {'case', 'name', 'control', 'entry', 'fixture', 'input_tree'} and
                type(row['case']) is int and row['case'] == number and
                re.fullmatch('[a-z0-9_]+', row['name']) is not None, 'ordered profile declaration')
        require(line == ' '.join(str(row['control'][k]) for k in data['columns']),
                'actual consumed TSV agrees with full input declaration')
        require(row['control']['case'] == number and row['control']['name'] == row['name'],
                'profile controls identify row')
    return rows


def checked_fields(before, after, allowed, label):
    require(type(before) is dict and type(after) is dict and before.keys() == after.keys(),
            label + ' complete instance-field inventory')
    unchanged = [name for name in before if name not in allowed]
    require(all(exact(before[name], after[name]) for name in unchanged),
            label + ' source-unselected field preservation')
    return len(unchanged)


def code_inventory(records, source, helpers):
    codes = [r for r in records if r.get('event') == 'code']
    found = set()
    for code in codes:
        require(set(code) == {'event', 'class', 'method', 'descriptor', 'bytes', 'sha256'},
                'closed live Code record')
        name = code['class']
        require(name in source['classes'] or name in helpers, 'genuine class or exact authored worker')
        selected = source['classes'][name] if name in source['classes'] else helpers[name]
        methods = [m for m in selected['methods'] if m['name'] == code['method'] and
                   m['descriptor'] == code['descriptor']]
        require(len(methods) == 1 and methods[0].get('code_sha256') == code['sha256'] and
                methods[0].get('code_length') == code['bytes'], 'actual live Code pin')
        key = (name, code['method'], code['descriptor'])
        require(key not in found, 'unique live Code record')
        found.add(key)
    require(('PlayerNbtSleepMatrixObservation$SourceWorker', 'run', '()V') in found,
            'actual source worker Code retained')
    return codes, found


def projection(folder, source, helpers, class_names):
    folder = Path(folder)
    for name in ('target.stdout', 'target.stderr', 'journal.jsonl'):
        require((folder / name).stat().st_size <= MAX_BYTES, 'physical stream bound')
    lines = (folder / 'target.stdout').read_text(encoding='utf-8').splitlines()
    require(lines, 'actual target output')
    marker = re.fullmatch(r'NBT_SLEEP_MATRIX_COMPLETE cases=40 workers_reaped=40 returned=(\d+) failed=(\d+) network=0 native=0 writes=0 exec=0 GUI=0', lines[-1])
    require(marker is not None, 'actual complete/reaped-source-worker marker')
    returned, failed = map(int, marker.groups())
    require(returned + failed == CASE_COUNT, 'all actual root outcomes')
    class_lines = ['CLASS ' + name + ' ' + str(source['classes'][name]['class_bytes']) + ' ' +
                   source['classes'][name]['class_sha256'] for name in class_names]
    require([line for line in lines if line.startswith('CLASS ')] == class_lines,
            'actual unchanged archive class origins/bytes')
    raw = [loads(line[len('NBT_SLEEP_MATRIX '):]) for line in lines if line.startswith('NBT_SLEEP_MATRIX ')]
    expected_prefix = [entry['text'] for entry in PREFIX]
    require(lines[:len(class_lines)] == class_lines and
            lines[len(class_lines):len(class_lines) + 2] == expected_prefix and
            all(line.startswith('NBT_SLEEP_MATRIX ') for line in lines[len(class_lines) + 2:-1]) and
            len(raw) == CASE_COUNT and len(lines) == len(class_lines) + 2 + CASE_COUNT + 1,
            'closed actual target output protocol')
    initializer_prefix = []
    for i, origin in enumerate(PREFIX):
        owner = source['classes'][origin['class']]
        methods = [m for m in owner['methods'] if m['name'] == origin['method'] and
                   m['descriptor'] == origin['descriptor']]
        require(owner['class_sha256'] == origin['class_sha256'] and len(methods) == 1 and
                methods[0]['code_length'] == origin['bytes'] and
                methods[0]['code_sha256'] == origin['sha256'], 'exact static initializer-output Code pins')
        initializer_prefix.append({**origin, 'raw_stdout_line': len(class_lines) + i + 1,
                                   'role': 'physical_stdout_prefix_static_formatter_origin',
                                   'origin_proof': 'static_unchanged_class_and_Code_only',
                                   'execution_extent': 'UNOBSERVED_UNQUALIFIED',
                                   'live_initializer_entry_claimed': False})
    stderr = (folder / 'target.stderr').read_text(encoding='utf-8').splitlines()
    require(len(stderr) == 4 and all(line.startswith('WARNING:') for line in stderr) and
            'System::setSecurityManager' in '\n'.join(stderr), 'only retained JDK warnings')
    records = [loads(line) for line in (folder / 'journal.jsonl').read_text(encoding='utf-8').splitlines()]
    require(all(type(r) is dict and r.get('event') in ('journal', 'code', 'complete') for r in records),
            'closed journal record kinds')
    codes, pinned = code_inventory(records, source, helpers)
    journals = [r for r in records if r['event'] == 'journal']
    complete = [r for r in records if r['event'] == 'complete']
    require(len(journals) == CASE_COUNT and len(complete) == 1 and
            complete[0] == {'event': 'complete', 'profiles': CASE_COUNT,
                            'source_roots_returned': returned, 'source_roots_failed': failed,
                            'jdi_target_invocations': 0, 'jdi_source_writes': 0, 'jdi_forced_returns': 0},
            'actual parent completion extent')
    rows = []
    actual_returns = 0
    for number, (row, journal, declared) in enumerate(zip(raw, journals, profiles())):
        require(set(row) == {'case', 'name', 'entry', 'before', 'after', 'worker'} and
                row['case'] == journal['case'] == number and row['name'] == declared['name'] and
                row['entry'] == declared['entry'], 'actual profile/entry identity')
        worker = row['worker']
        require(set(worker) == {'name', 'alive', 'returned', 'failure'} and
                worker['name'] == 'original-nbt-matrix-' + str(number) and worker['alive'] is False and
                type(worker['returned']) is bool and journal['root_returned'] is worker['returned'] and
                worker['returned'] is (worker['failure'] is None) and
                journal['first_exception'] == worker['failure'], 'actual worker/source return versus first throw')
        actual_returns += int(worker['returned'])
        require(all(type(journal[n]) is int and journal[n] == 0 for n in
                    ('jdi_target_invocations', 'jdi_source_writes', 'jdi_forced_returns')),
                'passive debugger counters')
        events, calls = journal['events'], journal['calls']
        require(0 < len(calls) <= 512 and len(events) <= 8192 and
                [e['tick'] for e in events] == list(range(len(events))), 'bounded actual total event order')
        root = calls[0]
        is_fd = declared['entry'] == 'FD_ATTACH'
        require(root['receiver']['identity'] == ('world' if is_fd else 'actor') and
                root['site']['class'] == ('fd' if is_fd else 'sn') and
                root['site']['method'] == ('a' if is_fd else 'e') and
                root['site']['descriptor'] == ('(Lgs;)V' if is_fd else '(Lnu;)V') and
                root['caller']['class'] == 'PlayerNbtSleepMatrixObservation$SourceWorker',
                'actual root receiver and ordinary authored invocation')
        for i, call in enumerate(calls):
            site = call['site']
            require(call['id'] == i and (site['class'], site['method'], site['descriptor']) in pinned,
                    'every entered original body pinned')
        require(root['completed'] is worker['returned'], 'root actual return is never helper catch completion')
        exceptions = [e for e in events if e['kind'] == 'EXCEPTION']
        require(len(exceptions) == (0 if worker['returned'] else 1) and
                (not exceptions or exceptions[0]['class'] == worker['failure']), 'actual first exception extent')
        checkpoints = [e for e in events if e['kind'] == 'CHECKPOINT93']
        wake = [c for c in calls if c['site']['class'] == 'gs' and
                c['site']['method'] == 'a' and c['site']['descriptor'] == '(ZZZ)V']
        require(len(checkpoints) == len(wake) <= 1, 'actual reached93 versus non-waking/early-fault path')
        if wake:
            require(exact(wake[0]['arguments'], [True, True, False]) and
                    wake[0]['caller'] == {'class': 'gs', 'method': 'a', 'descriptor': '(Lnu;)V', 'pc': 93},
                    'genuine93 wake invocation/flags')
        before, after = row['before'], row['after']
        require(len(before['actor_fields']) == len(after['actor_fields']) == 156 and
                len(before['world_fields']) == len(after['world_fields']) == 43 and
                len(before['provider_fields']) == len(after['provider_fields']) == 8,
                'complete original instance-field snapshots')
        require(exact(tag_declaration(before['input_tree']), declared['input_tree']) and
                exact(before['input_tree'], after['input_tree']), 'exact declared input tags and unchanged input')
        require(exact(before['random'], after['random']), 'within-call Actor/World Random preservation')
        require(exact(before['old_main'], after['old_main']) and exact(before['old_armor'], after['old_armor']),
                'old Inventory arrays/Stack values preserved after replacement')
        p, setup, b, a = declared['control'], declared['fixture'], before['selected'], after['selected']
        require(b['client_null'] is True and a['client_null'] is True and
                b['world_same'] is (not bool(p['world_null'])) and
                b['bounds_same'] is (not bool(p['bounds_null'])) and
                b['score'] == 900 and b['Q'] == setup['old_Q_setter_input'] and
                b['private_bN'] == [-11, 65, 13] and
                b['world_players'] == b['world_entities'] == p['published_sleeper'] and
                b['info_player_input'] is bool(p['info_tag']) and
                b['info_player_present'] is bool(p['info_tag']), 'actual declared receiver identity/topology')
        require(before['world_fields']['fd.B'] is bool(p['remote']), 'actual remote input')
        if p['published_sleeper']:
            require(b['sleeping'] is True and b['world_J'] is True and b['v'] == [8, 64, 8],
                    'actual genuine sleeper preparation, not assigned J')
        else:
            require(b['position'] == setup['actor_initial_position'], 'actual declared pre-call position')
        preservation = {
            'actor_fields': checked_fields(before['actor_fields'], after['actor_fields'], ACTOR_WRITES, 'Actor'),
            'world_fields': checked_fields(before['world_fields'], after['world_fields'], {'fd.J'}, 'World'),
            'provider_fields': checked_fields(before['provider_fields'], after['provider_fields'], set(), 'Provider')}
        require(set(a['planes']) == {'blocks', 'metadata', 'sky', 'block_light', 'height'} and
                all(re.fullmatch('[0-9a-f]{64}', value) for value in a['planes'].values()),
                'all five actual full Chunk planes')
        selected = clean(a)
        within_call_only = []
        if number == 35:
            # No reader is entered, so constructor-derived historical XYZ are
            # tested for within-call preservation, not cross-VM constants.
            require(exact(b['history'], a['history']), 'no-reader history remains unchanged within call')
            selected.pop('history')
            within_call_only.append('history')
        rows.append({'case': number, 'name': row['name'], 'entry': row['entry'],
                     'after': selected, 'within_call_only': within_call_only,
                     'random_preserved': True, 'input_unchanged': True,
                     'old_inventory_preserved': True, 'unselected_preserved': preservation,
                     'journal': clean(journal)})
    require(actual_returns == returned, 'complete marker equals all actual row outcomes')
    return {'schema': 'PLAYER_SLEEP_NBT_MATRIX_V1', 'rows': rows, 'live_code': codes,
            'initializer_prefix': initializer_prefix,
            'scope': {'original_full_parent_components': True, 'native': False,
                      'loaded_client': False, 'original_draw': False,
                      'constructor_entropy_cross_vm': False, 'whole_restart': False}}

