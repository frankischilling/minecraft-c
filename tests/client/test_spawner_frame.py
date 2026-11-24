"""Loaded MobSpawner through real Windows World frames on physical monitor 2."""
import argparse
import gzip
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/tools/client'))
import test_chest_loop as G

require = G.require


def regions(world, directory):
    """Author one saved Spawner; no native runtime or production save is edited."""
    coordinates = [None, None]

    class InputTags:
        @staticmethod
        def tag(kind, name, payload):
            if name in ('xPos', 'zPos'):
                coordinates[name == 'zPos'], = struct.unpack('>i', payload)
            if coordinates == [0, 0]:
                if name == 'Blocks':
                    blocks = bytearray(payload)
                    blocks[4 + (2 << 7) + 64] = 52
                    payload = bytes(blocks)
                elif name == 'TileEntities':
                    tile = world.tag(8, 'id', world.string('MobSpawner'))
                    tile += b''.join(world.tag(3, axis, struct.pack('>i', value))
                                     for axis, value in zip(('x', 'y', 'z'), (0, 64, 2)))
                    tile += world.tag(8, 'EntityId', world.string('Pig'))
                    tile += world.tag(2, 'Delay', struct.pack('>h', 32767))
                    payload = b'\x0a' + struct.pack('>i', 1) + tile + b'\0'
            return world.tag(kind, name, payload)

    G.regions(InputTags, directory, 'placement')


def receipts(stdout, stderr, monitor):
    require(type(stdout) is str and stderr == '', 'Clean successful native transport')
    lines = stdout.splitlines()
    exact = [f'SPAWNER_FRAME epoch={epoch} frames=8 tiles=1 n-pc=569 full-world=1 hud=1 saved-input=1 '
             'source-field-repair=0' for epoch in range(2)]
    require([line for line in lines if line.startswith('SPAWNER_FRAME')] == exact,
            'Unique complete returned Spawner frame receipt')
    cache = 'SPAWNER_CACHE epochs=2 preview-same=1 world-rebound=1 factory-calls=1 ordinary-save-reload=1'
    require([line for line in lines if line.startswith('SPAWNER_CACHE')] == [cache],
            'One cached preview rebound to the reselected World')
    draws = []
    for line in lines:
        if not line.startswith('SPAWNER_DRAW'):
            continue
        match = re.fullmatch(r'SPAWNER_DRAW epoch=([01]) frame=([0-7]) cache-hit=([01]) cache-miss=([01]) '
                             r'factory-calls=([01]) models=([1-9][0-9]{0,6}) compiled=([0-9]{1,7}) '
                             r'faces=([0-9]{1,7}) vertices=([0-9]{1,7}) preview-pig=1 world-same=1 th-returned=1 '
                             r'th-retired=1 ll-retired=1 world-list-absent=1', line)
        require(match is not None, 'Complete actual LL/AG/TH draw receipt')
        row = list(map(int, match.groups()))
        require(row[2] + row[3] == 1 and row[4] == row[3], 'Actual hit or one reached factory call')
        draws.append(row)
    require([row[:2] for row in draws] == [[epoch, frame] for epoch in range(2) for frame in range(8)] and
            sum(row[3] for row in draws) == 1 and draws[0][3] == 1 and
            all(row[2] == 1 for row in draws[1:]) and
            all(sum(row[index] for row in draws) > 0 for index in (6, 7, 8)),
            'First factory/compile followed by fifteen cached draws across reselection')
    finish = [line for line in lines if line.startswith('SPAWNER_FINISH')]
    require(len(finish) == 1 and re.fullmatch(
        r'SPAWNER_FINISH cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9})', finish[0]) and
        int(finish[0].split('checks=')[1]) <= 4294967295 and lines[-1] == finish[0],
        'Terminal checked cleanup and absent native windows')
    bounds = monitor['selected']['work']
    require(len(bounds) == 4 and all(type(value) is int for value in bounds) and
            bounds[2] > 0 and bounds[3] > 0, 'Physical monitor work rectangle')
    observations = []
    for line in lines:
        if not line.startswith('WORLD_SELECTION_MONITOR'):
            continue
        match = re.fullmatch(r'WORLD_SELECTION_MONITOR stage=(open|after-spawner) '
                             r'window=(frame|canvas) x=(-?[0-9]+) y=(-?[0-9]+) '
                             r'width=([1-9][0-9]*) height=([1-9][0-9]*) target=(\S+)', line)
        require(match is not None and match[7] == ','.join(map(str, bounds)),
                'Exact native monitor witness')
        x, y, width, height = map(int, match.group(3, 4, 5, 6))
        require((width, height) == (854, 480) and x >= bounds[0] and y >= bounds[1] and
                x + width <= bounds[0] + bounds[2] and y + height <= bounds[1] + bounds[3],
                'Source windows fit physical monitor 2')
        observations.append((match[1], match[2]))
    require(observations == [(stage, window) for stage in ('open', 'after-spawner')
                             for window in ('frame', 'canvas')], 'Both windows at both stages')
    return {'frames': 16, 'epochs': 2, 'tiles_per_frame': 1,
            'checks': int(finish[0].split('checks=')[1]), 'monitor_observations': observations,
            'draws': draws}


