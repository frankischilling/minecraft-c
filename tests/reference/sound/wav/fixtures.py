"""Deterministic authored WAVE bytes and the sixty declared call profiles."""

from dataclasses import asdict, dataclass
import hashlib
import struct


@dataclass(frozen=True)
class Recipe:
    name: str
    tag: int = 1
    channels: int = 1
    bits: int = 16
    alignment: int = 0
    rate: int = 32000
    valid_bits: int = 0
    mask: int = 0
    float_subtype: bool = False
    before_junk: int = 0
    after_junk: int = 0
    declared_bytes: int = 24
    data_bytes: int = 24
    odd_format: bool = False
    ascending_data: bool = False


RECIPES = (
    Recipe("pcm8", bits=8), Recipe("pcm16"), Recipe("pcm24", bits=24),
    Recipe("pcm32", bits=32), Recipe("pcm7", bits=7), Recipe("pcm12", bits=12),
    Recipe("stereo16", channels=2, alignment=1),
    Recipe("alaw8", tag=6, bits=8), Recipe("ulaw8", tag=7, bits=8), Recipe("alaw16", tag=6),
    Recipe("float32", tag=3, bits=32), Recipe("float64", tag=3, bits=64),
    Recipe("extensible24", tag=65534, bits=24, valid_bits=20, mask=3),
    Recipe("extensible-float32", tag=65534, bits=32, valid_bits=32, mask=3, float_subtype=True),
    Recipe("extensible-float64", tag=65534, bits=64, valid_bits=64, mask=3, float_subtype=True),
    Recipe("junk-odd", before_junk=3, after_junk=5),
    Recipe("classic-negative-rate", rate=0xffffffff),
    Recipe("float-unsigned-rate", tag=3, bits=32, rate=0xffffffff),
    Recipe("pcm16-empty", data_bytes=0, declared_bytes=0, ascending_data=True),
    Recipe("pcm16-trunc", declared_bytes=40, ascending_data=True),
    Recipe("pcm16-odd", data_bytes=23, declared_bytes=23, ascending_data=True),
    Recipe("junk8192", before_junk=8192, ascending_data=True),
    Recipe("fmt-odd", odd_format=True, ascending_data=True),
    Recipe("bad-tag", tag=2, ascending_data=True),
)


def junk(size):
    return b"JUNK" + struct.pack("<I", size) + bytes(size + size % 2) if size else b""


def encode(recipe):
    alignment = recipe.alignment or ((recipe.bits + 7) // 8) * recipe.channels
    fmt = struct.pack("<HHIIHH", recipe.tag, recipe.channels, recipe.rate,
                      recipe.rate * alignment & 0xffffffff, alignment, recipe.bits)
    if recipe.tag == 65534:
        fmt += struct.pack("<HHIIHH", 22, recipe.valid_bits, recipe.mask,
                           3 if recipe.float_subtype else 1, 0, 16)
        fmt += bytes.fromhex("800000aa00389b71")
    elif recipe.odd_format:
        fmt += b"X"
    data = bytes((i + 1 if recipe.ascending_data else i * 17 + 3) & 255
                 for i in range(recipe.data_bytes))
    chunks = junk(recipe.before_junk) + b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += bytes(len(fmt) % 2) + junk(recipe.after_junk)
    chunks += b"data" + struct.pack("<I", recipe.declared_bytes) + data
    return b"RIFF" + struct.pack("<I", len(chunks) + 4) + b"WAVE" + chunks


def profiles():
    result = [{"name": recipe.name, "mode": mode, "streaming_size": 7,
               "file_chunk_size": 9, "max_file_size": 1000}
              for recipe in RECIPES[:18] for mode in ("read", "all")]
    result += [{"name": "pcm16", "mode": "read", "streaming_size": size,
                "file_chunk_size": 9, "max_file_size": 1000}
               for size in (0, -1, 1, 2, 24, 26)]
    result += [{"name": recipe.name, "mode": mode, "streaming_size": 7,
                "file_chunk_size": 9, "max_file_size": maximum}
               for recipe in RECIPES[18:]
               for mode, maximum in (("read", 1000), ("all", 1000), ("all", 0))]
    return result


def materialize(root):
    manifest = {}
    for recipe in RECIPES:
        raw = encode(recipe)
        path = root / (recipe.name + ".wav")
        path.write_bytes(raw)
        manifest[recipe.name] = {"recipe": asdict(recipe), "bytes": len(raw),
                                 "sha256": hashlib.sha256(raw).hexdigest()}
    return manifest
