"""Canonical deferred malformed source inputs; no IO on import."""
import gzip
from pathlib import Path
import struct
import zlib


def string(value):
    data = value.encode("ascii")
    return struct.pack(">H", len(data)) + data


def tag(kind, key, value):
    return bytes([kind]) + string(key) + value


def metadata(version=19132, nether=False):
    data = tag(8, "LevelName", string("Disk name"))
    data += tag(4, "RandomSeed", struct.pack(">q", 23))
    data += tag(4, "Time", struct.pack(">q", 6000))
    data += tag(4, "LastPlayed", struct.pack(">q", 1304000000000))
    data += tag(3, "SpawnX", struct.pack(">i", 8))
    data += tag(3, "SpawnY", struct.pack(">i", 70))
    data += tag(3, "SpawnZ", struct.pack(">i", 9))
    data += tag(3, "version", struct.pack(">i", version))
    if nether:
        data += tag(10, "Player", tag(3, "Dimension", struct.pack(">i", -1)) + b"\x00")
    return b"\x0a\x00\x00" + tag(10, "Data", data + b"\x00") + b"\x00"


def raw_ids():
    entries = tag(2, "a", struct.pack(">h", 7))
    entries += tag(3, "ignored", struct.pack(">i", 123))
    entries += tag(2, "b", struct.pack(">h", -4))
    return b"\x0a\x00\x00" + entries + b"\x00"


def malformed_region():
    """A complete source region; its one zlib record has a non-Compound root."""
    payload = zlib.compress(b"\x01\x00\x00\x07")
    header = struct.pack(">I", (2 << 8) | 1) + bytes(8192 - 4)
    record = struct.pack(">I", len(payload) + 1) + b"\x02" + payload
    return header + record + bytes(4096 - len(record))


def prepared_bytes():
    """Return literal authored bytes; Root's future controller owns creation."""
    return {
        'world/level.dat': gzip.compress(metadata(), mtime=0),
        'world/data/idcounts.dat': raw_ids(),
        'world/region/r.0.0.mcr': malformed_region(),
        'world/preserve.bin': b'kernel direct-loader unrelated data\x00\xff',
    }


def require(value, message):
    if not value:
        raise ValueError(message)


def prepare_lazy(module):
    info, image = module.seed("empty")
    image = bytearray(image)
    require(len(image) == 12288 and struct.unpack_from(">I", image, 0)[0] == (2 << 8) | 1,
            "Canonical one-sector selected empty seed")
    length = struct.unpack_from(">i", image, 8192)[0]
    require(1 < length < 4092 and image[8196] == 2, "Actual selector2 seed record")
    decoder = zlib.decompressobj()
    raw = decoder.decompress(image[8197:8196 + length], module.BOUND + 1)
    require(len(raw) <= module.BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, "Complete bounded canonical compressed seed")
    before = module.Nbt(raw).root()["Level"][1]
    require(before["TileEntities"] == [9, [10, []]] and before["Entities"] == [9, [10, []]],
            "Real empty source Entity/Tile lists before authored input")
    old_blocks = module.named(7, "Blocks", struct.pack(">i", 32768) + bytes(32768))
    require(raw.count(old_blocks) == 1, "Exactly one complete stored Blocks array")
    blocks = bytearray(32768)
    blocks[(8 << 11) | (8 << 7) | 64] = 54  # Literal authored Chest block input, before bootstrap.
    new_blocks = module.named(7, "Blocks", struct.pack(">i", 32768) + bytes(blocks))
    raw = raw.replace(old_blocks, new_blocks, 1)
    after = module.Nbt(raw).root()["Level"][1]
    require(after["Blocks"] == [7, bytes(blocks)] and
            after["TileEntities"] == before["TileEntities"] and
            after["Entities"] == before["Entities"] and
            {key: value for key, value in after.items() if key != "Blocks"} ==
            {key: value for key, value in before.items() if key != "Blocks"},
            "Only actual stored Blocks byte changed; all lists/other fields identical")
    compressed = zlib.compress(raw)
    require(len(compressed) + 5 < 4096, "Actual authored record fits original one sector")
    image[8192:12288] = bytes(4096)
    record = struct.pack(">i", len(compressed) + 1) + b"\2" + compressed
    image[8192:8192 + len(record)] = record
    return info, image


def prepared(kind, mode, source_saving):
    """Return pre-bootstrap literal bytes, never execute or model a native outcome."""
    require(kind in ('loader', 'lookup'), 'Unknown actual recipe kind')
    if kind == 'loader':
        require(mode in ('error', 'null-error'), 'Unknown loader mode')
        return prepared_bytes()
    require(mode in ('map', 'lazy', 'armed', 'held', 'nested'), 'Unknown lookup mode')
    info, image = prepare_lazy(source_saving) if mode == 'lazy' else source_saving.seed('item-chest')
    return {'world/level.dat': info, 'world/region/r.0.0.mcr': bytes(image),
            'world/preserve.bin': b'unrelated disposable World file\0\xff'}
