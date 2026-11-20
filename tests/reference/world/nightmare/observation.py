"""Explicit readback of recorded original fields; never reads native implementation text."""
import copy
import struct


MULTIPLIER = 0x5DEECE66D
MASK = (1 << 48) - 1
INVERSE = pow(MULTIPLIER, -1, 1 << 48)


def need(condition, message):
    if not condition:
        raise ValueError(message)


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def fhex(value):
    return struct.pack('>f', value).hex()


def pitch(seed):
    """The last two private nextFloat outputs, recovered from the recorded final state."""
    second = int(seed, 16)
    first = ((second - 11) * INVERSE) & MASK
    return fhex(f32(f32(f32((first >> 24) / 16777216.0 -
                           (second >> 24) / 16777216.0) * f32(.2)) + f32(1.0)))


def own_random_checks(row):
    """Independent default-constructor streams stay independent; their operations are checked."""
    before = {p['id']: p for p in row['before']['players']}
    for player in row['after']['players']:
        need(player['random'] == before[player['id']]['random'], 'Player private Random preserved')
    entities = row['after']['entities']
    sounds = [event for event in row['notifications'] if event['event'] == 'sound']
    for event in sounds:
        matches = [entity for entity in entities if entity['id'] >= 1000 and
                   entity['position'] == event['position'] and
                   {'cn': 'mob.spider', 'uz': 'mob.zombie', 'fr': 'mob.skeleton'}.get(entity['class']) == event['name']]
        need(len(matches) == 1, 'actual ambient receiver identity')
        receiver = matches[0]
        need(receiver['random'][1:] == [False, '0000000000000000'] and
             0 <= int(receiver['random'][0], 16) <= MASK and
             pitch(receiver['random'][0]) == event['pitch'], 'private Random ambient pitch formula')


def deterministic(row):
    """Selected equalities with only explicitly independent constructor entropy removed."""
    own_random_checks(row)
    value = copy.deepcopy(row)
    for phase in ('before', 'after'):
        for group in ('players', 'entities'):
            for entity in value[phase][group]:
                del entity['random']
    for event in value['notifications']:
        if event['event'] == 'sound':
            del event['pitch']
    return value


def project_state(state, entry):
    known = {entity['identity']: entity['fields']['sn.aD']
             for entity in state['players'] + state['entities']}
    entries = {entity['fields']['sn.aD']: entity for entity in entry['players']}

    def ref(value):
        return None if value is None else value['identity']

    def entity(value):
        fields = value['fields']
        result = {'class': value['class'], 'id': fields['sn.aD'],
                  'position': [fields['sn.' + field]['D'] for field in ('aM', 'aN', 'aO')],
                  'motion': [fields['sn.' + field]['D'] for field in ('aP', 'aQ', 'aR')],
                  'history': [fields['sn.' + field]['D'] for field in ('aJ', 'aK', 'aL', 'bl', 'bm', 'bn')],
                  'angles': [fields['sn.' + field]['F'] for field in ('aS', 'aT', 'aU', 'aV')],
                  'dimensions': [fields['sn.' + field]['F'] for field in ('bg', 'bh', 'bf')],
                  'health': fields['ls.Y'], 'random': value['random'],
                  'mount_id': None if value['mount'] is None else known[value['mount']],
                  'rider_id': None if value['rider'] is None else known[value['rider']],
                  'bounds': value['bounds']}
        if 'bed' in value:
            initial = entries[result['id']]['fields']
            result.update(sleeping=fields['gs.u'], timer=value['timer'], bed=value['bed'],
                          spawn=value['spawn'], private_bN=value['private_coordinate'])
            for name, field in (('bounds', 'sn.aW'), ('bed', 'gs.v'), ('spawn', 'gs.b'), ('private_bN', 'gs.bN')):
                result[name + '_same'] = ref(fields[field]) == ref(initial[field])
            result['spawn_bed_same'] = ref(fields['gs.b']) == ref(fields['gs.v'])
        return result

    def ids(value):
        return None if value is None else [known[ref(member)] for member in value['members']]

    return {'players': [entity(value) for value in state['players']],
            'entities': [entity(value) for value in state['entities']],
            'world_random': state['world_random'], 'math_random': state['math_random'],
            'time': state['info_fields']['ei.e']['J'], 'sky_darkness': state['world_fields']['fd.f'],
            'world_sleeping': state['world_sleeping'], 'entity_counter': state['entity_counter'],
            'world_player_ids': ids(state['world_players']), 'world_entity_ids': ids(state['world_entities']),
            'passed_ids': ids(state['passed']), 'passed_is_world_players': state['passed_is_world_players'],
            'class_array': state['class_array'], 'chunks': state['chunks']}


def finder(value, searches):
    if value is None:
        return None
    return {'searches': searches, 'heap_count': value['heap']['lf.b'],
            'heap_capacity': value['heap_array']['length'], 'map_count': value['map']['jx.b'],
            'map_capacity': value['map_array']['length'], 'map_threshold': value['map']['jx.c'],
            'map_modifications': value['map']['jx.e']}


def project(raw, journal):
    counters = journal['counters']
    selected = {name: counters.get(name, 0) for name in ('players_started', 'attempts_started',
                'attempts_returned', 'random_calls', 'constructors', 'predicates', 'paths')}
    for target, source in (('publications', 'publication_returns'), ('postspawn', 'postspawn_returns'),
                           ('wakes', 'wake_returns'), ('ambient', 'ambient_returns')):
        selected[target] = counters.get(source, 0)
    helper_publications = sum(event['event'] == 'entity_added' for event in raw['after']['notifications']) - counters.get('publications', 0)
    need(helper_publications >= 0, 'actual primary/helper publication accounting')
    if helper_publications:
        need(counters.get('postspawn_returns', 0) == counters.get('postspawn', 0) > 0,
             'actual helper completion proves its earlier publication returned')
    selected['publications'] += helper_publications
    caught = [event['value']['class'] for event in journal['events'] if event['value']['kind'] == 'EXCEPTION'
              and event['value']['catch'] is not None and event['value']['catch']['class'] == 'cq'
              and event['value']['catch']['pc'] == 358]
    need(len(caught) <= 1, 'bounded actual reflection catch')
    need(journal['returned'] == (raw['exception'] == 'NONE') and journal['result'] == raw['result'],
         'passive journal agrees with actual whole original call')
    return {'case': raw['case'], 'input': raw['input'], 'returned': journal['returned'],
            'result': raw['result'], 'exception': raw['exception'],
            'printer_exception': caught[0] if caught else None, 'counters': selected,
            'finder': finder(journal['finder'], len(journal['searches'])),
            'before': project_state(raw['before'], raw['before']),
            'after': project_state(raw['after'], raw['before']),
            'notifications': raw['after']['notifications']}
