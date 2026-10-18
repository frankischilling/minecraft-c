#!/usr/bin/env python3
"""Observe paired chunk schemas and direct original region loader/writer APIs.

C covers the owned chunk codec and explicit registry normalization. Original
entity/tile attachment and world filesystem behavior remain observed evidence.
"""
from __future__ import annotations
from dataclasses import asdict
from pathlib import Path
import argparse
import copy
import hashlib
import json
import os
import platform
import re
import shutil
import struct
import sys
import time
import types
import uuid
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
MAX_RECORD = 32*1024*1024
MAX_NBT = 1024*1024
MARKER = b"fresh chunk storage reference\n"
SOURCES = ("tests/reference/storage/ChunkStorageObservation.java", "tests/reference/chunk/ChunkObservation.java", "tests/reference/cache/CacheCallbacksObservation.java")
CLASS_NAMES = {'CacheCallbacksObservation.class', 'CacheCallbacksObservation$Observer.class', 'CacheCallbacksObservation$SetupUnavailable.class',
               'ChunkObservation.class', 'ChunkObservation$Model.class', 'ChunkObservation$SetupUnavailable.class',
               'ChunkStorageObservation.class', 'ChunkStorageObservation$Observer.class', 'ChunkStorageObservation$OriginalFailure.class', 'ChunkStorageObservation$BoundedOutput.class'}
LEVEL_PATH = ROOT/"scripts/reference/storage/compare_level.py"
LEVEL_RAW = LEVEL_PATH.read_bytes()
level = types.ModuleType("_stored_chunk_level")
level.__file__ = str(LEVEL_PATH)
exec(compile(LEVEL_RAW, str(LEVEL_PATH), "exec"), level.__dict__)
Failure = level.Failure
require = level.require
same_json = level.same_json
strict_json = level.strict_json
keys = level.keys
fp = level.fingerprint
CASE_NAMES = ("valid_saved", "normalization_all_ids", "missing_scalars", "wrong_x", "wrong_z", "wrong_populated", "ignored_wrong_last_update", "wrong_Blocks", "wrong_Data", "wrong_SkyLight", "wrong_BlockLight", "wrong_HeightMap", "wrong_Entities", "wrong_TileEntities", "missing_Blocks", "missing_Data", "missing_SkyLight", "missing_BlockLight", "missing_HeightMap", "missing_all_arrays", "short_all_arrays", "missing_level", "wrong_level", "wrong_location", "nonzero_populated", "unknown_entity", "unknown_tile", "valid_boat_and_sign")
ARRAYS = {"blocks": ("Blocks",32768), "metadata": ("Data",16384), "skylight": ("SkyLight",16384), "block_light": ("BlockLight",16384), "heightmap": ("HeightMap",256)}
WRONG = {"wrong_x":"xPos", "wrong_z":"zPos", "wrong_populated":"TerrainPopulated", "wrong_level":"Level", **{"wrong_"+name:name for name in ("Blocks","Data","SkyLight","BlockLight","HeightMap","Entities","TileEntities")}}
SHAPE = {**{"missing_"+name:name for name in ("Data","SkyLight","BlockLight","HeightMap")}, "short_all_arrays":"Blocks"}
SUPPORTED = {"valid_saved","normalization_all_ids","missing_scalars","ignored_wrong_last_update","wrong_location","nonzero_populated","unknown_entity","unknown_tile","valid_boat_and_sign"}
OPAQUE = {"unknown_entity","unknown_tile","valid_boat_and_sign"}


def parse_raw(raw):
    require(type(raw) is bytes and 4 <= len(raw) <= MAX_NBT, 'bounded raw chunk compound')
    reader = level.Reader(raw)
    tree = reader.tag()
    require(reader.p == len(raw) and tree['type'] == 10, 'one complete raw chunk root compound')
    return tree


def artifact(descriptor, files):
    keys(descriptor, ('artifact', 'bytes', 'sha256'), 'raw original NBT descriptor')
    name = descriptor['artifact']
    require(type(name) is str and re.fullmatch(r'[A-Za-z0-9_-]{1,96}\.nbt', name), 'owned raw NBT artifact name')
    level.integer_value(descriptor['bytes'], 4, MAX_NBT, 'original NBT length')
    level.hex_value(descriptor['sha256'], 32, 'original NBT SHA256')
    require(name in files and fp(files[name]) == {k: descriptor[k] for k in ('bytes', 'sha256')}, 'complete original NBT artifact bytes')
    return files[name]


def child(value, name):
    return level.child(value, name)


def put(value, field):
    for index, previous in enumerate(value['value']):
        if previous['name_utf16'] == field['name_utf16']:
            value['value'][index] = field
            return
    value['value'].append(field)


def remove(value, *names):
    value['value'] = [x for x in value['value'] if x['name_utf16'] not in set(map(level.u16, names))]


def actor(valid, tile):
    out = level.compound('', [level.string('id', ('Sign' if tile else 'Boat') if valid else 'UnregisteredChunkResearchRecord')])
    if tile:
        out['value'] += [level.integer('x', 16), level.integer('y', 64), level.integer('z', 0)]
        out['value'] += [level.string('Text'+str(i), 'private proof' if i == 1 else 'NUL\0' if i == 2 else '') for i in range(1, 5)]
    elif valid:
        for name, kind, values in (('Pos', 6, (16.5, 64.0, 0.5)), ('Motion', 6, (0.0, 0.0, 0.0)), ('Rotation', 5, (0.0, 0.0))):
            out['value'].append(level.tag(9, name, {'element_type': kind, 'items': [level.tag(kind, '', struct.pack('>d' if kind == 6 else '>f', x).hex()) for x in values]}))
        out['value'] += [level.tag(5, 'FallDistance', '00000000'), level.tag(2, 'Fire', 0), level.tag(2, 'Air', 300), level.tag(1, 'OnGround', 1)]
    return out


def input_tree(name, baseline):
    out = copy.deepcopy(baseline)
    current = child(out, 'Level')
    put(out, level.string('OpaqueRoot', 'retained input only'))
    put(current, level.string('OpaqueLevel', 'retained input only'))
    if name == 'normalization_all_ids':
        put(current, level.tag(7, 'Blocks', (bytes(range(256))*128).hex()))
    elif name == 'missing_scalars':
        remove(current, 'xPos', 'zPos', 'TerrainPopulated', 'LastUpdate', 'Entities', 'TileEntities')
    elif name in ('wrong_x', 'wrong_z', 'wrong_populated'):
        put(current, level.long('xPos', 1) if name == 'wrong_x' else level.string('zPos', 'wrong') if name == 'wrong_z' else level.integer('TerrainPopulated', 1))
    elif name == 'ignored_wrong_last_update':
        put(current, level.string('LastUpdate', 'ignored'))
    elif name in WRONG and name != 'wrong_level':
        put(current, level.string(WRONG[name], 'wrong'))
    elif name == 'missing_all_arrays':
        remove(current, *(x[0] for x in ARRAYS.values()))
    elif name.startswith('missing_') and name != 'missing_level':
        remove(current, name[8:])
    elif name == 'short_all_arrays':
        for index, (field, _) in enumerate(ARRAYS.values()):
            put(current, level.tag(7, field, (b'\x00\xff\x01' if index == 0 else bytes(index)).hex()))
    elif name == 'wrong_location':
        put(current, level.integer('xPos', -7)); put(current, level.integer('zPos', 19))
    elif name == 'nonzero_populated':
        put(current, level.tag(1, 'TerrainPopulated', -128))
    elif name in OPAQUE:
        valid = name == 'valid_boat_and_sign'
        if name != 'unknown_tile':
            put(current, level.tag(9, 'Entities', {'element_type': 10, 'items': [actor(valid, False)]}))
        if name != 'unknown_entity':
            put(current, level.tag(9, 'TileEntities', {'element_type': 10, 'items': [actor(valid, True)]}))
        if valid:
            blocks = bytearray.fromhex(child(current, 'Blocks')['value']); blocks[64] = 63
            put(current, level.tag(7, 'Blocks', blocks.hex()))
    if name == 'missing_level':
        remove(out, 'Level')
    elif name == 'wrong_level':
        put(out, level.integer('Level', 1))
    return out


def world_types(value, side):
    roles = ('world', 'provider', 'storage', 'cache', 'manager', 'generator', 'loader', 'provider_world', 'cache_world')
    names = ('fd', 'rh', 'mx', 'ok', 'xv', 'yf', 'ld', 'fd', 'fd') if side == 'client' else ('dj', 'kp', 'ie', 'jb', 'ph', 'pk', 'hb', 'dj', 'dj')
    require(all(value[role]['class'] == name for role, name in zip(roles, names)), 'actual base-world owner/provider/manager/cache/loader receiver types')
    require(all(value[role]['object']['class'] == name for role, name in
                (('map', 'java.util.HashMap'), ('insertion_list', 'java.util.ArrayList'), ('pending_set', 'java.util.HashSet'), ('queue', 'java.util.ArrayList'))), 'actual retained collection owner types')


def world_context(value, initial, old, size, side):
    keys(value, ('world', 'world_time', 'size_on_disk'), 'complete base-world context')
    require(value['world_time'] == '00000000000004d2' and value['size_on_disk'] == f'{size:016x}', 'actual original world time and direct-writer size accounting')
    checked_world(old, value['world'], 'base-world context')
    world_types(value['world'], side)
    if initial is not None:
        require(same_json(value['world'], initial['world']), 'all global owners, cache/map/list/pending/queue and cached payloads retained')
    require(not value['world']['map']['entries'] and not value['world']['insertion_list']['entries'] and
            not value['world']['pending_set']['entries'] and not value['world']['queue']['entries'], 'direct loader/writer does not enter coordinator or publish chunks')
    require(len(value['world']['chunks']) == 1 and value['world']['chunks'][0]['object']['class'] == ('li' if side == 'client' else 'hg') and
            value['world']['chunks'][0]['empty'] is True and value['world']['chunks'][0]['fallback'] is True,
            'only the actual blank fallback exists in the unpopulated direct-world cache')
    queries = [([1, 0], 65536), ([0, 0], 0), ([2, 0], 131072), ([1, -1], 131071), ([1, 1], 65537)]
    expected_queries = [{'coordinates': coordinates, 'key': key, 'present': False, 'contains': False, 'value': None} for coordinates, key in queries]
    require(same_json(value['world']['presence'], expected_queries), 'all five fixed ordered original presence inputs and packed-key results')


def checked_world(old, value, context):
    try:
        old.validate_world(value, 32768, context)
    except old.Failure as error:
        raise Failure(str(error)) from error


