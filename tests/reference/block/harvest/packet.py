"""Validate the finite unchanged-original Block harvest corpus and typed rows."""
import hashlib
import json
import math
from pathlib import Path
import re
import struct

PROFILE_COLUMNS = ('index mode block metadata remote held_id held_count held_damage below_id '
                   'world_seed math_seed fault').split()
RESULT_COLUMNS = ('profile status removed current_present current_id current_count current_damage '
                  'captured_present captured_id captured_count captured_damage target_id target_metadata '
                  'mined_delta used_delta broken_delta world48_before world48_after math48_before '
                  'math48_after item_count event_count').split()
ITEM_COLUMNS = ('profile index id count damage delay age x64 y64 z64 motionx64 motiony64 '
                'motionz64 yaw32 phase32 in_world in_chunk').split()
EVENT_COLUMNS = ('profile sequence kind target_id target_metadata mined_delta used_delta broken_delta '
                 'item_count item_id item_delay in_world in_chunk world48 math48').split()
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
CLASS_HASHES = {
    'bk': 'e02f35d24f4cdd895f0969d016d35038d32fbef5fe610003f8b8aec1fa38dc82',
    'jr': 'c11fc4c4b9c858a8b517895f9bd679338af8388a72df2ff9392934459bd66752',
    'nk': 'ea0f58f93ee122552f3dea277fd195ed14147c2649ee043837f5dc5053562ec1',
    'nr': '3fa11554ddc55e4bdbebbc29631593f65d230eb1bdacfeb06960c413fe68be0d',
    'jp': 'f4806b9b574a1d6302d8a15d2a4a8a456985696a4cc8f73369b09731102f14b2',
    'uu': 'caeaf354357d66fc168c7f03bf62bb6c6a1fe459e9be6b537d4d5d1efabd0845',
    'os': 'd53eb2ca4c005c2dbc2fbdb752611f33615ebf403d031109b901482814238b1f',
}
GUARDS = ['GUARD\tsource_network_attempts\t0', 'GUARD\tsource_native_attempts\t0',
          'GUARD\tconnect_denied\t1', 'GUARD\tlisten_denied\t1', 'GUARD\tnative_link_denied\t1']


def require(value, message):
    if not value:
        raise ValueError(message)


def integer(text, low=-(1 << 31), high=(1 << 31) - 1):
    require(re.fullmatch(r'-?(0|[1-9][0-9]*)', text) is not None, 'harvest integer encoding')
    value = int(text)
    require(str(value) == text and low <= value <= high, 'harvest integer range/canonical form')
    return value


def lines(directory, name):
    raw = (directory / name).read_bytes()
    require(len(raw) <= 1024 * 1024 and raw.endswith(b'\n') and b'\r' not in raw,
            'bounded LF harvest transport')
    return raw.decode('utf-8', 'strict').splitlines()


def raw_row(line, tag, columns):
    cells = line.split('\t')
    require(len(cells) == len(columns) + 1 and cells[0] == tag, 'harvest row shape')
    return dict(zip(columns, cells[1:]))


def float_word(word, width):
    require(re.fullmatch('[0-9a-f]{' + str(width * 2) + '}', word) is not None,
            'harvest raw floating word')
    require(math.isfinite(struct.unpack('>d' if width == 8 else '>f', bytes.fromhex(word))[0]),
            'harvest finite prepared floating value')


