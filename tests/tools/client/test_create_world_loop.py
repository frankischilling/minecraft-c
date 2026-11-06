"""Actual SDK fresh World creation, ordinary save/title and normal reselection.

Only imported resources and viewDistance:3 are supplied. The isolated saves
directory starts empty; no metadata, terrain, statistics, pose, clock or Random
input is authored. Static original pins and native model controls are separate
evidence from this native graphical scenario.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
NATIVE_RECEIPT = ROOT / 'tests/reference/client/create_world/native-validation.json'
FOLDER = 'SDK Fresh_777'
DISPLAY = ' SDK Fresh:777 '


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= 256 * 1048576,
            'Bounded unchanged source/product input: ' + str(path))
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            value.update(block)
    return value.hexdigest()


def inventory(directory):
    result, total = {}, 0
    for path in sorted(directory.rglob('*')):
        require(not path.is_symlink(), 'No symlink in isolated input/product inventory')
        if not path.is_file():
            continue
        size = path.stat().st_size
        total += size
        require(len(result) < 8192 and size <= 32 * 1048576 and total <= 256 * 1048576,
                'Bounded complete isolated input/product inventory')
        result[path.relative_to(directory).as_posix()] = {'bytes': size, 'sha256': digest(path)}
    return result


def receipts(mining, stdout, menu_only):
    require(mining.rows(stdout, 'CREATE_WORLD_OPTIONS') == [
        {'view-distance': '3', 'lines': '1', 'bytes': '15', 'source-loaded': '1'}],
        'Actual GameSettings source read of the sole pre-bootstrap Tiny options line')
    menu = mining.rows(stdout, 'CREATE_WORLD_MENU')
    finish = mining.rows(stdout, 'CREATE_WORLD_FINISH')
    require(len(menu) == len(finish) == 1, 'Complete fresh World menu/cleanup receipts')
    exact = {'labels': '5', 'fields': '2', 'rectangles': '4', 'buttons': '2',
             'escape-noop': '1', 'cancel-exact-parent': '1', 'repeat-closed': '1',
             'reopened': '1', 'typed-name': '1', 'sanitized-folder': '1',
             'tab-sdk': '1', 'typed-seed': '777'}
    require(set(menu[0]) == set(exact) | {'draws'} and
            all(menu[0].get(key) == value for key, value in exact.items()) and
            int(menu[0]['draws']) >= 3, 'Actual FJ/RO draw and SDK input observations')
    require(finish[0].get('mode') == ('menu-only' if menu_only else 'full') and
            finish[0].get('cleanup') == finish[0].get('windows-absent') == '1' and
            finish[0].get('result') == '0' and int(finish[0]['checks']) > 0,
            'Checked native cleanup and both original host windows absent')
    created = mining.rows(stdout, 'CREATE_WORLD_CREATED')
    saved = mining.rows(stdout, 'CREATE_WORLD_SAVE')
    reloaded = mining.rows(stdout, 'CREATE_WORLD_RELOAD')
    if menu_only:
        require(not created and not saved and not reloaded,
                'Menu-only diagnostic cannot stand in for fresh World acceptance')
        return {'menu': menu[0], 'finish': finish[0], 'full_creation': False}
    require(len(created) == len(saved) == len(reloaded) == 1,
            'Fresh create/save/reselection lifecycle receipts')
    require(created[0].get('seed') == '777' and created[0].get('enter-sdk') ==
            created[0].get('normal-loader') == created[0].get('world-returned') ==
            created[0].get('hud-returned') == created[0].get('initial-save') == '1' and
            int(created[0]['frames']) >= 3,
            'Actual Enter-created seed777 World and returned HUD frames')
    require(int(saved[0]['samples']) > 0 and saved[0].get('paused') ==
            saved[0].get('owned-player') == saved[0].get('observed-info') ==
            saved[0].get('selected-before-leave-frame') == saved[0].get('ordinary-title') == '1' and
            int(saved[0]['files']) > 0 and int(saved[0]['bytes']) > 0 and
            int(saved[0]['first']) <= int(saved[0]['last']),
            'Selected live paused-entry values before actual save/title frame')
    require(reloaded[0].get('epochs') == '2' and int(reloaded[0]['frames']) >= 3 and
            reloaded[0].get('distinct-owners') == reloaded[0].get('ordinary-selection') == '1' and
            reloaded[0].get('initial-save') == '0',
            'Second genuine World/Player/handler/controller/load epoch')
    return {'menu': menu[0], 'created': created[0], 'saved': saved[0],
            'reloaded': reloaded[0], 'finish': finish[0], 'full_creation': True}


def validate_info(mining, data, observed, first, last):
    expected = {'RandomSeed': [4, 777], 'LevelName': [8, DISPLAY], 'version': [3, 19132]}
    for key, value in expected.items():
        require(data.get(key) == value and type(data[key][1]) is type(value[1]),
                'Actual saved fresh metadata identity: ' + key)
    mining.same_fields(data, observed, ('RandomSeed', 'LevelName', 'SpawnX', 'SpawnY', 'SpawnZ',
                       'Time', 'SizeOnDisk', 'rainTime', 'thunderTime', 'raining', 'thundering'),
                       'Disk metadata versus selected actual live WorldInfo')
    require(data.get('LastPlayed', [None])[0] == 4 and first <= data['LastPlayed'][1] <= last,
            'Actual metadata writer LastPlayed falls inside observed save call')
    require(data.get('Player', [None])[0] == 10, 'Actual source Player Compound')
    require(set(data) == {'RandomSeed', 'SpawnX', 'SpawnY', 'SpawnZ', 'Time', 'SizeOnDisk',
                        'LastPlayed', 'LevelName', 'version', 'rainTime', 'raining',
                        'thunderTime', 'thundering', 'Player'},
            'Complete freshly serialized metadata field inventory')


def decode_chunks(parser, directory):
    regions = sorted((directory / 'region').glob('r.*.*.mcr'))
    require(0 < len(regions) <= 64, 'Actual natural World has bounded saved regions')
    chunks, nonair, total = {}, 0, 0
    for path in regions:
        match = re.fullmatch(r'r\.(-?\d+)\.(-?\d+)\.mcr', path.name)
        require(match is not None and not path.is_symlink() and
                8192 <= path.stat().st_size <= 32 * 1048576,
                'Actual source region filename/extent')
        raw = path.read_bytes()
        total += len(raw)
        require(len(raw) % 4096 == 0 and total <= 256 * 1048576,
                'Complete bounded saved region sector images')
        rx, rz = map(int, match.groups())
        used = {0, 1}
        for slot in range(1024):
            location, = struct.unpack_from('>I', raw, slot * 4)
            if location == 0:
                continue
            sector, count = location >> 8, location & 255
            require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(raw) and
                    not used.intersection(range(sector, sector + count)),
                    'Actual allocated disjoint Chunk sectors')
            used.update(range(sector, sector + count))
            offset = sector * 4096
            size, = struct.unpack_from('>I', raw, offset)
            require(1 < size <= count * 4096 - 4 and raw[offset + 4] == 2,
                    'Actual selector2 compressed Chunk extent')
            tree = parser.inflate(raw[offset + 5:offset + 4 + size], 15)
            require(set(tree) == {'Level'} and tree['Level'][0] == 10,
                    'Actual freshly serialized Chunk root')
            level = tree['Level'][1]
            coordinate = (rx * 32 + slot % 32, rz * 32 + slot // 32)
            require(level.get('xPos') == [3, coordinate[0]] and
                    level.get('zPos') == [3, coordinate[1]] and coordinate not in chunks,
                    'Saved actual Chunk coordinate and physical slot agree')
            for key, length in (('Blocks', 32768), ('Data', 16384), ('SkyLight', 16384),
                                ('BlockLight', 16384), ('HeightMap', 256)):
                require(level.get(key, [None])[0] == 7 and len(level[key][1]) == length,
                        'Typed actual natural Chunk array: ' + key)
            for key in ('Entities', 'TileEntities'):
                require(level.get(key, [None])[0] == 9,
                        'Typed actual natural Chunk list: ' + key)
                element, values = level[key][1]
                require(element == (10 if values else 1), 'Exact source serialized List type')
            require(level.get('LastUpdate', [None])[0] == 4 and
                    level.get('TerrainPopulated', [None])[0] == 1,
                    'Typed natural Chunk time/population metadata')
            for entity in level['Entities'][1][1]:
                require(entity.get('id', [None])[0] == 8 and entity['id'][1] != 'Player',
                        'Source Player appears in metadata, never saved Chunk Entity list')
            nonair += sum(value != 0 for value in level['Blocks'][1])
            chunks[coordinate] = hashlib.sha256(raw[offset + 5:offset + 4 + size]).hexdigest()
            require(len(chunks) <= 4096, 'Bounded complete natural saved Chunk inventory')
    require(chunks and nonair > 0, 'Generated natural terrain persisted without authored terrain input')
    return {'regions': len(regions), 'chunks': len(chunks), 'nonair_blocks': nonair,
            'compressed_chunk_sha256': {f'{x},{z}': value for (x, z), value in sorted(chunks.items())}}


def semantics(mining, parser, evidence, actual):
    saved = evidence / 'snapshots/saved' / FOLDER
    metadata = parser.inflate(parser.read(saved / 'level.dat'), 31)
    require(set(metadata) == {'Data'} and metadata['Data'][0] == 10,
            'Actual source-created fresh metadata root')
    data = metadata['Data'][1]
    entry = parser.Nbt(parser.read(evidence / 'info-entry.nbt')).root()
    validate_info(mining, data, entry, int(actual['saved']['first']), int(actual['saved']['last']))
    player = parser.Nbt(parser.read(evidence / 'player-entry.nbt')).root()
    require(mining.typed([10, data['Player'][1]]) == mining.typed([10, player]),
            'Every typed saved Player field equals the selected live paused-entry serialization')
    reopened = parser.Nbt(parser.read(evidence / 'player-reloaded.nbt')).root()
    mining.same_fields(data['Player'][1], reopened, ('Inventory', 'Score', 'Dimension', 'Health'),
                       'Actual reselected Player stable fields')
    reloaded_info = parser.Nbt(parser.read(evidence / 'info-reloaded.nbt')).root()
    mining.same_fields(data, reloaded_info, ('RandomSeed', 'LevelName', 'SpawnX', 'SpawnY', 'SpawnZ',
                       'version'), 'Actual reselected WorldInfo stable fields')
    # These negative controls mutate decoded copies only. They cannot write a
    # save, feed the process, or turn menu-only evidence into creation success.
    controls = 0
    for key, replacement in [('RandomSeed', [4, 778]), ('LevelName', [8, 'wrong']),
                             ('Time', [3, entry['Time'][1]])]:
        changed = copy.deepcopy(data)
        changed[key] = replacement
        try:
            validate_info(mining, changed, entry, int(actual['saved']['first']),
                          int(actual['saved']['last']))
        except RuntimeError:
            controls += 1
        else:
            raise RuntimeError('Independent metadata corruption escaped: ' + key)
    validate_info(mining, data, entry, int(actual['saved']['first']), int(actual['saved']['last']))
    return {'seed': 777, 'folder': FOLDER, 'display': DISPLAY,
            'level_sha256': digest(saved / 'level.dat'), 'negative_controls': controls,
            'player_fields': sorted(player), 'chunks': decode_chunks(parser, saved)}


def main():
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument('--executable', type=Path, required=True)
    arguments.add_argument('--assets', type=Path, required=True)
    arguments.add_argument('--artifacts', type=Path, required=True)
    arguments.add_argument('--menu-only', action='store_true',
                           help='Diagnostic menu phase only; never full creation acceptance')
    args = arguments.parse_args()
    require(os.name == 'nt', 'Disposable Linux account/mount qualification required before live World inputs')
    mining = module('fresh_world_mining_support', ROOT / 'tests/tools/client/test_mining_loop.py')
    parser = module('fresh_world_independent_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='fresh-world-', dir=args.artifacts.resolve()))
    home = evidence / 'appdata'
    game = home / '.minecraft'
    saves = game / 'saves'
    saves.mkdir(parents=True)
    (evidence / 'snapshots').mkdir()
    (game / 'options.txt').write_bytes(b'viewDistance:3\n')
    inputs = [Path(__file__).resolve(), executable,
              ROOT / 'tests/client/gameplay/test_create_world.c',
              ROOT / 'tests/client/gameplay/test_mining.c',
              ROOT / 'tests/client/startup/test_world_selection_loop.c',
              ROOT / 'tests/client/startup/test_world_reload.inc',
              ROOT / 'tests/tools/client/test_mining_loop.py',
              ROOT / 'tests/tools/world/test_source_saving.py', ROOT / 'tests/support/java_random.h']
    # Published native receipts are outputs, not source/execution inputs.
    inputs += sorted(path for folder in ('src', 'include', 'tests/reference/client/create_world')
                     for path in (ROOT / folder).rglob('*') if path.is_file() and path != NATIVE_RECEIPT and
                     path.suffix in ('.c', '.h', '.inc', '.py', '.json', '.md'))
    before_inputs = {str(path): digest(path) for path in inputs}
    before_assets, before_game = inventory(assets), inventory(game)
    require(inventory(saves) == {} and before_game == {'options.txt': {
        'bytes': 15, 'sha256': hashlib.sha256(b'viewDistance:3\n').hexdigest()}},
        'Isolated fresh startup has only declared Tiny options, no save/statistics/terrain input')
    report = {'status': 'FAIL', 'scope': __doc__, 'evidence': str(evidence),
              'mode': 'menu-only' if args.menu_only else 'full', 'inputs_before': before_inputs,
              'assets_before': before_assets, 'game_before': before_game,
              'authored_world_inputs': False, 'original_game_class_execution': False,
              'native_sdk_execution': True}
    environment = os.environ.copy()
    environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                       ALSOFT_DRIVERS='null', MC_FRESH_WORLD_SAVES=str(saves))
    command = [str(executable), str(assets), str(evidence)]
    if args.menu_only:
        command.append('--menu-only')
    report['command'] = command
    report['process_first_millis'] = time.time_ns() // 1000000
    try:
        try:
            result = subprocess.run(command, env=environment, capture_output=True,
                                    timeout=1200, check=False)
        except BaseException as error:
            for name, value in [('stdout.txt', getattr(error, 'stdout', None)),
                                ('stderr.txt', getattr(error, 'stderr', None))]:
                (evidence / name).write_bytes(value or b'')
            report['execution'] = {'event': 'timeout' if isinstance(error, subprocess.TimeoutExpired)
                                   else 'launch-error', 'error': str(error)}
            raise
        (evidence / 'stdout.txt').write_bytes(result.stdout)
        (evidence / 'stderr.txt').write_bytes(result.stderr)
        report['execution'] = {'event': 'result', 'returncode': result.returncode}
        stdout, stderr = result.stdout.decode('utf-8'), result.stderr.decode('utf-8')
        print(stdout, end='')
        print(stderr, end='', file=sys.stderr)
        require(result.returncode == 0, 'Actual fresh World native scenario failed; source diagnostics retained')
        report['actual'] = receipts(mining, stdout, args.menu_only)
        if args.menu_only:
            require(inventory(saves) == {}, 'Cancelled diagnostic menu created no World files')
            report['status'] = 'PASS_MENU_ONLY'
        else:
            report['semantics'] = semantics(mining, parser, evidence, report['actual'])
            require({path.name for path in saves.iterdir()} == {FOLDER},
                    'One genuinely created World folder after normal reselection')
            saved, final = inventory(evidence / 'snapshots/saved'), inventory(saves)
            require((saves / FOLDER / 'session.lock').stat().st_size == 8,
                    'Reselection retains an actual eight-byte session token')
            report['saved_files'] = saved
            report['reselection_changed_files'] = [name for name in sorted(set(saved) | set(final))
                                                   if saved.get(name) != final.get(name)]
            report['final_chunks'] = decode_chunks(parser, saves / FOLDER)
            report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'error': str(error)}
        raise
    finally:
        report['process_last_millis'] = time.time_ns() // 1000000
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['assets_after'] = inventory(assets)
        report['game_after'] = inventory(game)
        if report['inputs_after'] != before_inputs or report['assets_after'] != before_assets:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Source/product/imported resources changed'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == ('PASS_MENU_ONLY' if args.menu_only else 'PASS'),
            'Fresh World scenario input continuity failed')
    print('Actual SDK fresh World ' + report['status'] + '; ' + str(evidence))


if __name__ == '__main__':
    main()
