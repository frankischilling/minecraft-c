"""Exercise the genuine Windows source loop with isolated empty/populated saves."""

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib


def string(value):
    data = value.encode("ascii")
    return struct.pack(">H", len(data)) + data


def tag(kind, key, value):
    return bytes([kind]) + string(key) + value


def metadata(display, played, version, partial=False):
    data = tag(8, "LevelName", string(display))
    data += tag(4, "LastPlayed", struct.pack(">q", played))
    data += tag(4, "SizeOnDisk", struct.pack(">q", 3145728))
    data += tag(3, "version", struct.pack(">i", version))
    data += tag(4, "RandomSeed", struct.pack(">q", 123))
    for name, value in (("SpawnX", 0), ("SpawnY", 64), ("SpawnZ", 0)):
        data += tag(3, name, struct.pack(">i", value))
    if partial:
        # Authored saved input: the real Entity reader reaches Pos[0] only
        # after Motion and after the client acquired its World/Player graph.
        player = tag(3, "Dimension", struct.pack(">i", 0))
        player += tag(9, "Motion", b"\x06" + struct.pack(">i3d", 3, .25, -.5, .75))
        player += tag(9, "Pos", b"\x06" + struct.pack(">i", 0))
        player += tag(9, "Rotation", b"\x05" + struct.pack(">i2f", 2, 0, 0))
        data += tag(10, "Player", player + b"\x00")
    return b"\x0a\x00\x00" + tag(10, "Data", data + b"\x00") + b"\x00"