def read_packet(directory):
    directory = Path(directory)
    proof = json.loads((directory / 'provenance.json').read_text(encoding='utf-8'))
    require(proof['schema'] == 1 and proof['client_jar_sha256'] == JAR,
            'harvest original archive identity')
    for name in ('profiles.tsv', 'observations.tsv', 'guards.tsv', 'source-pins.json', 'schema.json'):
        require(hashlib.sha256((directory / name).read_bytes()).hexdigest() == proof['files'][name]['sha256'],
                'harvest packet hash mismatch')
    schema = json.loads((directory / 'schema.json').read_text())
    require(schema['PROFILE'] == PROFILE_COLUMNS and schema['RESULT'] == RESULT_COLUMNS and
            schema['ITEM'] == ITEM_COLUMNS and schema['EVENT'] == EVENT_COLUMNS,
            'harvest typed schema identity')
    pins = json.loads((directory / 'source-pins.json').read_text())
    require(pins['client_jar_sha256'] == JAR, 'harvest class archive identity')
    for name, expected in CLASS_HASHES.items():
        value = pins['classes'][name]
        require(value['class_sha256'] == expected, 'harvest concrete class identity')
        declarations = {(m['name'], m['descriptor']): m for m in value['methods']}
        method = ('a', '(Lfd;Lgs;IIII)V')
        if name in ('nr', 'jp'):
            require(method not in declarations, 'harvest inherited declaring-class resolution')
        elif name in ('bk', 'jr', 'nk', 'uu'):
            require(method in declarations and declarations[method]['code_bytes'] > 0 and
                    re.fullmatch('[0-9a-f]{64}', declarations[method]['code_sha256']) is not None,
                    'harvest method Code identity')
    require(lines(directory, 'guards.tsv') == GUARDS, 'harvest denial guards')
    profile_lines = lines(directory, 'profiles.tsv')
    require(len(profile_lines) == 92, 'harvest profile inventory')
    profiles = []
    for index, line in enumerate(profile_lines):
        row = {key: integer(value) for key, value in raw_row(line, 'PROFILE', PROFILE_COLUMNS).items()}
        require(row['index'] == index and row['mode'] == (1 if index < 63 else 0) and
                row['block'] in (18, 78, 79) and row['metadata'] in (-1, *range(16)) and
                row['remote'] in (0, 1) and row['held_id'] in (-1, 268, 269, 359) and
                row['below_id'] in (0, 1, 8, 9, 10, 11, 20, 78) and
                row['world_seed'] in (0, 1) and row['math_seed'] == 911 and row['fault'] in range(4),
                'harvest prepared profile domains/order')
        profiles.append(row)
    observations = lines(directory, 'observations.tsv')
    by_profile = {}
    cursor = 0
    totals = {'ITEM': 0, 'EVENT': 0}
    for profile, profile_line in zip(profiles, profile_lines):
        index = profile['index']
        require(cursor < len(observations) and observations[cursor] == profile_line,
                'harvest exact input/output profile pairing')
        cursor += 1
        require(cursor < len(observations), 'harvest result exists')
        start = cursor
        result = raw_row(observations[cursor], 'RESULT', RESULT_COLUMNS)
        cursor += 1
        for key in RESULT_COLUMNS:
            if key == 'status':
                require(result[key] == ('NULL' if profile['fault'] in (2, 3) else 'OK'),
                        'harvest source outcome')
            else:
                result[key] = integer(result[key], 0, (1 << 48) - 1) if '48_' in key else integer(result[key])
        require(result['profile'] == index and result['removed'] ==
                (-1 if profile['mode'] == 0 or result['status'] == 'NULL' else 1),
                'harvest reached controller return')
        require(result['target_id'] in (0, 8, 18, 78, 79) and 0 <= result['target_metadata'] <= 15 and
                result['item_count'] in (0, 1) and 0 <= result['event_count'] <= 4,
                'harvest result domains')
        for role in ('current', 'captured'):
            require(result[role + '_present'] in (0, 1), 'harvest nullable Stack presence')
            if result[role + '_present'] == 0:
                require(result[role + '_id'] == -1 and result[role + '_count'] ==
                        result[role + '_damage'] == 0, 'harvest empty Stack sentinels')
            else:
                require(result[role + '_id'] == profile['held_id'], 'harvest Stack identity')
        if result['current_present']:
            require(result['captured_present'] and result['current_count'] == result['captured_count'] and
                    result['current_damage'] == result['captured_damage'], 'harvest retained Stack correspondence')
        require(result['world48_before'] == ((profile['world_seed'] ^ 0x5deece66d) & ((1 << 48) - 1)) and
                result['math48_before'] == ((profile['math_seed'] ^ 0x5deece66d) & ((1 << 48) - 1)),
                'harvest declared ordinary Random preparation')
        items = []
        for item_index in range(result['item_count']):
            require(cursor < len(observations), 'harvest complete Item observations')
            item = raw_row(observations[cursor], 'ITEM', ITEM_COLUMNS)
            cursor += 1
            for key in ITEM_COLUMNS:
                if key.endswith('64') or key.endswith('32'):
                    float_word(item[key], 8 if key.endswith('64') else 4)
                else:
                    item[key] = integer(item[key])
            require(item['profile'] == index and item['index'] == item_index and
                    item['id'] in (6, 18, 332) and item['count'] == 1 and 0 <= item['damage'] <= 3 and
                    item['delay'] == 10 and item['age'] == 0 and item['in_world'] == item['in_chunk'] == 1,
                    'harvest actual Item payload/publication')
            items.append(item)
        insertions = 0
        events = []
        for sequence in range(result['event_count']):
            require(cursor < len(observations), 'harvest complete WorldAccess events')
            event = raw_row(observations[cursor], 'EVENT', EVENT_COLUMNS)
            cursor += 1
            for key in EVENT_COLUMNS:
                if key == 'kind':
                    require(event[key] in ('CHANGE', 'INSERT'), 'harvest observer event kind')
                else:
                    event[key] = integer(event[key], 0, (1 << 48) - 1) if key in ('world48', 'math48') else integer(event[key])
            require(event['profile'] == index and event['sequence'] == sequence and
                    0 <= event['item_count'] <= result['item_count'] and
                    event['target_id'] in (0, 8, 18, 78, 79) and
                    0 <= event['target_metadata'] <= 15,
                    'harvest observer event order/count/domain')
            if event['kind'] == 'CHANGE':
                require(event['item_id'] == event['item_delay'] == -1 and
                        event['in_world'] == event['in_chunk'] == 0, 'harvest non-Entity event sentinels')
            else:
                require(insertions < len(items) and event['item_id'] == items[insertions]['id'] and
                        event['item_delay'] == 10 and event['in_world'] == event['in_chunk'] == 1,
                        'harvest actual insertion event identity/lifetime')
                insertions += 1
            events.append(event)
        require(insertions == len(items), 'harvest complete actual insertion trace')
        if profile['mode'] == 1:
            require(events and events[0]['kind'] == 'CHANGE' and events[0]['target_id'] == 0 and
                    events[0]['mined_delta'] == events[0]['used_delta'] == events[0]['broken_delta'] == 0,
                    'harvest original controller removal precedes tool/harvest work')
        if profile['block'] == 78 and profile['mode'] == 0:
            require(len(items) == 1 and items[0]['id'] == 332 and
                    [event['kind'] for event in events] == ['INSERT', 'CHANGE'] and
                    events[0]['target_id'] == 78 and events[1]['target_id'] == 0 and
                    all(event['mined_delta'] == 0 for event in events),
                    'harvest direct Snow publication then air then statistic order')
        if profile['block'] == 79:
            require(not items and all(event['mined_delta'] == result['mined_delta']
                                      for event in events if event['target_id'] == 8),
                    'harvest Ice statistic precedes flowing Water publication')
        if profile['block'] == 18 and profile['mode'] == 1 and profile['held_id'] == 359:
            if result['current_present']:
                require(len(items) == 1 and items[0]['id'] == 18 and
                        items[0]['damage'] == (profile['metadata'] & 3),
                        'harvest fresh surviving Shears Leaf path')
            else:
                require(result['captured_count'] == 0 and
                        (not items or items[0]['id'] == 6),
                        'harvest cleared Shears use fresh hand superclass path')
        by_profile[index] = observations[start:cursor]
        totals['ITEM'] += len(items)
        totals['EVENT'] += result['event_count']
    require(cursor == len(observations) and totals == {'ITEM': 51, 'EVENT': 135},
            'harvest complete corpus without trailing records')
    return {'profiles': profiles, 'by_profile': by_profile}
