"""Eighteen genuine Block tick profiles and optional original deterministic comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib

from test_random_tick import array_hash, digest, tag


SCENARIOS = ('grass_no_decay', 'grass_decay', 'grass_dark_air', 'grass_spread', 'grass_light4',
             'grass_callback_failure', 'leaves_flag0', 'leaves_fresh_absent', 'leaves_log_distance4',
             'leaves_isolated', 'leaves_reused_absent', 'ice8', 'ice9', 'ice_remote9', 'snow11',
             'snow_remote12', 'snow_drop4', 'snow_add_failure')
STREAMS = ('raw', 'nibbles', 'height', 'scratch')


def need(condition, message):
    if not condition:
        raise AssertionError(message)


def inputs(index):
    arrays = [bytearray(length) for length in (32768, 16384, 16384, 16384, 256)]

    def nibble(array, x, y, z, value):
        cell = (x << 11) | (z << 7) | y
        shift = (cell & 1) * 4
        array[cell >> 1] = (array[cell >> 1] & ~(15 << shift)) | (value << shift)

    def cell(x, y, z, block, metadata=0):
        arrays[0][(x << 11) | (z << 7) | y] = block
        nibble(arrays[1], x, y, z, metadata)
        if block:
            arrays[4][(z << 4) | x] = max(arrays[4][(z << 4) | x], y + 1)

    block = 2 if index < 6 else 18 if index < 11 else 79 if index < 14 else 80
    x = 12 if index in (7, 10) else 8
    cell(x, 64, 8, block, 8 if 7 <= index <= 10 else 0)
    if index < 6:
        if index in (0, 1, 4):
            cell(8, 65, 8, 1)
        if index in (3, 5):
            nibble(arrays[2], 8, 65, 8, 15)
            nibble(arrays[2], 7, 65, 8, 15)
            cell(7, 64, 8, 3)
        if index == 4:
            nibble(arrays[2], 8, 65, 8, 4)
    if index == 8:
        for xx in range(9, 12):
            cell(xx, 64, 8, 18, 8)
        cell(12, 64, 8, 17)
    if 11 <= index <= 13:
        nibble(arrays[3], x, 64, 8, 8 if index == 11 else 9)
        cell(x, 63, 8, 1)
    if index >= 14:
        nibble(arrays[3], x, 64, 8, 11 if index == 14 else 12)
        cell(x, 63, 8, 1)
    return arrays


def authored_world(path, index):
    path.mkdir(parents=True)
    arrays = inputs(index)
    names = ('Blocks', 'Data', 'SkyLight', 'BlockLight', 'HeightMap')
    fields = [tag(3, 'xPos', bytes(4)), tag(3, 'zPos', bytes(4)), tag(4, 'LastUpdate', bytes(8))]
    fields += [tag(7, name, struct.pack('>i', len(data)) + data) for name, data in zip(names, arrays)]
    fields += [tag(1, 'TerrainPopulated', b'\1'), tag(9, 'Entities', b'\x0a\0\0\0\0'),
               tag(9, 'TileEntities', b'\x0a\0\0\0\0')]
    data = tag(10, '', tag(10, 'Level', b''.join(fields) + b'\0') + b'\0')
    payload = b'\2' + zlib.compress(data)
    record = struct.pack('>I', len(payload)) + payload
    sectors = (len(record) + 4095) // 4096
    header = bytearray(8192)
    struct.pack_into('>I', header, 0, (2 << 8) | sectors)
    region = path / 'region'
    region.mkdir()
    (region / 'r.0.0.mcr').write_bytes(header + record + bytes(sectors * 4096 - len(record)))


def read_source(path):
    text = path.read_text()
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        data = [json.loads(line) for line in text.splitlines() if line.strip()]
    if isinstance(data, dict):
        data = data.get('block_records', data.get('rows', data.get('original_rows')))
    need(isinstance(data, list), 'original Block records required')
    result = [row for row in data if row.get('event') == 'block']
    need([row['index'] for row in result] == list(range(18)), 'eighteen original Block profiles')
    return result


def native_contracts(rows, states, events, items, hashes):
    need(len(rows) == len(states) == 18, 'all eighteen native leaf results/state records')
    expected_centers = [2, 3, 2, 2, 2, 2, 18, 18, 18, 0, 0, 79, 9, 9, 80, 0, 0, 80]
    for index, row in enumerate(rows):
        cells = row.split()
        block = 2 if index < 6 else 18 if index < 11 else 79 if index < 14 else 80
        need(len(cells) == 15 and int(cells[1]) == index and cells[3] == str(block) and
             cells[4] == SCENARIOS[index] and int(cells[5]) == (-1 if index in (5, 17) else 1),
             'native leaf identity/source result')
        need(int(cells[8]) == expected_centers[index], f'authored source center result{index}: {row}')
        need(int(cells[13]) == (0 if index < 7 else 1), 'persistent actual Leaves scratch presence')
        state = states[index].split()
        need(len(state) == 7 and state[:2] == ['LEAF_STATE', str(index)] and state[4] == '1',
             'same Leaves scratch identity across fresh Worlds')
        need(int(state[2]) == sum(e.startswith('CELL ') for e in events[index]) and
             int(state[3]) == sum(e.startswith('RANGE ') for e in events[index]), 'callback counts')
        need(int(cells[12]) == len(items[index]), 'all actually published Item occurrences')
        for item in items[index]:
            words = item.split()
            need(len(words) == 10 and words[0] == 'DROPPED' and words[1] == str(index) and
                 words[4:7] == ['1', '0', '10'], 'real Item Stack count/damage/pickup delay')
        if index < 7:
            need(hashes[index]['scratch'] == '-', 'unallocated Leaves scratch')
    need(rows[8].split()[14] == '4' and rows[10].split()[14] == rows[9].split()[14],
         'actual BFS distance4 and stale center on later absent area')


def source_event(event, index):
    words = event.split()
    if words[0] == 'ADD':
        need(words[1] == 'hl' and len(words) == 5, 'original Item observer shape')
    else:
        need(words[0] in ('CELL', 'RANGE'), 'unexpected original Block observer kind')
    return ' '.join([words[0], str(index), *words[1:]])


def native_event(event):
    return event


def compare_original(rows, states, events, items, hashes, source):
    event_count = stack_count = add_count = 0
    for index, original in enumerate(source):
        need(rows[index] == original['row'], f'original deterministic leaf row{index} differs: {rows[index]} / {original["row"]}')
        need([native_event(e) for e in events[index]] == [source_event(e, index) for e in original['events']],
             f'original callback order/kind/numeric effects{index}')
        event_count += len(events[index])
        add_count += sum(event.startswith('ADD ') for event in events[index])
        for name in ('raw', 'nibbles', 'height'):
            source_key = 'nibble_hash' if name == 'nibbles' else name + '_hash'
            need(hashes[index][name] == original[source_key], f'original live {name} bytes{index}')
        need(hashes[index]['scratch'] == original['scratch_hash'] and original['scratch_same'],
             f'original persistent scratch contents/identity{index}')
        need(not original['parameter_is_world_random'] and original['ice_opacity'] == int(states[index].split()[5]),
             'distinct supplied Random and actual Ice opacity')
        need(len(items[index]) == len(original['items']), 'original published Item count')
        for native, observed in zip(items[index], original['items']):
            need(native == observed, 'original complete Stack/drop row including deterministic XYZ')
            stack_count += 1
    return {'block_rows': 18, 'post_array_hashes': 54, 'persistent_scratch_hashes': 18,
            'callback_events': event_count, 'published_item_stacks': stack_count,
            'drop_position_words': 3 * stack_count, 'add_callback_position_words': 3 * add_count,
            'scope': 'Direct original Grass/Leaves/Ice/Snow Block bodies on matching declared fresh GAME World/single-Chunk inputs. The same actual Registry owns the Leaves scratch throughout all eighteen profiles. Native callback IO failures pair reached source effects for cases5/17 without equating Java Throwables. Supplied seeded Random and World.r progression are deterministic and distinct. Complete DROPPED rows and ADD callback XYZ words are compared exactly: their positions come from World.r. Item Entity constructor phase, yaw, motion and default-Random entropy remain independent and are outside these transported fields. No source effects or scratch are repaired for comparison.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--observations', type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='world-random-blocks-', dir=args.work_dir.resolve()))
    worlds = out / 'worlds'
    for index in range(18):
        authored_world(worlds / f'case{index}' / 'world0', index)
    executable = args.executable.resolve(strict=True)
    paths = [executable, Path(__file__).resolve(), Path(__file__).with_name('test_random_tick.py').resolve(),
             *sorted(worlds.rglob('*.mcr'))]
    if args.observations:
        paths.append(args.observations.resolve(strict=True))
    before = {str(p): digest(p) for p in paths}
    report = {'status': 'FAIL', 'input_before_sha256': before, 'original_comparison': None}
    try:
        bare = out / 'bare'
        bare.mkdir()
        commands = [[str(executable), 'world-random-blocks', str(bare).encode('utf-8').hex()],
                    [str(executable), 'world-random-blocks-probe', str(worlds).encode('utf-8').hex(), str(out).encode('utf-8').hex()]]
        for label, command in zip(('bare', 'probe'), commands):
            result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=180)
            (out / (label + '.stdout')).write_bytes(result.stdout)
            (out / (label + '.stderr')).write_bytes(result.stderr)
            need(result.returncode == 0 and not result.stderr, f'native {label} failed; inspect {out}')
        report['commands'] = commands
        footer = re.search(rb'PASS: ([1-9][0-9]*) World random Block probe checks \(18 profiles\)\r?\n$', result.stdout)
        need(footer is not None, 'native probe completion receipt')
        lines = (out / 'native.txt').read_text().splitlines()
        rows = [line for line in lines if line.startswith('BLOCK_TICK ')]
        states = [line for line in lines if line.startswith('LEAF_STATE ')]
        events = {i: [] for i in range(18)}
        items = {i: [] for i in range(18)}
        for line in lines:
            if line.startswith(('CELL ', 'RANGE ', 'ADD ')):
                events[int(line.split()[1])].append(line)
            elif line.startswith('DROPPED '):
                items[int(line.split()[1])].append(line)
            else:
                need(line.startswith(('BLOCK_TICK ', 'LEAF_STATE ')), 'complete native transport inventory')
        hashes = {}
        transports = {}
        for index in range(18):
            hashes[index] = {}
            for name in STREAMS:
                path = out / f'leaf{index}-{name}.bin'
                need(path.stat().st_size < 262144, 'bounded post-state transport')
                value = digest(path)
                hashes[index][name] = '-' if name == 'scratch' and path.stat().st_size == 0 else value
                transports[path.name] = {'sha256': value, 'bytes': path.stat().st_size}
        native_contracts(rows, states, events, items, hashes)
        if args.observations:
            report['original_comparison'] = compare_original(rows, states, events, items, hashes, read_source(args.observations))
        report.update(status='PASS', checks=int(footer.group(1)), profiles=18, transports=transports,
                      scope='Mandatory bare native contracts plus optional eighteen transported actual Block tick calls, fresh real GAME World/one loaded Chunk per profile, one persistent Registry and actual Leaves scratch, seeded parameter Random separate from World.r; authored callback failures preserve reached effects.')
        (out / 'actual.json').write_text(json.dumps({'block_rows': rows, 'state_rows': states, 'events': events,
            'items': items, 'hashes': hashes}, indent=2) + '\n', newline='\n')
        print(f'World random Block probes: {report["checks"]} checks; {out / "run.json"}')
    finally:
        report['input_after_sha256'] = {str(p): digest(p) for p in paths}
        unchanged = before == report['input_after_sha256']
        if not unchanged:
            report['status'] = 'FAIL'
        (out / 'run.json').write_text(json.dumps(report, indent=2) + '\n', newline='\n')
        need(unchanged, 'authored Block probe inputs changed')


if __name__ == '__main__':
    main()
