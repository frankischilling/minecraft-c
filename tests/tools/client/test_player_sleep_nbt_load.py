"""Prepared saved Sleeping Player through actual SDK Load and retained Respawn.

Authored native caller inputs only; no natural night or original-frame claim.
"""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def require(condition, message):
    if not condition:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, 'actual helper available')
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compound(world, fields):
    names = [name for _, name, _ in fields]
    require(len(names) == len(set(names)), 'authored compound has no duplicate names')
    return b''.join(world.tag(kind, name, value) for kind, name, value in fields) + b'\x00'


def player_fields(score_short):
    return [(9, 'Pos', b'\x06' + struct.pack('>i3d', 3, 3.5, 64.9375, 4.5)),
            (9, 'Motion', b'\x06' + struct.pack('>i3d', 3, 0., 0., 0.)),
            (9, 'Rotation', b'\x05' + struct.pack('>i2f', 2, 0., 0.)),
            (5, 'FallDistance', struct.pack('>f', 0.)),
            (2, 'Fire', struct.pack('>h', -20)), (2, 'Air', struct.pack('>h', 300)),
            (1, 'OnGround', b'\x01'), (2, 'Health', struct.pack('>h', 20)),
            (2, 'HurtTime', struct.pack('>h', 0)), (2, 'DeathTime', struct.pack('>h', 0)),
            (2, 'AttackTime', struct.pack('>h', 0)),
            (9, 'Inventory', b'\x0a' + struct.pack('>i', 0)),
            (3, 'Dimension', struct.pack('>i', 0)), (1, 'Sleeping', b'\x01'),
            (2, 'SleepTimer', struct.pack('>h', 17)),
            (3, 'SpawnX', struct.pack('>i', 3)), (3, 'SpawnY', struct.pack('>i', 64)),
            (3, 'SpawnZ', struct.pack('>i', 4)),
            (2 if score_short else 3, 'Score', struct.pack('>h' if score_short else '>i', 7))]


def sleeping_metadata(world, display, played, score_short):
    # A complete small input factory; no slicing/searching an encoded compound.
    data = [(8, 'LevelName', world.string(display)),
            (4, 'LastPlayed', struct.pack('>q', played)),
            (4, 'SizeOnDisk', struct.pack('>q', 3145728)),
            (3, 'version', struct.pack('>i', 19132)),
            (4, 'RandomSeed', struct.pack('>q', 123)), (4, 'Time', struct.pack('>q', 0)),
            (3, 'SpawnX', struct.pack('>i', 0)), (3, 'SpawnY', struct.pack('>i', 64)),
            (3, 'SpawnZ', struct.pack('>i', 0)),
            (10, 'Player', compound(world, player_fields(score_short)))]
    return b'\x0a\x00\x00' + compound(world, [(10, 'Data', compound(world, data))])


def validate_player(player, score_short):
    require(set(player) == {name for _, name, _ in player_fields(score_short)},
            'independent complete Player field inventory')
    wanted = {'Dimension': [3, 0], 'Sleeping': [1, 1], 'SleepTimer': [2, 17],
              'Health': [2, 20], 'Inventory': [9, [10, []]],
              'SpawnX': [3, 3], 'SpawnY': [3, 64], 'SpawnZ': [3, 4],
              'Score': [2 if score_short else 3, 7], 'OnGround': [1, 1]}
    for name, value in wanted.items():
        require(player[name] == value, 'independent typed authored Player ' + name)
    words = lambda values: [int.from_bytes(struct.pack('>d', n), 'big') for n in values]
    require(player['Pos'] == [9, [6, words((3.5, 64.9375, 4.5))]] and
            player['Motion'] == [9, [6, words((0., 0., 0.))]] and
            player['Rotation'] == [9, [5, [0, 0]]],
            'actual saved sleeping pose floors to head(3,64,4)')


