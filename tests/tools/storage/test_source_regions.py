"""Authored source qj transport controls; no source oracle or cache image stand-in."""
import argparse
import gzip
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib


def named(kind, name, body):
    raw = name.encode("ascii")
    return bytes([kind]) + struct.pack(">H", len(raw)) + raw + body


def chunk(x, block=254):
    fields = [named(3, "xPos", struct.pack(">i", x)), named(3, "zPos", b"\0" * 4)]
    for name, length in [("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                         ("BlockLight", 16384), ("HeightMap", 256)]:
        data = (bytes([block]) + bytes(length - 1)) if name == "Blocks" else bytes(length)
        fields.append(named(7, name, struct.pack(">i", length) + data))
    fields.extend([named(1, "TerrainPopulated", b"\1"),
                   named(9, "Entities", b"\x0a" + bytes(4)),
                   named(9, "TileEntities", b"\x0a" + bytes(4))])
    return named(10, "", named(10, "Level", b"".join(fields) + b"\0") + b"\0")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--work-dir", required=True)
    args = parser.parse_args()
    work = Path(args.work_dir).resolve()
    work.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="source-regions-", dir=work))
    region = root / "region"
    region.mkdir()
    image = bytearray(6 * 4096)
    payloads = [(2, zlib.compress(chunk(0))), (1, gzip.compress(chunk(1), mtime=0)),
                (3, b"unknown"), (2, b"invalid-length")]
    for slot, (selector, compressed) in enumerate(payloads):
        sector = slot + 2
        struct.pack_into(">I", image, slot * 4, (sector << 8) | 1)
        if slot == 1:
            compressed = compressed[:-8] + bytes([compressed[-8] ^ 1]) + compressed[-7:]
        struct.pack_into(">I", image, sector * 4096, 4097 if slot == 3 else len(compressed) + 1)
        image[sector * 4096 + 4] = selector
        image[sector * 4096 + 5:sector * 4096 + 5 + len(compressed)] = compressed
    (region / "r.0.0.mcr").write_bytes(image)
    replacement = zlib.compress(chunk(0, 253))
    (root / "replacement-record.bin").write_bytes(struct.pack(">I", len(replacement) + 1) + b"\2" + replacement)
    subprocess.run([str(Path(args.executable).resolve()), str(root)], check=True)


if __name__ == "__main__":
    main()