def chunk_record(chunk_x, chunk_z):
    blocks = bytearray(32768)
    skylight = bytearray(16384)
    heights = bytearray([64] * 256)
    for x in range(16):
        for z in range(16):
            base = (x << 11) | (z << 7)
            blocks[base] = 7
            blocks[base + 1:base + 60] = bytes([1] * 59)
            blocks[base + 60:base + 63] = bytes([3] * 3)
            blocks[base + 63] = 2
            skylight[(base + 64) // 2:(base + 128) // 2] = bytes([255] * 32)
    # A valid high marker distinguishes the actual stored Chunk from generation.
    if (chunk_x, chunk_z) == (0, 0):
        blocks[127] = 57
        skylight[127 // 2] &= 15
        heights[0] = 128
    level = tag(3, "xPos", struct.pack(">i", chunk_x))
    level += tag(3, "zPos", struct.pack(">i", chunk_z))
    level += tag(4, "LastUpdate", struct.pack(">q", 0))
    for name, values in (("Blocks", blocks), ("Data", bytes(16384)),
                         ("SkyLight", skylight), ("BlockLight", bytes(16384)),
                         ("HeightMap", heights)):
        level += tag(7, name, struct.pack(">i", len(values)) + values)
    level += tag(1, "TerrainPopulated", b"\x01")
    level += tag(9, "Entities", b"\x0a\x00\x00\x00\x00")
    level += tag(9, "TileEntities", b"\x0a\x00\x00\x00\x00")
    raw = b"\x0a\x00\x00" + tag(10, "Level", level + b"\x00") + b"\x00"
    compressed = zlib.compress(raw)
    record = struct.pack(">I", len(compressed) + 1) + b"\x02" + compressed
    sectors = (len(record) + 4095) // 4096
    return record + bytes(sectors * 4096 - len(record)), sectors


def saved_regions(directory, generate):
    # The default UI profile reads all 289 existing Chunks. The optional
    # generation profile retains one saved marker Chunk and generates its
    # surroundings through the same source path.
    coordinates = [(0, 0)] if generate else [(x, z) for x in range(-8, 9)
                                            for z in range(-8, 9)]
    regions = {}
    for x, z in coordinates:
        body, sectors = chunk_record(x, z)
        region = regions.setdefault((x >> 5, z >> 5), bytearray(8192))
        offset = len(region) // 4096
        index = (x & 31) + (z & 31) * 32
        struct.pack_into(">I", region, index * 4, (offset << 8) | sectors)
        region.extend(body)
    for (x, z), data in regions.items():
        (directory / f"r.{x}.{z}.mcr").write_bytes(data)


def snapshot(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*") if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--mode", action="append",
                        choices=("empty", "populated", "large", "double", "load", "generate",
                                 "load-terminal", "load-partial", "repeat"))
    parser.add_argument("--timeout", type=float)
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This native APPDATA isolation profile requires Windows")
    executable = args.executable.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    artifacts = args.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    resources_before = snapshot(assets)
    for mode in args.mode or ("empty", "populated", "large", "double", "load", "load-terminal",
                             "load-partial", "repeat"):
        with tempfile.TemporaryDirectory(prefix="mc-rq-loop-") as work:
            appdata = Path(work)
            game = appdata / ".minecraft"
            saves = game / "saves"
            saves.mkdir(parents=True)
            (saves / "preserve-me.bin").write_bytes(b"unrelated save-directory file\x00\xff")
            if mode == "large":
                worlds = [(f"world{i:02d}", f"Scroll world {i:02d}", 1304000000000, 19132)
                          for i in range(16)]
            elif mode in ("populated", "double", "load", "generate", "load-terminal", "load-partial", "repeat"):
                worlds = [
                    ("older", "Old world", 1303000000000, 19132),
                    ("bravo", "", 1304000000000, 19132),
                    ("alpha", "Alpha world", 1304000000000,
                     19132 if mode in ("load", "generate", "load-terminal", "load-partial", "repeat") else 0),
                ]
            else:
                worlds = []
            for folder, display, played, version in worlds:
                directory = saves / folder
                directory.mkdir()
                filename = "level.dat_old" if folder == "bravo" else "level.dat"
                (directory / filename).write_bytes(
                    gzip.compress(metadata(display, played, version,
                                           partial=mode == "load-partial" and folder == "alpha"),
                                  mtime=0))
                (directory / "preserve-chunk.dat").write_bytes(b"authored untouched chunk")
            if mode in ("load", "generate", "load-terminal", "load-partial", "repeat"):
                regions = saves / "alpha" / "region"
                regions.mkdir()
                saved_regions(regions, generate=mode == "generate")
            before = snapshot(saves)
            environment = os.environ.copy()
            environment["APPDATA"] = str(appdata)
            capture = artifacts / (mode + ".png")
            result = subprocess.run([str(executable), str(assets), mode, str(capture)],
                                    env=environment,
                                    timeout=args.timeout if args.timeout is not None else
                                    (600 if mode == "generate" else 110), check=False,
                                    capture_output=True, text=True)
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            if result.returncode:
                print(f"World-selection native {mode} exited {result.returncode} "
                      f"(0x{result.returncode & 0xffffffff:08x})", file=sys.stderr)
                return result.returncode
            if mode in ("load", "generate", "load-terminal"):
                fixture = Path(__file__).resolve().parents[2] / "reference/client/renderer_focus/finite.json"
                expected = json.loads(fixture.read_text(encoding="utf-8"))["rows"]
                observed = [line for line in result.stdout.splitlines() if line.startswith("FOCUS ")]
                if observed != expected:
                    raise RuntimeError("Actual SDK focus segment differs from 72 original PX histories")
            if mode == "repeat":
                if "WORLD_FOCUS hidden-inactive=1 restored-active=1 component-preserved=1 source-grab-preserved=1" not in result.stdout:
                    raise RuntimeError("Focused repeat fixture did not restore actual focus without source mutation")
                receipt = re.search(
                    r"^WORLD_FRAME first-returned=1 next-unpaused-pc=(80|1216) scheduler=(0|80) "
                    r"random=(0) chunks=(0) samples=(0) "
                    r"helper=([0-9]+) mob=([a-z]+) constructors=([0-9]+) "
                    r"predicates=([0-9]+) rejected=([0-9]+) published=([0-9]+) completed-draws-retired=1 "
                    r"stale-hud-cleared=1 completed-random-calls=([0-9]+) "
                    r"completed-chunks=([0-9]+) completed-samples=([0-9]+) "
                    r"display-calls=([0-9]+) display-samples=([0-9]+) display-random=([0-9]+) "
                    r"active=([a-z]+) active-tick=([0-9]+)$", result.stdout, re.MULTILINE)
                if receipt is None:
                    raise RuntimeError("Real repeated frame did not expose the expected source dependency")
                prefix = tuple(int(receipt[i]) for i in range(1, 6))
                if prefix == (1216, 0, 0, 0, 0):
                    calls, chunks, samples, displays, display_samples, draws = (
                        int(receipt[i]) for i in range(12, 18))
                    if (calls < 1 or chunks != 361 or samples != calls * 28880 or
                            displays != calls or display_samples != displays * 1000 or
                            draws != displays * 6000 or receipt[18] not in
                            ("cn", "uz", "fr", "gb", "uw", "wh", "dl", "bx", "ww", "gi", "xt") or
                            int(receipt[19]) != 121):
                        raise RuntimeError("Active Mob tick did not follow complete random Chunk/display calls")
                elif not (prefix == (80, 80, 0, 0, 0) and int(receipt[6]) == 56 and
                          receipt[18] == "none" and int(receipt[19]) == 0):
                    raise RuntimeError("Repeated frame did not reach the genuine active Mob tick or jockey mount")
                if (receipt[7] not in ("none", "cn", "uz", "fr", "gb", "uw", "wh", "dl", "bx", "ww", "gi", "xt") or
                        int(receipt[8]) != int(receipt[9]) or int(receipt[10]) > int(receipt[9]) or
                        (receipt[7] == "none") != (int(receipt[8]) == 0)):
                    raise RuntimeError("Repeated frame skipped or misreported its actual Mob predicates")
            after = snapshot(saves)
            if mode in ("load", "generate", "load-terminal", "load-partial", "repeat"):
                if any(after.get(name) != digest for name, digest in before.items()):
                    raise RuntimeError("World loading changed pre-existing fixture bytes")
                if not (saves / "alpha" / "session.lock").is_file():
                    raise RuntimeError("Actual source SaveHandler did not create its session lock")
            elif after != before:
                raise RuntimeError("World-selection loop modified save files or created a lock")
            data = capture.read_bytes()
            if data[:8] != b"\x89PNG\r\n\x1a\n" or len(data) < 4096:
                raise RuntimeError("World-selection capture is missing or incomplete")
            if struct.unpack(">II", data[16:24]) != (854, 480):
                raise RuntimeError("World-selection capture has unexpected native dimensions")
            if not (game / "stats").is_dir() or not (game / "options.txt").is_file():
                raise RuntimeError("The genuine native default game directory was not used")
    if snapshot(assets) != resources_before:
        raise RuntimeError("World-selection loop modified imported source resources")
    print("World-selection loop: original-receiver input, held entry, rows, Cancel, "
          "large-list scrollbar drag, real-clock double-click failure order, "
          "World-load frontier, captures, save preservation and cleanup passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
