"""Exercise the genuine Windows source loop with isolated empty/populated saves.

The save-title profiles use the actual visible selected World, real Escape and
pause button1 SDK inputs, and a pre-bootstrap saved inventory achievement. The
success profile independently decodes actual source metadata and region commits
before accepting a fresh title screen. Its session control corrupts/restores the
real lock file and retains the fm.b42 native exception-allocation frontier.
Neither publishes a prepared NPC or claims original whole-frame equivalence.

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
The optional creatures mode runs four further independent sessions for a real
Spider, Slime, Squid or Creeper. Slime uses the declared actual size1 setter
after its raw constructor. First compilation and reuse must follow complete
real ticks and World/HUD returns. Spider's two models, Slime's initially NULL
secondary and actual outer-model alias, Squid's nine parts and Creeper's unused
parts are checked through live ownership and copied N receipts. Unsupported
simulation or presentation children remain failures; no source prefix is repaired.
The creatures-slime and creatures-creeper selectors run their same prepared
profiles alone.
The optional remaining-mobs mode adds three independent real loaded sessions
for Wolf, Zombie and Skeleton. Each keeps actual ticks and raw constructor
entropy; unsupported attack/presentation work remains a retained failure.
Copied N receipts include value-only V/RA work recorded before TH retirement,
so Skeleton must prove six actual Bow batches on both compilation and reuse.
No native full-frame result is asserted until those checks execute.
Other profiles retain fresh statistics. With an absent inventory entry, the
real tutorial can reach the still-missing active notice draw at vu.a108 before
Mob rendering. That fresh-player route is outside the repeat renderer proof.
"""

import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True


def save_title_files(root, destination):
    """Preserve bounded actual inputs/products before the isolated APPDATA ends."""
    result, total = {}, 0
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if len(result) >= 1024 or path.stat().st_size > 1048576:
            raise RuntimeError("Save/title bounded file inventory")
        data = path.read_bytes()
        total += len(data)
        if total > 16 * 1048576:
            raise RuntimeError("Save/title bounded aggregate inventory")
        relative = path.relative_to(root)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        result[relative.as_posix()] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    return result