def chunk_schema(value, side, source, table, normalized, initial, identities, name, after=False):
    keys(value, ('object', 'world', 'coordinates', 'data', 'flags', 'last_save_time', 'array_references', 'entity_slices', 'tile_map'), 'complete actual chunk observation')
    level_tag = child(source, 'Level') or level.compound('Level', [])
    expected_coordinates = [(child(level_tag, field) or {'value': 0})['value'] for field in ('xPos', 'zPos')]
    require(type(value['coordinates']) is list and same_json(value['coordinates'], expected_coordinates), 'actual Level coordinates, including unchanged wrong-location values')
    require(value['world'] == initial['world']['world'] and value['object']['class'] == ('lm' if side == 'client' else 'hi'), 'actual chunk/world receiver relationships')
    refs = [value['object'], *value['array_references'], *[x['object'] for x in value['entity_slices']], value['tile_map']['object']]
    require(type(value['array_references']) is list and len(value['array_references']) == 8 and len(value['entity_slices']) == 8, 'all array/wrapper and entity-slice owners')
    for ref in refs:
        require(type(ref) is dict and set(ref) == {'ref', 'class'} and type(ref['ref']) is str and re.fullmatch(r'r[0-9]+', ref['ref']) and type(ref['class']) is str, 'typed nonnull actual object identity')
    require(len({x['ref'] for x in refs}) == len(refs), 'distinct chunk payload/list/map owners')
    if not after:
        require(not ({x['ref'] for x in refs} & identities), 'distinct ordinary chunk owners across actual decode/load calls')
        identities.update(x['ref'] for x in refs)
    wrapper = 'wi' if side == 'client' else 'ob'
    require([x['class'] for x in value['array_references']] == ['[B', wrapper, '[B', wrapper, '[B', wrapper, '[B', '[B'], 'actual nibble-wrapper/backing types')
    keys(value['data'], (*ARRAYS, 'minimum_height'), 'all five arrays and minimum')
    for role, (field, maximum) in ARRAYS.items():
        item = value['data'][role]
        keys(item, ('length', 'values'), 'complete original array')
        level.integer_value(item['length'], 0, maximum, 'original imported array shape')
        level.hex_value(item['values'], item['length'], 'every original array byte')
        tag = child(level_tag, field)
        raw = b'' if tag is None else bytes.fromhex(tag['value'])
        if normalized and role == 'blocks':
            raw = bytes(table[x] for x in raw)
        require(item['length'] == len(raw) and item['values'] == raw.hex(), 'actual complete imported/normalized array: '+role)
    require(type(value['data']['minimum_height']) is int and value['data']['minimum_height'] == 0 and value['last_save_time'] == '0000000000000000', 'static decode and direct loader/writer keep minimum/r zero; LastUpdate ignored')
    populated = child(level_tag, 'TerrainPopulated')
    entity_tag = child(level_tag, 'Entities')
    q = bool(entity_tag is not None and entity_tag['value']['items'])
    if after and name == 'unknown_entity':
        q = False
    require(same_json(value['flags'], {'c': False, 'n': populated is not None and populated['value'] != 0, 'o': False, 'p': False, 'q': q}), 'all five actual transient/population/entity flags')
    count = 0
    for row in value['entity_slices']:
        keys(row, ('object', 'entries'), 'full actual entity slice')
        require(type(row['entries']) is list and len(row['entries']) <= 1, 'bounded observed entity slice')
        count += len(row['entries'])
        for ref in row['entries']:
            require(type(ref) is dict and set(ref) == {'ref', 'class'} and type(ref['ref']) is str and re.fullmatch(r'r[0-9]+', ref['ref']) and
                    ref['class'] == ('fz' if side == 'client' else 'eb'), 'actual factory Boat reference')
    keys(value['tile_map'], ('object', 'entries'), 'full actual tile map')
    require(type(value['tile_map']['entries']) is list and len(value['tile_map']['entries']) == (1 if name == 'valid_boat_and_sign' else 0) and count == (1 if name == 'valid_boat_and_sign' else 0), 'genuine Boat/Sign attachment vs unknown-ID factory discard')
    for row in value['tile_map']['entries']:
        keys(row, ('key_class', 'key_fields', 'object'), 'actual Sign tile entry')
        require(row['key_class'] == ('wf' if side == 'client' else 'oa') and same_json(row['key_fields'], {'a': 0, 'b': 64, 'c': 0}) and
                type(row['object']) is dict and set(row['object']) == {'ref', 'class'} and type(row['object']['ref']) is str and
                re.fullmatch(r'r[0-9]+', row['object']['ref']) and row['object']['class'] == ('yk' if side == 'client' else 'pq'), 'actual Sign local coordinates and receiver type')


def validate_threads(value, names):
    require(type(value) is list and 1 <= len(value) <= 64, 'complete bounded thread inventory')
    main = 0
    seen = set()
    for row in value:
        keys(row, ('id', 'name', 'daemon', 'state', 'original_frame', 'frames'), 'complete actual thread')
        level.integer_value(row['id'], 1, (1 << 63)-1, 'thread ID')
        require(row['id'] not in seen and type(row['name']) is str and 0 < len(row['name']) <= 256 and type(row['daemon']) is bool and
                row['state'] in ('NEW', 'RUNNABLE', 'BLOCKED', 'WAITING', 'TIMED_WAITING', 'TERMINATED') and row['original_frame'] is False, 'actual thread roles/state')
        seen.add(row['id'])
        require(type(row['frames']) is list and len(row['frames']) <= 64 and all(type(frame) is str and 0 < len(frame) <= 4096 for frame in row['frames']), 'complete bounded thread frames')
        for frame in row['frames']:
            class_method = frame.split('(', 1)[0].rsplit('/', 1)[-1]
            require('.' in class_method and class_method.rsplit('.', 1)[0] not in names, 'independent original-frame exclusion, including main')
        if row['name'] == 'main':
            main += 1
            require(row['daemon'] is False and row['state'] == 'RUNNABLE' and any('ChunkStorageObservation.threads(' in frame for frame in row['frames']) and any('ChunkStorageObservation.main(' in frame for frame in row['frames']), 'actual main observation frames')
        else:
            require(row['daemon'] is True, 'no additional non-daemon thread')
    require(main == 1, 'one main observer thread')


def serialized_level(tree, original, name, region):
    keys_ = {'xPos', 'zPos', 'LastUpdate', 'Blocks', 'Data', 'SkyLight', 'BlockLight', 'HeightMap', 'TerrainPopulated', 'Entities', 'TileEntities'}
    current = child(tree, 'Level') if region else tree
    require(tree['name_utf16'] == '' and (not region or tree['type'] == 10 and len(tree['value']) == 1 and current is not None), 'actual fresh original root schema')
    require(current['type'] == 10 and {x['name_utf16'] for x in current['value']} == set(map(level.u16, keys_)), 'all and only eleven known original Level fields')
    for key, value in zip(('xPos', 'zPos'), original['coordinates']):
        require(same_json(child(current, key), level.integer(key, value)), 'actual serialized coordinates')
    require(same_json(child(current, 'LastUpdate'), level.long('LastUpdate', 1234)) and
            same_json(child(current, 'TerrainPopulated'), level.tag(1, 'TerrainPopulated', int(original['flags']['n']))), 'explicit original world clock and canonical population Byte')
    for role, (field, _) in ARRAYS.items():
        require(same_json(child(current, field), level.tag(7, field, original['data'][role]['values'])), 'every original serialized array byte')
    for field in ('Entities', 'TileEntities'):
        item = child(current, field)
        require(item['type'] == 9 and len(item['value']['items']) == (1 if name == 'valid_boat_and_sign' else 0), 'actual factory result serialization; unknown IDs are dropped')
        if item['value']['items']:
            require(item['value']['element_type'] == 10 and child(item['value']['items'][0], 'id')['value'] == level.u16('Boat' if field == 'Entities' else 'Sign'), 'genuine original actor record')
        else:
            require(item['value']['element_type'] == 1, 'actual empty-list writer type Byte')


def validate_original(value, side, files, before_threads, after_threads, jar_url, old):
    keys(value, ('schema', 'scope', 'side', 'seed_hex', 'jar_sha256', 'platform_parent', 'class_origins', 'bindings', 'fields',
                 'time_api', 'setup', 'registry_before', 'saved_baseline', 'cases', 'after', 'registry_after'), 'complete original chunk record')
    require(type(value['schema']) is int and value['schema'] == 1 and value['side'] == side and value['seed_hex'] == '0000000000000017' and
            value['jar_sha256'] == level.PIN[side] and value['platform_parent'] is True and value['scope'] ==
            'untouched original static chunk schemas and direct persistent base-world region loader/writer APIs; no cache coordinator or running game claim', 'finite original schema and provenance')
    pin = BINDING_PINS[side]
    origins = {name: {'origin': jar_url, 'loader': 'isolated', **record} for name, record in pin['classes'].items()}
    bindings = {name: {key: row[key] for key in ('class', 'method', 'signature', 'modifiers')} for name, row in pin['methods'].items()}
    require(same_json(value['class_origins'], origins) and same_json(value['bindings'], bindings) and same_json(value['fields'], pin['fields']), 'exact pinned original classes, method/constructor signatures and field bindings')
    require(same_json(value['time_api'], {'method': 'a', 'signature': '(J)V', 'value': 1234}), 'genuine original world-time setter')
    registry = value['registry_before']
    keys(registry, ('map', 'map_identity', 'registry_identity', 'registered_classes'), 'complete frozen registry')
    require(same_json(registry, value['registry_after']), 'all registry bytes, registration classes and owners preserved')
    keys(registry['map'], ('length', 'values'), 'complete normalizer map')
    require(type(registry['map']['length']) is int and registry['map']['length'] == 256, 'normalizer length')
    level.hex_value(registry['map']['values'], 256, 'all normalizer entries')
    require(type(registry['registered_classes']) is list and len(registry['registered_classes']) == 256 and
            all(x is None or type(x) is str and x in JAR_CLASS_NAMES[side] for x in registry['registered_classes']), 'all actual block registry entries')
    require(same_json(registry['registered_classes'], REGISTRY_CLASSES[side]), 'exact retained frozen original registration table')
    table = bytes.fromhex(registry['map']['values'])
    require(table == bytes(i if i == 0 or item is not None else 0 for i, item in enumerate(registry['registered_classes'])), 'actual registry-derived identity-or-zero normalization')
    require(registry['map_identity'] == {'ref': 'r20', 'class': '[B'} and
            registry['registry_identity'] == {'ref': 'r21', 'class': '[L'+('uu' if side == 'client' else 'na')+';'}, 'actual retained registry array identities')
    world_context(value['setup'], None, old, 0, side)
    baseline = parse_raw(artifact(value['saved_baseline'], files))
    require(child(baseline, 'Level') is not None and child(child(baseline, 'Level'), 'xPos')['value'] == 1 and
            child(child(baseline, 'Level'), 'zPos')['value'] == 0, 'actual saved baseline at ordinary neighbor')
    require(type(value['cases']) is list and [row.get('name') for row in value['cases']] == list(CASE_NAMES), 'complete ordered 28-case corpus')
    all_ids = set()
    def collect(node):
        if type(node) is dict:
            for item in node.values():
                collect(item)
        elif type(node) is list:
            for item in node:
                collect(item)
        elif type(node) is str and re.fullmatch(r'r[0-9]+', node):
            all_ids.add(node)
    collect(value['setup']['world'])
    all_ids |= {'r20', 'r21'}
    used_artifacts = {value['saved_baseline']['artifact']}
    for index, row in enumerate(value['cases']):
        keys(row, ('name', 'input', 'static', 'region', 'context_after'), 'complete original chunk case')
        name = row['name']
        raw = artifact(row['input'], files)
        used_artifacts.add(row['input']['artifact'])
        tree = parse_raw(raw)
        require(same_json(level.order_only(tree), level.order_only(input_tree(name, baseline))), 'exact fixed original input tree: '+name)
        source = child(tree, 'Level')
        for role in ('static', 'region'):
            current = row[role]
            expected_status = 'exception' if name in WRONG else 'null' if role == 'region' and name in ('missing_Blocks', 'missing_all_arrays', 'missing_level') else 'ok'
            require(type(current) is dict and current.get('status') == expected_status, 'actual fixed original outcome: '+name+'/'+role)
            prefix = ('request',) if role == 'region' else ('input_after',)
            if role == 'region':
                require(same_json(current['request'], [1, 0]), 'actual original region request independent of returned coordinates')
            if expected_status == 'exception':
                keys(current, ('status', 'stage', 'class', 'message', *prefix), 'complete original exception')
                require(current['stage'] == ('decode' if role == 'static' else 'load') and current['class'] == 'java.lang.ClassCastException' and
                        (current['message'] is None or type(current['message']) is str and len(current['message']) <= 4096), 'original exact-type cast failure before decode/load completion')
            elif expected_status == 'null':
                keys(current, ('status', 'stage', 'chunk_before_write', *prefix), 'actual guarded null load')
                require(current['stage'] == 'complete' and current['chunk_before_write'] is None, 'presence-only original load rejection')
            else:
                output = 'output_root' if role == 'region' else 'output_level'
                keys(current, ('status', 'stage', 'chunk_before_write', 'chunk_after_write', output, *prefix), 'complete original decode/serialize')
                require(current['stage'] == 'complete', 'all original stages completed')
                chunk_schema(current['chunk_before_write'], side, tree, table, role == 'region', value['setup'], all_ids, name)
                chunk_schema(current['chunk_after_write'], side, tree, table, role == 'region', value['setup'], all_ids, name, True)
                expected_after = copy.deepcopy(current['chunk_before_write'])
                if name == 'unknown_entity':
                    expected_after['flags']['q'] = False
                require(same_json(current['chunk_after_write'], expected_after), 'writer retains all owners/scalars/arrays except actual unknown-entity q reset')
                serialized_level(parse_raw(artifact(current[output], files)), current['chunk_before_write'], name, role == 'region')
                used_artifacts.add(current[output]['artifact'])
            if role == 'static':
                require(artifact(current['input_after'], files) == raw, 'every original static input byte preserved')
                used_artifacts.add(current['input_after']['artifact'])
        world_context(row['context_after'], value['setup'], old, 0 if index < 23 else 12288, side)
    world_context(value['after'], value['setup'], old, 12288, side)
    require(set(files) == used_artifacts and len(files) == 88, 'all raw NBT artifacts retained with no omissions or extras')
    validate_threads(before_threads, set(JAR_CLASS_NAMES[side]))
    validate_threads(after_threads, set(JAR_CLASS_NAMES[side]))


