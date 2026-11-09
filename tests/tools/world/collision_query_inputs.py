"""Literal pre-bootstrap source input preparation; no IO on import or launch."""
import gzip
import struct
import zlib

MODES = ("id-first", "metadata-first", "domain", "loader-error", "loader-null-error", "armed", "held", "material-nullable")


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


def nullable_prepared(saving, construction):
    """One literal pre-bootstrap ID edit through the existing canonical producers."""
    result = prepared("id-first", saving, construction)
    original = result["world/region/r.0.0.mcr"]
    require(type(original) is bytes and len(original) == 12288, "Canonical three-sector MCR")
    length = struct.unpack_from(">i", original, 8192)[0]
    require(1 < length < 4092 and original[8196] == 2, "Canonical bounded zlib record")
    decoder = zlib.decompressobj()
    raw = decoder.decompress(original[8197:8196 + length], saving.BOUND + 1)
    require(len(raw) <= saving.BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, "Complete canonical source record")
    before = saving.Nbt(raw).root()["Level"][1]
    old = before["Blocks"][1]
    require(before["Blocks"][0] == 7 and type(old) is bytes and len(old) == 32768,
            "Canonical full Blocks plane")
    cell = (8 << 11) | (8 << 7) | 66
    require(old[cell] == 0, "Declared originally-air cell")
    blocks = bytearray(old)
    blocks[cell] = 200
    encoded = saving.named(7, "Blocks", struct.pack(">i", len(old)) + old)
    require(raw.count(encoded) == 1, "One complete Blocks tag")
    raw = raw.replace(encoded, saving.named(7, "Blocks", struct.pack(">i", len(blocks)) + bytes(blocks)), 1)
    after = saving.Nbt(raw).root()["Level"][1]
    require(after["Blocks"] == [7, bytes(blocks)] and
            {k: v for k, v in before.items() if k != "Blocks"} ==
            {k: v for k, v in after.items() if k != "Blocks"},
            "Only the declared stored ID changes")
    require(all(a == b for i, (a, b) in enumerate(zip(old, blocks)) if i != cell),
            "Every other Block byte unchanged")
    compressed = zlib.compress(raw)
    require(len(compressed) + 5 < 4096, "Original sector capacity")
    region = bytearray(original)
    region[8192:12288] = bytes(4096)
    record = struct.pack(">i", len(compressed) + 1) + b"\2" + compressed
    region[8192:8192 + len(record)] = record
    result["world/region/r.0.0.mcr"] = bytes(region)
    return result


def prepared(mode, saving, construction):
    require(mode in MODES, "Declared scalar control mode")
    if mode == "material-nullable":
        return nullable_prepared(saving, construction)
    if mode.startswith("loader-"):
        info = gzip.compress(construction.metadata(), mtime=0)
        region = construction.malformed_region()
    else:
        info, region = values(saving)
    return {"world/level.dat": info, "world/data/idcounts.dat": construction.raw_ids(),
            "world/region/r.0.0.mcr": region,
            "world/preserve.bin": b"collision query unrelated data\0\xff"}



SCANNER_MODES = ('match', 'liquid', 'accelerate', 'admission', 'scratch', 'loader-error')


def scanner_prepared(mode, saving, construction):
    require(mode in SCANNER_MODES, "Closed scanner mode")
    if mode == "loader-error":
        info = gzip.compress(construction.metadata(), mtime=0)
        image = construction.malformed_region()
    elif mode == "scratch":
        info, image = saving.seed("item-chest")
    else:
        info, image = values(saving)
        length = struct.unpack_from(">i", image, 8192)[0]
        require(len(image) == 12288 and 1 < length < 4092 and image[8196] == 2,
                "Complete canonical one-sector source record")
        decoder = zlib.decompressobj()
        raw = decoder.decompress(image[8197:8196 + length], saving.BOUND + 1)
        require(len(raw) <= saving.BOUND and decoder.eof and not decoder.unused_data and
                not decoder.unconsumed_tail, "Bounded complete zlib input")
        before = saving.Nbt(raw).root()["Level"][1]
        require(before["Entities"] == [9, [10, []]] and
                before["TileEntities"] == [9, [10, []]], "Real empty stored lists")
        blocks, data = bytearray(before["Blocks"][1]), bytearray(before["Data"][1])

        def set_cell(x, y, z, block, metadata):
            cell = (x << 11) | (z << 7) | y
            require(blocks[cell] == 0, "Originally-air declared cell")
            blocks[cell] = block
            shift = (cell & 1) * 4
            data[cell >> 1] = (data[cell >> 1] & ~(15 << shift)) | (metadata << shift)

        if mode == "liquid":
            set_cell(8, 66, 8, 8, 0)
            set_cell(8, 67, 8, 200, 0)
        if mode == "accelerate":
            for row in [(10, 70, 10, 8, 0), (9, 70, 10, 8, 1),
                        (10, 70, 9, 8, 2), (11, 70, 10, 8, 3), (10, 70, 11, 8, 4)]:
                set_cell(*row)
        for key, updated in [("Blocks", bytes(blocks)), ("Data", bytes(data))]:
            old = before[key][1]
            encoded = saving.named(7, key, struct.pack(">i", len(old)) + old)
            require(raw.count(encoded) == 1, "One complete stored plane: " + key)
            raw = raw.replace(encoded, saving.named(7, key, struct.pack(">i", len(updated)) + updated), 1)
        after = saving.Nbt(raw).root()["Level"][1]
        require(after["Blocks"] == [7, bytes(blocks)] and after["Data"] == [7, bytes(data)] and
                {k: v for k, v in before.items() if k not in ("Blocks", "Data")} ==
                {k: v for k, v in after.items() if k not in ("Blocks", "Data")},
                "Only declared Block/metadata literals change before bootstrap")
        compressed = zlib.compress(raw)
        require(len(compressed) + 5 < 4096, "Original sector capacity")
        region = bytearray(image)
        region[8192:12288] = bytes(4096)
        record = struct.pack(">i", len(compressed) + 1) + b"\2" + compressed
        region[8192:8192 + len(record)] = record
        image = bytes(region)
    return {"world/level.dat": bytes(info), "world/data/idcounts.dat": construction.raw_ids(),
            "world/region/r.0.0.mcr": bytes(image),
            "world/preserve.bin": b"owned scanner unrelated bytes\0\xff"}
