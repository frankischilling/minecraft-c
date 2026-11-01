"""Finite native SDK-selected terrain; actual picking, zero progress and retained NW observations."""
import argparse
from fractions import Fraction
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import time
import zlib

ROOT = Path(__file__).resolve().parents[3]


def module(label, path):
    spec = importlib.util.spec_from_file_location(label, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def read(path, limit=1048576):
    if not path.is_file() or path.is_symlink() or path.stat().st_size > limit:
        raise RuntimeError('Bounded required regular evidence file')
    with path.open('rb') as source:
        data = source.read(limit + 1)
    if len(data) > limit:
        raise RuntimeError('Evidence file grew beyond bound')
    return data


def fingerprint(path, limit=256 * 1048576):
    if not path.is_file() or path.stat().st_size > limit:
        raise RuntimeError('Bounded input/product')
    digest, total = hashlib.sha256(), 0
    with path.open('rb') as source:
        while block := source.read(1048576):
            total += len(block)
            if total > limit:
                raise RuntimeError('Input/product grew beyond bound')
            digest.update(block)
    return digest.hexdigest()


def fields(text, prefix):
    result = []
    for line in text.splitlines():
        if line.startswith(prefix + ' '):
            pairs = [item.split('=', 1) for item in line.split()[1:]]
            row = dict(pairs)
            if len(row) != len(pairs):
                raise RuntimeError('Unique actual receipt fields')
            result.append(row)
    return result


def double(bits):
    return struct.unpack('>d', bytes.fromhex(bits))[0]


def doubles(text):
    return [double(bits) for bits in text.split(',')]


def double_bits(value):
    return struct.pack('>d', value).hex()


def f32(bits):
    return struct.unpack('>f', bytes.fromhex(bits))[0]


def rounded_fraction(value):
    """Binary32 nearest-even from an exact finite rational, independent of native SIMD."""
    sign = 0x80000000 if value < 0 else 0
    value = abs(Fraction(value))
    if not value:
        return sign
    numerator, denominator = value.numerator, value.denominator
    exponent = numerator.bit_length() - denominator.bit_length()
    if (numerator << max(0, -exponent)) < (denominator << max(0, exponent)):
        exponent -= 1
    shift = 23 - exponent
    a, b = (numerator << shift, denominator) if shift >= 0 else (numerator, denominator << -shift)
    mantissa, remainder = divmod(a, b)
    if remainder * 2 > b or (remainder * 2 == b and mantissa & 1):
        mantissa += 1
    if mantissa == 1 << 24:
        mantissa >>= 1
        exponent += 1
    if not -126 <= exponent <= 127 or not 1 << 23 <= mantissa < 1 << 24:
        raise RuntimeError('This finite process-local oracle requires normal binary32 operands')
    return sign | ((exponent + 127) << 23) | (mantissa - (1 << 23))


def number(bits):
    return Fraction(f32(f'{bits:08x}'))


def pulse(clock, table):
    converted = rounded_fraction(clock)
    angle = rounded_fraction(number(converted) / 100)
    multiplier = number(0x4622f983)  # Original float10430.378 lookup multiplier.
    scaled = rounded_fraction(number(angle) * multiplier)
    value = f32(f'{scaled:08x}')
    narrowed = 2147483647 if value >= 2147483647 else -2147483648 if value <= -2147483648 else int(value)
    index = narrowed & 65535
    sine = table[index]
    scaled_sine = rounded_fraction(number(sine) * number(0x3e4ccccd))
    shifted = rounded_fraction(number(scaled_sine) + number(0x3ecccccd))
    alpha = rounded_fraction(number(shifted) * number(0x3f000000))
    return alpha, index


def metadata(world, name):
    tag, string = world.tag, world.string
    body = tag(8, 'LevelName', string(name)) + tag(4, 'LastPlayed', struct.pack('>q', 1304000000000))
    body += tag(4, 'SizeOnDisk', struct.pack('>q', 3145728)) + tag(3, 'version', struct.pack('>i', 19132))
    body += tag(4, 'RandomSeed', struct.pack('>q', 123))
    for key, value in [('SpawnX', 0), ('SpawnY', 64), ('SpawnZ', 0)]:
        body += tag(3, key, struct.pack('>i', value))
    saved_y = 64 + struct.unpack('>f', struct.pack('>f', 1.62))[0]
    player = tag(3, 'Dimension', struct.pack('>i', 0))
    player += tag(9, 'Pos', b'\x06' + struct.pack('>i3d', 3, .5, saved_y, .5))
    player += tag(9, 'Motion', b'\x06' + struct.pack('>i3d', 3, 0., 0., 0.))
    player += tag(9, 'Rotation', b'\x05' + struct.pack('>i2f', 2, 0., 45.))
    player += tag(2, 'Health', struct.pack('>h', 20)) + tag(3, 'Score', struct.pack('>i', 7))
    player += tag(9, 'Inventory', b'\x0a\x00\x00\x00\x00')
    return b'\x0a\x00\x00' + tag(10, 'Data', body + tag(10, 'Player', player + b'\x00') + b'\x00') + b'\x00'


def saved_regions(world, directory, target):
    regions = {}
    for x in range(-8, 9):
        for z in range(-8, 9):
            blocks, light, data = bytearray(32768), bytearray(16384), bytearray(16384)
            heights = bytearray([64] * 256)
            for bx in range(16):
                for bz in range(16):
                    at = (bx << 11) | (bz << 7)
                    blocks[at] = 7
                    blocks[at + 1:at + 64] = bytes([3] * 63)
                    light[(at + 64) // 2:(at + 128) // 2] = bytes([255] * 32)
            if (x, z) == (0, 0):
                at = (2 << 7) | 64
                blocks[at] = target['id']
                data[at // 2] = target['metadata']
                if target['id'] == 3:
                    heights[2 * 16] = 65
                    light[at // 2] &= 240
            level = world.tag(3, 'xPos', struct.pack('>i', x)) + world.tag(3, 'zPos', struct.pack('>i', z))
            level += world.tag(4, 'LastUpdate', struct.pack('>q', 0))
            for key, value in [('Blocks', blocks), ('Data', data), ('SkyLight', light),
                               ('BlockLight', bytes(16384)), ('HeightMap', heights)]:
                level += world.tag(7, key, struct.pack('>i', len(value)) + value)
            level += world.tag(1, 'TerrainPopulated', b'\x01')
            level += world.tag(9, 'Entities', b'\x0a\x00\x00\x00\x00')
            level += world.tag(9, 'TileEntities', b'\x0a\x00\x00\x00\x00')
            raw = b'\x0a\x00\x00' + world.tag(10, 'Level', level + b'\x00') + b'\x00'
            compressed = zlib.compress(raw)
            record = struct.pack('>I', len(compressed) + 1) + b'\x02' + compressed
            count = (len(record) + 4095) // 4096
            region = regions.setdefault((x >> 5, z >> 5), bytearray(8192))
            sector = len(region) // 4096
            struct.pack_into('>I', region, ((x & 31) + (z & 31) * 32) * 4, sector * 256 + count)
            region += record + bytes(count * 4096 - len(record))
    for (x, z), data in regions.items():
        (directory / f'r.{x}.{z}.mcr').write_bytes(data)


def target_readback(decoder, directory, target):
    data = read(directory / 'r.0.0.mcr')
    if len(data) < 8192 or len(data) % 4096:
        raise RuntimeError('Complete bounded target region')
    offset = struct.unpack_from('>I', data, 0)[0]
    sector, count = offset >> 8, offset & 255
    if sector < 2 or count == 0 or (sector + count) * 4096 > len(data):
        raise RuntimeError('Actual target allocation')
    at = sector * 4096
    size = struct.unpack_from('>I', data, at)[0]
    if not 1 < size <= count * 4096 - 4 or data[at + 4] != 2:
        raise RuntimeError('Actual target selector2 extent')
    root = decoder.inflate(data[at + 5:at + 4 + size], 15)
    if root.get('Level', [None])[0] != 10:
        raise RuntimeError('Actual target Level Compound')
    level = root['Level'][1]
    if level.get('xPos') != [3, 0] or level.get('zPos') != [3, 0]:
        raise RuntimeError('Actual target Chunk coordinates')
    arrays = {}
    for key, length in [('Blocks', 32768), ('Data', 16384)]:
        if key not in level or level[key][0] != 7 or len(level[key][1]) != length:
            raise RuntimeError('Actual typed target block/nibble arrays')
        arrays[key] = level[key][1]
    at = (2 << 7) | 64
    if arrays['Blocks'][at] != target['id'] or arrays['Data'][at // 2] & 15 != target['metadata']:
        raise RuntimeError('Aiming changed the authored selected block or metadata')
    return {'coordinate': [0, 64, 2], 'id': arrays['Blocks'][at],
            'metadata': arrays['Data'][at // 2] & 15, 'selector': 2}


def verify(stdout, evidence, profile, target, first, last):
    draws = fields(stdout, 'BLOCK_SELECTION')
    operands = fields(stdout, 'SELECTION_OPERANDS')
    boxes = fields(stdout, 'SELECTION_BOX')
    batches = fields(stdout, 'SELECTION_BATCH')
    vertices = fields(stdout, 'SELECTION_VERTEX')
    table = struct.unpack('<65536I', read(evidence / 'math-table.bin'))
    if len(draws) != 3 or len(operands) != 3 or len(batches) != 9 or len(vertices) != 54:
        raise RuntimeError('Exact three actual targeted frames and returned batch observations')
    expected_order = [(0,0,0),(1,0,0),(1,0,1),(0,0,1),(0,0,0),
                      (0,1,0),(1,1,0),(1,1,1),(0,1,1),(0,1,0),
                      (0,0,0),(0,1,0),(1,0,0),(1,1,0),(1,0,1),(1,1,1),(0,0,1),(0,1,1)]
    expected_local = [0., 0., 0., 1., target['height'], 1.]
    epsilon = f32('3b03126f')
    observed = []
    for sample, (draw, op) in enumerate(zip(draws, operands)):
        if int(draw['sample']) != sample or op['sample'] != str(sample):
            raise RuntimeError('Ordered actual frame labels')
        if any(draw[key] != value for key, value in {'world-pc':'1013','overlay':'506','outline':'235',
                'alpha-restored':'1','progress':'00000000','block':str(target['id']),'xyz':'0,64,2',
                'draws':'3','vertices':'18','empty-hand':'1','sdk-selection':'1','hit-injected':'0','progress-assigned':'0'}.items()):
            raise RuntimeError('Actual ordinary zero-progress source caller/picking result')
        if not draw['hit'] == draw['overlay-hit'] == draw['outline-hit'] or int(draw['hit'],16) == 0:
            raise RuntimeError('Same actual picked Hit passed to both source expressions')
        now = int(draw['now'])
        alpha, index = pulse(now, table)
        if not first <= now <= last or draw['pulse'] != f'{alpha:08x}':
            raise RuntimeError('Actual process-local long/float/division/lookup/pulse bits')
        previous, current, camera = map(doubles, [op['previous'], op['current'], op['camera']])
        partial = f32(draw['partial'])
        expected_camera = [a + (b - a) * partial for a,b in zip(previous,current)]
        if [double_bits(v) for v in camera] != [double_bits(v) for v in expected_camera]:
            raise RuntimeError('Actual ordered Player interpolation')
        if doubles(op['block-bounds']) != expected_local:
            raise RuntimeError('Live Dirt/Snow state-shape bounds; no unit-cube substitute')
        selected = [expected_local[i] + [0,64,2][i % 3] for i in range(6)]
        expanded = [v + (-epsilon if i < 3 else epsilon) for i,v in enumerate(selected)]
        shifted = [v + -camera[i % 3] for i,v in enumerate(expanded)]
        for kind, expected in [('selected',selected),('expanded',expanded),('shifted',shifted)]:
            row = next(row for row in boxes if row['sample']==str(sample) and row['kind']==kind)
            if row['values'].split(',') != [double_bits(v) for v in expected]:
                raise RuntimeError('Actual selected/float-epsilon-expanded/camera-shifted bounds')
        selected_batches = [row for row in batches if row['sample']==str(sample)]
        if len({row['nw'] for row in selected_batches}) != 1 or int(selected_batches[0]['nw'],16) == 0:
            raise RuntimeError('Same actual NW receiver across all three returned draws')
        for batch, count in enumerate([5,5,8]):
            row = next(row for row in selected_batches if row['batch']==str(batch))
            expected = {'mode':str(3 if batch<2 else 1),'vertices':str(count),'bytes':str(count*32),
                        'line':'40000000','color':'00000000,00000000,00000000,3ecccccd',
                        'texture':'0','alpha':'0','depth-mask':'0','blend':'1','blend-source':'770','blend-destination':'771',
                        'uv-array':'0','color-array':'0','normal-array':'0','color-disabled':'0','fog':'1','lighting':'0'}
            if any(row.get(key)!=value for key,value in expected.items()):
                raise RuntimeError('Actual requested outline GL prefix and batch modes')
            batch_vertices = [row for row in vertices if row['sample']==str(sample) and row['batch']==str(batch)]
            if [int(row['index']) for row in batch_vertices] != list(range(count)):
                raise RuntimeError('Actual bounded vertex sequence')
            offset = 0 if batch==0 else 5 if batch==1 else 10
            translation = doubles(row['translation'])
            for position, row in enumerate(batch_vertices):
                words = row['words'].split(',')
                coordinate = [shifted[axis + expected_order[offset+position][axis]*3] + translation[axis] for axis in range(3)]
                expected_words = [struct.pack('>f',value).hex() for value in coordinate]
                if len(words)!=8 or words[:3]!=expected_words:
                    raise RuntimeError('Actual NW raw position words differ from independent outline order')
        pool = [row for row in fields(stdout, 'SELECTION_POOL') if row['sample']==str(sample)]
        cursors = [int(value) for value in pool[0]['cursors'].split(',')] if len(pool)==1 else []
        if len(cursors)!=4 or cursors!=list(range(cursors[0],cursors[0]+4)):
            raise RuntimeError('Three actual ordered pool factory advances without child reset')
        observed.append({'sample':sample,'clock':now,'math_index':index,'pulse_bits':draw['pulse'],
                         'camera_bits':op['camera'],'actual_vertices':18})
    finish = fields(stdout, 'BLOCK_SELECTION_FINISH')
    held = fields(stdout, 'SELECTION_HELD')
    if len(finish)!=1 or finish[0]['profile']!=profile['name'] or any(finish[0].get(key)!='1' for key in ['cleanup','windows-absent']):
        raise RuntimeError('Actual checked terminal/window cleanup')
    if finish[0]['compact-nbt-error']!='544' or len(held)!=1 or held[0]['balanced-release']!='1':
        raise RuntimeError('Actual held pool storage survives a real next frame and releases')
    return {'samples':observed,'batch_draws':9,'actual_vertices':54,
            'cleanup_checks':int(finish[0]['checks']),
            'scope':'Native actual source caller and geometry/clock/state evidence; no original pixels/mining/positive-progress/normal-playability claim'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--mode', choices=['dirt','snow','dirt-rounded','snow-rounded'], required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        raise RuntimeError('Actual disposable account/mount qualification required before Linux World input; HOME alone is insufficient')
    world = module('selection_world_helpers', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    decoder_path = ROOT / 'tests/tools/world/test_source_saving.py'
    decoder = module('selection_independent_nbt', decoder_path)
    scenarios_path = ROOT / 'tests/client/gui/block_selection_scenarios.json'
    scenarios = json.loads(read(scenarios_path))
    profile = next(row for row in scenarios['profiles'] if row['name']==args.mode)
    target = scenarios['target'][profile['block']]
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True,exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix=args.mode+'-',dir=args.artifacts.resolve()))
    inputs = [Path(__file__), scenarios_path, ROOT/'tests/client/gui/test_block_selection.c',
              ROOT/'tests/client/startup/test_world_selection_loop.c',
              ROOT/'tests/tools/client/test_world_selection_loop.py', decoder_path, executable]
    before_inputs = {str(path):fingerprint(path) for path in inputs}
    before_assets = world.snapshot(assets)
    report = {'status':'FAIL','mode':args.mode,'scope':__doc__}
    primary_error = False
    (evidence/'run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        with tempfile.TemporaryDirectory(prefix='selection-appdata-',dir=evidence) as temporary:
            home = Path(temporary); game = home/'.minecraft'
            world.repeat_statistics(game, assets)
            (game/'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game/'saves'; saves.mkdir()
            for folder,name,played in [('older','Old world',1303000000000),('bravo','',1304000000000),('alpha','Aiming world',1304000000000)]:
                directory=saves/folder;directory.mkdir()
                data = metadata(world,name) if folder=='alpha' else world.metadata(name,played,19132)
                (directory/('level.dat_old' if folder=='bravo' else 'level.dat')).write_bytes(gzip.compress(data,mtime=0))
                (directory/'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
            directory=saves/'alpha/region';directory.mkdir();saved_regions(world,directory,target)
            before = world.save_title_files(saves,evidence/'before')
            environment=os.environ.copy();environment.update(APPDATA=str(home),LOCALAPPDATA=str(home),USERPROFILE=str(home),ALSOFT_DRIVERS='null')
            first=time.time_ns()//1000000
            result=world.retained_native_run([str(executable),str(assets),args.mode,str(evidence)],
                environment,evidence,saves,before,args.mode,first,240)
            last=time.time_ns()//1000000
            print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
            if result.returncode:
                raise RuntimeError(f'Native selection exited{result.returncode}')
            after=world.save_title_files(saves,evidence/'after-verified')
            for name,value in before.items():
                if not name.startswith('alpha/') or name=='alpha/preserve-chunk.dat':
                    if after.get(name)!=value:
                        raise RuntimeError('Aiming changed unrelated World input')
            if any(not name.startswith('alpha/') for name in set(after)-set(before)):
                raise RuntimeError('Aiming created unrelated World product')
            report['actual']=verify(result.stdout,evidence,profile,target,first,last)
            report['target_readback']=target_readback(decoder,directory,target)
        report['status']='PASS'
    except BaseException as error:
        primary_error = True
        report['error']={'type':type(error).__name__,'error':str(error)}
        raise
    finally:
        after_inputs, final_errors = {}, {}
        for path in inputs:
            try:
                after_inputs[str(path)] = fingerprint(path)
            except Exception as error:
                final_errors[str(path)] = {'type':type(error).__name__,'error':str(error)}
        try:
            after_assets=world.snapshot(assets)
        except Exception as error:
            after_assets=None
            final_errors['assets']={'type':type(error).__name__,'error':str(error)}
        report.update(inputs_before=before_inputs,inputs_after=after_inputs,assets_before=before_assets,assets_after=after_assets)
        if before_inputs!=after_inputs or before_assets!=after_assets:
            report['status']='FAIL';report['continuity_error']='Source/product/imported assets changed'
        if final_errors:
            report['status']='FAIL';report['finalization_errors']=final_errors
        try:
            (evidence/'run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        except Exception:
            if not primary_error:
                raise
        if not primary_error and ('continuity_error' in report or final_errors):
            raise RuntimeError(report.get('continuity_error','Evidence finalization failed'))
    print(f'Native actual selected-block{args.mode}PASS;{evidence}')


if __name__=='__main__':
    main()
