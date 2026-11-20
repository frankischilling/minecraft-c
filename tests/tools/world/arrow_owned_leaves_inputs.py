"""Pre-bootstrap literal inputs only; no IO or producer invocation on import.

Root supplies the genuine existing test_source_saving and world_query_inputs
modules. This only authors NBT bytes before bootstrap, never a native outcome.
"""
import struct
import zlib

MODES = ('base-air', 'base-water', 'base-lava', 'base-splash',
         'collision-unused', 'collision-portal', 'collision-metadata', 'collision-piston')


def require(value, text):
    if not value:
        raise ValueError(text)


def prepared(mode, saving, construction):
    require(mode in MODES, 'Closed owned-leaf mode')
    info, image = saving.seed('empty')
    require(len(image) == 12288 and struct.unpack_from('>I', image, 0)[0] == ((2 << 8) | 1),
            'Genuine canonical empty one-sector MCR')
    length = struct.unpack_from('>i', image, 8192)[0]
    require(1 < length < 4092 and image[8196] == 2, 'Canonical selected zlib record')
    decoder = zlib.decompressobj()
    raw = decoder.decompress(image[8197:8196 + length], saving.BOUND + 1)
    require(len(raw) <= saving.BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, 'Complete bounded original record')
    before = saving.Nbt(raw).root()['Level'][1]
    require(before['Entities'] == [9, [10, []]] and before['TileEntities'] == [9, [10, []]],
            'Genuine empty stored lists')
    blocks, data = bytearray(before['Blocks'][1]), bytearray(before['Data'][1])
    declared = []

    def cell(x, y, z, block, metadata=0):
        index = (x << 11) | (z << 7) | y
        require(blocks[index] == 0 and 0 <= metadata <= 15, 'Declared originally-air cell')
        blocks[index] = block
        shift = 4 * (index & 1)
        data[index >> 1] = (data[index >> 1] & ~(15 << shift)) | (metadata << shift)
        declared.append((x, y, z, block, metadata))

    if mode in ('base-water', 'base-splash'):
        # Actual positive X/Z gradient, used by the genuine fluid-vector body.
        for entry in [(10, 66, 10, 8, 0), (9, 66, 10, 8, 1),
                      (10, 66, 9, 8, 2), (11, 66, 10, 8, 3), (10, 66, 11, 8, 4)]:
            cell(*entry)
    if mode == 'base-lava':
        cell(13, 66, 13, 11)
    if mode == 'collision-portal':
        cell(4, 66, 4, 90)
        cell(3, 66, 4, 90)
        cell(7, 66, 4, 90)
    if mode == 'collision-metadata':
        cell(6, 66, 8, 65, 2)
        cell(6, 67, 8, 65, 5)
    if mode == 'collision-piston':
        cell(6, 66, 6, 36)
    for key, value in [('Blocks', bytes(blocks)), ('Data', bytes(data))]:
        old = before[key][1]
        encoded = saving.named(7, key, struct.pack('>i', len(old)) + old)
        require(raw.count(encoded) == 1, 'One actual source plane ' + key)
        raw = raw.replace(encoded, saving.named(7, key, struct.pack('>i', len(value)) + value), 1)

    if mode == 'collision-piston':
        # Genuine persistent Tile Piston ctor + read, never an authored native
        # object, private progress write, phase alteration or provider shim.
        tile = saving.compound([
            saving.named(8, 'id', saving.text('Piston')),
            saving.named(3, 'x', struct.pack('>i', 6)),
            saving.named(3, 'y', struct.pack('>i', 66)),
            saving.named(3, 'z', struct.pack('>i', 6)),
            saving.named(3, 'blockId', struct.pack('>i', 1)),
            saving.named(3, 'blockData', struct.pack('>i', 0)),
            saving.named(3, 'facing', struct.pack('>i', 5)),
            saving.named(5, 'progress', struct.pack('>f', .5)),
            saving.named(1, 'extending', b'\1')])
        old = saving.named(9, 'TileEntities', b'\12' + struct.pack('>i', 0))
        require(raw.count(old) == 1, 'One genuine original empty Tile list')
        raw = raw.replace(old, saving.named(9, 'TileEntities', b'\12' + struct.pack('>i', 1) + tile), 1)

    after = saving.Nbt(raw).root()['Level'][1]
    changed = {'Blocks', 'Data'} | ({'TileEntities'} if mode == 'collision-piston' else set())
    require(after['Blocks'] == [7, bytes(blocks)] and after['Data'] == [7, bytes(data)] and
            {k: v for k, v in before.items() if k not in changed} ==
            {k: v for k, v in after.items() if k not in changed},
            'Only declared pre-bootstrap cells and optional genuine Piston record change')
    require(blocks[(8 << 11) | (8 << 7) | 66] == 0,
            'Prime and prior-air constructor position remains genuine air')
    compressed = zlib.compress(raw)
    require(len(compressed) + 5 < 4096, 'Actual record remains within original sector')
    result = bytearray(image)
    result[8192:12288] = bytes(4096)
    record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
    result[8192:8192 + len(record)] = record
    return {'world/level.dat': bytes(info), 'world/data/idcounts.dat': construction.raw_ids(),
            'world/region/r.0.0.mcr': bytes(result),
            'world/preserve.bin': b'owned Arrow leaf unrelated literal\0\xff'}