def projection(value):
    return {key: value[key] for key in ('coordinates', 'data', 'flags', 'last_save_time')} | {'blank': False}


def replay_plans(value, files, safety=True):
    plans = []
    table = value['registry_before']['map']['values']
    for row in value['cases']:
        name = row['name']
        raw = artifact(row['input'], files)
        if name in WRONG:
            expected = {'status': 'wrong chunk field type', 'field': WRONG[name], 'nbt_status': 'ok'}
        elif name in SHAPE:
            expected = {'status': 'unsupported chunk array shape', 'field': SHAPE[name], 'nbt_status': 'ok'}
        elif name in ('missing_Blocks', 'missing_all_arrays'):
            expected = {'status': 'missing Blocks', 'field': 'Blocks', 'nbt_status': 'ok'}
        elif name == 'missing_level':
            expected = {'status': 'missing Level', 'field': 'Level', 'nbt_status': 'ok'}
        else:
            expected = {'status': 'ok', 'consumed': len(raw), 'chunk': projection(row['static']['chunk_before_write']), 'nbt_hex': None}
        plans.append({'name': name+'-raw', 'kind': 'raw-decode-or-explicit-departure', 'argv': ['decode', '-'], 'input': raw, 'expected': expected})
        if name not in SUPPORTED:
            continue
        expected = {'status': 'ok', 'consumed': len(raw), 'chunk': projection(row['region']['chunk_before_write']), 'nbt_hex': None}
        plans.append({'name': name+'-normalized', 'kind': 'explicit-frozen-normalization', 'argv': ['decode', table], 'input': raw, 'expected': expected})
        if name not in OPAQUE:
            for role, map_value in (('static', '-'), ('region', table)):
                current = row[role]
                tree = parse_raw(artifact(current['output_level' if role == 'static' else 'output_root'], files))
                if role == 'static':
                    tree['name_utf16'] = level.u16('Level')
                    tree = level.compound('', [tree])
                plans.append({'name': name+'-'+role+'-fresh', 'kind': 'supported-empty-opaque-fresh-serialization',
                              'argv': ['build', '00000000000004d2', map_value, 'fresh', 'none'], 'input': raw,
                              'expected': {'status': 'ok', 'consumed': len(raw), 'chunk': projection(current['chunk_before_write'])}, 'nbt': tree})
        elif safety:
            plans.append({'name': name+'-opaque-fresh-refusal', 'kind': 'C-only-opaque-fresh-refusal',
                          'argv': ['build', '00000000000004d2', '-', 'fresh', 'none'], 'input': raw,
                          'expected': {'status': 'unsupported entity or tile edit'}})
            tree = parse_raw(raw)
            put(child(tree, 'Level'), level.long('LastUpdate', 1234))
            pop = child(child(tree, 'Level'), 'TerrainPopulated')
            pop['value'] = int(pop['value'] != 0)
            expected = {'status': 'ok', 'consumed': len(raw), 'chunk': projection(row['static']['chunk_before_write'])}
            plans.append({'name': name+'-opaque-retention', 'kind': 'C-only-opaque-preservation-policy',
                          'argv': ['build', '00000000000004d2', '-', 'keep', 'none'], 'input': raw, 'expected': expected, 'nbt': level.normalized(tree)})
            for edit in ('x', 'block', 'population', 'q'):
                edited = copy.deepcopy(projection(row['static']['chunk_before_write']))
                if edit == 'x':
                    x = edited['coordinates'][0]
                    edited['coordinates'][0] = -(1 << 31) if x == (1 << 31)-1 else x+1
                elif edit == 'block':
                    data = bytearray.fromhex(edited['data']['blocks']['values']); data[0] ^= 1
                    edited['data']['blocks']['values'] = data.hex()
                else:
                    flag = 'n' if edit == 'population' else 'q'
                    edited['flags'][flag] = not edited['flags'][flag]
                plans.append({'name': name+'-opaque-refuse-'+edit, 'kind': 'C-only-opaque-edit-refusal',
                              'argv': ['build', '00000000000004d2', '-', 'keep', edit], 'input': raw,
                              'expected': {'status': 'unsupported entity or tile edit', 'consumed': len(raw), 'chunk': edited, 'nbt_hex': None}})
    require(len(plans) == (67 if safety else 49), 'complete bounded chunk codec and separate-policy replay plan')
    return plans


def compare_c(actual, plan):
    if 'nbt' not in plan:
        require(same_json(actual, plan['expected']), 'strict complete C fields or explicit bounded departure: '+plan['name'])
    else:
        keys(actual, ('status', 'consumed', 'chunk', 'nbt_hex'), 'complete C serialization')
        require(same_json({key: actual[key] for key in ('status', 'consumed', 'chunk')}, plan['expected']) and
                type(actual['nbt_hex']) is str and len(actual['nbt_hex']) % 2 == 0 and len(actual['nbt_hex']) <= MAX_NBT*2 and re.fullmatch(r'[0-9a-f]*', actual['nbt_hex']) is not None, 'strict complete serialized state and bytes')
        require(same_json(level.order_only(parse_raw(bytes.fromhex(actual['nbt_hex']))), level.order_only(plan['nbt'])), 'all serialized tag types, values, lists and arrays; compound order only')

