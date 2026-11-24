"""Independent disk audit of the attack fixture's actual fd.l177 autosaves.

The native observer supplies live operands and raw arrays. This module decodes
the resulting files independently; it does not call a native serializer.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[3]


def require(value, message):
    if not value:
        raise AssertionError('Attack autosave: ' + message)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def unique(pairs):
    value = dict(pairs)
    require(len(value) == len(pairs), 'unique manifest fields')
    return value


def bounded(path, limit=1048576):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= limit,
            'bounded regular evidence file ' + str(path))
    return path.read_bytes()


def manifest(path):
    return json.loads(bounded(path), object_pairs_hook=unique)


def integer(value, bits=64):
    require(type(value) is int and -(1 << (bits - 1)) <= value < (1 << (bits - 1)),
            'signed integer operand')
    return value


def add64(a, b):
    return ((a + b + (1 << 63)) % (1 << 64)) - (1 << 63)


def selection(entry):
    """Derive cp.save(false)'s insertion-order, 24-attempt prefix independently."""
    now, period = integer(entry['time']), integer(entry['period'], 32)
    require(period == 40 and entry['next_time'] == add64(now, 1) and
            entry['next_time'] % period == 0, 'actual next-Time modulo autosave trigger')
    ledger = entry['cache']
    require(type(ledger) is list and len(ledger) <= 4096, 'bounded insertion ledger')
    selected, simulated = [], {}
    for index, row in enumerate(ledger):
        require(set(row) == {'identity', 'x', 'z', 'blank', 'never_save', 'dirty',
                             'has_entities', 'last_save'}, 'exact cache operand schema')
        identity = row['identity']
        require(type(identity) is str and re.fullmatch('[0-9a-f]+', identity) and
                int(identity, 16) != 0, 'actual nonnull Chunk identity')
        for key in ('x', 'z'):
            integer(row[key], 32)
        integer(row['last_save'])
        for key in ('blank', 'never_save', 'dirty', 'has_entities'):
            require(type(row[key]) is bool, 'typed cache flags')
        old = simulated.setdefault(identity, row.copy())
        require((old['x'], old['z']) == (row['x'], row['z']), 'stable Chunk identity coordinate')
        eligible = not old['blank'] and not old['never_save'] and (old['dirty'] or
            (old['has_entities'] and now >= add64(old['last_save'], 600)))
        if eligible:
            selected.append(index)
            old['dirty'], old['last_save'] = False, now
            if len(selected) == 24:
                break
    actual = entry['selected']
    require(type(actual) is list and [row['cache_index'] for row in actual] == selected,
            'exact independent partial-save selection and order')
    coordinates = set()
    for row, index in zip(actual, selected):
        require(set(row) == {'cache_index', 'x', 'z', 'populated', 'entities'} and
                type(row['cache_index']) is int and type(row['populated']) is bool and
                type(row['entities']) is int and 0 <= row['entities'] <= 1024,
                'exact selected Chunk schema')
        require((row['x'], row['z']) == (ledger[index]['x'], ledger[index]['z']),
                'actual selected coordinates')
        coordinates.add((row['x'], row['z']))
    require(len(coordinates) == len(actual), 'distinct selected disk coordinates')
    return coordinates


def files(directory):
    result, total = {}, 0
    for path in sorted(directory.rglob('*')):
        require(not path.is_symlink(), 'no linked save objects')
        if path.is_file():
            data = bounded(path, 16 * 1048576)
            total += len(data)
            require(len(result) < 4096 and total <= 128 * 1048576, 'bounded saved inventory')
            result[path.relative_to(directory).as_posix()] = data
    require(result, 'nonempty actual save snapshot')
    return result


def initial_continuity(before, entry):
    require(set(entry) - set(before) == {'alpha/session.lock'} and
            len(entry['alpha/session.lock']) == 8 and
            all(entry.get(name) == data for name, data in before.items()),
            'only source session token before the first autosave')


