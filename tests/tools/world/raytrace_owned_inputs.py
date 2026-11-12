"""Closed literal pre-bootstrap Ray scenes. No IO or producer call on import."""
import gzip
import struct
import zlib

MODES = ('clear', 'miss', 'block', 'state-shape', 'null-end',
         'math-missing', 'math-storage', 'loader-error')


def require(value, message):
    if not value:
        raise ValueError(message)


def prepared(mode, saving, construction):
    """Root supplies actual public test_source_saving/source_world_construction modules."""
    require(mode in MODES, 'Declared owned-Ray mode')
    if mode == 'loader-error':
        info = gzip.compress(construction.metadata(), mtime=0)
        image = construction.malformed_region()
    else:
        info, image = saving.seed('empty')
        require(len(image) == 12288 and struct.unpack_from('>I', image, 0)[0] == ((2 << 8) | 1),
                'Canonical complete one-sector empty scene')
        length = struct.unpack_from('>i', image, 8192)[0]
        require(1 < length < 4092 and image[8196] == 2, 'Canonical zlib record')
        decoder = zlib.decompressobj()
        raw = decoder.decompress(image[8197:8196 + length], saving.BOUND + 1)
        require(len(raw) <= saving.BOUND and decoder.eof and not decoder.unused_data and
                not decoder.unconsumed_tail, 'Complete bounded seed record')
        before = saving.Nbt(raw).root()['Level'][1]
        require(before['Entities'] == [9, [10, []]] and before['TileEntities'] == [9, [10, []]],
                'Real empty source lists; no configured callbacks or producer outcomes')
        blocks, metadata = bytearray(before['Blocks'][1]), bytearray(before['Data'][1])
        if mode in ('block', 'state-shape'):
            cell = (4 << 11) | (4 << 7) | 64
            require(blocks[cell] == 0 and metadata[cell >> 1] == 0, 'Originally-air scene cell')
            blocks[cell] = 1 if mode == 'block' else 66
            if mode == 'state-shape':
                metadata[cell >> 1] = 2
        for key, updated in [('Blocks', bytes(blocks)), ('Data', bytes(metadata))]:
            old = before[key][1]
            encoded = saving.named(7, key, struct.pack('>i', len(old)) + old)
            require(raw.count(encoded) == 1, 'Exactly one complete plane ' + key)
            raw = raw.replace(encoded, saving.named(7, key, struct.pack('>i', len(updated)) + updated), 1)
        after = saving.Nbt(raw).root()['Level'][1]
        require(after['Blocks'] == [7, bytes(blocks)] and after['Data'] == [7, bytes(metadata)] and
                {k: v for k, v in before.items() if k not in ('Blocks', 'Data')} ==
                {k: v for k, v in after.items() if k not in ('Blocks', 'Data')},
                'Only the declared literal block/metadata planes change')
        compressed = zlib.compress(raw)
        require(len(compressed) + 5 < 4096, 'Original one-sector bound')
        image = bytearray(image)
        image[8192:12288] = bytes(4096)
        record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
        image[8192:8192 + len(record)] = record
    return {'world/level.dat': bytes(info), 'world/data/idcounts.dat': construction.raw_ids(),
            'world/region/r.0.0.mcr': bytes(image),
            'world/preserve.bin': b'owned Ray unrelated literal\0\xff'}
