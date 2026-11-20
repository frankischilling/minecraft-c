"""Literal pre-bootstrap inputs only. Import prepares no file or Source call."""
import gzip
import struct
import zlib

MODES = ("query", "prefix", "armed", "held", "acquire-loader")


def require(value, message):
    if not value:
        raise ValueError(message)


def values(saving):
    info, region = saving.seed("empty")
    region = bytearray(region)
    require(len(region) == 12288 and struct.unpack_from(">I", region, 0)[0] == ((2 << 8) | 1),
            "Canonical complete one-sector empty source image")
    length = struct.unpack_from(">i", region, 8192)[0]
    require(1 < length < 4092 and region[8196] == 2, "Canonical selector2 record")
    decoder = zlib.decompressobj()
    raw = decoder.decompress(region[8197:8196 + length], saving.BOUND + 1)
    require(len(raw) <= saving.BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, "Complete bounded canonical compressed seed")
    before = saving.Nbt(raw).root()["Level"][1]
    require(before["Entities"] == [9, [10, []]] and before["TileEntities"] == [9, [10, []]],
            "Genuine empty stored Entity and TileEntity lists")
    blocks, data = bytearray(32768), bytearray(16384)
    cell = (8 << 11) | (8 << 7) | 64
    blocks[cell], blocks[cell + 1], data[cell >> 1] = 1, 3, 0xA5
    for key, old, new in (("Blocks", bytes(32768), bytes(blocks)), ("Data", bytes(16384), bytes(data))):
        encoded = saving.named(7, key, struct.pack(">i", len(old)) + old)
        require(raw.count(encoded) == 1, "Exactly one complete stored array: " + key)
        raw = raw.replace(encoded, saving.named(7, key, struct.pack(">i", len(new)) + new), 1)
    after = saving.Nbt(raw).root()["Level"][1]
    require(after["Blocks"] == [7, bytes(blocks)] and after["Data"] == [7, bytes(data)] and
        {k: v for k, v in before.items() if k not in ("Blocks", "Data")} ==
        {k: v for k, v in after.items() if k not in ("Blocks", "Data")},
        "Only declared literal block bytes and packed metadata byte changed")
    compressed = zlib.compress(raw)
    require(len(compressed) + 5 < 4096, "Authored source record fits original sector")
    region[8192:12288] = bytes(4096)
    record = struct.pack(">i", len(compressed) + 1) + b"\2" + compressed
    region[8192:8192 + len(record)] = record
    return info, bytes(region)


def prepared(mode, saving, construction):
    require(mode in MODES, "Declared owned nearby component mode")
    if mode == "acquire-loader":
        info = gzip.compress(construction.metadata(), mtime=0)
        region = construction.malformed_region()
    else:
        info, region = values(saving)
    require(type(info) is bytes and type(region) is bytes, "Immutable canonical bytes")
    return {"world/level.dat": info,
            "world/data/idcounts.dat": construction.raw_ids(),
            "world/region/r.0.0.mcr": region,
            "world/preserve.bin": b"owned nearby unrelated data\0\xff"}
