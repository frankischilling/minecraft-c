"""Exercise the genuine Windows source loop with isolated empty/populated saves.

The repeat profile declares an existing player whose saved openInventory
achievement is already present. Its valid pre-bootstrap statistics file uses
real imported GUIDs and the source checksum; the actual client loads it.
After a fully returned loaded frame, repeat also declares one genuine native
Sheep constructor/position/publication in the actual Player view. Its saved
Chunks remain Entity-empty: serialized Mob loading is not qualified. The
renderer check selects that exact published Sheep in copied N receipts after
real timed ticking, then requires returned World/HUD frames and genuine model
compilation followed by reuse. Exact initial delayed/undrawn frames consume
the observation budget without counting as Sheep draws. Real elapsed time
lets the actual Timer choose the next ticks. Natural spawning need not supply
a visible Mob within a deadline.
Part receipts retain the 24 original TZ vertex calls per cube. The ordinary
NW.b=true expansion records 36 triangle vertices in each six-face Display list.
The optional animals mode runs three independent loaded invocations, one real
Pig, Cow or Chicken per invocation. Each uses the same declared saved-player
statistic and genuine native constructor/position/publication input, then
requires first compilation and reuse in full World/HUD frames with real ticks.
The existing N first-Entity receipt can identify each actor without adding a
production trace or obscuring a middle actor in a three-animal traversal.
This is native integration evidence; it makes no original whole-frame claim.
Other profiles retain fresh statistics. With an absent inventory entry, the
real tutorial can reach the still-missing active notice draw at vu.a108 before
Mob rendering. That fresh-player route is outside the repeat renderer proof.
"""

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


def repeat_statistics(game, assets):
    # Saved player input, before construction: CJ reads this unsent file
    # synchronously into the real xi Maps. No notice or runtime Map is edited.
    mapping = assets / "jar" / "achievement" / "map.txt"
    keys = [line.split(",", 1)[1] for line in mapping.read_text(encoding="ascii").splitlines()
            if line.startswith("5242880,")]
    if len(keys) != 1 or re.fullmatch(r"[0-9a-f]{1,32}", keys[0]) is None:
        raise RuntimeError("Imported openInventory GUID is missing or ambiguous")
    payload = ("local" + keys[0] + ",1,").encode("ascii")
    # hf formats positive MD5 as unpadded lowercase BigInteger hexadecimal.
    checksum = format(int.from_bytes(hashlib.md5(payload).digest(), "big"), "x")
    stats = game / "stats"
    stats.mkdir(parents=True)
    (stats / "stats_selection control_unsent.dat").write_text(
        json.dumps({"stats-change": [{"5242880": 1}], "checksum": checksum}) + "\n",
        encoding="ascii", newline="\n")


