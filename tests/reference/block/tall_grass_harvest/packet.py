"""Audit unchanged-original TallGrass controller rows and their prepared owners."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SHARED = HERE.parent / 'harvest/packet.py'
SPEC = importlib.util.spec_from_file_location('tall_grass_harvest_transport', SHARED)
T = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(T)
PROFILE_COLUMNS, RESULT_COLUMNS = T.PROFILE_COLUMNS, T.RESULT_COLUMNS
ITEM_COLUMNS, EVENT_COLUMNS = T.ITEM_COLUMNS, T.EVENT_COLUMNS
require, integer, raw_row, lines = T.require, T.integer, T.raw_row, T.lines
JAR, GUARDS = T.JAR, T.GUARDS
CLASS_HASHES = {
    'ru': '798fb8e40028cb1a9d031690c5ee44adb4fb2af14b75778d857290dcfb7371eb',
    'wb': 'c843c6873acbc51586f64468ab2635341caf44109f7ae4ec410a414dca3543da',
    'uu': 'caeaf354357d66fc168c7f03bf62bb6c6a1fe459e9be6b537d4d5d1efabd0845',
    'os': 'd53eb2ca4c005c2dbc2fbdb752611f33615ebf403d031109b901482814238b1f',
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_packet(directory=HERE):
    directory = Path(directory)
    proof = json.loads((directory / 'provenance.json').read_text(encoding='utf-8'))
    require(proof['schema'] == 1 and proof['client_jar_sha256'] == JAR,
            'TallGrass archive identity')
    for name in ('profiles.tsv', 'observations.tsv', 'guards.tsv', 'source-pins.json', 'schema.json'):
        path = directory / name
        require(path.stat().st_size == proof['files'][name]['bytes'] and
                digest(path) == proof['files'][name]['sha256'], 'TallGrass packet hash ' + name)
    for name, expected in proof['shared_helper_sha256'].items():
        require(digest(ROOT / name) == expected, 'TallGrass shared capture/transport helper ' + name)
    for name, expected in proof['helper_sha256'].items():
        require(digest(ROOT / name) == expected, 'TallGrass observer/capture helper ' + name)
    schema = json.loads((directory / 'schema.json').read_text(encoding='utf-8'))
    require(schema['PROFILE'] == PROFILE_COLUMNS and schema['RESULT'] == RESULT_COLUMNS and
            schema['ITEM'] == ITEM_COLUMNS and schema['EVENT'] == EVENT_COLUMNS,
            'TallGrass typed transport schema')
    pins = json.loads((directory / 'source-pins.json').read_text(encoding='utf-8'))
    require(pins['client_jar_sha256'] == JAR and len(pins['classes']) == 30,
            'TallGrass complete reached-class inventory')
    harvest = ('a', '(Lfd;Lgs;IIII)V')
    for name, expected in CLASS_HASHES.items():
        value = pins['classes'][name]
        require(value['class_sha256'] == expected, 'TallGrass concrete class identity ' + name)
        declarations = {(m['name'], m['descriptor']): m for m in value['methods']}
        if name in ('ru', 'wb'):
            require(harvest not in declarations, 'TallGrass inherited UU harvest selection')
        if name == 'ru':
            drop = declarations[('a', '(ILjava/util/Random;)I')]
            require(drop['code_bytes'] == 18 and drop['code_sha256'] ==
                    '741a2edb01dfb345007402e335fc993e4931a675826708ef97b6cd69b634d6f7',
                    'TallGrass actual Seed-ID branch')
        if name == 'uu':
            require(declarations[harvest]['code_bytes'] == 26 and
                    declarations[harvest]['code_sha256'] ==
                    'b04364c787b7aefcc1e6155e00df18dc5cde29a51f8488f147448a39e642bf11',
                    'TallGrass actual inherited harvest body')
    require(lines(directory, 'guards.tsv') == GUARDS, 'TallGrass actual denial guards')
    capture = proof['original_capture']
    require(capture['fresh_vm_repeats'] == 2 and capture['all_selected_rows_identical'] and
            capture['whole_stdout_identical'] and capture['all_supplied_inputs_unchanged'] and
            capture['input_count'] == 14 and capture['timer_workers_joined'] and
            capture['original_classes_rewritten_or_substituted'] is False and
            capture['source_guards'] == {'network_attempts': 0, 'native_link_attempts': 0} and
            capture['actual_denial_controls'] == {'connect': 1, 'listen': 1, 'native_link': 1},
            'TallGrass original invocation qualification')
    profile_lines = lines(directory, 'profiles.tsv')
    require(len(profile_lines) == 74, 'TallGrass exact profile count')
    profiles = []
    for index, line in enumerate(profile_lines):
        p = {key: integer(value) for key, value in raw_row(line, 'PROFILE', PROFILE_COLUMNS).items()}
        require(p['index'] == index and p['mode'] == (1 if index < 68 else 0) and
                p['block'] == 31 and p['metadata'] in (-1, 0, 1, 2, 3, 15) and
                p['remote'] in (0, 1) and p['held_id'] in (-1, 359) and
                p['held_count'] in (-1, 0, 1) and p['held_damage'] in (0, 238, 239) and
                p['below_id'] == 3 and p['world_seed'] in (0, 1) and p['math_seed'] == 911 and
                p['fault'] in ((0, 1, 2) if p['mode'] else (3,)),
                'TallGrass exact arranged input domains')
        profiles.append(p)
    observations, by_profile, cursor = lines(directory, 'observations.tsv'), {}, 0
    totals = {'ITEM': 0, 'EVENT': 0}
    for p, declared in zip(profiles, profile_lines):
        require(cursor < len(observations) and observations[cursor] == declared,
                'TallGrass exact declared input/output pair')
        cursor += 1
        start = cursor
        require(cursor < len(observations), 'TallGrass result exists')
        result = raw_row(observations[cursor], 'RESULT', RESULT_COLUMNS)
        cursor += 1
        for key in RESULT_COLUMNS:
            if key != 'status':
                result[key] = integer(result[key], 0, (1 << 48) - 1) if '48_' in key else integer(result[key])
        null = p['fault'] in (2, 3)
        seed = p['mode'] == 1 and p['fault'] != 2 and not p['remote'] and p['world_seed'] == 1
        mined = int(p['mode'] == 1 and p['fault'] not in (1, 2))
        require(result['profile'] == p['index'] and result['status'] == ('NULL' if null else 'OK') and
                result['removed'] == (-1 if null or not p['mode'] else 1) and
                result['target_id'] == (0 if p['mode'] else 31) and
                result['target_metadata'] == (0 if p['mode'] else p['metadata'] & 15) and
                result['mined_delta'] == mined and result['used_delta'] == result['broken_delta'] == 0 and
                result['item_count'] == int(seed) and
                result['event_count'] == (1 + int(seed) if p['mode'] else 0),
                'TallGrass source result/removal/statistic/drop domains')
        present = int(p['held_id'] >= 0)
        require(result['captured_present'] == present and
                result['captured_id'] == p['held_id'] and
                result['captured_count'] == p['held_count'] and
                result['captured_damage'] == p['held_damage'],
                'TallGrass Shears never wear or decrement')
        current = int(present and p['held_count'] != 0)
        require(result['current_present'] == current and
                result['current_id'] == (p['held_id'] if current else -1) and
                result['current_count'] == (p['held_count'] if current else 0) and
                result['current_damage'] == (p['held_damage'] if current else 0),
                'TallGrass count-zero clear and negative-count retention')
        require(result['world48_before'] == ((p['world_seed'] ^ 0x5deece66d) & ((1 << 48) - 1)) and
                result['math48_before'] == ((p['math_seed'] ^ 0x5deece66d) & ((1 << 48) - 1)),
                'TallGrass exact authored Random starting owners')
        if p['remote'] or null:
            require(result['world48_before'] == result['world48_after'] and
                    result['math48_before'] == result['math48_after'],
                    'TallGrass remote/exception prefix consumes no drop entropy')
        elif not seed:
            require(result['math48_before'] == result['math48_after'],
                    'TallGrass no-Seed branch leaves Math Random untouched')
        for item_index in range(result['item_count']):
            require(cursor < len(observations), 'TallGrass complete Item row')
            item = raw_row(observations[cursor], 'ITEM', ITEM_COLUMNS)
            cursor += 1
            for key in ITEM_COLUMNS:
                if key.endswith(('64', '32')):
                    T.float_word(item[key], 8 if key.endswith('64') else 4)
                else:
                    item[key] = integer(item[key])
            require(item['profile'] == p['index'] and item['index'] == item_index and
                    item['id'] == 295 and item['count'] == 1 and item['damage'] == 0 and
                    item['delay'] == 10 and item['age'] == 0 and
                    item['in_world'] == item['in_chunk'] == 1,
                    'TallGrass actual Seeds payload/delay and both ownership memberships')
        for sequence in range(result['event_count']):
            require(cursor < len(observations), 'TallGrass complete ordered event row')
            event = raw_row(observations[cursor], 'EVENT', EVENT_COLUMNS)
            cursor += 1
            for key in EVENT_COLUMNS:
                if key != 'kind':
                    event[key] = integer(event[key], 0, (1 << 48) - 1) if key in ('world48', 'math48') else integer(event[key])
            insertion = sequence == 1
            require(event['profile'] == p['index'] and event['sequence'] == sequence and
                    event['kind'] == ('INSERT' if insertion else 'CHANGE') and
                    event['target_id'] == event['target_metadata'] == 0 and
                    event['mined_delta'] == (mined if insertion else 0) and
                    event['used_delta'] == event['broken_delta'] == 0 and
                    event['item_count'] == int(insertion) and
                    event['item_id'] == (295 if insertion else -1) and
                    event['item_delay'] == (10 if insertion else -1) and
                    event['in_world'] == event['in_chunk'] == int(insertion) and
                    event['world48'] == result['world48_after' if insertion else 'world48_before'] and
                    event['math48'] == result['math48_after' if insertion else 'math48_before'],
                    'TallGrass air precedes harvest; statistic precedes exact Seed publication')
        by_profile[p['index']] = observations[start:cursor]
        totals['ITEM'] += result['item_count']
        totals['EVENT'] += result['event_count']
    require(cursor == len(observations) and totals == {'ITEM': 28, 'EVENT': 96} and
            proof['profiles'] == {'total': 74, 'controller': 68, 'direct': 6, 'items': 28, 'events': 96},
            'TallGrass complete original corpus without extra/trailing rows')
    return {'profiles': profiles, 'by_profile': by_profile}
