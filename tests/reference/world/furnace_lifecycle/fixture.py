"""Author saved Furnace/Chunk inputs before any original process starts."""
import importlib.util
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[4]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


W = module('furnace_saved_world_base', ROOT / 'tests/reference/client/picking/observe.py')
P = module('furnace_saved_chunk_base', ROOT / 'tests/tools/client/test_picking.py')


def world(path, declaration):
    W.world(path)
    f = declaration['furnace']
    records, header = [], bytearray(8192)
    for cx in range(2):
        for cz in range(2):
            blocks, data = bytearray(32768), bytearray(16384)
            tiles = []
            if (cx, cz) == (0, 0):
                x, y, z = f['position']
                at = (x << 11) | (z << 7) | y
                blocks[at] = f['block']
                data[at // 2] |= f['metadata'] << ((at & 1) * 4)
                tile = P.tag(8, 'id', b'\0\7Furnace')
                tile += b''.join(P.tag(3, key, struct.pack('>i', value)) for key, value in zip(('x', 'y', 'z'), f['position']))
                tile += P.tag(2, 'BurnTime', struct.pack('>h', f['burn'])) + P.tag(2, 'CookTime', struct.pack('>h', f['cook']))
                items = []
                for slot, item_id, count, damage in f['items']:
                    items.append(P.tag(1, 'Slot', struct.pack('>b', slot)) +
                                 P.tag(2, 'id', struct.pack('>h', item_id)) +
                                 P.tag(1, 'Count', struct.pack('>b', count)) +
                                 P.tag(2, 'Damage', struct.pack('>h', damage)) + b'\0')
                tile += P.tag(9, 'Items', b'\x0a' + struct.pack('>i', len(items)) + b''.join(items))
                tiles.append(tile + b'\0')
            level = P.tag(3, 'xPos', struct.pack('>i', cx)) + P.tag(3, 'zPos', struct.pack('>i', cz))
            level += P.tag(4, 'LastUpdate', bytes(8))
            for key, value in (('Blocks', blocks), ('Data', data), ('SkyLight', bytes(16384)),
                               ('BlockLight', bytes(16384)), ('HeightMap', bytes(256))):
                level += P.tag(7, key, struct.pack('>i', len(value)) + bytes(value))
            level += P.tag(1, 'TerrainPopulated', b'\1') + P.tag(9, 'Entities', b'\x0a' + bytes(4))
            level += P.tag(9, 'TileEntities', b'\x0a' + struct.pack('>i', len(tiles)) + b''.join(tiles))
            raw = P.tag(10, '', P.tag(10, 'Level', level + b'\0') + b'\0')
            compressed = zlib.compress(raw)
            record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
            if len(record) > 4096:
                raise ValueError('one-sector declared Furnace Chunk')
            struct.pack_into('>I', header, 4 * (cx + 32 * cz), ((2 + len(records)) << 8) | 1)
            records.append(record + bytes(4096 - len(record)))
    (path / 'region/r.0.0.mcr').write_bytes(header + b''.join(records))
