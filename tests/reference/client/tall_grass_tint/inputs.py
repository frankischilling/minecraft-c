"""Authored saved Chunks; no imported assets or Source state repairs."""
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


def world(path, original=False):
    picking = module('tint_world_inputs', ROOT / 'tests/tools/client/test_picking.py')
    if original:
        module('tint_original_world', ROOT / 'tests/reference/client/picking/observe.py').world(path)
    else:
        picking.authored_world(path)
    for x, z in ((43, 43), (-44, -44)):
        payload = b'\2' + zlib.compress(picking.chunk(x, z))
        record = struct.pack('>I', len(payload)) + payload
        assert len(record) <= 4096
        header = bytearray(8192)
        struct.pack_into('>I', header, 4 * ((x & 31) + 32 * (z & 31)), (2 << 8) | 1)
        (path / 'region' / f'r.{x >> 5}.{z >> 5}.mcr').write_bytes(
            header + record + bytes(4096 - len(record)))
