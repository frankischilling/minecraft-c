"""Author saved far Chunks; never invokes or edits original game code."""
import importlib.util
from pathlib import Path
import struct
import zlib

HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p / 'tests/reference/client/controller/capture.py').is_file())
BASE_CHUNKS = [(0, 0), (0, 1), (1, 0), (1, 1)]
FAR_CHUNKS = [(x, z) for x in (-1048577, -1048576, 1048575, 1048576)
              for z in (0, 1)]
CHUNKS = BASE_CHUNKS + FAR_CHUNKS


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def far_regions():
    """Use the unchanged picking fixture's actual Chunk NBT body and sector form."""
    fixture = module("signed_saved_chunk_nbt", ROOT / "tests/tools/client/test_picking.py")
    grouped = {}
    for x, z in FAR_CHUNKS:
        grouped.setdefault((x >> 5, z >> 5), []).append((x, z))
    result = {}
    for (rx, rz), chunks in grouped.items():
        header = bytearray(8192)
        sectors = []
        for x, z in chunks:
            payload = b"\2" + zlib.compress(fixture.chunk(x, z))
            record = struct.pack(">I", len(payload)) + payload
            if len(record) > 4096:
                raise AssertionError("finite authored Chunk exceeded one sector")
            location = (2 + len(sectors)) << 8 | 1
            struct.pack_into(">I", header, 4 * ((x & 31) + 32 * (z & 31)), location)
            sectors.append(record + bytes(4096 - len(record)))
        result[f"region/r.{rx}.{rz}.mcr"] = bytes(header) + b"".join(sectors)
    return result


def world(path):
    original = module("signed_inherited_world_inputs", ROOT / "tests/reference/client/picking/observe.py")
    original.world(path)
    for name, wire in far_regions().items():
        target = path / name
        if target.exists():
            raise AssertionError("far Region must not replace inherited base input")
        target.write_bytes(wire)
