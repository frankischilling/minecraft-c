"""Pure structural validation of unchanged-original NBT parent observations."""
import json
import re

SHA = re.compile(r'[0-9a-f]{64}\Z')
D = re.compile(r'[0-9a-f]{16}\Z')
F = re.compile(r'[0-9a-f]{8}\Z')
AFTER = frozenset(('Q', 'Q_same', 'Q_v_same', 'angles', 'armor', 'armor_same',
                  'base_flags', 'bounds', 'bounds_same', 'client_null', 'dimension',
                  'dimensions', 'head', 'history', 'info_player_input',
                  'info_player_present', 'info_same', 'inventory_dirty',
                  'inventory_selected', 'living', 'main', 'main_same', 'motion',
                  'old_bounds', 'planes', 'player_list_members', 'player_list_same',
                  'position', 'private_bN', 'private_bN_same', 'score',
                  'sleep_offsets', 'sleeping', 'timer', 'v', 'v_same', 'world_J',
                  'world_entities', 'world_players', 'world_same'))
ROW = frozenset(('case', 'name', 'entry', 'after', 'within_call_only',
                'random_preserved', 'input_unchanged', 'old_inventory_preserved',
                'unselected_preserved', 'journal'))
ACTOR_WRITES = frozenset(
    ['gs.' + n for n in ('a', 'b', 'g', 'm', 'u', 'v')]
    + ['ls.' + n for n in ('Y', 'aa', 'ad', 'ae')]
    + ['sn.' + n for n in ('aJ', 'aK', 'aL', 'aM', 'aN', 'aO', 'aP', 'aQ', 'aR',
                           'aS', 'aT', 'aU', 'aV', 'aX', 'bf', 'bg', 'bh', 'bk',
                           'bl', 'bm', 'bn', 'bv', 'bz', 'bF', 'bG', 'bH', 'bI')])


def require(value, message):
    if not value:
        raise ValueError(message)


def pairs(items):
    result = {}
    for name, value in items:
        require(name not in result, 'duplicate JSON key')
        result[name] = value
    return result


def loads(text):
    def invalid(value):
        raise ValueError('nonfinite JSON: ' + value)
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid)