def snapshot(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*") if path.is_file()}


ANIMAL_PROFILES = {
    "animals-pig": ("pig", "wh", "me", "eh", 6, 6),
    "animals-cow": ("cow", "bx", "va", "hh", 9, 0),
    "animals-chicken": ("chicken", "ww", "ip", "td", 8, 0),
}


def animal_receipts(mode, stdout):
    name, source, renderer, model, parts, unused = ANIMAL_PROFILES[mode]
    rows = stdout.splitlines()
    declaration = (f"WORLD_ANIMAL_PROFILE profile={name} scopes=independent-loaded "
                   "statistics-input=pre-bootstrap-unsent-file open-inventory=1 "
                   "canonical=ep.f notice-inactive=1 native-integration-only=1")
    entity_input = (f"WORLD_ANIMAL_ENTITY profile={name} input=native-constructed "
                    "pose=actual-player-look distance=6 world-and-chunk=1 "
                    "same-world-and-classes=1 constructor-returned=1 "
                    "constructor-hold-released=1 player-unchanged=1 world-random-unchanged=1")
    if rows.count(declaration) != 1 or rows.count(entity_input) != 1:
        raise RuntimeError(f"{name} did not prove its declared saved-player and actual Entity input")
    frames = [row for row in rows if row.startswith("WORLD_ANIMAL_FRAME ")]
    if len(frames) != 1:
        raise RuntimeError(f"{name} must expose one exact returned-frame receipt")
    receipt = re.fullmatch(
        rf"WORLD_ANIMAL_FRAME profile={name} frames=(?P<frames>[0-9]+) "
        r"entity-frames=(?P<entity_frames>[0-9]+) delayed-frames=(?P<delayed>[0-9]+) "
        r"undrawn-frames=(?P<undrawn>[0-9]+) source=px\.a:1013 entities=n\.a:569 hud=uq\.a:2101 "
        rf"mob={source} renderer={renderer} primary={model} exact-declared-actor=1 "
        r"renderer-map=29 constructor-owners=28 local-player-alias=gs same-world-and-classes=1 "
        rf"compiled-parts={parts} reused-parts=0 compiled-faces={parts * 6} "
        rf"compiled-vertices={parts * 24} part-lists={parts} unused-secondary-parts={unused} "
        r"part-source-vertices=24 part-list-vertices=36 stable-list-ids=1 "
        r"shadow-quads=(?P<shadows>[0-9]+) checked-draw-retirement=1 copied-receipts=1 "
        r"tick-return=1328 scheduler=198 completed-random-calls=(?P<calls>[0-9]+) "
        r"completed-chunks=361 completed-samples=(?P<samples>[0-9]+) "
        r"display-calls=(?P<displays>[0-9]+) display-samples=(?P<display_samples>[0-9]+) "
        r"display-random=(?P<draws>[0-9]+)", frames[0])
    if receipt is None:
        raise RuntimeError(f"{name} did not prove its exact canonical renderer/model and returned frames")
    values = {key: int(value) for key, value in receipt.groupdict().items()}
    if (not 2 <= values["entity_frames"] <= values["frames"] <= 8 or
            values["frames"] != values["entity_frames"] + values["delayed"] + values["undrawn"] or
            values["shadows"] < values["entity_frames"] or values["calls"] < 1 or
            values["samples"] != values["calls"] * 28880 or
            values["displays"] != values["calls"] or
            values["display_samples"] != values["displays"] * 1000 or
            values["draws"] != values["displays"] * 6000):
        raise RuntimeError(f"{name} returned frames lack complete real tick/random/display work")
    cleanup = [row for row in rows if row.startswith(f"WORLD_SELECTION mode={mode} cleanup=")]
    if len(cleanup) != 1 or re.fullmatch(
            rf"WORLD_SELECTION mode={mode} cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
        raise RuntimeError(f"{name} did not complete checked native retirement")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--mode", action="append",
                        choices=("empty", "populated", "large", "double", "load", "generate",
                                 "load-terminal", "load-partial", "repeat", "load-light", "animals"))
    parser.add_argument("--timeout", type=float)
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This native APPDATA isolation profile requires Windows")
    executable = args.executable.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    artifacts = args.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    resources_before = snapshot(assets)
    requested = args.mode or ("empty", "populated", "large", "double", "load", "load-terminal",
                              "load-partial", "repeat", "load-light")
    modes = [expanded for mode in requested
             for expanded in (ANIMAL_PROFILES if mode == "animals" else (mode,))]
    for mode in modes:
        animal = mode in ANIMAL_PROFILES
        with tempfile.TemporaryDirectory(prefix="mc-rq-loop-") as work:
            appdata = Path(work)
            game = appdata / ".minecraft"
            if mode == "repeat" or animal:
                repeat_statistics(game, assets)
            saves = game / "saves"
            saves.mkdir(parents=True)
            (saves / "preserve-me.bin").write_bytes(b"unrelated save-directory file\x00\xff")
            if mode == "large":
                worlds = [(f"world{i:02d}", f"Scroll world {i:02d}", 1304000000000, 19132)
                          for i in range(16)]
            elif mode in ("populated", "double", "load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal:
                worlds = [
                    ("older", "Old world", 1303000000000, 19132),
                    ("bravo", "", 1304000000000, 19132),
                    ("alpha", "Alpha world", 1304000000000,
                     19132 if mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal else 0),
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
            if mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal:
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
                                    (600 if mode == "generate" else 240 if animal else 110), check=False,
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
            if animal:
                animal_receipts(mode, result.stdout)
            if mode == "repeat":
                declaration = ("WORLD_REPEAT profile=existing-inventory-player "
                               "statistics-input=pre-bootstrap-unsent-file open-inventory=1 "
                               "canonical=ep.f notice-inactive=1")
                if result.stdout.splitlines().count(declaration) != 1:
                    raise RuntimeError("Repeat renderer profile did not prove its declared existing-player input")
                entity_input = ("WORLD_REPEAT_ENTITY input=native-constructed-sheep "
                                "pose=actual-player-look distance=6 world-and-chunk=1 "
                                "same-world-and-classes=1 constructor-returned=1 "
                                "constructor-hold-released=1 player-unchanged=1 "
                                "world-random-unchanged=1")
                if result.stdout.splitlines().count(entity_input) != 1:
                    raise RuntimeError("Repeat profile did not prove its declared real Sheep input")
                if "WORLD_FOCUS hidden-inactive=1 restored-active=1 component-preserved=1 source-grab-preserved=1" not in result.stdout:
                    raise RuntimeError("Focused repeat fixture did not restore actual focus without source mutation")
                rows = [line for line in result.stdout.splitlines()
                        if line.startswith("WORLD_FRAME ")]
                if len(rows) != 1:
                    raise RuntimeError("Repeated frame must expose one exact returned Sheep receipt")
                receipt = re.fullmatch(
                    r"WORLD_FRAME first-returned=1 continuation=returned-sheep "
                    r"frames=(?P<frames>[0-9]+) entity-frames=(?P<entity_frames>[0-9]+) "
                    r"delayed-frames=(?P<delayed>[0-9]+) undrawn-frames=(?P<undrawn>[0-9]+) "
                    r"source=px\.a:1013 entities=n\.a:569 hud=uq\.a:2101 "
                    r"mob=dl renderer=xy primary=mw secondary=ea exact-declared-actor=1 "
                    r"renderer-map=29 constructor-owners=28 local-player-alias=gs "
                    r"same-world-and-classes=1 compiled-parts=12 reused-parts=0 "
                    r"compiled-faces=72 compiled-vertices=288 part-lists=12 part-source-vertices=24 part-list-vertices=36 stable-list-ids=1 "
                    r"shadow-quads=(?P<shadows>[0-9]+) checked-draw-retirement=1 copied-receipts=1 "
                    r"tick-return=1328 scheduler=198 "
                    r"completed-random-calls=(?P<calls>[0-9]+) completed-chunks=361 "
                    r"completed-samples=(?P<samples>[0-9]+) display-calls=(?P<displays>[0-9]+) "
                    r"display-samples=(?P<display_samples>[0-9]+) "
                    r"display-random=(?P<draws>[0-9]+)", rows[0])
                if receipt is None:
                    raise RuntimeError("Repeated frame did not prove its exact returned Sheep/World/HUD path")
                values = {key: int(value) for key, value in receipt.groupdict().items()}
                if (not 2 <= values["entity_frames"] <= values["frames"] <= 8 or
                        values["frames"] != values["entity_frames"] + values["delayed"] + values["undrawn"] or
                        values["shadows"] < values["entity_frames"] or values["calls"] < 1 or
                        values["samples"] != values["calls"] * 28880 or
                        values["displays"] != values["calls"] or
                        values["display_samples"] != values["displays"] * 1000 or
                        values["draws"] != values["displays"] * 6000):
                    raise RuntimeError("Returned Sheep frames did not follow complete real tick/random/display calls")
            if mode == "load-light":
                expected = ("WORLD_LIGHT mode=load-light declared-paused-entries=2 completed-frames=2 "
                            "queue=500,1,0 same-queue=1 oldest-survives=1 block-light=15,0,0 "
                            "world-time-unchanged=1 world-random-unchanged=1 normal-world-access=1 "
                            "source-batch=499")
                observed = [line for line in result.stdout.splitlines()
                            if line.startswith("WORLD_LIGHT ")]
                if observed != [expected]:
                    raise RuntimeError("Declared paused caller did not retain the real 499-record light batch")
            after = snapshot(saves)
            if mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal:
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
    if "animals" in requested:
        print("Native animals: three independent loaded scopes, real Pig/Cow/Chicken "
              "constructor/publication, full tick/World/HUD compile and reuse, "
              "copied actor receipts and checked cleanup passed; no original-frame parity credit")
    print("World-selection loop: original-receiver input, held entry, rows, Cancel, "
          "large-list scrollbar drag, real-clock double-click failure order, "
          "World-load frontier, captures, save preservation and cleanup passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