BINDING_PINS = {'client': {'classes': {'fd': {'bytes': 33396, 'sha256': 'b91d4726c43ed2dad1a72fd8d925eaded8a7c365af5d9f37c3ad101b957a4100'}, 'lm': {'bytes': 9050, 'sha256': 'ce93bdfff48935ca907ef111233cab0645dfb720ec9e9b5795fef65e9d093aa2'}, 'nu': {'bytes': 4122, 'sha256': 'a1a5701ec2180d01ddc604cc8c2e5aff1176b10faf83bdb64d2f1cc8c806ac4a'}, 'ij': {'bytes': 1724, 'sha256': '9db862e2ee83e95a2db7a8495594d2c0e0bbe904d96c3f09f167f2b47b72b8d0'}, 'sp': {'bytes': 1529, 'sha256': '13321d5389bea2e16c35fe29c39676bc921f4e3b29b2114f5a0d2c880856eacd'}, 'wt': {'bytes': 244, 'sha256': '0147951c7b3b7036b4a79066dd932651a90351c698c9d0a121180fdaab6ab8e9'}, 'xa': {'bytes': 2050, 'sha256': '8b72b6ce0a7b402b5177623251a38ff305573f16f3172efa3290bb45b8fe3afa'}, 'as': {'bytes': 1077, 'sha256': 'c840daad77df4a1ad92367abce5b3d2035362bc2b010df1ff7b4faee3bbfb652'}, 'to': {'bytes': 5029, 'sha256': '004eb0e6395fc9c764a7b5e91929ec007112c4d175b1f9384333d94ed2f30d72'}, 'rj': {'bytes': 2079, 'sha256': '67a53d7ce669738d8d56a77e986cb0abe6af78acfb00f3255a7c6eef36fb267b'}, 'jc': {'bytes': 3076, 'sha256': '2097eee35d1de249a4bb5fa097e54bed51783c04349f7d2907d6013a1a41da21'}, 'sn': {'bytes': 17172, 'sha256': 'b53107a5bdd5ff92637582aaed4fd49366443a11cba96cef0a6ad719e197b7be'}, 'ow': {'bytes': 3062, 'sha256': 'b3d2c691cab2c8f78ba810a214e235b80f42c549a32453ada6b3b563c0b189cf'}, 'vi': {'bytes': 521, 'sha256': 'edcd0005c72518c36510c13994feffb9f1f655d734b88f75f02c19284565375a'}, 'hi': {'bytes': 4023, 'sha256': 'f4a89d4268ba827357b151cc73c404be042d76132090224a3834a4ef2b5f43cf'}, 'mx': {'bytes': 617, 'sha256': 'a0b3d564fd55705c52fc2fca350afcad516b224d03ca21ea9485908764f85417'}, 'rh': {'bytes': 154, 'sha256': 'b29262494d6c942c3f2484eccfeb3db6e5703c348bb999b6fb8b0667fa56cbda'}, 'ld': {'bytes': 2288, 'sha256': '014da1a3310123f9b5a0f5320866b7ebbfd2d67d4ffbcc64e91ac40eaa41e1f4'}, 'ok': {'bytes': 3561, 'sha256': '0030b48ceb7c6ae5086de17a777c7226fc39d076d11548c0a8d31c28bbbb0732'}, 'ei': {'bytes': 3748, 'sha256': '5d01d1dda7be346c57fd00846d2425615186c3ef1c65c06aa0bd0f45aa9f7fef'}, 'uu': {'bytes': 17378, 'sha256': 'caeaf354357d66fc168c7f03bf62bb6c6a1fe459e9be6b537d4d5d1efabd0845'}, 'fz': {'bytes': 5579, 'sha256': 'f1a2e8afeb5edf3dacbeb8c36a1c5ad2d33b7954120a2c98d13a4dbf3225a48d'}, 'yk': {'bytes': 1046, 'sha256': '6c7245ad28444d045fff078aef8be80e6e40402d51f8ba18c77ad937ed4149e3'}, 'wf': {'bytes': 500, 'sha256': 'b6a5e47120868ba2d2b35e8ef8aca5f0637438cfdc1628a1fa5d633771f5c805'}}, 'methods': {'nbt_read': {'class': 'as', 'method': 'a', 'signature': '(Ljava/io/DataInput;)Lnu;', 'modifiers': 9, 'bytes': 27, 'sha256': '58c33140343be85c6cce0a31d7200fdb78b5a3eff52f84a504177ebd080ac748'}, 'nbt_write': {'class': 'as', 'method': 'a', 'signature': '(Lnu;Ljava/io/DataOutput;)V', 'modifiers': 9, 'bytes': 6, 'sha256': 'a675f14a496eb80da4334b317669a35434f8e6d50296a092a0b7be9d4f949c61'}, 'schema_decode': {'class': 'to', 'method': 'a', 'signature': '(Lfd;Lnu;)Llm;', 'modifiers': 9, 'bytes': 364, 'sha256': '06b0ecc199f8e5dcb51137bc4a01adac365d36ac71eb7a33bf8950d9b14d8faf'}, 'schema_encode': {'class': 'to', 'method': 'a', 'signature': '(Llm;Lfd;Lnu;)V', 'modifiers': 9, 'bytes': 293, 'sha256': 'd2377ea0633eccf25973659878ba0173083fcdf35b3450ed280847d9609fe1c4'}, 'entity_factory': {'class': 'jc', 'method': 'a', 'signature': '(Lnu;Lfd;)Lsn;', 'modifiers': 9, 'bytes': 104, 'sha256': '7500db159c0ea4452d014ed02429bee0d85f96372f40ac9ab91d08769ea0645c'}, 'tile_factory': {'class': 'ow', 'method': 'c', 'signature': '(Lnu;)Low;', 'modifiers': 9, 'bytes': 84, 'sha256': 'f24f576ace5140b8c9273270dc337e626f3fbafe94f8b61c7d4725ea536b2b08'}, 'post_load': {'class': 'lm', 'method': 'i', 'signature': '()V', 'modifiers': 1, 'bytes': 8, 'sha256': '8cda22ce14b459797108896ea2e52f3d7cf7730e851f16c8b89f78b327627f9c'}, 'normalize_blocks': {'class': 'vi', 'method': 'a', 'signature': '([B)V', 'modifiers': 9, 'bytes': 29, 'sha256': '7acdd662a756c347071db2ab6cb64061d12b9cd85c8fec87c3592f6d03bdd475'}, 'nbt_set_tag': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;Lij;)V', 'modifiers': 1, 'bytes': 17, 'sha256': '67a50f478de7c18d6ec74a399442e08e406de37b433aa6d573ad6948138d366c'}, 'nbt_get_compound': {'class': 'nu', 'method': 'k', 'signature': '(Ljava/lang/String;)Lnu;', 'modifiers': 1, 'bytes': 35, 'sha256': '245af2d00d29370bf057f023736f3e16cb274e13c3482baf0f9e1d14a23d749c'}, 'region_read': {'class': 'rj', 'method': 'c', 'signature': '(Ljava/io/File;II)Ljava/io/DataInputStream;', 'modifiers': 9, 'bytes': 20, 'sha256': '88d97fd00ca57a3f869c7f8ae11234696f1a7787144146c271b309930a1dadc4'}, 'region_write': {'class': 'rj', 'method': 'd', 'signature': '(Ljava/io/File;II)Ljava/io/DataOutputStream;', 'modifiers': 9, 'bytes': 20, 'sha256': '164151a47031f2647e50dacee9b00d84c58fb4139be427d886bfae4dd7792fbe'}, 'world_time': {'class': 'fd', 'method': 't', 'signature': '()J', 'modifiers': 1, 'bytes': 8, 'sha256': '45dc4cb00dc68fb8b32bfbc0641b6657d794beec59ff2697e48bbe6b18c43929'}, 'world_set_time': {'class': 'fd', 'method': 'a', 'signature': '(J)V', 'modifiers': 1, 'bytes': 9, 'sha256': 'b3d812a16755874725c695d086993aba96cdf4cd5354be43667f09f94c29be4a'}, 'hi(Ljava/io/File;)V': {'class': 'hi', 'method': '<init>', 'signature': '(Ljava/io/File;)V', 'modifiers': 1, 'bytes': 6, 'sha256': '4772ffe893e1bc977cf771a6bc2c44e82f47404e348077b7100de817d2dbfc34'}, 'save_handler': {'class': 'hi', 'method': 'a', 'signature': '(Ljava/lang/String;Z)Lwt;', 'modifiers': 1, 'bytes': 14, 'sha256': '7d52a74ec79d94268ad695030af73b686b0e4ab65ec4d1b6b0ae95b059cd66b0'}, 'fd(Lwt;Ljava/lang/String;J)V': {'class': 'fd', 'method': '<init>', 'signature': '(Lwt;Ljava/lang/String;J)V', 'modifiers': 1, 'bytes': 9, 'sha256': 'bae6b5018dfd3d8ebb1d9033ea04127e43caae681bebbbf5f2a55d49c755b3c2'}, 'close_regions': {'class': 'hi', 'method': 'c', 'signature': '()V', 'modifiers': 1, 'bytes': 4, 'sha256': '17398dea85d2f6031497381755ad7113a82fe1b1e5a6bd045a76c2b23a766a64'}, 'actual_loader': {'class': 'ld', 'method': 'a', 'signature': '(Lfd;II)Llm;', 'modifiers': 1, 'bytes': 278, 'sha256': '3349469872749baec1a3d455f296827315f1b81245397abef0fa54c3b521c38e'}, 'actual_writer': {'class': 'ld', 'method': 'a', 'signature': '(Lfd;Llm;)V', 'modifiers': 1, 'bytes': 106, 'sha256': '010f2b7fb21f81dedaa4063990d235d9b16e25d1345a0369f095b612483f73c9'}, 'nbt_put_Ljava/lang/String;': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;Ljava/lang/String;)V', 'modifiers': 1, 'bytes': 24, 'sha256': '8278b1cafd671d9252cebd73f985ceab99525f193d899658ef233ee83bc09177'}, 'nbt_put_Lnu;': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;Lnu;)V', 'modifiers': 1, 'bytes': 17, 'sha256': '2f4b6294d755e38152b4d1e82cb51f6f35298f2a5e867514fd13b2f1c8d7cb98'}, 'nu()V': {'class': 'nu', 'method': '<init>', 'signature': '()V', 'modifiers': 1, 'bytes': 16, 'sha256': '425b169627fb4c4110a332b3f13115a079b37386c676508ef638ba2a01b5e9ab'}, 'nbt_put_[B': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;[B)V', 'modifiers': 1, 'bytes': 24, 'sha256': 'b825f4cc7191def6bfad00cfd2372d8d68e08be3a2996b86c993b6314eb2f93f'}, 'nbt_put_J': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;J)V', 'modifiers': 1, 'bytes': 24, 'sha256': 'a2d50a6458ee1d1f4ab4f3401fb898b017851062ef3c1e33279fdb375f3e275d'}, 'nbt_put_I': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;I)V', 'modifiers': 1, 'bytes': 24, 'sha256': '2522f637def6e86bc22e4caebd624c92e5d2d6a1085b8933b832551597d20d3f'}, 'nbt_put_B': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;B)V', 'modifiers': 1, 'bytes': 24, 'sha256': 'c0b05fb125adef9e9422b8c7f0e8534ba4a6411251272af0ad899ff3c50a1c04'}, 'sp()V': {'class': 'sp', 'method': '<init>', 'signature': '()V', 'modifiers': 1, 'bytes': 16, 'sha256': '0dc4d69522f992576e69290cc8e91dcb33871c98fe7af2d4c4de5f10b742d192'}, 'list_append': {'class': 'sp', 'method': 'a', 'signature': '(Lij;)V', 'modifiers': 1, 'bytes': 20, 'sha256': 'faeff3441eae188c4eb8386cb0f278189c0071cd990f425b3a7e9b06a8f0679a'}, 'get_array': {'class': 'nu', 'method': 'j', 'signature': '(Ljava/lang/String;)[B', 'modifiers': 1, 'bytes': 34, 'sha256': '71007fab635d00085dc4d01c5eaeb9656db8e59658aaa15cba72ad35e38ec063'}, 'nbt_put_D': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;D)V', 'modifiers': 1, 'bytes': 24, 'sha256': '47b044fa901af4e1626c3c8eeaeba017a8fe65e9c1e7e51fe076c3fba3aadf56'}, 'nbt_put_F': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;F)V', 'modifiers': 1, 'bytes': 24, 'sha256': '654859531a1e86376ce99a72cf31abb1247909908304843c75e89b5db297f973'}, 'nbt_put_S': {'class': 'nu', 'method': 'a', 'signature': '(Ljava/lang/String;S)V', 'modifiers': 1, 'bytes': 24, 'sha256': '744354080646590180b74cf844e0454c272edb9de99a3b007019ae89c33f9898'}}, 'fields': {'fd.x': {'class': 'fd', 'name': 'x', 'signature': 'Lei;', 'modifiers': 4}, 'ei.g': {'class': 'ei', 'name': 'g', 'signature': 'J', 'modifiers': 2}, 'vi.a': {'class': 'vi', 'name': 'a', 'signature': '[B', 'modifiers': 10}, 'uu.m': {'class': 'uu', 'name': 'm', 'signature': '[Luu;', 'modifiers': 25}, 'nu.a': {'class': 'nu', 'name': 'a', 'signature': 'Ljava/util/Map;', 'modifiers': 2}}}, 'server': {'classes': {'dj': {'bytes': 29347, 'sha256': '8ca0e21e9e27eaf903faf4a08c3d15728e0caccc002c9b3691acfe41bcc92599'}, 'hi': {'bytes': 9046, 'sha256': '9256153cd5af8c0106f4c61a94047a41988ad6cc67cc939027ba0387c2e4cf80'}, 'iq': {'bytes': 4118, 'sha256': '9e669793b602888407b6d092eaaf5778b0edaa79f4692da8a995efc63ded2223'}, 'fo': {'bytes': 1724, 'sha256': 'b320f3a314b4cdc81a766d1c799016611e2bea0eccccccc98556a42ca39442d1'}, 'lr': {'bytes': 1529, 'sha256': '9f5660b427bf0e21ea8d7ccc1d625ddb5df799193f03e180c723fdaa3097c3fa'}, 'om': {'bytes': 277, 'sha256': '7272fce8e533561ef2d4fd50d1fa873d6a93ab1cb2c16ac63e79580eb5ba6cb9'}, 'os': {'bytes': 1411, 'sha256': 'ed6be5ac72ddc00186f7d9c529867bf563a3175b7d1790297d7d4424745096d2'}, 'ag': {'bytes': 1077, 'sha256': 'd0eb17e39056968710c661d07a1880263a47c67fab9c1779975097155bd4d4c8'}, 'mg': {'bytes': 5013, 'sha256': '259b979084d80df1eee8e56947df93b60af87da4bab12d52daa2eec22c099bbb'}, 'kq': {'bytes': 2079, 'sha256': 'b73f741b618e9f5dd29d6d63b945e3111e5f731edeb2d159effb2353cc6fb505'}, 'ga': {'bytes': 2850, 'sha256': 'a72a9d0b37003b2e48f4914d53955c3e8829017585e70961422592a11e4a6614'}, 'lq': {'bytes': 15824, 'sha256': '3cf239e967aa9da83d62de29175a50c9845736477dc1ebc1ba0f8ab19fb37f07'}, 'jh': {'bytes': 2869, 'sha256': '24486589c2a2a8e9f44efd8e859dd1add59f6a59ae3d4b6f521b5c224836ade8'}, 'nn': {'bytes': 521, 'sha256': '502581cb5446e0f2431230a6e7174a39c2d0819955b75f7d7c5488bada4253cd'}, 'ie': {'bytes': 687, 'sha256': '2dcd09358ba2f3878d282848eb2fa4056b6b175ac6d730d195e62b27f89b306b'}, 'kp': {'bytes': 154, 'sha256': 'fbe590f31d2c3307a949a8e2423254ed6b8967fa53e6808db795116550c8731e'}, 'hb': {'bytes': 2284, 'sha256': 'dd69bda918468b8e5727c4e9b728f17590ec3469cf8b30ff290e3847fd25f821'}, 'jb': {'bytes': 3245, 'sha256': '7fa5670442cb7f1d4a1fc25b8051c109266142658ec6d8ffed424dbc6dfd8552'}, 'ct': {'bytes': 3400, 'sha256': 'ce28db56968ac4129348e09d956e7f33fa809db04130cbdbb2856b89dd44b68f'}, 'na': {'bytes': 16628, 'sha256': '08ff7bb1c04b7811e96666efc12571f4e61b90377332d387329dea015f9015a0'}, 'eb': {'bytes': 5165, 'sha256': '79573997f1550d19aedcfd854275129f03716b099244de3ec8ac3bad6ed33e73'}, 'pq': {'bytes': 1349, 'sha256': '5bf8cff12c11e2a6f1b2ae9e7a0f566ae557c8a4f44b40187e99aefad86452cc'}, 'oa': {'bytes': 500, 'sha256': '44135438a9b95a06829930668644cd9a1a63f9231efc9d37ccc7e80f2ebd3ede'}}, 'methods': {'nbt_read': {'class': 'ag', 'method': 'a', 'signature': '(Ljava/io/DataInput;)Liq;', 'modifiers': 9, 'bytes': 27, 'sha256': 'a9ed3400db48264c39cd049bd3bfacbceeb21a6b46cea28fc2cba91ecb7fafdf'}, 'nbt_write': {'class': 'ag', 'method': 'a', 'signature': '(Liq;Ljava/io/DataOutput;)V', 'modifiers': 9, 'bytes': 6, 'sha256': 'a675f14a496eb80da4334b317669a35434f8e6d50296a092a0b7be9d4f949c61'}, 'schema_decode': {'class': 'mg', 'method': 'a', 'signature': '(Ldj;Liq;)Lhi;', 'modifiers': 9, 'bytes': 364, 'sha256': 'b2f9564cff28edc37bb8553f051a719adfaf0284ce45e67bccaac1bfa44ca091'}, 'schema_encode': {'class': 'mg', 'method': 'a', 'signature': '(Lhi;Ldj;Liq;)V', 'modifiers': 9, 'bytes': 293, 'sha256': '228cdc8b09f273853918b0f6ede4d8a5f7f41c0d0bdf8093e738abb16740c4bc'}, 'entity_factory': {'class': 'ga', 'method': 'a', 'signature': '(Liq;Ldj;)Llq;', 'modifiers': 9, 'bytes': 104, 'sha256': '78eb912360cdd0d1b16557d1fd43a3055d63ec3d08700f9035dd10471a5f080f'}, 'tile_factory': {'class': 'jh', 'method': 'c', 'signature': '(Liq;)Ljh;', 'modifiers': 9, 'bytes': 84, 'sha256': '89e90257856a08226af7a7c0a11a75cef37e52e134fe64c5b06b55b9cecd0f73'}, 'post_load': {'class': 'hi', 'method': 'h', 'signature': '()V', 'modifiers': 1, 'bytes': 8, 'sha256': '3e90f9617da2fe895e24cc5587f2c1f1ba171f57ae4642ed2983636569a129fe'}, 'normalize_blocks': {'class': 'nn', 'method': 'a', 'signature': '([B)V', 'modifiers': 9, 'bytes': 29, 'sha256': '7acdd662a756c347071db2ab6cb64061d12b9cd85c8fec87c3592f6d03bdd475'}, 'nbt_set_tag': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;Lfo;)V', 'modifiers': 1, 'bytes': 17, 'sha256': '67a50f478de7c18d6ec74a399442e08e406de37b433aa6d573ad6948138d366c'}, 'nbt_get_compound': {'class': 'iq', 'method': 'k', 'signature': '(Ljava/lang/String;)Liq;', 'modifiers': 1, 'bytes': 35, 'sha256': 'faf93050df89070aed6459edefe3e27bb28863ed00a6ee11586c038dd376fa78'}, 'region_read': {'class': 'kq', 'method': 'c', 'signature': '(Ljava/io/File;II)Ljava/io/DataInputStream;', 'modifiers': 9, 'bytes': 20, 'sha256': '88d97fd00ca57a3f869c7f8ae11234696f1a7787144146c271b309930a1dadc4'}, 'region_write': {'class': 'kq', 'method': 'd', 'signature': '(Ljava/io/File;II)Ljava/io/DataOutputStream;', 'modifiers': 9, 'bytes': 20, 'sha256': '164151a47031f2647e50dacee9b00d84c58fb4139be427d886bfae4dd7792fbe'}, 'world_time': {'class': 'dj', 'method': 'm', 'signature': '()J', 'modifiers': 1, 'bytes': 8, 'sha256': '740127b35e4a8179dba415c21e1219e2f8df40b83de974bfff7a666ae5fe796f'}, 'world_set_time': {'class': 'dj', 'method': 'a', 'signature': '(J)V', 'modifiers': 1, 'bytes': 9, 'sha256': '290fe9fe23ef3b2fcac19e5bb34433e2fe0c3df2e1f2e52cc07e90a27b831682'}, 'ie(Ljava/io/File;Ljava/lang/String;Z)V': {'class': 'ie', 'method': '<init>', 'signature': '(Ljava/io/File;Ljava/lang/String;Z)V', 'modifiers': 1, 'bytes': 8, 'sha256': '0a89ec8d2eb1c824548206cf5622efd57da63ee5432ca47400b7266a042fe111'}, 'kp()V': {'class': 'kp', 'method': '<init>', 'signature': '()V', 'modifiers': 1, 'bytes': 5, 'sha256': '698b4bbff28b9283e5cfcc5e527afbe2c78f48eccafc11d5007412d00dc1b76d'}, 'dj(Lom;Ljava/lang/String;JLos;)V': {'class': 'dj', 'method': '<init>', 'signature': '(Lom;Ljava/lang/String;JLos;)V', 'modifiers': 1, 'bytes': 427, 'sha256': '34ac1db97254bec1deb3cf223e8dac8667f5ab2d068c3c655143a3174df2528f'}, 'close_regions': {'class': 'ie', 'method': 'e', 'signature': '()V', 'modifiers': 1, 'bytes': 4, 'sha256': '6d8689355eeed09893eb8ee9aabeb5aae85eef58f7a1c3ea27e3bb6fe22660ae'}, 'actual_loader': {'class': 'hb', 'method': 'a', 'signature': '(Ldj;II)Lhi;', 'modifiers': 1, 'bytes': 278, 'sha256': '04af2e7d7021541054131879e77e901e013ba8ff15f9ce2e56d916f82d9a186e'}, 'actual_writer': {'class': 'hb', 'method': 'a', 'signature': '(Ldj;Lhi;)V', 'modifiers': 1, 'bytes': 106, 'sha256': 'de8cbb5272fd6ba5e6a5d74e3985db5298a5e2b89fb4476bf0442dcc7d417146'}, 'nbt_put_Ljava/lang/String;': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;Ljava/lang/String;)V', 'modifiers': 1, 'bytes': 24, 'sha256': '8278b1cafd671d9252cebd73f985ceab99525f193d899658ef233ee83bc09177'}, 'nbt_put_Liq;': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;Liq;)V', 'modifiers': 1, 'bytes': 17, 'sha256': 'bdc4e36ee9caa9d8d187048b658dbea5576691ef481b1f18630532ffa91368b3'}, 'iq()V': {'class': 'iq', 'method': '<init>', 'signature': '()V', 'modifiers': 1, 'bytes': 16, 'sha256': '5af0c53ab1b39f2e3fa057af948fc489dcaf182927e29c62410597856d7b90ea'}, 'nbt_put_[B': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;[B)V', 'modifiers': 1, 'bytes': 24, 'sha256': 'b825f4cc7191def6bfad00cfd2372d8d68e08be3a2996b86c993b6314eb2f93f'}, 'nbt_put_J': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;J)V', 'modifiers': 1, 'bytes': 24, 'sha256': '23684a44e469567d2c671387ec239c03587755ba1c342b1988c0a52f9376a930'}, 'nbt_put_I': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;I)V', 'modifiers': 1, 'bytes': 24, 'sha256': 'e5f8ace6e7a5a0a6bdcfa98b7b9f691c8e35825db83805485d84ee27de45475d'}, 'nbt_put_B': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;B)V', 'modifiers': 1, 'bytes': 24, 'sha256': 'c0b05fb125adef9e9422b8c7f0e8534ba4a6411251272af0ad899ff3c50a1c04'}, 'lr()V': {'class': 'lr', 'method': '<init>', 'signature': '()V', 'modifiers': 1, 'bytes': 16, 'sha256': '0dc4d69522f992576e69290cc8e91dcb33871c98fe7af2d4c4de5f10b742d192'}, 'list_append': {'class': 'lr', 'method': 'a', 'signature': '(Lfo;)V', 'modifiers': 1, 'bytes': 20, 'sha256': 'faeff3441eae188c4eb8386cb0f278189c0071cd990f425b3a7e9b06a8f0679a'}, 'get_array': {'class': 'iq', 'method': 'j', 'signature': '(Ljava/lang/String;)[B', 'modifiers': 1, 'bytes': 34, 'sha256': '71007fab635d00085dc4d01c5eaeb9656db8e59658aaa15cba72ad35e38ec063'}, 'nbt_put_D': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;D)V', 'modifiers': 1, 'bytes': 24, 'sha256': '47b044fa901af4e1626c3c8eeaeba017a8fe65e9c1e7e51fe076c3fba3aadf56'}, 'nbt_put_F': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;F)V', 'modifiers': 1, 'bytes': 24, 'sha256': '2d0df9e19d442debe806e590ade25238d1b8a6cc7320b02eb96f642937eb0327'}, 'nbt_put_S': {'class': 'iq', 'method': 'a', 'signature': '(Ljava/lang/String;S)V', 'modifiers': 1, 'bytes': 24, 'sha256': '744354080646590180b74cf844e0454c272edb9de99a3b007019ae89c33f9898'}}, 'fields': {'dj.x': {'class': 'dj', 'name': 'x', 'signature': 'Lct;', 'modifiers': 4}, 'ct.g': {'class': 'ct', 'name': 'g', 'signature': 'J', 'modifiers': 2}, 'nn.a': {'class': 'nn', 'name': 'a', 'signature': '[B', 'modifiers': 10}, 'na.m': {'class': 'na', 'name': 'm', 'signature': '[Lna;', 'modifiers': 25}, 'iq.a': {'class': 'iq', 'name': 'a', 'signature': 'Ljava/util/Map;', 'modifiers': 2}}}}
JAR_CLASS_NAMES = {'client': ['a', 'aa', 'ab', 'ac', 'ad', 'ae', 'af', 'ag', 'ah', 'ai', 'aj', 'ak', 'al', 'am', 'an', 'ao', 'ap', 'aq', 'ar', 'as', 'at', 'au', 'av', 'aw', 'ax', 'ay', 'az', 'b', 'ba', 'bb', 'bc', 'bd', 'be', 'bf', 'bg', 'bh', 'bi', 'bj', 'bk', 'bl', 'bm', 'bn', 'bo', 'bp', 'bq', 'br', 'bs', 'bt', 'bu', 'bv', 'bw', 'bx', 'by', 'bz', 'c', 'ca', 'cb', 'cc', 'cd', 'ce', 'cf', 'cg', 'ch', 'ci', 'cj', 'ck', 'cl', 'cm', 'cn', 'co', 'com.jcraft.jogg.Buffer', 'com.jcraft.jogg.Packet', 'com.jcraft.jogg.Page', 'com.jcraft.jogg.StreamState', 'com.jcraft.jogg.SyncState', 'com.jcraft.jorbis.Block', 'com.jcraft.jorbis.CodeBook', 'com.jcraft.jorbis.CodeBook$DecodeAux', 'com.jcraft.jorbis.Comment', 'com.jcraft.jorbis.Drft', 'com.jcraft.jorbis.DspState', 'com.jcraft.jorbis.Floor0', 'com.jcraft.jorbis.Floor0$EchstateFloor0', 'com.jcraft.jorbis.Floor0$InfoFloor0', 'com.jcraft.jorbis.Floor0$LookFloor0', 'com.jcraft.jorbis.Floor1', 'com.jcraft.jorbis.Floor1$EchstateFloor1', 'com.jcraft.jorbis.Floor1$InfoFloor1', 'com.jcraft.jorbis.Floor1$LookFloor1', 'com.jcraft.jorbis.Floor1$Lsfit_acc', 'com.jcraft.jorbis.FuncFloor', 'com.jcraft.jorbis.FuncMapping', 'com.jcraft.jorbis.FuncResidue', 'com.jcraft.jorbis.FuncTime', 'com.jcraft.jorbis.Info', 'com.jcraft.jorbis.InfoMode', 'com.jcraft.jorbis.Lookup', 'com.jcraft.jorbis.Lpc', 'com.jcraft.jorbis.Lsp', 'com.jcraft.jorbis.Mapping0', 'com.jcraft.jorbis.Mapping0$InfoMapping0', 'com.jcraft.jorbis.Mapping0$LookMapping0', 'com.jcraft.jorbis.Mdct', 'com.jcraft.jorbis.PsyInfo', 'com.jcraft.jorbis.PsyLook', 'com.jcraft.jorbis.Residue0', 'com.jcraft.jorbis.Residue0$InfoResidue0', 'com.jcraft.jorbis.Residue0$LookResidue0', 'com.jcraft.jorbis.Residue1', 'com.jcraft.jorbis.Residue2', 'com.jcraft.jorbis.StaticCodeBook', 'com.jcraft.jorbis.Time0', 'com.jcraft.jorbis.Util', 'cp', 'cq', 'cr', 'cs', 'ct', 'cu', 'cv', 'cw', 'cx', 'cy', 'cz', 'd', 'da', 'db', 'dc', 'dd', 'de', 'df', 'dg', 'dh', 'di', 'dj', 'dk', 'dl', 'dm', 'dn', 'do', 'dp', 'dq', 'dr', 'ds', 'dt', 'du', 'dv', 'dw', 'dx', 'dy', 'dz', 'e', 'ea', 'eb', 'ec', 'ed', 'ee', 'ef', 'eg', 'eh', 'ei', 'ej', 'ek', 'el', 'em', 'en', 'eo', 'ep', 'eq', 'er', 'es', 'et', 'eu', 'ev', 'ew', 'ex', 'ey', 'ez', 'f', 'fa', 'fb', 'fc', 'fd', 'fe', 'ff', 'fg', 'fh', 'fi', 'fj', 'fk', 'fl', 'fm', 'fn', 'fo', 'fp', 'fq', 'fr', 'fs', 'ft', 'fu', 'fv', 'fw', 'fx', 'fy', 'fz', 'g', 'ga', 'gb', 'gc', 'gd', 'ge', 'gf', 'gg', 'gh', 'gi', 'gj', 'gk', 'gl', 'gm', 'gn', 'go', 'gp', 'gq', 'gr', 'gs', 'gt', 'gu', 'gv', 'gw', 'gx', 'gy', 'gz', 'h', 'ha', 'hb', 'hc', 'hd', 'he', 'hf', 'hg', 'hh', 'hi', 'hj', 'hk', 'hl', 'hm', 'hn', 'ho', 'hp', 'hq', 'hr', 'hs', 'ht', 'hu', 'hv', 'hw', 'hx', 'hy', 'hz', 'i', 'ia', 'ib', 'ic', 'id', 'ie', 'if', 'ig', 'ih', 'ii', 'ij', 'ik', 'il', 'im', 'in', 'io', 'ip', 'iq', 'ir', 'is', 'it', 'iu', 'iv', 'iw', 'ix', 'iy', 'iz', 'j', 'ja', 'jb', 'jc', 'jd', 'je', 'jf', 'jg', 'jh', 'ji', 'jj', 'jk', 'jl', 'jm', 'jn', 'jo', 'jp', 'jq', 'jr', 'js', 'jt', 'ju', 'jv', 'jw', 'jx', 'jy', 'jz', 'k', 'ka', 'kb', 'kc', 'kd', 'ke', 'kf', 'kg', 'kh', 'ki', 'kj', 'kk', 'kl', 'km', 'kn', 'ko', 'kp', 'kq', 'kr', 'ks', 'kt', 'ku', 'kv', 'kw', 'kx', 'ky', 'kz', 'l', 'la', 'lb', 'lc', 'ld', 'le', 'lf', 'lg', 'lh', 'li', 'lj', 'lk', 'll', 'lm', 'ln', 'lo', 'lp', 'lq', 'lr', 'ls', 'lt', 'lu', 'lv', 'lw', 'lx', 'ly', 'lz', 'm', 'ma', 'mb', 'mc', 'md', 'me', 'mf', 'mg', 'mh', 'mi', 'mj', 'mk', 'ml', 'mm', 'mn', 'mo', 'mp', 'mq', 'mr', 'ms', 'mt', 'mu', 'mv', 'mw', 'mx', 'my', 'mz', 'n', 'na', 'nb', 'nc', 'nd', 'ne', 'net.minecraft.client.Minecraft', 'net.minecraft.client.MinecraftApplet', 'net.minecraft.isom.IsomPreviewApplet', 'nf', 'ng', 'nh', 'ni', 'nj', 'nk', 'nl', 'nm', 'nn', 'no', 'np', 'nq', 'nr', 'ns', 'nt', 'nu', 'nv', 'nw', 'nx', 'ny', 'nz', 'o', 'oa', 'ob', 'oc', 'od', 'oe', 'of', 'og', 'oh', 'oi', 'oj', 'ok', 'ol', 'om', 'on', 'oo', 'op', 'oq', 'or', 'os', 'ot', 'ou', 'ov', 'ow', 'ox', 'oy', 'oz', 'p', 'pa', 'paulscode.sound.Channel', 'paulscode.sound.CommandObject', 'paulscode.sound.CommandThread', 'paulscode.sound.FilenameURL', 'paulscode.sound.ICodec', 'paulscode.sound.Library', 'paulscode.sound.ListenerData', 'paulscode.sound.MidiChannel', 'paulscode.sound.MidiChannel$1', 'paulscode.sound.MidiChannel$FadeThread', 'paulscode.sound.SimpleThread', 'paulscode.sound.SimpleThread$1', 'paulscode.sound.SoundBuffer', 'paulscode.sound.SoundSystem', 'paulscode.sound.SoundSystemConfig', 'paulscode.sound.SoundSystemConfig$Codec', 'paulscode.sound.SoundSystemException', 'paulscode.sound.SoundSystemLogger', 'paulscode.sound.Source', 'paulscode.sound.StreamThread', 'paulscode.sound.Vector3D', 'paulscode.sound.codecs.CodecJOrbis', 'paulscode.sound.codecs.CodecWav', 'paulscode.sound.libraries.ChannelLWJGLOpenAL', 'paulscode.sound.libraries.LibraryLWJGLOpenAL', 'paulscode.sound.libraries.SourceLWJGLOpenAL', 'pb', 'pc', 'pd', 'pe', 'pf', 'pg', 'ph', 'pi', 'pj', 'pk', 'pl', 'pm', 'pn', 'po', 'pp', 'pq', 'pr', 'ps', 'pt', 'pu', 'pv', 'pw', 'px', 'py', 'pz', 'q', 'qa', 'qb', 'qc', 'qd', 'qe', 'qf', 'qg', 'qh', 'qi', 'qj', 'qk', 'ql', 'qm', 'qn', 'qo', 'qp', 'qq', 'qr', 'qs', 'qt', 'qu', 'qv', 'qw', 'qx', 'qy', 'qz', 'r', 'ra', 'rb', 'rc', 'rd', 're', 'rf', 'rg', 'rh', 'ri', 'rj', 'rk', 'rl', 'rm', 'rn', 'ro', 'rp', 'rq', 'rr', 'rs', 'rt', 'ru', 'rv', 'rw', 'rx', 'ry', 'rz', 's', 'sa', 'sb', 'sc', 'sd', 'se', 'sf', 'sg', 'sh', 'si', 'sj', 'sk', 'sl', 'sm', 'sn', 'so', 'sp', 'sq', 'sr', 'ss', 'st', 'su', 'sv', 'sw', 'sx', 'sy', 'sz', 't', 'ta', 'tb', 'tc', 'td', 'te', 'tf', 'tg', 'th', 'ti', 'tj', 'tk', 'tl', 'tm', 'tn', 'to', 'tp', 'tq', 'tr', 'ts', 'tt', 'tu', 'tv', 'tw', 'tx', 'ty', 'tz', 'u', 'ua', 'ub', 'uc', 'ud', 'ue', 'uf', 'ug', 'uh', 'ui', 'uj', 'uk', 'ul', 'um', 'un', 'uo', 'up', 'uq', 'ur', 'us', 'ut', 'uu', 'uv', 'uw', 'ux', 'uy', 'uz', 'v', 'va', 'vb', 'vc', 'vd', 've', 'vf', 'vg', 'vh', 'vi', 'vj', 'vk', 'vl', 'vm', 'vn', 'vo', 'vp', 'vq', 'vr', 'vs', 'vt', 'vu', 'vv', 'vw', 'vx', 'vy', 'vz', 'w', 'wa', 'wb', 'wc', 'wd', 'we', 'wf', 'wg', 'wh', 'wi', 'wj', 'wk', 'wl', 'wm', 'wn', 'wo', 'wp', 'wq', 'wr', 'ws', 'wt', 'wu', 'wv', 'ww', 'wx', 'wy', 'wz', 'x', 'xa', 'xb', 'xc', 'xd', 'xe', 'xf', 'xg', 'xh', 'xi', 'xj', 'xk', 'xl', 'xm', 'xn', 'xo', 'xp', 'xq', 'xr', 'xs', 'xt', 'xu', 'xv', 'xw', 'xx', 'xy', 'xz', 'y', 'ya', 'yb', 'yc', 'yd', 'ye', 'yf', 'yg', 'yh', 'yi', 'yj', 'yk', 'yl', 'ym', 'yn', 'yo', 'yp', 'yq', 'yr', 'ys', 'yt', 'yu', 'yv', 'yw', 'yx', 'yy', 'z'], 'server': ['a', 'aa', 'ab', 'ac', 'ad', 'ae', 'af', 'ag', 'ah', 'ai', 'aj', 'ak', 'al', 'am', 'an', 'ao', 'ap', 'aq', 'ar', 'as', 'at', 'au', 'av', 'aw', 'ax', 'ay', 'az', 'b', 'ba', 'bb', 'bc', 'bd', 'be', 'bf', 'bg', 'bh', 'bi', 'bj', 'bk', 'bl', 'bm', 'bn', 'bo', 'bp', 'bq', 'br', 'bs', 'bt', 'bu', 'bv', 'bw', 'bx', 'by', 'bz', 'c', 'ca', 'cb', 'cc', 'cd', 'ce', 'cf', 'cg', 'ch', 'ci', 'cj', 'ck', 'cl', 'cm', 'cn', 'co', 'cp', 'cq', 'cr', 'cs', 'ct', 'cu', 'cv', 'cw', 'cx', 'cy', 'cz', 'd', 'da', 'db', 'dc', 'dd', 'de', 'df', 'dg', 'dh', 'di', 'dj', 'dk', 'dl', 'dm', 'dn', 'do', 'dp', 'dq', 'dr', 'ds', 'dt', 'du', 'dv', 'dw', 'dx', 'dy', 'dz', 'e', 'ea', 'eb', 'ec', 'ed', 'ee', 'ef', 'eg', 'eh', 'ei', 'ej', 'ek', 'el', 'em', 'en', 'eo', 'ep', 'eq', 'er', 'es', 'et', 'eu', 'ev', 'ew', 'ex', 'ey', 'ez', 'f', 'fa', 'fb', 'fc', 'fd', 'fe', 'ff', 'fg', 'fh', 'fi', 'fj', 'fk', 'fl', 'fm', 'fn', 'fo', 'fp', 'fq', 'fr', 'fs', 'ft', 'fu', 'fv', 'fw', 'fx', 'fy', 'fz', 'g', 'ga', 'gb', 'gc', 'gd', 'ge', 'gf', 'gg', 'gh', 'gi', 'gj', 'gk', 'gl', 'gm', 'gn', 'go', 'gp', 'gq', 'gr', 'gs', 'gt', 'gu', 'gv', 'gw', 'gx', 'gy', 'gz', 'h', 'ha', 'hb', 'hc', 'hd', 'he', 'hf', 'hg', 'hh', 'hi', 'hj', 'hk', 'hl', 'hm', 'hn', 'ho', 'hp', 'hq', 'hr', 'hs', 'ht', 'hu', 'hv', 'hw', 'hx', 'hy', 'hz', 'i', 'ia', 'ib', 'ic', 'id', 'ie', 'if', 'ig', 'ih', 'ii', 'ij', 'ik', 'il', 'im', 'in', 'io', 'ip', 'iq', 'ir', 'is', 'it', 'iu', 'iv', 'iw', 'ix', 'iy', 'iz', 'j', 'ja', 'jb', 'jc', 'jd', 'je', 'jf', 'jg', 'jh', 'ji', 'jj', 'jk', 'jl', 'jm', 'jn', 'jo', 'jp', 'jq', 'jr', 'js', 'jt', 'ju', 'jv', 'jw', 'jx', 'jy', 'jz', 'k', 'ka', 'kb', 'kc', 'kd', 'ke', 'kf', 'kg', 'kh', 'ki', 'kj', 'kk', 'kl', 'km', 'kn', 'ko', 'kp', 'kq', 'kr', 'ks', 'kt', 'ku', 'kv', 'kw', 'kx', 'ky', 'kz', 'l', 'la', 'lb', 'lc', 'ld', 'le', 'lf', 'lg', 'lh', 'li', 'lj', 'lk', 'll', 'lm', 'ln', 'lo', 'lp', 'lq', 'lr', 'ls', 'lt', 'lu', 'lv', 'lw', 'lx', 'ly', 'lz', 'm', 'ma', 'mb', 'mc', 'md', 'me', 'mf', 'mg', 'mh', 'mi', 'mj', 'mk', 'ml', 'mm', 'mn', 'mo', 'mp', 'mq', 'mr', 'ms', 'mt', 'mu', 'mv', 'mw', 'mx', 'my', 'mz', 'n', 'na', 'nb', 'nc', 'nd', 'ne', 'net.minecraft.server.MinecraftServer', 'nf', 'ng', 'nh', 'ni', 'nj', 'nk', 'nl', 'nm', 'nn', 'no', 'np', 'nq', 'nr', 'ns', 'nt', 'nu', 'nv', 'nw', 'nx', 'ny', 'nz', 'o', 'oa', 'ob', 'oc', 'od', 'oe', 'of', 'og', 'oh', 'oi', 'oj', 'ok', 'ol', 'om', 'on', 'oo', 'op', 'oq', 'or', 'os', 'ot', 'ou', 'ov', 'ow', 'ox', 'oy', 'oz', 'p', 'pa', 'pb', 'pc', 'pd', 'pe', 'pf', 'pg', 'ph', 'pi', 'pj', 'pk', 'pl', 'pm', 'pn', 'po', 'pp', 'pq', 'pr', 'ps', 'pt', 'pu', 'pv', 'pw', 'px', 'py', 'pz', 'q', 'qa', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z']}



REGISTRY_CLASSES = {'client': [None, 'eo', 'wp', 'ot', 'uu', 'uu', 'he', 'uu', 'om', 'nx', 'om', 'nx', 'gk', 'ne', 'mt', 'mt', 'mt', 'vg', 'bk', 'xf', 'fk', 'mt', 'uu', 'xq', 'rd', 'pt', 've', 'pc', 'ph', 'jq', 'rn', 'ru', 'jb', 'jq', 'h', 'ee', 'ut', 'wb', 'wb', 'tl', 'tl', 'l', 'l', 'ys', 'ys', 'uu', 'ah', 'hb', 'uu', 'fb', 'vm', 'yq', 'dd', 'ss', 'e', 'sm', 'mt', 'l', 'fi', 'ni', 'vl', 'tc', 'tc', 'uj', 'le', 'dp', 'pc', 'ss', 'uj', 'xr', 'bv', 'le', 'bv', 'bs', 'bs', 'db', 'db', 'oi', 'jr', 'nk', 'ac', 'or', 'rz', 'ri', 'fo', 'jw', 'fc', 'yi', 'oa', 'ly', 'ak', 'fc', 'um', 'wo', 'wo', 'ks', 'oq', None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None], 'server': [None, 'cx', 'oj', 'jf', 'na', 'na', 'eu', 'na', 'jc', 'is', 'jc', 'is', 'ei', 'ig', 'ic', 'ic', 'ic', 'nk', 'ar', 'ov', 'do', 'ic', 'na', 'pa', 'kl', 'jr', 'nj', 'jj', 'jn', 'gi', 'ku', 'kz', 'fz', 'gi', 'g', 'cq', 'mz', 'nw', 'nw', 'md', 'md', 'j', 'j', 'pw', 'pw', 'na', 't', 'er', 'na', 'dh', 'np', 'pt', 'bz', 'lu', 'e', 'lo', 'ic', 'j', 'dn', 'ij', 'no', 'lz', 'lz', 'mt', 'hc', 'cg', 'jj', 'lu', 'mt', 'pd', 'bc', 'hc', 'bc', 'az', 'az', 'by', 'by', 'iz', 'gj', 'ik', 'p', 'je', 'lf', 'ko', 'dt', 'gn', 'di', 'po', 'iu', 'hr', 'z', 'di', 'mw', 'oi', 'oi', 'gv', 'jd', None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]}

def helpers():
    require(Path(__file__).resolve() == (ROOT/'scripts/reference/storage/compare_chunk_storage.py').resolve(), 'canonical executing chunk-storage source identity')
    old_path = ROOT/'scripts/reference/chunk/compare_chunk.py'
    raw = old_path.read_bytes()
    old = types.ModuleType('_stored_chunk_world')
    old.__file__ = str(old_path)
    exec(compile(raw, str(old_path), 'exec'), old.__dict__)
    common, executed = old.shared_helper(ROOT)
    executed += [{'path': str(old_path.resolve()), **fp(raw)}, {'path': str(LEVEL_PATH.resolve()), **fp(LEVEL_RAW)}]
    return common, old, executed


def files_in(directory, common, limit=MAX_RECORD):
    inventory = common.world_inventory(directory, hashes=True)
    require(inventory['exists'] and inventory['complete'] and inventory['error'] is None and inventory['files'] <= 128 and inventory['bytes'] <= MAX_RECORD, 'complete bounded owned file inventory')
    files = {}
    for row in inventory['entries']:
        if row['kind'] == 'file':
            path = directory/row['path']
            common.no_link_components(path)
            require(row['bytes'] <= limit, 'bounded owned file')
            with path.open('rb') as stream:
                raw = stream.read(limit+1)
            require(fp(raw) == {key: row[key] for key in ('bytes', 'sha256')}, 'stable complete owned bytes')
            files[row['path']] = raw
    return files, inventory


def archive(files, directory):
    directory.mkdir()
    for name, raw in files.items():
        require(type(name) is str and not name.startswith('/') and '\\' not in name and all(x not in ('', '.', '..') for x in name.split('/')), 'safe owned relative path')
        path = directory/name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(raw)


def world_changes(before, after):
    require(set(before) <= set(after) and before.get('.chunk-storage-owned') == after.get('.chunk-storage-owned'), 'copied owned world retains every input path and marker')
    changed = [name for name in set(before)|set(after) if before.get(name) != after.get(name)]
    require(all(name == 'world/session.lock' or name.startswith('world/region/') and re.fullmatch(r'world/region/r\.-?[0-9]+\.-?[0-9]+\.mcr', name) for name in changed), 'only genuine session and region APIs change the copied world')
    require(set(after)-set(before) == {'world/region/r.-1.0.mcr'}, 'actual wrong-location direct writer creates only its original target region')
    return sorted(changed)


def baseline_record(files):
    raw = files['world/region/r.0.0.mcr']
    require(8192 <= len(raw) <= MAX_RECORD, 'complete saved baseline region')
    packed, = struct.unpack_from('>I', raw, 4)
    start, sectors = (packed >> 8)*4096, packed & 255
    require(sectors and start >= 8192 and start+sectors*4096 <= len(raw), 'saved neighbor has bounded allocated region entry')
    length, = struct.unpack_from('>I', raw, start)
    require(1 <= length <= sectors*4096-4 and raw[start+4] in (1, 2), 'complete saved neighbor region record')
    decoder = zlib.decompressobj(31 if raw[start+4] == 1 else 15)
    value = decoder.decompress(raw[start+5:start+4+length], MAX_NBT+1)
    require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and len(value) <= MAX_NBT, 'one bounded genuine saved neighbor container')
    return parse_raw(value)


def validate_preparation(value, side, files, jar_url, old):
    keys(value, ('schema', 'scope', 'side', 'scenario', 'seed_hex', 'jar_sha256', 'platform_parent', 'class_origins', 'bindings',
                 'setup_metadata', 'time_api', 'initial', 'operations', 'saved', 'external_effects'), 'complete genuine saved-world preparation')
    require(type(value['schema']) is int and value['schema'] == 1 and value['scope'] == 'cache-callback-boundary' and value['side'] == side and
            value['scenario'] == 'prepare' and value['seed_hex'] == '0000000000000017' and value['jar_sha256'] == level.PIN[side] and
            value['platform_parent'] is True and value['time_api'] is None and value['external_effects'] ==
            'Original loader, generation and terrain-feature internals remain opaque; only cache coordinator transitions are compared.', 'actual original preparation context')
    expected = dict(zip(('world', 'cache', 'storage', 'loader', 'generator'), ('fd', 'ok', 'mx', 'ld', 'yf') if side == 'client' else ('dj', 'jb', 'ie', 'hb', 'pk')))
    require(same_json(value['bindings'], expected) and type(value['class_origins']) is dict and set(expected.values()) <= set(value['class_origins']) and
            all(name in JAR_CLASS_NAMES[side] and origin == jar_url for name, origin in value['class_origins'].items()), 'genuine base-world constructor/cache/storage/provider class origins')
    require(type(value['operations']) is list and len(value['operations']) == 1, 'one genuine neighbor acquisition for saved baseline')
    operation = value['operations'][0]
    require(type(operation['index']) is int and operation['index'] == 0 and operation['method'] == 'c' and same_json(operation['coordinates'], [1, 0]) and
            operation['result']['class'] == ('lm' if side == 'client' else 'hi'), 'actual saved neighbor operation')
    for snapshot in (value['initial'], operation['before'], operation['after'], value['saved']):
        checked_world(old, snapshot['world'], 'genuine preparation world')
        world_types(snapshot['world'], side)
        require(snapshot['world_time'] == '0000000000000000', 'preparation saves actual zero world time')
    saved = value['saved']
    require(len(saved['world']['map']['entries']) == len(saved['world']['insertion_list']['entries']) == 2 and len(saved['cached_locals']) == 2, 'complete ordinary origin/neighbor saved baseline')
    neighbor = next((row for row in saved['cached_locals'] if row['object'] == operation['result']), None)
    require(neighbor is not None and same_json(neighbor['coordinates'], [1, 0]), 'saved neighbor identity and coordinates')
    tree = baseline_record(files)
    current = child(tree, 'Level')
    require(current is not None and child(current, 'xPos')['value'] == 1 and child(current, 'zPos')['value'] == 0 and child(current, 'LastUpdate')['value'] == 0, 'genuine raw baseline schema and world clock')
    for role, (field, length) in ARRAYS.items():
        require(neighbor['data'][role]['length'] == length and child(current, field)['type'] == 7 and child(current, field)['value'] == neighbor['data'][role]['values'], 'all saved baseline arrays agree with genuine creator snapshot')
    require(set(files) == {'world/level.dat', 'world/session.lock', 'world/region/r.0.0.mcr'}, 'complete three-file original saved baseline')


def verify_bindings(side, jar):
    with zipfile.ZipFile(jar) as archive_:
        names = sorted(name[:-6].replace('/', '.') for name in archive_.namelist() if name.endswith('.class'))
        require(names == JAR_CLASS_NAMES[side], 'complete pinned original class-name inventory')
        parsed = {}
        for name, expected in BINDING_PINS[side]['classes'].items():
            raw = archive_.read(name.replace('.', '/')+'.class')
            require(fp(raw) == expected, 'actual original class body fingerprint')
            parsed[name] = level.class_members(raw)
        for row in BINDING_PINS[side]['methods'].values():
            method = parsed[row['class']][1][(row['method'], row['signature'])]
            require(method['modifiers'] == row['modifiers'] and fp(method['code']) == {key: row[key] for key in ('bytes', 'sha256')}, 'actual exact original constructor/method bytecode fingerprint')
        for row in BINDING_PINS[side]['fields'].values():
            require(parsed[row['class']][0][(row['name'], row['signature'])] == row['modifiers'], 'actual exact original field binding')


def original_signature(value, files):
    result = {'registry_map': value['registry_before']['map'], 'cases': []}
    for row in value['cases']:
        current = {'name': row['name'], 'input': level.order_only(parse_raw(artifact(row['input'], files)))}
        for role in ('static', 'region'):
            original = row[role]
            data = {'status': original['status'], 'stage': original['stage']}
            if original['status'] == 'ok':
                data['before'] = projection(original['chunk_before_write'])
                data['after'] = projection(original['chunk_after_write'])
                data['output'] = level.order_only(parse_raw(artifact(original['output_level' if role == 'static' else 'output_root'], files)))
            elif original['status'] == 'exception':
                data['exception_class'] = original['class']
            current[role] = data
        result['cases'].append(current)
    return result


def checked_process(common, command, directory, name, *, world=None, stdin=None, seconds=90):
    result = common.run_live(list(map(str, command)), directory/(name+'-out.txt'), directory/(name+'-err.txt'), directory,
                             seconds=seconds, stdout_limit=MAX_RECORD, stderr_limit=4*1024*1024, world_root=world, stdin_path=stdin)
    require(type(result.returncode) is int and result.returncode == 0 and result.timed_out is False and result.stop_reason is None and result.stdout_observed == result.stdout_retained and
            result.stderr_observed == result.stderr_retained, 'successful complete bounded child; child77 is failure')
    for count in (result.stdout_observed, result.stdout_retained, result.stderr_observed, result.stderr_retained, result.inventory_polls):
        level.integer_value(count, 0, MAX_RECORD, 'complete typed bounded child counter')
    return {**asdict(result), 'stdout': common.file_record(directory/(name+'-out.txt'), byte_limit=MAX_RECORD),
            'stderr': common.file_record(directory/(name+'-err.txt'), byte_limit=4*1024*1024)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('client', 'server', 'work-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--capture-only', action='store_true')
    parser.add_argument('--tested-commit', default='')
    for name in ('java', 'javac', 'javap'):
        parser.add_argument('--'+name)
    args = parser.parse_args(argv)
    output, common, inputs = None, None, {}
    report = {'schema': 1, 'status': 'running', 'tested_commit': args.tested_commit or None, 'inputs_before': inputs, 'errors': []}
    executing, status = False, 1
    try:
        common, old, executed = helpers()
        require(not args.tested_commit or re.fullmatch(r'[0-9a-f]{40}', args.tested_commit), 'full lowercase tested source label')
        require(not any(os.environ.get(name) for name in level.JAVA_ENVIRONMENT), 'uninjected Java environment')
        require(args.capture_only or args.executable is not None, '--executable required for C comparison')
        source_paths = [ROOT/name for name in SOURCES]
        attributed = [Path(__file__).resolve(), Path(sys.executable).resolve(), ROOT/'CMakeLists.txt', ROOT/'tests/world/storage/chunk_probe.c', *source_paths]
        attributed += list((ROOT/'src').rglob('*.c')) + list((ROOT/'src').rglob('*.h')) + list((ROOT/'include').rglob('*.h')) + [Path(row['path']) for row in executed]
        work = level.validate_work_dir(args.work_dir, attributed+[args.client, args.server, args.executable, args.java, args.javac, args.javap])
        tools = common.runtime_tools(args)
        release = tools['jdk_release'].read_text(encoding='utf-8')
        match = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
        if match is None or int(match.group(1)) < 17:
            raise common.Unavailable('chunk storage helper requires JDK17 or newer')
        jars = {side: common.no_link_components(getattr(args, side)).resolve() for side in level.PIN}
        for side, jar in jars.items():
            if not jar.is_file():
                raise common.Unavailable('missing supplied '+side+' original archive')
            require(common.file_record(jar, byte_limit=2*1024*1024)['sha256'] == level.PIN[side], 'exact pinned original archive')
        if args.executable is not None:
            common.no_link_components(args.executable)
            if not args.executable.is_file():
                raise common.Unavailable('missing C chunk storage executable')
            attributed.append(args.executable.resolve())
        attributed += list(jars.values())+list(tools.values())
        for path in attributed:
            common.no_link_components(path)
            inputs[str(path.resolve())] = common.file_record(path, byte_limit=512*1024*1024)
        for row in executed:
            require(inputs[str(Path(row['path']).resolve())]['sha256'] == row['sha256'], 'executed helper source attribution')
        work = level.validate_work_dir(work, attributed)
        work.mkdir(parents=True, exist_ok=True)
        candidate = work/('capture-'+uuid.uuid4().hex)
        candidate.mkdir()
        output = candidate.resolve()
        report = {'schema': 1, 'status': 'running', 'scope': 'chunk-storage-codec', 'mode': 'original-only' if args.capture_only else 'live-original-C',
                  'tested_commit': args.tested_commit or None, 'source_label': 'Caller label; actual executing source, tools/classes and binary bytes are attributed independently.',
                  'started_utc': common.utc_now(), 'artifact_root': str(output), 'python': sys.version, 'platform': platform.platform(),
                  'executed_source_modules': executed, 'preparations': {}, 'runs': [], 'c_records': [], 'errors': [],
                  'limits': {'child_seconds': 90, 'heap_mib': 512, 'stream_bytes': MAX_RECORD, 'world_entries': 256, 'world_bytes': MAX_RECORD,
                             'policy': 'Bounded children and monitored complete inventories, not an OS process-tree/filesystem quota.'},
                  'qualifications': 'C fixed-array shape validation and opaque/unknown retention are explicit safety departures. C does not run entity/tile factories or attachment, nor own world filesystem save/lock/rotation/cache operations.'}
        report['inputs_before'] = inputs
        common.write_json(output/'manifest.json', report)
        for name in ('source', 'classes', 'empty'):
            (output/name).mkdir()
        for path in source_paths+[Path(__file__).resolve()]:
            (output/'source'/path.name).write_bytes(path.read_bytes())
        executing = True
        common.tool_versions(tools, output, report)
        require(all(row['major'] >= 17 for row in report['versions'].values()), 'actual JDK17 tools')
        command = [tools['javac'], '-J-Xmx512m', '--release', '17', '-proc:none', '-implicit:none', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
                   '-classpath', output/'empty', '-sourcepath', output/'empty', '--module-path', output/'empty', '-d', output/'classes',
                   *[output/'source'/path.name for path in source_paths]]
        report['compile'] = checked_process(common, command, output, 'compile', seconds=30)
        class_files = list((output/'classes').iterdir())
        require({path.name for path in class_files} == CLASS_NAMES and all(path.is_file() for path in class_files), 'all ten exact project helper classes')
        report['classes_before'] = {path.name: common.file_record(path, byte_limit=2*1024*1024) for path in sorted(class_files)}
        for path in class_files:
            require(path.read_bytes()[:8] == b'\xca\xfe\xba\xbe\x00\x00\x00\x3d', 'strict Java17 helper class')
        signatures = {}
        for side, jar in jars.items():
            verify_bindings(side, jar)
            preparation_dir = output/('prepare-'+side); preparation_dir.mkdir()
            prepared_world = preparation_dir/'owned'
            preparation_process = checked_process(common, [tools['java'], '-Xint', '-Xmx512m', '-cp', output/'classes', 'CacheCallbacksObservation', jar, side, 'prepare', '0000000000000017', prepared_world, preparation_dir/'snapshots.json'], preparation_dir, 'prepare', world=prepared_world)
            require((preparation_dir/'snapshots.json').stat().st_size <= MAX_RECORD, 'complete preparation snapshot')
            preparation = strict_json((preparation_dir/'snapshots.json').read_bytes())
            baseline_files, baseline_inventory = files_in(prepared_world, common)
            validate_preparation(preparation, side, baseline_files, jar.as_uri().replace('file:///', 'file:/', 1), old)
            baseline = preparation_dir/'baseline'; shutil.copytree(prepared_world, baseline)
            copied_files, copied_inventory = files_in(baseline, common)
            require(copied_files == baseline_files, 'complete independent genuine saved baseline')
            report['preparations'][side] = {'process': preparation_process, 'snapshot': common.file_record(preparation_dir/'snapshots.json', byte_limit=MAX_RECORD),
                                          'world': baseline_inventory, 'baseline_before': copied_inventory}
            documents, artifacts_equal = [], []
            for repeat in (0, 1):
                label = side+'-'+str(repeat); directory = output/label; directory.mkdir()
                world = directory/'owned'; shutil.copytree(baseline, world); (world/'.chunk-storage-owned').write_bytes(MARKER)
                before, before_inventory = files_in(world, common); archive(before, directory/'before')
                snapshot = directory/'snapshots.json'
                process = checked_process(common, [tools['java'], '-Xint', '-Xmx512m', '-Djava.awt.headless=true', '-cp', output/'classes', 'ChunkStorageObservation', jar, side, world, snapshot], directory, 'original', world=world)
                require(snapshot.stat().st_size <= MAX_RECORD, 'complete chunk snapshot byte bound')
                raw = snapshot.read_bytes(); value = strict_json(raw)
                artifacts, artifact_inventory = files_in(Path(str(snapshot)+'.nbt'), common, MAX_NBT)
                threads = {}
                for phase in ('before', 'after'):
                    path = Path(str(snapshot)+'.threads-'+phase+'.json')
                    require(path.stat().st_size <= 1024*1024, 'complete thread inventory byte bound')
                    threads[phase] = strict_json(path.read_bytes())
                validate_original(value, side, artifacts, threads['before'], threads['after'], jar.as_uri().replace('file:///', 'file:/', 1), old)
                require(same_json(level.order_only(parse_raw(artifact(value['saved_baseline'], artifacts))), level.order_only(baseline_record(baseline_files))), 'observed raw baseline equals the genuine preparer saved bytes')
                after, after_inventory = files_in(world, common); archive(after, directory/'after')
                changes = world_changes(before, after)
                report['runs'].append({'side': side, 'repeat': repeat, 'process': process, 'snapshot': common.file_record(snapshot, byte_limit=MAX_RECORD),
                                       'world_before': before_inventory, 'world_after': after_inventory, 'world_changed': changes,
                                       'artifacts': artifact_inventory, 'threads': {phase: common.file_record(Path(str(snapshot)+'.threads-'+phase+'.json'), byte_limit=1024*1024) for phase in ('before', 'after')},
                                       'bindings': BINDING_PINS[side]})
                documents.append(raw); artifacts_equal.append(artifacts)
                if not args.capture_only:
                    c_dir = directory/'c'; c_dir.mkdir()
                    for plan in replay_plans(value, artifacts):
                        stdin = c_dir/(plan['name']+'-in.txt'); stdin.write_bytes(plan['input'].hex().encode('ascii')+b'\n')
                        c_process = checked_process(common, [args.executable.resolve(), *plan['argv']], c_dir, plan['name'], stdin=stdin, seconds=15)
                        require(c_process['stderr_retained'] == 0, 'successful C probe has no sanitizer/internal diagnostics')
                        c_raw = Path(c_process['stdout']['path']).read_bytes()
                        require(c_raw.endswith(b'\n') and c_raw.count(b'\n') == 1 and len(c_raw) <= 2*1024*1024, 'one bounded complete C JSON response')
                        compare_c(strict_json(c_raw), plan)
                        report['c_records'].append({'side': side, 'repeat': repeat, 'name': plan['name'], 'kind': plan['kind'], 'input': common.file_record(stdin, byte_limit=MAX_RECORD), 'process': c_process, 'comparison': 'pass'})
                if repeat == 0:
                    signatures[side] = original_signature(value, artifacts)
                common.write_json(output/'manifest.json', report)
            require(documents[0] == documents[1] and artifacts_equal[0] == artifacts_equal[1], 'full original observation/control JSON and every raw NBT artifact byte equal')
            final_files, final_inventory = files_in(baseline, common)
            require(final_files == baseline_files, 'complete immutable genuine baseline retained')
            report['preparations'][side]['baseline_after'] = final_inventory
        require(same_json(signatures['client'], signatures['server']), 'all paired original inputs, status/classes, full arrays/flags and serialized typed trees agree; compound order only')
        require(len(report['runs']) == 4 and len(report['c_records']) == (0 if args.capture_only else 268), 'all contexts and C comparisons')
        report['comparisons'] = {'client/control': 'byte-equal-complete-records-and-NBT', 'server/control': 'byte-equal-complete-records-and-NBT', 'client/server': 'complete-supported-projection-and-typed-NBT'}
        report['status'], status = 'pass', 0
    except Exception as error:
        unavailable = common is not None and isinstance(error, common.Unavailable) and not executing
        status = 77 if unavailable else 1
        if report is not None:
            report['status'] = 'unavailable' if unavailable else 'fail'; report['errors'].append(str(error))
        if output is not None:
            print(('UNAVAILABLE: ' if unavailable else 'FAILURE: ')+str(error), file=sys.stderr)
    finally:
        if report is not None:
            report['inputs_after'] = {}
            for name, before in inputs.items():
                try:
                    current = common.file_record(Path(name), byte_limit=before['bytes']); report['inputs_after'][name] = current
                    require(current == before, 'attributed input changed')
                except Exception as error:
                    report['status'], status = 'fail', 1; report['errors'].append('attributed input changed: '+name+': '+str(error))
            if 'classes_before' in report:
                try:
                    current = {path.name: common.file_record(path, byte_limit=2*1024*1024) for path in sorted((output/'classes').iterdir())}
                    report['classes_after'] = current; require(current == report['classes_before'], 'compiled helper inventory/bytes retained')
                except Exception as error:
                    report['status'], status = 'fail', 1; report['errors'].append(str(error))
            if common is not None:
                report['finished_utc'] = common.utc_now()
            if output is None:
                print(json.dumps(report), file=sys.stderr)
            else:
                try:
                    common.write_json(output/'manifest.json', report)
                    print(report['status'].upper()+': '+str(output/'manifest.json'))
                except Exception as error:
                    report['status'], status = 'fail', 1
                    report['errors'].append('cannot retain outcome manifest: '+str(error))
                    print(json.dumps(report), file=sys.stderr)
    return status


if __name__ == '__main__':
    sys.exit(main())