def exact(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return a.keys() == b.keys() and all(exact(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
    return a == b


def keys(value, expected, label):
    require(type(value) is dict and set(value) == set(expected), label)


def integer(value):
    return type(value) is int and -2147483648 <= value <= 2147483647


def vector(value, count, item):
    return type(value) is list and len(value) == count and all(item(x) for x in value)


def coordinate(value):
    return value is None or vector(value, 3, integer)


def words(value, count, pattern):
    return vector(value, count, lambda x: type(x) is str and pattern.fullmatch(x) is not None)


def scalar_tag(value):
    require(type(value) is dict and type(value.get('kind')) is int, 'actual NBT tag')
    kind = value['kind']
    if kind == 10:
        keys(value, ('kind', 'values'), 'compound tag keys')
        require(type(value['values']) is dict, 'compound entries')
        for child in value['values'].values():
            scalar_tag(child)
    elif kind == 9:
        keys(value, ('kind', 'element_type', 'values'), 'list tag keys')
        require(type(value['element_type']) is int and type(value['values']) is list,
                'list tag declaration')
        for child in value['values']:
            scalar_tag(child)
            require(child['kind'] == value['element_type'], 'typed list element')
    else:
        keys(value, ('kind', 'value'), 'scalar tag keys')
        if kind in (1, 2, 3):
            lo, hi = {1: (-128, 127), 2: (-32768, 32767), 3: (-2147483648, 2147483647)}[kind]
            require(type(value['value']) is int and lo <= value['value'] <= hi, 'signed tag value')
        else:
            require(kind in (5, 6), 'declared scalar kind')
            keys(value['value'], ('F',) if kind == 5 else ('D',), 'floating tag words')
            require(words(list(value['value'].values()), 1, F if kind == 5 else D), 'floating tag bits')


def strip_tag(value):
    require(type(value) is dict, 'actual raw tag')
    return {k: ({n: strip_tag(v) for n, v in part.items()} if k == 'values' and type(part) is dict
                else [strip_tag(v) for v in part] if k == 'values' else part)
            for k, part in value.items() if k != 'class'}


def profiles(data, tsv):
    keys(data, ('schema', 'columns', 'profiles'), 'profile schema keys')
    require(data['schema'] == 'PLAYER_SLEEP_NBT_MATRIX_INPUTS_V1' and
            type(data['profiles']) is list and len(data['profiles']) == 40 and
            type(data['columns']) is list and len(data['columns']) == 26 and
            len(set(data['columns'])) == 26, 'fixed forty full declarations')
    lines = tsv.splitlines()
    require(len(lines) == 40, 'complete consumed profile TSV')
    for number, (row, line) in enumerate(zip(data['profiles'], lines)):
        keys(row, ('case', 'name', 'control', 'entry', 'fixture', 'input_tree'), 'profile row keys')
        require(type(row['case']) is int and row['case'] == number and
                type(row['name']) is str and re.fullmatch('[a-z0-9_]+', row['name']) is not None,
                'ordered named profile')
        keys(row['control'], data['columns'], 'complete consumed controls')
        require(line == ' '.join(str(row['control'][key]) for key in data['columns']) and
                row['control']['case'] == number and row['control']['name'] == row['name'] and
                row['entry'] == ('FD_ATTACH' if row['control']['entry'] else 'SN_READ'),
                'full declared input and actual consumed TSV agreement')
        require(all(integer(value) for key, value in row['control'].items() if key != 'name'),
                'typed control scalars')
        scalar_tag(row['input_tree'])
    return data['profiles']


def selected(value, number, *, raw=False):
    keys(value, AFTER if raw or number != 35 else AFTER - {'history'}, 'closed selected after fields')
    for name in ('position', 'motion'):
        require(words(value[name], 3, D), name + ' raw double words')
    for name, count in (('angles', 4), ('dimensions', 3), ('sleep_offsets', 2)):
        require(words(value[name], count, F), name + ' raw float words')
    for name in ('bounds', 'old_bounds'):
        require(value[name] is None or words(value[name], 6, D), name + ' six actual planes')
    if 'history' in value:
        require(vector(value['history'], 6, lambda x: type(x) is dict and set(x) == {'D'} and
                       type(x['D']) is str and D.fullmatch(x['D']) is not None), 'source history words')
    for name in ('Q', 'v', 'private_bN'):
        require(coordinate(value[name]), 'nullable coordinate ' + name)
    for name in ('dimension', 'inventory_selected', 'score', 'timer', 'world_entities', 'world_players'):
        require(integer(value[name]), 'source integer ' + name)
    for name in ('Q_same', 'Q_v_same', 'armor_same', 'bounds_same', 'client_null',
                 'info_player_input', 'info_player_present', 'info_same', 'inventory_dirty',
                 'main_same', 'player_list_same', 'private_bN_same', 'sleeping', 'v_same',
                 'world_J', 'world_same'):
        require(type(value[name]) is bool, 'source Boolean ' + name)
    require(vector(value['living'], 4, integer) and vector(value['head'], 2, integer),
            'actual Living fields and head cell')
    keys(value['base_flags'], ('bk', 'bv', 'bz', 'aX'), 'base source flags')
    keys(value['base_flags']['bk'], ('F',), 'FallDistance raw float')
    require(words([value['base_flags']['bk']['F']], 1, F) and
            integer(value['base_flags']['bv']) and integer(value['base_flags']['bz']) and
            type(value['base_flags']['aX']) is bool, 'typed base source flags')
    seen_stacks = {}
    for name, count in (('main', 36), ('armor', 4)):
        require(type(value[name]) is list and len(value[name]) == count, 'complete inventory slots')
        for stack in value[name]:
            if stack is None:
                continue
            keys(stack, ('identity', 'fields'), 'actual Stack identity/values')
            require(type(stack['identity']) is int and stack['identity'] > 0, 'observer Stack identity')
            keys(stack['fields'], ('iz.a', 'iz.b', 'iz.c', 'iz.d'), 'all original Stack fields')
            require(all(integer(x) for x in stack['fields'].values()), 'typed Stack fields')
            if stack['identity'] in seen_stacks:
                require(exact(seen_stacks[stack['identity']], stack['fields']), 'aliased actual Stack has one state')
            seen_stacks[stack['identity']] = stack['fields']
    keys(value['planes'], ('blocks', 'metadata', 'sky', 'block_light', 'height'), 'full five planes')
    require(all(type(x) is str and SHA.fullmatch(x) for x in value['planes'].values()), 'full plane SHA256')
    require(type(value['player_list_members']) is list and
            len(value['player_list_members']) == value['world_players'], 'actual Player list members')
    for member in value['player_list_members']:
        require(member == {'class': 'dc', 'actor': True}, 'actual published receiver identity')


def methods(source, helper):
    result = {}
    for name, owner in {**source['classes'], **helper}.items():
        for method in owner['methods']:
            if 'code_length' in method:
                key = (name, method['name'], method['descriptor'])
                require(key not in result, 'unique pinned method')
                result[key] = method
    return result


def site(value, index):
    keys(value, ('class', 'method', 'descriptor', 'pc'), 'structured source site')
    key = (value['class'], value['method'], value['descriptor'])
    require(key in index and type(value['pc']) is int and
            0 <= value['pc'] < index[key]['code_length'], 'source Code PC in actual pinned method')


def finite(data, declared, source, helper):
    keys(data, ('schema', 'rows', 'live_code', 'initializer_prefix', 'scope'), 'finite graph keys')
    require(data['schema'] == 'PLAYER_SLEEP_NBT_MATRIX_V1' and len(data['rows']) == 40,
            'actual forty-row original graph')
    require(exact(data['scope'], {'original_full_parent_components': True, 'native': False,
                             'loaded_client': False, 'original_draw': False,
                             'constructor_entropy_cross_vm': False, 'whole_restart': False}),
            'bounded original-only qualification scope')
    require(len(data['initializer_prefix']) == 2, 'exact retained physical prefix pair')
    for number, item in enumerate(data['initializer_prefix']):
        require(item['text'] == ('16 achievements', '151 recipes')[number] and
                item['raw_stdout_line'] == 35 + number and
                item['execution_extent'] == 'UNOBSERVED_UNQUALIFIED' and
                item['origin_proof'] == 'static_unchanged_class_and_Code_only' and
                item['live_initializer_entry_claimed'] is False,
                'static prefix origins have no live execution-extent qualification')
    index = methods(source, helper)
    codes = set()
    for code in data['live_code']:
        keys(code, ('event', 'class', 'method', 'descriptor', 'bytes', 'sha256'), 'live Code record')
        key = (code['class'], code['method'], code['descriptor'])
        require(code['event'] == 'code' and key in index and key not in codes and
                index[key]['code_length'] == code['bytes'] and
                index[key]['code_sha256'] == code['sha256'], 'actual entered-body Code pin')
        codes.add(key)
    require(len(codes) == 62 and ('PlayerNbtSleepMatrixObservation$SourceWorker', 'run', '()V') in codes,
            'sixty-one entered original bodies plus authored worker')
    returns = call_count = event_count = 0
    for number, (row, profile) in enumerate(zip(data['rows'], declared)):
        keys(row, ROW, 'finite row keys')
        require(type(row['case']) is int and row['case'] == number and row['name'] == profile['name'] and
                row['entry'] == profile['entry'], 'original profile row identity')
        selected(row['after'], number)
        require(row['within_call_only'] == (['history'] if number == 35 else []) and
                row['random_preserved'] is True and row['input_unchanged'] is True and
                row['old_inventory_preserved'] is True, 'within-call versus absolute comparison scope')
        journal = row['journal']
        keys(journal, ('event', 'case', 'root_returned', 'first_exception', 'calls', 'events',
                       'jdi_target_invocations', 'jdi_source_writes', 'jdi_forced_returns'), 'closed source journal')
        require(journal['event'] == 'journal' and type(journal['case']) is int and journal['case'] == number and
                type(journal['root_returned']) is bool and
                journal['root_returned'] is (journal['first_exception'] is None) and
                all(type(journal[key]) is int and journal[key] == 0 for key in
                    ('jdi_target_invocations', 'jdi_source_writes', 'jdi_forced_returns')),
                'actual parent return or first throw and passive observation')
        calls, events = journal['calls'], journal['events']
        require(type(calls) is list and 0 < len(calls) <= 512 and
                type(events) is list and len(events) <= 8192, 'actual bounded call/event arrays')
        entered, completed, thrown = set(), set(), []
        for i, call in enumerate(calls):
            keys(call, ('id', 'caller', 'site', 'receiver', 'arguments', 'entered', 'returned',
                        'completed', 'exception', 'result'), 'source call keys')
            require(type(call['id']) is int and call['id'] == i and
                    type(call['completed']) is bool, 'source call identity/completion')
            site(call['site'], index)
            site(call['caller'], index)
            require((call['site']['class'], call['site']['method'], call['site']['descriptor']) in codes,
                    'every actual entered source Code retained')
        for tick, event in enumerate(events):
            require(type(event) is dict and type(event['tick']) is int and event['tick'] == tick,
                    'exact source total order')
            kind = event['kind']
            if kind in ('ENTER', 'RETURN'):
                require(type(event['call']) is int and 0 <= event['call'] < len(calls), 'event actual call')
                i = event['call']
                if kind == 'ENTER':
                    require(i not in entered and calls[i]['entered'] == tick, 'actual call entered once')
                    entered.add(i)
                else:
                    require(i in entered and i not in completed and calls[i]['returned'] == tick and
                            calls[i]['completed'] is True, 'actual call returned once')
                    completed.add(i)
            elif kind == 'STORE':
                site(event['site'], index)
                require(all(name in event for name in ('owner', 'field', 'receiver', 'before', 'value')),
                        'actual ordered source store')
            elif kind == 'CHECKPOINT93':
                require(event['site'] == {'class': 'gs', 'method': 'a', 'descriptor': '(Lnu;)V', 'pc': 93},
                        'passive original93 checkpoint only')
            elif kind == 'EXCEPTION':
                site(event['site'], index)
                if event['catch'] is not None:
                    site(event['catch'], index)
                for frame in event['stack']:
                    site(frame, index)
                thrown.append(event)
            else:
                raise ValueError('unknown source event')
        require(entered == set(range(len(calls))) and completed == {i for i, c in enumerate(calls) if c['completed']},
                'complete actual entered/returned event coverage')
        require(len(thrown) == (0 if journal['root_returned'] else 1) and
                (not thrown or thrown[0]['class'] == journal['first_exception']), 'actual first source exception')
        require(calls[0]['completed'] is journal['root_returned'] and
                calls[0]['receiver']['identity'] == ('world' if row['entry'] == 'FD_ATTACH' else 'actor'),
                'source root return is distinct from authored catch')
        wake = [c for c in calls if c['site']['class'] == 'gs' and c['site']['method'] == 'a' and
                c['site']['descriptor'] == '(ZZZ)V']
        checkpoints = [e for e in events if e['kind'] == 'CHECKPOINT93']
        require(len(wake) == len(checkpoints) <= 1, 'genuine reached or skipped wake93')
        if wake:
            require(exact(wake[0]['arguments'], [True, True, False]) and
                    wake[0]['caller'] == {'class': 'gs', 'method': 'a', 'descriptor': '(Lnu;)V', 'pc': 93},
                    'actual NBT wake flags/source caller')
        returns += int(journal['root_returned'])
        call_count += len(calls)
        event_count += len(events)
    require((returns, call_count, event_count) == (29, 5393, 13062), 'actual canonical source coverage')


def witnesses(data, canonical, declared, source):
    keys(data, ('schema', 'observations', 'comparison_scope'), 'within-call witness keys')
    require(data['schema'] == 'PLAYER_SLEEP_NBT_PARENT_WITNESSES_V1' and
            data['comparison_scope'] == {'constructor_entropy': 'within_call_only',
                                         'Random': 'within_call_only',
                                         'source_unselected_fields': 'within_call_preservation',
                                         'identity_numbers': 'equivalence_only'}, 'witness qualification scope')
    require(type(data['observations']) is list and len(data['observations']) == 2, 'two independent actual VMs')
    counts = {'actor_fields': 156, 'world_fields': 43, 'provider_fields': 8}
    allowed = {'actor_fields': ACTOR_WRITES, 'world_fields': {'fd.J'}, 'provider_fields': set()}
    owners = {'actor_fields': ('sn', 'ls', 'gs', 'dc'), 'world_fields': ('fd',), 'provider_fields': ('xa',)}
    expected_fields = {group: {owner + '.' + field['name'] for owner in classes
                               for field in source['classes'][owner]['fields'] if not field['access'] & 8}
                       for group, classes in owners.items()}
    for vm, observation in enumerate(data['observations']):
        keys(observation, ('vm', 'rows'), 'independent witness VM keys')
        require(type(observation['vm']) is int and observation['vm'] == vm and
                len(observation['rows']) == 40, 'complete actual witness VM')
        for number, (row, selected_row, profile) in enumerate(zip(observation['rows'], canonical['rows'], declared)):
            keys(row, ('case', 'before', 'after'), 'within-call witness row keys')
            require(type(row['case']) is int and row['case'] == number, 'ordered witness profile')
            before, after = row['before'], row['after']
            selected(before['selected'], number, raw=True)
            selected(after['selected'], number, raw=True)
            projected = after['selected'] if number != 35 else {k: v for k, v in after['selected'].items() if k != 'history'}
            require(exact(projected, selected_row['after']), 'actual captured selected values retained unchanged')
            if number == 35:
                require(exact(before['selected']['history'], after['selected']['history']),
                        'no-reader constructor history preserved within call only')
            require(exact(before['random'], after['random']), 'actual per-call Random preservation')
            require(exact(before['input_tree'], after['input_tree']) and
                    exact(strip_tag(before['input_tree']), profile['input_tree']), 'declared actual immutable input tags')
            require(exact(before['old_main'], after['old_main']) and exact(before['old_armor'], after['old_armor']),
                    'old arrays and Stack values preserved within call')
            require(exact(before['old_main'], before['selected']['main']) and
                    exact(before['old_armor'], before['selected']['armor']) and
                    before['selected']['main'][0]['identity'] == before['selected']['armor'][0]['identity'],
                    'declared initial main/armor alias and observed old arrays')
            for name, count in counts.items():
                a, b = before[name], after[name]
                require(type(a) is dict and type(b) is dict and len(a) == len(b) == count and a.keys() == b.keys(),
                        'complete original instance-field inventory ' + name)
                require(set(a) == expected_fields[name], 'actual declared source field names ' + name)
                unchanged = [field for field in a if field not in allowed[name]]
                require(all(exact(a[field], b[field]) for field in unchanged) and
                        selected_row['unselected_preserved'][name] == len(unchanged),
                        'actual source-unselected field preservation ' + name)
            b, a = before['selected'], after['selected']
            for field, flag in (('gs.b', 'Q_same'), ('gs.v', 'v_same'), ('gs.bN', 'private_bN_same')):
                require(exact(before['actor_fields'][field], after['actor_fields'][field]) is a[flag],
                        'actual before/after coordinate identity relation')
            require(exact(after['actor_fields']['gs.b'], after['actor_fields']['gs.v']) is a['Q_v_same'],
                    'actual saved-Spawn and sleeping-coordinate alias relation')
            for field, flag in (('ix.a', 'main_same'), ('ix.b', 'armor_same')):
                require(exact(before['inventory_fields'][field], after['inventory_fields'][field]) is a[flag],
                        'actual Inventory array replacement relation')
            control = profile['control']
            require(b['client_null'] is True and a['client_null'] is True and
                    b['world_same'] is (not bool(control['world_null'])) and
                    b['bounds_same'] is (not bool(control['bounds_null'])) and
                    b['Q'] == profile['fixture']['old_Q_setter_input'] and b['score'] == 900 and
                    b['world_players'] == b['world_entities'] == control['published_sleeper'] and
                    b['info_player_input'] is bool(control['info_tag']), 'declared receiver input/topology')
            if control['published_sleeper']:
                require(b['sleeping'] is True and b['world_J'] is True and b['v'] == [8, 64, 8],
                        'genuine prepared published sleeper')
            else:
                require(b['position'] == profile['fixture']['actor_initial_position'], 'actual declared pre-call pose')