def controls():
    class Controls(unittest.TestCase):
        def test_authored_saved_spawner(self):
            world = G.module('spawner_control_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
            decoder = G.module('spawner_control_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
            furnace = G.module('spawner_control_chunk', ROOT / 'tests/client/test_furnace_gui.py')
            with tempfile.TemporaryDirectory(prefix='spawner-input-') as temporary:
                directory = Path(temporary)
                region = directory / 'region'
                region.mkdir()
                regions(world, region)
                level, blocks, _ = furnace.selected_chunk(decoder, directory)
                self.assertEqual(blocks[(2 << 7) + 64], 52)
                self.assertEqual(blocks.count(52), 1)
                child, tiles = G.typed(level['TileEntities'], 9, 'Tile List')
                self.assertEqual(child, 10)
                self.assertEqual(tiles, [{'id': [8, 'MobSpawner'], 'x': [3, 0], 'y': [3, 64],
                                        'z': [3, 2], 'EntityId': [8, 'Pig'], 'Delay': [2, 32767]}])

        def test_receipt_refuses_incomplete_native_success(self):
            monitor = {'selected': {'work': [-1920, 0, 1920, 1040]}}
            lines = [f'WORLD_SELECTION_MONITOR stage={stage} window={window} x=-1387 y=280 '
                     'width=854 height=480 target=-1920,0,1920,1040'
                     for stage in ('open', 'after-spawner') for window in ('frame', 'canvas')]
            generated = []
            for epoch in range(2):
                for index in range(8):
                    first = epoch == index == 0
                    generated.append(f'SPAWNER_DRAW epoch={epoch} frame={index} cache-hit={int(not first)} '
                        f'cache-miss={int(first)} factory-calls={int(first)} '
                        f'models=6 compiled={6 if first else 0} faces={36 if first else 0} '
                        f'vertices={144 if first else 0} preview-pig=1 world-same=1 th-returned=1 '
                        'th-retired=1 ll-retired=1 world-list-absent=1')
                generated.append(f'SPAWNER_FRAME epoch={epoch} frames=8 tiles=1 n-pc=569 full-world=1 hud=1 saved-input=1 source-field-repair=0')
            generated.append('SPAWNER_CACHE epochs=2 preview-same=1 world-rebound=1 factory-calls=1 ordinary-save-reload=1')
            lines[2:2] = generated
            lines.append('SPAWNER_FINISH cleanup=1 windows-absent=1 checks=100')
            raw = '\n'.join(lines) + '\n'
            self.assertEqual(receipts(raw, '', monitor)['checks'], 100)
            for changed in (raw.replace('frames=8', 'frames=0'), raw + 'trailing\n',
                            raw.replace('checks=100', 'checks=0'), raw.replace('x=-1387', 'x=1'),
                            raw.replace('SPAWNER_FRAME', 'OTHER_FRAME'), raw + raw,
                            raw.replace('frame=7 cache-hit', 'frame=6 cache-hit'),
                            raw.replace('th-returned=1', 'th-returned=0'),
                            raw.replace('cache-hit=1', 'cache-hit=0'),
                            raw.replace('factory-calls=0', 'factory-calls=1')):
                with self.subTest(changed=changed), self.assertRaises(RuntimeError):
                    receipts(changed, '', monitor)
            with self.assertRaises(RuntimeError):
                receipts(raw, 'native diagnostic', monitor)

    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    require(result.wasSuccessful(), 'Spawner input/receipt controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.controls:
        controls()
        return
    require(os.name == 'nt' and all((args.executable, args.assets, args.artifacts)),
            'Native Windows executable, assets and private artifacts required')
    world = G.module('spawner_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    selected = G.module('spawner_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    mining = G.module('spawner_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    monitor = G.module('spawner_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='spawner-', dir=args.artifacts.resolve()))
    before, assets_before = G.fingerprints(G.source_inputs(executable)), world.snapshot(assets)
    require(all(type(pin) is str for pin in before.values()), 'Readable current inputs')
    report = {'status': 'FAIL', 'monitor': monitor, 'inputs_before': before,
              'scope': 'One authored saved Pig Spawner, sixteen returned Windows World/HUD frames across ordinary save/reselection, same cached Pig rebound to the new World and native cleanup; no original GL/pixel, sustained play or FPS qualification.',
              'input_scope': 'Selected current source/helpers/product/DLLs and imported assets; excludes compiler, full dependencies and desktop state.'}
    try:
        with tempfile.TemporaryDirectory(prefix='spawner-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            for folder, name in (('older', 'Old world'), ('bravo', ''), ('alpha', 'Spawner world')):
                directory = saves / folder
                directory.mkdir(parents=True)
                wire = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, 1303000000000, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
            region = saves / 'alpha/region'
            region.mkdir()
            regions(world, region)
            before_files = world.save_title_files(saves, evidence / 'before')
            for name in ('snapshots', 'entries'):
                (evidence / name).mkdir()
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds,
                               MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds, MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first_millis = time.time_ns() // 1000000
            result = world.retained_native_run([str(executable), str(assets), str(evidence)], environment,
                                               evidence, saves, before_files, 'spawner-frame', first_millis, 360)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native loaded Spawner World frame returned')
            report['native'] = receipts(result.stdout, result.stderr, monitor)
            report['images'] = {name: G.png_gate(evidence / name, first_millis) for name in
                                ('spawner-frame.png', 'spawner-title.png', 'spawner-reloaded.png')}
            decoder = G.module('spawner_saved_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
            chunk = G.module('spawner_saved_chunk', ROOT / 'tests/client/test_furnace_gui.py')
            level, blocks, _ = chunk.selected_chunk(decoder, evidence / 'snapshots/spawner-saved/alpha')
            require(blocks[(2 << 7) + 64] == 52, 'Ordinarily saved Spawner Block')
            child, tiles = G.typed(level.get('TileEntities'), 9, 'Saved TileEntities List')
            require(child == 10 and len(tiles) == 1, 'One actually saved Spawner Tile')
            tile = tiles[0]
            require({key: tile[key] for key in ('id', 'x', 'y', 'z', 'EntityId')} ==
                    {'id': [8, 'MobSpawner'], 'x': [3, 0], 'y': [3, 64], 'z': [3, 2], 'EntityId': [8, 'Pig']} and
                    type(G.typed(tile.get('Delay'), 2, 'Saved Spawner Short delay')) is int and
                    0 < tile['Delay'][1] <= 32767, 'Actual saved Spawner identity and positive delay')
            _, entities = G.typed(level.get('Entities'), 9, 'Saved ordinary Entities List')
            require(entities == [], 'Selected saved Chunk has no ordinary Entity entries')
            report['saved_spawner'] = {'tile': tile, 'selected_saved_chunk_entities_empty': True,
                                      'preview_absent_from_live_world_list_each_draw': True}
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = G.fingerprints(G.source_inputs(executable))
        report['assets_unchanged'] = world.snapshot(assets) == assets_before
        if report['inputs_after'] != before or not report['assets_unchanged']:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Current source/product/DLL or assets changed during process'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print('Spawner artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Spawner process continuity and cleanup')


if __name__ == '__main__':
    main()