def occupied_bed(world, bed, parser, directory):
    original, authored = bed.author_bed(world, directory, 'valid')
    raw = bytearray(bed.record_raw(authored[0, 0]))
    offset = bed.array_offset(world, raw, 'Data', 16384)
    index = (3 << 11) | (4 << 7) | 64
    require(raw[offset + index // 2] & 15 == 8, 'declared head initially metadata8')
    raw[offset + index // 2] = (raw[offset + index // 2] & 240) | 12
    compressed = zlib.compress(raw)
    body = struct.pack('>I', len(compressed) + 1) + b'\x02' + compressed
    sectors = (len(body) + 4095) // 4096
    path = directory / 'r.0.0.mcr'
    region = bytearray(path.read_bytes())
    require(len(region) % 4096 == 0, 'ordinary region input extent')
    struct.pack_into('>I', region, 0, ((len(region) // 4096) << 8) | sectors)
    region.extend(body + bytes(sectors * 4096 - len(body)))
    path.write_bytes(region)
    records = bed.region_records(directory)
    require(records.keys() == original.keys() and len(records) == 289 and
            all(records[key] == original[key] for key in records if key != (0, 0)) and
            records[0, 0]['timestamp'] == original[0, 0]['timestamp'],
            'all289 inputs and other288 records/timestamps preserved')
    level = parser.Nbt(bed.record_raw(records[0, 0])).root()['Level'][1]
    original_level = parser.Nbt(bed.record_raw(original[0, 0])).root()['Level'][1]
    expected_blocks = bytearray(original_level['Blocks'][1])
    expected_metadata = bytearray(original_level['Data'][1])
    require(level['xPos'] == [3, 0] and level['zPos'] == [3, 0] and
            level['TerrainPopulated'] == [1, 1], 'independently decoded normal central Chunk')
    for x, z, metadata in ((3, 3, 0), (3, 4, 12)):
        at = (x << 11) | (z << 7) | 64
        expected_blocks[at] = 26
        expected_metadata[at // 2] = ((expected_metadata[at // 2] &
                                      (15 if at & 1 else 240)) |
                                     (metadata << (4 * (at & 1))))
        require(level['Blocks'][1][at] == 26 and level['Blocks'][1][at - 1] == 2 and
                ((level['Data'][1][at // 2] >> (4 * (at & 1))) & 15) == metadata,
                'actual saved Bed half/support/occupied input')
    require(level['Blocks'][1] == bytes(expected_blocks) and
            level['Data'][1] == bytes(expected_metadata) and level['Blocks'][1][127] == 57 and
            all(level[name] == original_level[name] for name in original_level
                if name not in ('Blocks', 'Data')), 'only declared two Bed cells; stored marker57 retained')
    return {'chunks': 289, 'head': [3, 64, 4], 'foot': [3, 64, 3],
            'head_metadata': 12, 'foot_metadata': 0,
            'region_sha256': {p.name: digest(p) for p in sorted(directory.iterdir())}}


def source_inputs(executable):
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format'}
    for folder in ('src', 'include', 'cmake', 'tests/client/startup', 'tests/client/world',
                   'tests/support', 'tests/tools/client', 'tests/tools/world',
                   'tests/reference/client/player_statistics', 'tests/reference/client/controller'):
        paths.update(p for p in (ROOT / folder).rglob('*') if p.is_file() and
                     p.suffix in ('.c', '.h', '.inc', '.cmake', '.py', '.json', '.txt'))
    paths.update(executable.parent.glob('*.dll'))
    return {str(path): digest(path) for path in sorted(paths)}


def semantic_player(parser, path, wrapped=False):
    value = parser.inflate(parser.read(path), 31)['Data'][1]['Player'] if wrapped else None
    if wrapped:
        require(value[0] == 10, 'actual saved Player compound')
        return value[1]
    return parser.Nbt(parser.read(path)).root()


def check_awake(player, *, empty_type):
    for name, value in {'Sleeping': [1, 0], 'SleepTimer': [2, 0], 'Score': [3, 7],
                        'Health': [2, 20], 'Dimension': [3, 0],
                        'SpawnX': [3, 3], 'SpawnY': [3, 64], 'SpawnZ': [3, 4]}.items():
        require(player.get(name) == value, 'actual source save/reselection ' + name)
    require(player.get('Inventory') == [9, [empty_type, []]],
            'actual empty Inventory writer type, not nonempty normalization')


def receipts(stdout, profile):
    selected = [line for line in stdout.splitlines() if line.startswith('SLEEP_NBT_')]
    if profile == 'success':
        expected = [f'SLEEP_NBT_LOAD epoch={epoch} actual-load=1 awake=1 timer=0 reconstructed-v={int(epoch == 1)} '
                    'Q=3,64,4 Q-distinct-v=1 score=7 occupied=0 info-cleared=1 published=1 exact-wake-pose=0'
                    for epoch in (1, 2)]
        expected.append('SLEEP_NBT_SAVE_RESELECT source-save-title=1 sdk-alpha-again=1 epochs=2 '
                        'second-input-awake=1 info-cleared-twice=1 score7=1 original-restart-claim=0')
        expected.append('SLEEP_NBT_RESPAWN actual-parent=303 fd=14 gs=93 wake=80 native-io=1 retained=1 '
                        'cleanup-refused=1 load-lease-busy=1 second-admission-busy=1 repair-retired=1 '
                        'callbacks-unreplayed=1 input321-unreached=1')
    else:
        expected = ['SLEEP_NBT_LOAD_FAILURE actual-load324=1 fd88=1 gs93-returned=1 score-short=1 '
                    'dc9-cast=1 awake=1 spawn-read=1 info-not-cleared=1 publication-unreached=1 '
                    'entry-registry-refused=1 same-owner-retried=1 work-consumed=1 '
                    'source-unreplayed=1 coordinator-refused=1 explicit-legacy-cleanup=1']
    require(selected == expected, 'exact actual parent/cleanup receipts and order')
    footer = [line for line in stdout.splitlines() if line.startswith('WORLD_SELECTION mode=')]
    require(len(footer) == (2 if profile == 'success' else 1) and
            re.fullmatch(r'WORLD_SELECTION mode=repeat cleanup=1 checks=[1-9][0-9]*',
                         footer[-1]) is not None, 'actual checked final SDK cleanup')
    if profile == 'success':
        require(re.fullmatch(r'WORLD_SELECTION mode=repeat frames=[1-9][0-9]* ticks=[0-9]+ '
                             r'mouse=[0-9]+ keyboard=[0-9]+ held-entry=1 row=1 '
                             r'cancel-same-parent=1 select-frontier=1', footer[0]) is not None,
                'actual returned SDK frames footer before final cleanup')
    return selected


def source_stderr(stderr, profile):
    if profile == 'success':
        require(stderr == '', 'successful SDK case has no source failure output')
        return
    lines = stderr.splitlines()
    require(len(lines) == 2 and
            lines[0] == 'Client menu: Player NBT named tag cast (fd.a:88)' and
            re.fullmatch(
                r'WORLD_SAVE_TITLE_INPUT_FAILURE stage=initial-load-frame-failed fd\.a:88 '
                r'operation=Player NBT named tag cast failed=1 render-returned=0 screen=1 '
                r'source-focus=0 display-query=0 display-active=[01] input-read=1 grabbed=0 '
                r'frames=[1-9][0-9]* ticks=[0-9]+ mouse-events=[0-9]+ keyboard-events=[0-9]+',
                lines[1]) is not None,
            'exact expected caught Score failure output; no unknown SDK stderr')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--profile', choices=('success', 'load-failure'), required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'Windows APPDATA/native SDK scope')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    world = module('sleep_nbt_world_fixture', HERE / 'test_world_selection_loop.py')
    bed = module('sleep_nbt_bed_fixture', HERE / 'test_bed_respawn.py')
    nbt = module('sleep_nbt_independent_decoder', HERE.parent / 'world/test_source_saving.py')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='sleep-nbt-' + args.profile + '-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'
    game, shots, entries = appdata / '.minecraft', evidence / 'native-snapshots', evidence / 'entries'
    world.repeat_statistics(game, assets)
    options = b'viewDistance:3\n'
    (game / 'options.txt').write_bytes(options)
    (evidence / 'options-input.txt').write_bytes(options)
    saves = game / 'saves'
    saves.mkdir()
    shots.mkdir(); entries.mkdir()
    (saves / 'preserve-me.bin').write_bytes(b'unrelated save-directory file\x00\xff')
    for folder, title, played in (('older', 'Old world', 1303000000000),
                                 ('bravo', '', 1304000000000), ('alpha', 'Alpha world', 1304000000000)):
        directory = saves / folder
        directory.mkdir()
        raw = sleeping_metadata(world, title, played, args.profile == 'load-failure') if folder == 'alpha' \
            else world.metadata(title, played, 19132)
        decoded = nbt.Nbt(raw).root()
        require(set(decoded) == {'Data'} and decoded['Data'][0] == 10, 'authored Level root')
        if folder == 'alpha':
            validate_player(decoded['Data'][1]['Player'][1], args.profile == 'load-failure')
        else:
            require('Player' not in decoded['Data'][1], 'common metadata has NO Player compound')
        (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(raw, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored untouched chunk')
    region = saves / 'alpha/region'
    region.mkdir()
    input_report = occupied_bed(world, bed, nbt, region)
    payload = evidence / 'player-input.nbt'
    payload.write_bytes(b'\x0a\x00\x00' + compound(world, player_fields(False)))
    validate_player(nbt.Nbt(payload.read_bytes()).root(), False)
    before_sources, before_assets = source_inputs(executable), world.snapshot(assets)
    before_files = world.save_title_files(saves, evidence / 'before')
    payload_pin = digest(payload)
    environment = os.environ.copy()
    environment.update(APPDATA=str(appdata), MC_PLAYER_SLEEP_NBT_PROFILE=args.profile,
        MC_PLAYER_SLEEP_NBT_PAYLOAD=str(payload), MC_WORLD_SAVE_TITLE_SAVES=str(saves),
        MC_WORLD_RELOAD_SAVES=str(saves), MC_WORLD_SAVE_TITLE_ENTRIES=str(entries),
        MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(shots))
    screenshot = evidence / 'last-native-frame.png'
    report = {'status': 'FAIL', 'profile': args.profile, 'inputs': input_report,
              'scope': 'Prepared saved Sleeping input; actual SDK Load/save/reselection; direct real Respawn '
                       'native IO/metadata controls; no natural night, ordinary death button, original frame or FPS claim',
              'sources_before': before_sources}
    try:
        first = time.time_ns() // 1000000
        result = world.retained_native_run([str(executable), str(assets), 'repeat', str(screenshot)],
            environment, evidence, saves, before_files, 'sleep-nbt-' + args.profile, first, 240)
        print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
        require(result.returncode == 0, 'actual native SDK case failed; retain first focus/source failure')
        source_stderr(result.stderr, args.profile)
        report['receipts'] = receipts(result.stdout, args.profile)
        if args.profile == 'success':
            report['save'] = world.save_title_decode(shots / 'before-1', entries / 'epoch-1',
                shots / 'after-1', 'save-title', result.stdout, first,
                options_input=evidence / 'options-input.txt')
            check_awake(semantic_player(nbt, shots / 'after-1/alpha/level.dat', True), empty_type=1)
            # The generic semantic-observation encoder canonicalizes empty List to Byte1.
            check_awake(semantic_player(nbt, shots / 'loaded-player-1.nbt'), empty_type=1)
            check_awake(semantic_player(nbt, shots / 'loaded-player-2.nbt'), empty_type=1)
            before_png = Path(str(screenshot) + '-before-respawn.png')
            require(before_png.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), 'actual returned epoch2 capture before direct Respawn')
            report['returned_frame_sha256'] = digest(before_png)
        else:
            require((saves / 'alpha/level.dat').read_bytes() ==
                    (evidence / 'before/alpha/level.dat').read_bytes(),
                    'failed initial Load does not rewrite prepared Sleeping/ScoreSHORT metadata')
        after = {name.replace('\\', '/'): value for name, value in world.snapshot(saves).items()}
        for name, value in before_files.items():
            if name.startswith(('older/', 'bravo/')) or name == 'preserve-me.bin' or name.endswith('/preserve-chunk.dat'):
                require(after.get(name) == value['sha256'], 'protected unrelated save unchanged: ' + name)
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['sources_after'] = source_inputs(executable)
        report['resources_unchanged'] = world.snapshot(assets) == before_assets
        report['payload_unchanged'] = digest(payload) == payload_pin
        if report['sources_after'] != before_sources or not report['resources_unchanged'] or not report['payload_unchanged']:
            report['status'] = 'FAIL'
        (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    require(report['status'] == 'PASS', 'native SDK source/resource/payload inputs changed')
    print('Player Sleeping NBT SDK: PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
