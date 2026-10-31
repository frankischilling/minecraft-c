"""Construct real client Worlds from copied source-handler metadata/idcounts files."""

import argparse
import gzip
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
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


def snapshot(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*") if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="mc-source-world-") as work:
        root = Path(work)
        folders = ("primary", "old-only", "bad-primary", "bad-old", "bad-ids",
                   "limited-ids", "nether", "missing")
        for name in folders:
            folder = root / name
            (folder / "data").mkdir(parents=True)
            (folder / "preserve.bin").write_bytes(b"unrelated source World data\x00\xff")
            if name == "missing":
                continue
            data = metadata(version=17 if name == "old-only" else 19132, nether=name == "nether")
            filename = "level.dat_old" if name in ("old-only", "bad-old") else "level.dat"
            if name in ("bad-primary", "bad-old"):
                # Valid gzip, invalid non-Compound root: actual as.a finally29
                # runs before the unresolved fm.c print51/105 branch.
                data = b"\x01\x00\x00\x07"
            (folder / filename).write_bytes(gzip.compress(data, mtime=0))
            if name == "bad-primary":
                (folder / "level.dat_old").write_bytes(gzip.compress(metadata(), mtime=0))
            if name in ("primary", "limited-ids"):
                (folder / "data/idcounts.dat").write_bytes(raw_ids())
            elif name == "bad-ids":
                (folder / "data/idcounts.dat").write_bytes(b"\x01\x00\x00\x07")
            if name == "primary":
                (folder / "region").mkdir()
                (folder / "region/r.0.0.mcr").write_bytes(malformed_region())
        before = snapshot(root)
        result = subprocess.run([str(executable), str(root)], timeout=150, check=False)
        if result.returncode:
            return result.returncode
        after = snapshot(root)
        for name, digest in before.items():
            if after.get(name) != digest:
                raise RuntimeError("Source World construction modified an input: " + name)
        for name in after.keys() - before.keys():
            path = Path(name)
            if path.name == "session.lock" and len(path.parts) == 2:
                if len((root / path).read_bytes()) != 8:
                    raise RuntimeError("Source session.lock does not contain its long token")
            elif not (path.parts[0] == "missing" and path.suffix == ".mcr"):
                raise RuntimeError("Unexpected source-construction file: " + name)
        for folder in folders:
            if not (root / folder / "session.lock").is_file():
                raise RuntimeError("Actual source SaveHandler did not write session.lock")
    print("Source World constructor: READY existing Worlds, nullable metadata, source errors, "
          "String identity, pending HC and checked child cleanup passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