def regions(images, parser):
    result, records = {}, {}
    for name, data in images.items():
        match = re.fullmatch(r'alpha/region/r\.(-?\d+)\.(-?\d+)\.mcr', name)
        if match is None:
            continue
        require(8192 <= len(data) <= 16 * 1048576 and len(data) % 4096 == 0,
                'complete bounded region extent')
        rx, rz = map(int, match.groups())
        occupied = {0, 1}
        for slot in range(1024):
            location, = struct.unpack_from('>I', data, slot * 4)
            if not location:
                continue
            sector, count = location >> 8, location & 255
            require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(data) and
                    not occupied.intersection(range(sector, sector + count)),
                    'complete nonoverlapping region allocation')
            occupied.update(range(sector, sector + count))
            offset = sector * 4096
            length, = struct.unpack_from('>I', data, offset)
            require(1 < length <= count * 4096 - 4 and data[offset + 4] == 2,
                    'source selector2 record extent')
            tree = parser.inflate(data[offset + 5:offset + 4 + length], 15)
            require(tree.get('Level', [None])[0] == 10, 'actual Level compound')
            level = tree['Level'][1]
            coordinate = (rx * 32 + slot % 32, rz * 32 + slot // 32)
            require(coordinate not in records and level.get('xPos') == [3, coordinate[0]] and
                    level.get('zPos') == [3, coordinate[1]], 'actual unique saved coordinates')
            records[coordinate] = dict(name=name, slot=slot, offset=offset, count=count,
                record_bytes=4 + length,
                stamp=struct.unpack_from('>I', data, 4096 + slot * 4)[0],
                wire=data[offset:offset + count * 4096], tree=tree)
        result[name] = data
    require(result and len(records) <= 4096, 'bounded complete region inventory')
    return result, records


def region_changes(before, after, selected, parser):
    old_regions, old = regions(before, parser)
    new_regions, new = regions(after, parser)
    require(set(old) <= set(new) and set(new) - set(old) <= selected,
            'no lost or unselected added saved Chunk')
    require(selected <= set(new), 'every selected Chunk reached disk')
    for coordinate in set(new) - selected:
        require(coordinate in old and old[coordinate] == new[coordinate],
                'all unselected records and timestamps unchanged')
    require(set(old_regions) <= set(new_regions), 'no region removed')
    # The writer changes selected header cells and exactly 4+length bytes at
    # the NEW record offset. Reused tail padding and abandoned old payloads
    # are not writes. Newly appended bytes outside those records are zero.
    for name, data in new_regions.items():
        previous = old_regions.get(name, b'')
        require(len(data) >= len(previous), 'source region never shrinks')
        allowed = bytearray(len(data))
        for coordinate in selected:
            record = new[coordinate]
            if record['name'] != name:
                continue
            slot, at, size = record['slot'], record['offset'], record['record_bytes']
            allowed[slot * 4:slot * 4 + 4] = b'\1' * 4
            allowed[4096 + slot * 4:4100 + slot * 4] = b'\1' * 4
            allowed[at:at + size] = b'\1' * size
        require(all(a == b or allowed[index] for index, (a, b) in
                    enumerate(zip(previous, data))), 'no writes outside selected region extents')
        require(all(allowed[index] or data[index] == 0 for index in
                    range(len(previous), len(data))), 'only zero padding outside new selected extents')
    return new, sum(map(len, new_regions.values())) - sum(map(len, old_regions.values()))


def typed(tag):
    kind, value = tag
    if kind == 10:
        return [kind, {key: typed(item) for key, item in value.items()}]
    if kind == 9:
        child, values = value
        return [kind, [child if values else 0, [typed([child, item])[1] for item in values]]]
    return [kind, value]


def source_lists(tag):
    """Source nu writes an empty list's child byte as 1; all other bits matter."""
    kind, value = tag
    if kind == 10:
        for child in value.values():
            source_lists(child)
    if kind == 9:
        child, items = value
        require(child == (child if items else 1), 'source empty List child type1')
        for item in items:
            source_lists([child, item])


def metadata(images, entry, exit_value, folder, parser):
    tree = parser.inflate(images['alpha/level.dat'], 31)
    require(set(tree) == {'Data'} and tree['Data'][0] == 10, 'fresh metadata Data only')
    data = tree['Data'][1]
    expected = {'RandomSeed': [4, entry['seed']], 'Time': [4, entry['time']],
        'SizeOnDisk': [4, entry['size']], 'LevelName': [8, ''.join(map(chr, entry['name_units']))],
        'version': [3, 19132], 'rainTime': [3, entry['rain_time']],
        'raining': [1, int(entry['raining'])], 'thunderTime': [3, entry['thunder_time']],
        'thundering': [1, int(entry['thundering'])]}
    for key, value in zip(('SpawnX', 'SpawnY', 'SpawnZ'), entry['spawn']):
        expected[key] = [3, value]
    require(set(data) == set(expected) | {'LastPlayed', 'Player'} and
            all(data[key] == value for key, value in expected.items()),
            'every saved Info scalar including pre-store Time and pre-growth size')
    require(data['LastPlayed'][0] == 4 and entry['first_millis'] <=
            data['LastPlayed'][1] <= exit_value['last_millis'], 'actual writer clock interval')
    player = parser.Nbt(bounded(folder / 'player.nbt')).root()
    require(typed(data['Player']) == typed([10, player]), 'entire actual live Player typed NBT')
    source_lists(data['Player'])
    return data['Player'][1]


def entity_list(entry, exit_value):
    require(entry['observer_unchanged'] is True and entry['unnamed_player_excluded'] is True,
            'passive EntityList admission and genuine unnamed Player exclusion')
    for state in (entry['entity_list'], exit_value['entity_list']):
        require(type(state) is dict and set(state) == {'phase', 'bytes', 'maps', 'counts'} and
                type(state['phase']) is int and state['phase'] in (0, 2) and
                type(state['bytes']) is int and 0 <= state['bytes'] <= 256 * 1048576 and
                type(state['maps']) is list and len(state['maps']) == 4 and
                type(state['counts']) is list and len(state['counts']) == 4,
                'exact passive EntityList snapshot schema')
        require(all(type(value) is str and re.fullmatch('[0-9a-f]{1,16}', value)
                    for value in state['maps']) and
                all(type(value) is int for value in state['counts']),
                'typed actual map identities and counts')
        if state['phase'] == 0:
            require(state['maps'] == ['0'] * 4 and state['counts'] == [0] * 4,
                    'genuine cold EntityList has no published maps')
        else:
            require(len(set(state['maps'])) == 4 and all(int(value, 16) != 0
                    for value in state['maps']) and state['counts'] == [24] * 4,
                    'actual READY EntityList maps have all24 registrations')
    calls = entry['entity_list_calls']
    require(type(calls) is int and 0 <= calls <= 24 * 8 * 1024,
            'bounded actual selected slice calls')
    before, after = entry['entity_list'], exit_value['entity_list']
    if before['phase'] == 0 and calls:
        require(after['phase'] == 2 and after['bytes'] > before['bytes'],
                'only the actual source save initializes cold EntityList')
    else:
        require(after == before, 'no uncalled or repeated EntityList initialization')


def chunks(records, entry, exit_value, folder, parser):
    keys = {'xPos', 'zPos', 'LastUpdate', 'Blocks', 'Data', 'SkyLight', 'BlockLight',
            'HeightMap', 'TerrainPopulated', 'Entities', 'TileEntities'}
    files_used = {'entry.json', 'exit.json', 'player.nbt'}
    for ordinal, chunk in enumerate(entry['selected']):
        coordinate = chunk['x'], chunk['z']
        record = records[coordinate]
        tree = record['tree']
        require(set(tree) == {'Level'} and set(tree['Level'][1]) == keys,
                'fresh exact source Chunk field set')
        level = tree['Level'][1]
        require(level['LastUpdate'] == [4, entry['time']] and level['TerrainPopulated'] ==
                [1, int(chunk['populated'])], 'selected live Chunk scalars')
        raw_name = f'chunk-{ordinal}.bin'
        raw = bounded(folder / raw_name)
        require(len(raw) == 82176, 'all five live Chunk arrays')
        files_used.add(raw_name)
        offset = 0
        for key, length in [('Blocks', 32768), ('Data', 16384), ('SkyLight', 16384),
                            ('BlockLight', 16384), ('HeightMap', 256)]:
            require(level[key] == [7, raw[offset:offset + length]], 'all raw bytes of ' + key)
            offset += length
        entities = []
        for index in range(chunk['entities']):
            name = f'chunk-{ordinal}-entity-{index}.nbt'
            entities.append(parser.Nbt(bounded(folder / name)).root())
            files_used.add(name)
        require(typed(level['Entities']) == typed([9, [10, entities]]) and
                level['TileEntities'] == [9, [1, []]], 'entire ordered actual Entity slice records')
        source_lists(level['Entities'])
        require(entry['first_millis'] // 1000 <= record['stamp'] <=
                exit_value['last_millis'] // 1000, 'actual region writer clock interval')
    require({path.name for path in folder.iterdir() if path.is_file()} == files_used and
            {path.name for path in folder.iterdir() if path.is_dir()} == {'before', 'after'},
            'complete observer evidence inventory')


def audit(evidence, stdout):
    parser = load('attack_independent_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    entries = re.findall(r'^ATTACK_AUTOSAVE_ENTRY event=(\d+) time=(-?\d+) next=(-?\d+) '
        r'period=(\d+) selected=(\d+) source=fd\.l:177 focused=1$', stdout, re.M)
    returns = re.findall(r'^ATTACK_AUTOSAVE_RETURN event=(\d+) time=(-?\d+) selected=(\d+) '
        r'source-returned=1 time-store-not-entered=1$', stdout, re.M)
    summaries = re.findall(r'^ATTACK_AUTOSAVE_SUMMARY entries=(\d+) returns=(\d+) '
        r'passive-observer=1 normal-timer=1$', stdout, re.M)
    post_death = re.findall(r'^ATTACK_AUTOSAVE_POST_DEATH event=(\d+) world-remove=1 '
        r'player-score=(-?\d+)$', stdout, re.M)
    require(0 < len(entries) <= 32 and len(entries) == len(returns) and summaries ==
            [(str(len(entries)), str(len(entries)))], 'exact completed autosave receipt counts')
    require(sum(line.startswith('ATTACK_AUTOSAVE_') for line in stdout.splitlines()) ==
            2 * len(entries) + 2 and len(post_death) == 1,
            'no malformed or extra autosave receipts; one post-removal save')
    root = evidence / 'autosaves'
    require({path.name for path in root.iterdir()} == {f'save-{i}' for i in range(len(entries))},
            'all actual source autosave event folders')
    previous, report = files(evidence / 'before'), []
    for index, (entry_receipt, return_receipt) in enumerate(zip(entries, returns)):
        folder = root / f'save-{index}'
        entry, exit_value = manifest(folder / 'entry.json'), manifest(folder / 'exit.json')
        require(set(entry) == {'time', 'next_time', 'period', 'first_millis', 'seed', 'size',
            'spawn', 'rain_time', 'thunder_time', 'raining', 'thundering', 'name_units',
            'cache', 'selected', 'entity_list', 'entity_list_calls', 'observer_unchanged',
            'unnamed_player_excluded'}, 'exact entry operand schema')
        for key in ('time', 'next_time', 'first_millis', 'seed', 'size'):
            integer(entry[key])
        require(type(entry['spawn']) is list and len(entry['spawn']) == 3, 'three spawn operands')
        for value in entry['spawn'] + [entry['rain_time'], entry['thunder_time']]:
            integer(value, 32)
        require(type(entry['raining']) is bool and type(entry['thundering']) is bool and
                type(entry['name_units']) is list and len(entry['name_units']) <= 32767 and
                all(type(unit) is int and 0 <= unit <= 65535 for unit in entry['name_units']),
                'typed weather and exact UTF16 name')
        require(set(exit_value) == {'time', 'size', 'last_millis', 'source_returned', 'entity_list'} and
                exit_value['source_returned'] is True and exit_value['time'] == entry['time'],
                'actual save returned before source Time store185')
        integer(exit_value['size'])
        integer(exit_value['last_millis'])
        entity_list(entry, exit_value)
        selected = selection(entry)
        require(tuple(map(int, entry_receipt)) == (index, entry['time'], entry['next_time'],
                entry['period'], len(selected)) and tuple(map(int, return_receipt)) ==
                (index, entry['time'], len(selected)), 'receipt/live operand agreement')
        before, after = files(folder / 'before'), files(folder / 'after')
        if index == 0:
            initial_continuity(previous, before)
        else:
            require(previous == before, 'no disk writes between actual autosaves')
        require(set(before) <= set(after) and set(after) - set(before) <=
                {'alpha/level.dat_old'} | {f'alpha/region/r.{x // 32}.{z // 32}.mcr'
                    for x, z in selected}, 'only actual metadata rotation/selected regions added')
        require(after['alpha/level.dat_old'] == before['alpha/level.dat'],
                'exact source metadata backup rotation')
        for name in set(before) | set(after):
            if name in {'alpha/level.dat', 'alpha/level.dat_old'} or re.fullmatch(
                    r'alpha/region/r\.-?\d+\.-?\d+\.mcr', name):
                continue
            require(before.get(name) == after.get(name), 'untouched save file ' + name)
        records, growth = region_changes(before, after, selected, parser)
        require(exit_value['size'] == add64(entry['size'], growth),
                'actual Info size growth equals region allocation growth')
        player = metadata(after, entry, exit_value, folder, parser)
        chunks(records, entry, exit_value, folder, parser)
        report.append({'event': index, 'time_saved_before_store': entry['time'],
            'next_time': entry['next_time'], 'period': entry['period'],
            'selected_chunks': len(selected), 'region_growth': growth,
            'player_score': player.get('Score'), 'player_health': player.get('Health'),
            'entity_list_before': entry['entity_list'],
            'entity_list_after': exit_value['entity_list'],
            'actual_entity_slice_calls': entry['entity_list_calls'],
            'observer_unchanged': entry['observer_unchanged'],
            'entry_sha256': hashlib.sha256(bounded(folder / 'entry.json')).hexdigest()})
        previous = after
    require(previous == files(evidence / 'after') == files(evidence / 'after-verified'),
            'no out-of-autosave writes through final frame and cleanup')
    require(any(event['entity_list_before']['phase'] == 0 for event in report),
            'actual cold EntityList observer admission exercised without warmup')
    event, score = map(int, post_death[0])
    require(0 <= event < len(report) and score == 14 and
            report[event]['player_score'] == [3, score],
            'post-removal actual save includes independently decoded Player Score14')
    return report