def save_title_decode(before, entries, after, mode, stdout, process_first):
    # Independent Python NBT/MUTF8/zlib parser, shared with the disk-save
    # fixture, reads actual bytes and has no access to C runtime state.
    parser_path = Path(__file__).resolve().parents[1] / "world/test_source_saving.py"
    spec = importlib.util.spec_from_file_location("save_title_independent_nbt", parser_path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    if (parser.read(entries.parent / "options-input.txt") != b"viewDistance:3\n" or
            [line for line in stdout.splitlines() if line.startswith("WORLD_SAVE_TITLE_OPTIONS ")] !=
            ["WORLD_SAVE_TITLE_OPTIONS view-distance=3 lines=1 bytes=15 source-loaded=1"]):
        raise RuntimeError("Exact declared Tiny input and actual source GameSettings parse")
    rows = [line for line in stdout.splitlines() if line.startswith("WORLD_SAVE_TITLE ")]
    if len(rows) != 1:
        raise RuntimeError("One actual source save/title receipt required")
    pairs = [word.split("=", 1) for word in rows[0].split()[1:]]
    if any(len(pair) != 2 for pair in pairs) or len({key for key, _ in pairs}) != len(pairs):
        raise RuntimeError("Unique save/title receipt fields")
    row = dict(pairs)
    mismatch = mode == "save-title-session"
    exact = dict(mode=mode, **{"escape-sdk": "1", "button-id": "1", "leave-pc": "64" if mismatch else "366",
                              "i": "1", "f": "0" if mismatch else "1", "h": "0" if mismatch else "1",
                              "new-title": "0" if mismatch else "1", "old-renderer-world": "1",
                              "old-effects-world": "1", "original-load-returned": "1"})
    numeric = {"eligible", "time", "size-before", "size-after", "first", "last",
               "entry-frame", "entry-bytes"}
    if (set(row) != set(exact) | numeric or any(row[k] != v for k, v in exact.items()) or
            any(re.fullmatch(r"-?[0-9]+", row[k]) is None for k in numeric)):
        raise RuntimeError("Exact source save/title receipt shape")
    numbers = {k: int(row[k]) for k in numeric}
    # Full save checks the session BEFORE considering any Chunk eligibility.
    # Only the session-failure profile may enter with no remaining Chunk writes.
    if (numbers["eligible"] < 0 or (not mismatch and numbers["eligible"] == 0) or
            numbers["first"] > numbers["last"] or
            not 0 <= numbers["entry-frame"] < 64 or numbers["entry-bytes"] <= 0):
        raise RuntimeError("Actual source save eligibility/clock interval")
    admissions = [line for line in stdout.splitlines()
                  if line.startswith("WORLD_SAVE_TITLE_SESSION_ADMISSION ")]
    if mismatch:
        admitted = re.fullmatch(
            r"WORLD_SAVE_TITLE_SESSION_ADMISSION frames=([0-9]+) frame-ms=([0-9]+) "
            r"time=(-?[0-9]+) eligible=([0-9]+) source-frames=1 same-pause=1 world-time-unchanged=1",
            admissions[0]) if len(admissions) == 1 else None
        if (admitted is None or not 1 <= int(admitted[1]) <= 64 or not 0 <= int(admitted[2]) < 250 or
                int(admitted[3]) != numbers["time"] or int(admitted[4]) != numbers["eligible"]):
            raise RuntimeError("Actual bounded short Pause-frame session admission")
    elif admissions:
        raise RuntimeError("Normal save unexpectedly used session-only admission")
    # Every saved candidate is actual bytes immediately before one real frame.
    # Only the frame that entered button1 selects the full-save entry baseline.
    expected_entries = {f"frame-{i}" for i in range(numbers["entry-frame"] + 1)}
    if not entries.is_dir() or {path.name for path in entries.iterdir()} != expected_entries:
        raise RuntimeError("Exact bounded source entry candidate inventory")
    aggregate = 0
    for phase in entries.iterdir():
        files = [path for path in phase.rglob("*") if path.is_file()]
        total = sum(path.stat().st_size for path in files)
        if (not phase.is_dir() or not 0 < len(files) <= 1024 or total > 16 * 1048576 or
                any(path.stat().st_size > 1048576 for path in files)):
            raise RuntimeError("Bounded actual source entry snapshot")
        aggregate += total
    if aggregate > 64 * 16 * 1048576:
        raise RuntimeError("Bounded aggregate source entry snapshots")
    entry = entries / f"frame-{numbers['entry-frame']}"
    if sum(path.stat().st_size for path in entry.rglob("*") if path.is_file()) != numbers["entry-bytes"]:
        raise RuntimeError("Selected actual source entry byte receipt")
    initial_inventory = {Path(name).as_posix(): value for name, value in snapshot(before).items()}
    old_inventory = {Path(name).as_posix(): value for name, value in snapshot(entry).items()}
    new_inventory = {Path(name).as_posix(): value for name, value in snapshot(after).items()}

    def world_product(name):
        return name in {"alpha/level.dat", "alpha/level.dat_old", "alpha/session.lock"} or name.startswith("alpha/region/")

    protected_initial = {name: digest for name, digest in initial_inventory.items() if not world_product(name)}
    if any(old_inventory.get(name) != digest for name, digest in protected_initial.items()):
        raise RuntimeError("Pause prefix changed protected initial fixture bytes")
    if any(name not in initial_inventory and not world_product(name) for name in old_inventory):
        raise RuntimeError("Pause prefix unexpected new save file")
    protected = {name: digest for name, digest in old_inventory.items()
                 if name != "alpha/session.lock" and
                 (mismatch or not (name in {"alpha/level.dat", "alpha/level.dat_old"} or name.startswith("alpha/region/")))}
    if any(new_inventory.get(name) != digest for name, digest in protected.items()):
        raise RuntimeError("Full save changed protected entry fixture bytes")
    allowed = {"alpha/session.lock"} if mismatch else {"alpha/session.lock", "alpha/level.dat_old"}
    if any(name not in old_inventory and name not in allowed and
           not (not mismatch and name.startswith("alpha/region/")) for name in new_inventory):
        raise RuntimeError("Full save unexpected new save file")
    old_token = parser.read(entry / "alpha/session.lock")
    new_token = parser.read(after / "alpha/session.lock")
    expected_token = bytes([old_token[0] ^ 1]) + old_token[1:] if mismatch and old_token else old_token
    if len(old_token) != 8 or new_token != expected_token:
        raise RuntimeError("Full save exact original session token/restored fault input")

    entry_tree = parser.inflate(parser.read(entry / "alpha/level.dat"), 31)
    if set(entry_tree) != {"Data"} or entry_tree["Data"][0] != 10:
        raise RuntimeError("Pause prefix fresh metadata root")
    entry_data = entry_tree["Data"][1]
    fields = {"RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "SizeOnDisk", "LastPlayed",
              "LevelName", "version", "rainTime", "raining", "thunderTime", "thundering", "Player"}
    initial_size = parser.inflate(parser.read(before / "alpha/level.dat"), 31)["Data"][1]["SizeOnDisk"]
    if (set(entry_data) != fields or entry_data.get("RandomSeed") != [4, 123] or
            entry_data.get("version") != [3, 19132] or entry_data.get("LevelName") != [8, "Alpha world"] or
            entry_data.get("Time", [None])[0] != 4 or not 0 <= entry_data["Time"][1] <= numbers["time"] or
            entry_data.get("SizeOnDisk", [None])[0] != 4 or initial_size[0] != 4 or
            not initial_size[1] <= entry_data["SizeOnDisk"][1] <= numbers["size-before"] or
            entry_data.get("LastPlayed", [None])[0] != 4 or
            not process_first <= entry_data["LastPlayed"][1] <= numbers["first"] or
            entry_data.get("Player", [None])[0] != 10):
        raise RuntimeError("Pause prefix exact fresh metadata fields/source interval")

    def regions(root, initial=None):
        decoded = {}
        for path in sorted((root / "alpha/region").glob("r.*.*.mcr")):
            match = re.fullmatch(r"r\.(-?[0-9]+)\.(-?[0-9]+)\.mcr", path.name)
            if match is None:
                raise RuntimeError("Save/title actual region filename")
            rx, rz = map(int, match.groups())
            raw = parser.read(path)
            if len(raw) < 8192 or len(raw) % 4096:
                raise RuntimeError("Save/title actual region extent")
            used = {0, 1}
            for slot, offset in enumerate(struct.unpack_from(">1024I", raw)):
                if offset == 0:
                    continue
                sector, count = offset >> 8, offset & 255
                if (sector < 2 or count == 0 or (sector + count) * 4096 > len(raw) or
                        used.intersection(range(sector, sector + count))):
                    raise RuntimeError("Save/title complete disjoint region allocation")
                used.update(range(sector, sector + count))
                at = sector * 4096
                size = struct.unpack_from(">i", raw, at)[0]
                if not 1 < size <= count * 4096 - 4 or raw[at + 4] != 2:
                    raise RuntimeError("Save/title selector2 compressed extent")
                tree = parser.inflate(raw[at + 5:at + 4 + size], 15)
                if set(tree) != {"Level"} or tree["Level"][0] != 10:
                    raise RuntimeError("Save/title actual Chunk root")
                level = tree["Level"][1]
                coordinate = (rx * 32 + (slot & 31), rz * 32 + (slot >> 5))
                if level.get("xPos") != [3, coordinate[0]] or level.get("zPos") != [3, coordinate[1]]:
                    raise RuntimeError("Save/title persisted actual Chunk coordinates")
                for key, length in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                                    ("BlockLight", 16384), ("HeightMap", 256)):
                    if level.get(key, [None])[0] != 7 or len(level[key][1]) != length:
                        raise RuntimeError("Save/title actual Chunk array extent")
                stamp = struct.unpack_from(">i", raw, 4096 + slot * 4)[0]
                source_written = initial is not None and initial.get(coordinate) != (tree, stamp)
                for key in ("Entities", "TileEntities"):
                    if level.get(key, [None])[0] != 9:
                        raise RuntimeError("Save/title actual typed Chunk lists")
                    element_type, items = level[key][1]
                    # Source sp.a(DataOutput) forcibly writes Byte1 for empty
                    # lists. Untouched authored records retain Compound10;
                    # every nonempty Entity/TileEntity list is Compound10.
                    expected_type = 1 if source_written and not items else 10
                    if element_type != expected_type:
                        raise RuntimeError("Save/title exact source/original Chunk list type")
                for entity in level["Entities"][1][1]:
                    if entity.get("id", [None])[0] != 8 or entity["id"][1] == "Player":
                        raise RuntimeError("Save/title LocalPlayer Boolean false, named saved Entity")
                decoded[coordinate] = (tree, stamp)
        return decoded
    initial = regions(before)
    old, new = regions(entry, initial), regions(after, initial)
    if (not set(initial) <= set(old) <= set(new) or
            any(chunks[(0, 0)][0]["Level"][1]["Blocks"][1][127] != 57 for chunks in (initial, old, new))):
        raise RuntimeError("Pause/full save stored marker and original Chunk coverage")
    pause_changed = []
    for coordinate, (tree, stamp) in old.items():
        if coordinate in initial and (tree, stamp) == initial[coordinate]:
            continue
        last_update = tree["Level"][1].get("LastUpdate")
        if (last_update is None or last_update[0] != 4 or not 0 <= last_update[1] <= numbers["time"] or
                not process_first // 1000 <= stamp <= numbers["first"] // 1000):
            raise RuntimeError("Pause prefix actual Chunk time and QJ source interval")
        pause_changed.append(coordinate)
    if mismatch:
        if numbers["size-after"] != numbers["size-before"]:
            raise RuntimeError("Session mismatch changed actual entry Info size")
        return {"mode": mode, "receipt": row, "pause_changed_chunks": pause_changed,
                "entry_frame": numbers["entry-frame"],
                "scope": "fm.b42 native exception-allocation frontier; source entry bytes retained; no thrown us/title return"}

    metadata = parser.inflate(parser.read(after / "alpha/level.dat"), 31)
    if set(metadata) != {"Data"} or metadata["Data"][0] != 10:
        raise RuntimeError("Save/title fresh metadata root")
    data = metadata["Data"][1]
    if set(data) != {"RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "SizeOnDisk", "LastPlayed",
                     "LevelName", "version", "rainTime", "raining", "thunderTime", "thundering", "Player"}:
        raise RuntimeError("Save/title exact fresh source Data fields")
    for key, value in {"RandomSeed": [4, 123], "version": [3, 19132],
                       "LevelName": [8, "Alpha world"], "Time": [4, numbers["time"]],
                       "SizeOnDisk": [4, numbers["size-before"]]}.items():
        if data.get(key) != value:
            raise RuntimeError(f"Save/title source metadata {key}")
    if (data.get("LastPlayed", [None])[0] != 4 or
            not numbers["first"] <= data["LastPlayed"][1] <= numbers["last"] or
            data.get("Player", [None])[0] != 10 or numbers["size-after"] < numbers["size-before"]):
        raise RuntimeError("Save/title genuine Player metadata, reached clock and late Info size")
    player = data["Player"][1]
    if (player.get("Pos", [None])[0] != 9 or player["Pos"][1][0] != 6 or
            len(player["Pos"][1][1]) != 3 or player.get("Inventory", [None])[0] != 9 or
            player.get("Health", [None])[0] != 2):
        raise RuntimeError("Save/title actual LocalPlayer semantic output")
    if parser.read(after / "alpha/level.dat_old") != parser.read(entry / "alpha/level.dat"):
        raise RuntimeError("Save/title source metadata rotation backup")

    changed = []
    for coordinate, (tree, stamp) in new.items():
        if coordinate in old and (tree, stamp) == old[coordinate]:
            continue
        if (tree["Level"][1].get("LastUpdate") != [4, numbers["time"]] or
                not numbers["first"] // 1000 <= stamp <= numbers["last"] // 1000):
            raise RuntimeError("Save/title actual late Chunk time and QJ clock")
        changed.append(coordinate)
    if not changed:
        raise RuntimeError("Save/title did not commit a real region Chunk")
    return {"mode": mode, "receipt": row, "metadata": parser.compact(metadata),
            "decoded_chunks": len(new), "changed_chunks": changed,
            "pause_changed_chunks": pause_changed, "entry_frame": numbers["entry-frame"],
            "scope": "native visible UI save/title; no original whole-frame or source reload claim"}


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

CREATURE_PROFILES = {
    "creatures-spider": ("spider", "cn", "yx", "sc", 11, 11, 0, 0, "constructed", "constructed"),
    "creatures-slime": ("slime", "uw", "mj", "no", 4, 1, 0, 1, "null", "own-alias"),
    "creatures-squid": ("squid", "xt", "es", "wn", 9, 0, 0, 0, "null", "null"),
    "creatures-creeper": ("creeper", "gb", "m", "il", 6, 0, 8, 0, "null", "null"),
}


REMAINING_PROFILES = {
    "remaining-wolf": ("wolf", "gi", "we", "o", 11, 0, "not-applicable"),
    "remaining-zombie": ("zombie", "uz", "v", "ej", 7, 2, "not-applicable"),
    "remaining-skeleton": ("skeleton", "fr", "v", "lc", 7, 2, "unpaired"),
}


def remaining_receipts(mode, stdout):
    name, source, renderer, model, parts, unused, displaced = REMAINING_PROFILES[mode]
    rows = stdout.splitlines()
    declared = (f"WORLD_REMAINING_PROFILE profile={name} scopes=independent-loaded "
                "statistics-input=pre-bootstrap-unsent-file open-inventory=1 "
                "canonical=ep.f notice-inactive=1 native-integration-only=1")
    entity = (f"WORLD_REMAINING_ENTITY profile={name} input=native-constructed "
              "pose=actual-player-look distance=6 world-and-chunk=1 same-world-and-classes=1 "
              "constructor-returned=1 constructor-hold-released=1 player-unchanged=1 "
              "world-random-unchanged=1")
    if rows.count(declared) != 1 or rows.count(entity) != 1:
        raise RuntimeError(f"{name} missing genuine declared input")
    frames = [r for r in rows if r.startswith("WORLD_REMAINING_FRAME ")]
    if len(frames) != 1:
        raise RuntimeError(f"{name} missing actual returned-frame receipt")
    pairs = [word.split("=", 1) for word in frames[0].split()[1:]]
    if any(len(pair) != 2 for pair in pairs) or len({k for k, _ in pairs}) != len(pairs):
        raise RuntimeError("remaining frame transport keys")
    values = dict(pairs)
    exact = dict(profile=name, source="px.a:1013", entities="n.a:569", hud="uq.a:2101",
                 mob=source, renderer=renderer, primary=model)
    exact.update({"exact-declared-actor": "1", "compiled-parts": str(parts), "reused-parts": "0",
                  "compiled-faces": str(parts * 6), "compiled-vertices": str(parts * 24),
                  "part-lists": str(parts), "unused-primary-parts": str(unused),
                  "unused-bw-parts": "9", "displaced-graph": displaced,
                  "constructor-ledger-stable": "1", "stable-list-ids": "1",
                  "bow-batches-per-frame": "6" if name == "skeleton" else "0",
                  "bow-raw-words": "48,48,768,768,768,768" if name == "skeleton" else "none",
                  "checked-draw-retirement": "1", "copied-receipts": "1",
                  "tick-return": "1328", "scheduler": "198", "completed-chunks": "361"})
    if any(values.get(key) != value for key, value in exact.items()):
        raise RuntimeError(f"{name} exact graph/returned/Bow receipt")
    keys = ("frames", "entity-frames", "delayed-frames", "undrawn-frames", "shadow-quads",
            "bow-frames", "completed-random-calls", "completed-samples", "display-calls",
            "display-samples", "display-random")
    if set(values) != set(exact) | set(keys) or any(not values[k].isdigit() for k in keys):
        raise RuntimeError("remaining frame numeric closure")
    number = {k: int(values[k]) for k in keys}
    if (not 2 <= number["entity-frames"] <= number["frames"] <= 8 or
            number["frames"] != sum(number[k] for k in ("entity-frames", "delayed-frames", "undrawn-frames")) or
            number["shadow-quads"] < number["entity-frames"] or
            number["bow-frames"] != (number["entity-frames"] if name == "skeleton" else 0) or
            number["completed-random-calls"] < 1 or
            number["completed-samples"] != number["completed-random-calls"] * 28880 or
            number["display-calls"] != number["completed-random-calls"] or
            number["display-samples"] != number["display-calls"] * 1000 or
            number["display-random"] != number["display-calls"] * 6000):
        raise RuntimeError(f"{name} incomplete real timed World/display/Bow work")
    cleanup = [r for r in rows if r.startswith(f"WORLD_SELECTION mode={mode} cleanup=")]
    if len(cleanup) != 1 or re.fullmatch(
            rf"WORLD_SELECTION mode={mode} cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
        raise RuntimeError(f"{name} checked cleanup missing")


def creature_receipts(mode, stdout):
    name, source, renderer, model, primary, secondary, unused, displaced, initial, after = CREATURE_PROFILES[mode]
    parts = primary + secondary
    rows = stdout.splitlines()
    declaration = (f"WORLD_CREATURE_PROFILE profile={name} scopes=independent-loaded "
                   "statistics-input=pre-bootstrap-unsent-file open-inventory=1 "
                   "canonical=ep.f notice-inactive=1 native-integration-only=1")
    entity_input = (f"WORLD_CREATURE_ENTITY profile={name} input=native-constructed "
                    "pose=actual-player-look distance=6 world-and-chunk=1 "
                    "same-world-and-classes=1 constructor-returned=1 "
                    "constructor-hold-released=1 player-unchanged=1 world-random-unchanged=1 "
                    f"slime-size={'1' if name == 'slime' else 'not-applicable'}")
    if rows.count(declaration) != 1 or rows.count(entity_input) != 1:
        raise RuntimeError(f"{name} did not prove its declared saved-player and actual Entity input")
    frames = [row for row in rows if row.startswith("WORLD_CREATURE_FRAME ")]
    if len(frames) != 1:
        raise RuntimeError(f"{name} must expose one exact returned-frame receipt")
    receipt = re.fullmatch(
        rf"WORLD_CREATURE_FRAME profile={name} frames=(?P<frames>[0-9]+) "
        r"entity-frames=(?P<entity_frames>[0-9]+) delayed-frames=(?P<delayed>[0-9]+) "
        r"undrawn-frames=(?P<undrawn>[0-9]+) source=px\.a:1013 entities=n\.a:569 hud=uq\.a:2101 "
        rf"mob={source} renderer={renderer} primary={model} exact-declared-actor=1 "
        r"renderer-map=29 constructor-owners=28 local-player-alias=gs same-world-and-classes=1 "
        rf"primary-parts={primary} secondary-parts={secondary} compiled-parts={parts} reused-parts=0 "
        rf"compiled-faces={parts * 6} compiled-vertices={parts * 24} part-lists={parts} unused-parts={unused} "
        rf"displaced-parts={displaced} constructor-ledger-stable=1 secondary-initial={initial} secondary-after={after} "
        r"part-source-vertices=24 part-list-vertices=36 stable-list-ids=1 "
        r"shadow-quads=(?P<shadows>[0-9]+) checked-draw-retirement=1 copied-receipts=1 "
        r"tick-return=1328 scheduler=198 completed-random-calls=(?P<calls>[0-9]+) "
        r"completed-chunks=361 completed-samples=(?P<samples>[0-9]+) "
        r"display-calls=(?P<displays>[0-9]+) display-samples=(?P<display_samples>[0-9]+) "
        r"display-random=(?P<draws>[0-9]+)", frames[0])
    if receipt is None:
        raise RuntimeError(f"{name} did not prove its actual renderer/model graph and returned frames")
    values = {key: int(value) for key, value in receipt.groupdict().items()}
    if (not 2 <= values["entity_frames"] <= values["frames"] <= 8 or
            values["frames"] != values["entity_frames"] + values["delayed"] + values["undrawn"] or
            values["shadows"] < values["entity_frames"] or values["calls"] < 1 or
            values["samples"] != values["calls"] * 28880 or
            values["displays"] != values["calls"] or
            values["display_samples"] != values["displays"] * 1000 or
            values["draws"] != values["displays"] * 6000):
        raise RuntimeError(f"{name} frames lack complete real tick/random/display work")
    cleanup = [row for row in rows if row.startswith(f"WORLD_SELECTION mode={mode} cleanup=")]
    if len(cleanup) != 1 or re.fullmatch(
            rf"WORLD_SELECTION mode={mode} cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
        raise RuntimeError(f"{name} did not complete checked native retirement")


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
                                 "load-terminal", "load-partial", "repeat", "load-light", "animals", "creatures",
                                 "creatures-slime", "creatures-creeper", "remaining-mobs",
                                 "remaining-wolf", "remaining-zombie", "remaining-skeleton",
                                 "save-title", "save-title-session"))
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
             for expanded in (ANIMAL_PROFILES if mode == "animals" else
                              CREATURE_PROFILES if mode == "creatures" else
                              REMAINING_PROFILES if mode == "remaining-mobs" else (mode,))]
    for mode in modes:
        animal = mode in ANIMAL_PROFILES
        creature = mode in CREATURE_PROFILES
        remaining = mode in REMAINING_PROFILES
        save_title = mode in ("save-title", "save-title-session")
        evidence = Path(tempfile.mkdtemp(prefix=mode + "-", dir=artifacts)) if save_title else None
        with tempfile.TemporaryDirectory(prefix="mc-rq-loop-") as work:
            appdata = Path(work)
            game = appdata / ".minecraft"
            if mode == "repeat" or animal or creature or remaining or save_title:
                repeat_statistics(game, assets)
            if save_title:
                # Declared pre-bootstrap input; source kv.a() parses the real file.
                options_input = b"viewDistance:3\n"
                (game / "options.txt").write_bytes(options_input)
                (evidence / "options-input.txt").write_bytes(options_input)
            saves = game / "saves"
            saves.mkdir(parents=True)
            (saves / "preserve-me.bin").write_bytes(b"unrelated save-directory file\x00\xff")
            if mode == "large":
                worlds = [(f"world{i:02d}", f"Scroll world {i:02d}", 1304000000000, 19132)
                          for i in range(16)]
            elif mode in ("populated", "double", "load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal or creature or remaining or save_title:
                worlds = [
                    ("older", "Old world", 1303000000000, 19132),
                    ("bravo", "", 1304000000000, 19132),
                    ("alpha", "Alpha world", 1304000000000,
                     19132 if mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal or creature or remaining or save_title else 0),
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
            if mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal or creature or remaining or save_title:
                regions = saves / "alpha" / "region"
                regions.mkdir()
                saved_regions(regions, generate=mode == "generate")
            before = snapshot(saves)
            if save_title:
                before_files = save_title_files(saves, evidence / "before")
            environment = os.environ.copy()
            environment["APPDATA"] = str(appdata)
            if save_title:
                environment["MC_WORLD_SAVE_TITLE_SESSION"] = str(saves / "alpha/session.lock")
                environment["MC_WORLD_SAVE_TITLE_SAVES"] = str(saves)
                environment["MC_WORLD_SAVE_TITLE_ENTRIES"] = str(evidence / "entries")
            capture = artifacts / (mode + ".png")
            process_first = time.time_ns() // 1000000
            try:
                result = subprocess.run([str(executable), str(assets), mode, str(capture)],
                                        env=environment,
                                        timeout=args.timeout if args.timeout is not None else
                                        (600 if mode == "generate" else 240 if animal or creature or remaining or save_title else 110), check=False,
                                        capture_output=True, text=True)
            except subprocess.TimeoutExpired as error:
                if save_title:
                    for name, value in (("stdout.txt", error.stdout), ("stderr.txt", error.stderr)):
                        text = value.decode("utf-8", "replace") if isinstance(value, bytes) else value or ""
                        (evidence / name).write_text(text, encoding="utf-8")
                    (evidence / "execution.json").write_text(json.dumps({"mode": mode, "timeout": True,
                        "before": before_files}, indent=2) + "\n", encoding="utf-8")
                raise
            finally:
                if save_title:
                    after_files = save_title_files(saves, evidence / "after")
            if save_title:
                (evidence / "stdout.txt").write_text(result.stdout, encoding="utf-8")
                (evidence / "stderr.txt").write_text(result.stderr, encoding="utf-8")
                (evidence / "execution.json").write_text(json.dumps({"mode": mode, "exit_code": result.returncode,
                    "before": before_files, "after": after_files,
                    "process_first_millis": process_first}, indent=2) + "\n", encoding="utf-8")
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
            if creature:
                creature_receipts(mode, result.stdout)
            if remaining:
                remaining_receipts(mode, result.stdout)
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
            if save_title:
                decoded = save_title_decode(evidence / "before", evidence / "entries", evidence / "after",
                                            mode, result.stdout, process_first)
                cleanup = [line for line in result.stdout.splitlines()
                           if line.startswith(f"WORLD_SELECTION mode={mode} cleanup=")]
                if len(cleanup) != 1 or re.fullmatch(
                        rf"WORLD_SELECTION mode={mode} cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
                    raise RuntimeError("Save/title native terminal ownership cleanup receipt")
                (evidence / "report.json").write_text(json.dumps(decoded, indent=2) + "\n", encoding="utf-8")
            elif mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or animal or creature or remaining:
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
    if "creatures" in requested:
        print("Native creatures: four independent loaded scopes, real Spider/Slime/Squid/Creeper "
              "constructor/publication, declared Slime size1, full tick/World/HUD compile and reuse, "
              "actual model roles and constructor ownership, copied actor receipts and checked cleanup passed; "
              "no original-frame parity credit")
    if "remaining-mobs" in requested:
        print("Native remaining Mobs: three independent real Wolf/Zombie/Skeleton loaded scopes, "
              "full tick/World/HUD first compilation/reuse, actual V/RA/Bow receipts and checked "
              "cleanup passed; no original whole-frame parity credit")
    if all(mode in ("save-title", "save-title-session") for mode in modes):
        print("Native visible save/title: real Escape/button1, actual source save and independent "
              "metadata/region readback, fresh title, session allocation frontier and retained-load "
              "terminal cleanup passed; no original whole-frame or source reload claim")
    else:
        print("World-selection loop: original-receiver input, held entry, rows, Cancel, "
              "large-list scrollbar drag, real-clock double-click failure order, "
              "World-load frontier, captures, save preservation and cleanup passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
