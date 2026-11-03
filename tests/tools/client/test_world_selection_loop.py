"""Exercise the genuine Windows source loop with isolated empty/populated saves.

The save-title profiles use the actual visible selected World, real Escape and
pause button1 SDK inputs, and a pre-bootstrap saved inventory achievement. The
success profile independently decodes actual source metadata and region commits
before accepting a fresh title screen. Its session control corrupts/restores the
real lock file and retains the fm.b42 native exception-allocation frontier.
Neither publishes a prepared NPC or claims original whole-frame equivalence.

The optional load-cactus profile authors Sand12 at (3,64,3) and Cactus81 at
(3,65,3) in the saved (0,0) Chunk before bootstrap. Its saved Player stands at
(3.5,64,.5), faces +Z and uses the source's float eye offset in saved Pos.y.
The wrapper independently decodes that pose; native checks require the same
Player/camera pose, actual Cactus pick and returned selection outline. Actual selected-World
acquisition, returned World/HUD frames, the 60-vertex opaque section, immutable
list reuse and a World light notification followed by recompile are checked.
This is native integration evidence; no original pixels or general play claim.

The separate optional load-torch profile authors Sand12 and upright Torch50
metadata5 at the same coordinates. Saved Rotation is yaw0/pitch2, so the real
Player camera selects the source Torch ray box. Its 30 Torch vertices plus30
exposed Sand vertices, UVs, white emission color, bounds, list reuse, real
WorldAccess dirty notification and ownership cleanup are checked independently.
The genuine Torch callback also supplies both particle identities to an entered
World-draw component using the actual Timer partial and Settings/PX deadline.
This separately labelled draw leaves Timer/loop observations unchanged; an
ordinary returned World/HUD loop frame follows it.

The optional load-water/load-lava profiles declare still fluid above Sand and a
saved Player standing on a Stone platform. They compare fluid positions/UVs to
executed-original rows at the same source coordinates/NW translation; raw colors
are retained with an explicit match count because loaded lighting evolves.
Real Timer/ticks, fluid callbacks, light notification and cleanup remain active.

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
        with path.open("rb") as source:
            data = source.read(1048577)
        if len(data) > 1048576:
            raise RuntimeError("Save/title bounded file inventory")
        total += len(data)
        if total > 16 * 1048576:
            raise RuntimeError("Save/title bounded aggregate inventory")
        relative = path.relative_to(root)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        result[relative.as_posix()] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    return result



def retained_native_run(command, environment, evidence, saves, before_files, mode,
                        process_first, timeout):
    """Retain process evidence before attempting a fallible final disk copy."""
    result = primary = after_error = None
    try:
        result = subprocess.run(command, env=environment, timeout=timeout, check=False,
                                capture_output=True, text=True)
    except BaseException as error:
        primary = error
    stdout = result.stdout if result is not None else getattr(primary, "stdout", None)
    stderr = result.stderr if result is not None else getattr(primary, "stderr", None)
    for filename, value in (("stdout.txt", stdout), ("stderr.txt", stderr)):
        data = value if isinstance(value, bytes) else (value or "").encode("utf-8")
        (evidence / filename).write_bytes(data)
    execution = {"mode": mode, "before": before_files, "command": command,
                 "process_first_millis": process_first}
    if result is not None:
        execution.update(event="result", exit_code=result.returncode)
    else:
        execution.update(event="timeout" if isinstance(primary, subprocess.TimeoutExpired) else
                         "launch-error" if isinstance(primary, OSError) else "process-error",
                         error_type=type(primary).__name__, error=str(primary))
        if isinstance(primary, subprocess.TimeoutExpired):
            execution["timeout"] = True
    record = evidence / "execution.json"
    record.write_text(json.dumps(execution, indent=2) + "\n", encoding="utf-8")
    try:
        execution["after"] = save_title_files(saves, evidence / "after")
    except BaseException as error:
        after_error = error
        execution["after_snapshot_error"] = {"type": type(error).__name__, "error": str(error)}
    record.write_text(json.dumps(execution, indent=2) + "\n", encoding="utf-8")
    if primary is not None:
        raise primary
    if after_error is not None and result.returncode == 0:
        raise after_error
    return result


def save_title_decode(before, entries, after, mode, stdout, process_first, *,
                      folder="alpha", display="Alpha world", marker=57, options_input=None,
                      inherited_source_chunks=()):
    # Independent Python NBT/MUTF8/zlib parser, shared with the disk-save
    # fixture, reads actual bytes and has no access to C runtime state.
    parser_path = Path(__file__).resolve().parents[1] / "world/test_source_saving.py"
    spec = importlib.util.spec_from_file_location("save_title_independent_nbt", parser_path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    if (parser.read(entries.parent / "options-input.txt" if options_input is None else options_input) != b"viewDistance:3\n" or
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
        return name in {f"{folder}/level.dat", f"{folder}/level.dat_old", f"{folder}/session.lock"} or name.startswith(f"{folder}/region/")

    protected_initial = {name: digest for name, digest in initial_inventory.items() if not world_product(name)}
    if any(old_inventory.get(name) != digest for name, digest in protected_initial.items()):
        raise RuntimeError("Pause prefix changed protected initial fixture bytes")
    if any(name not in initial_inventory and not world_product(name) for name in old_inventory):
        raise RuntimeError("Pause prefix unexpected new save file")
    protected = {name: digest for name, digest in old_inventory.items()
                 if name != f"{folder}/session.lock" and
                 (mismatch or not (name in {f"{folder}/level.dat", f"{folder}/level.dat_old"} or name.startswith(f"{folder}/region/")))}
    if any(new_inventory.get(name) != digest for name, digest in protected.items()):
        raise RuntimeError("Full save changed protected entry fixture bytes")
    allowed = {f"{folder}/session.lock"} if mismatch else {f"{folder}/session.lock", f"{folder}/level.dat_old"}
    if any(name not in old_inventory and name not in allowed and
           not (not mismatch and name.startswith(f"{folder}/region/")) for name in new_inventory):
        raise RuntimeError("Full save unexpected new save file")
    old_token = parser.read(entry / f"{folder}/session.lock")
    new_token = parser.read(after / f"{folder}/session.lock")
    expected_token = bytes([old_token[0] ^ 1]) + old_token[1:] if mismatch and old_token else old_token
    if len(old_token) != 8 or new_token != expected_token:
        raise RuntimeError("Full save exact original session token/restored fault input")

    entry_tree = parser.inflate(parser.read(entry / f"{folder}/level.dat"), 31)
    if set(entry_tree) != {"Data"} or entry_tree["Data"][0] != 10:
        raise RuntimeError("Pause prefix fresh metadata root")
    entry_data = entry_tree["Data"][1]
    fields = {"RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "SizeOnDisk", "LastPlayed",
              "LevelName", "version", "rainTime", "raining", "thunderTime", "thundering", "Player"}
    initial_size = parser.inflate(parser.read(before / f"{folder}/level.dat"), 31)["Data"][1]["SizeOnDisk"]
    if (set(entry_data) != fields or entry_data.get("RandomSeed") != [4, 123] or
            entry_data.get("version") != [3, 19132] or entry_data.get("LevelName") != [8, display] or
            entry_data.get("Time", [None])[0] != 4 or not 0 <= entry_data["Time"][1] <= numbers["time"] or
            entry_data.get("SizeOnDisk", [None])[0] != 4 or initial_size[0] != 4 or
            not initial_size[1] <= entry_data["SizeOnDisk"][1] <= numbers["size-before"] or
            entry_data.get("LastPlayed", [None])[0] != 4 or
            not process_first <= entry_data["LastPlayed"][1] <= numbers["first"] or
            entry_data.get("Player", [None])[0] != 10):
        raise RuntimeError("Pause prefix exact fresh metadata fields/source interval")

    def regions(root, initial=None):
        decoded = {}
        for path in sorted((root / f"{folder}/region").glob("r.*.*.mcr")):
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
                source_written = coordinate in inherited_source_chunks or (initial is not None and initial.get(coordinate) != (tree, stamp))
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
            any(chunks[(0, 0)][0]["Level"][1]["Blocks"][1][127] != marker for chunks in (initial, old, new))):
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

    metadata = parser.inflate(parser.read(after / f"{folder}/level.dat"), 31)
    if set(metadata) != {"Data"} or metadata["Data"][0] != 10:
        raise RuntimeError("Save/title fresh metadata root")
    data = metadata["Data"][1]
    if set(data) != {"RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "SizeOnDisk", "LastPlayed",
                     "LevelName", "version", "rainTime", "raining", "thunderTime", "thundering", "Player"}:
        raise RuntimeError("Save/title exact fresh source Data fields")
    for key, value in {"RandomSeed": [4, 123], "version": [3, 19132],
                       "LevelName": [8, display], "Time": [4, numbers["time"]],
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
    if parser.read(after / f"{folder}/level.dat_old") != parser.read(entry / f"{folder}/level.dat"):
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



def reload_decode(evidence, stdout, process_first):
    """Read byte-for-byte snapshots taken after real source stages; no inputs rewritten."""
    shots = evidence / "native-snapshots"
    rows = stdout.splitlines()
    saves = [line for line in rows if line.startswith("WORLD_SAVE_TITLE ")]
    phases = [line for line in rows if line.startswith("WORLD_RELOAD_PHASE ")]
    folders = ("alpha", "alpha", "older", "alpha", "older", "alpha", "older", "alpha", "older", "alpha")
    count = len(folders)
    if len(saves) != count or phases != [
            f"WORLD_RELOAD_PHASE epoch={epoch} folder={folder} save-title=1"
            for epoch, folder in enumerate(folders, 1)]:
        raise RuntimeError("Ten actual ordered source save/title epochs required")
    suffix = (" distinct-os=1 distinct-load=1 distinct-handler=1 distinct-world=1 distinct-player=1"
              " distinct-input=1 n-ready=1 n-old-remove=1 n-th=1 n-k=1 n-cv=1 n-register=1"
              " n-rebuild=1 dn-replaced=1 previous-ready=1 returned=130 frame=1013")
    selections = [line for line in rows if line.startswith("WORLD_RELOAD_SELECT ")]
    if selections != [f"WORLD_RELOAD_SELECT epoch={epoch} folder={folder}" + suffix
                      for epoch, folder in enumerate(folders, 1) if epoch > 1]:
        raise RuntimeError("Actual same/different source selection and renderer receipts required")
    if rows.count("WORLD_RELOAD_NOTICE terminal-prefix-retired=1 source-fields-unchanged=1") != 1:
        raise RuntimeError("Actual Notice call retirement must precede all-epoch terminal audit")
    terminal = "WORLD_RELOAD_TERMINAL epochs=10 old-root-busy=1 all-worlds-ready=1 passive-invariant=1 released-root-retry=1 all-epoch-audit=1"
    if rows.count(terminal) != 1 or rows.count("WORLD_RELOAD_IDENTITIES epochs=10 all-pairs-distinct=1 source-owners-live=1") != 1:
        raise RuntimeError("All-pairs genuine identities, predecessor root refusal and passive retry required")
    if rows.count("WORLD_RELOAD_FAILED epoch=11 folder=bravo conversion=1 acquired-storage=0 preserved-prefix=1") != 1:
        raise RuntimeError("Actual failed successor conversion prefix required")
    snapshots = [line for line in rows if line.startswith("WORLD_RELOAD_SNAPSHOT ")]
    names = [f"{stage}-{epoch}" for epoch in range(1, count + 1) for stage in ("before", "after")]
    if len(snapshots) != 2 * count:
        raise RuntimeError("Twenty actual raw disk snapshots required")
    for row, name in zip(snapshots, names):
        found = re.fullmatch(rf"WORLD_RELOAD_SNAPSHOT phase={name} files=([1-9][0-9]*) bytes=([1-9][0-9]*) actual-file-copy=1", row)
        if found is None or not 0 < int(found[1]) <= 1024 or not 0 < int(found[2]) <= 16 * 1048576:
            raise RuntimeError("Bounded actual reload file-copy receipt")
        inventory = save_title_files(shots / name, evidence / ("audited-" + name))
        if len(inventory) != int(found[1]) or sum(value["bytes"] for value in inventory.values()) != int(found[2]):
            raise RuntimeError("Actual snapshot inventory differs from copy receipt")
    reports = []
    source_chunks = {}
    for epoch, folder in enumerate(folders, 1):
        display, marker = ("Alpha world", 57) if folder == "alpha" else ("Old world", 56)
        stage_stdout = "\n".join([line for line in rows if line.startswith("WORLD_SAVE_TITLE_OPTIONS ")] +
                                  [saves[epoch - 1]])
        reports.append(save_title_decode(shots / f"before-{epoch}", evidence / "entries" / f"epoch-{epoch}",
                                         shots / f"after-{epoch}", "save-title", stage_stdout, process_first,
                                         folder=folder, display=display, marker=marker,
                                         options_input=evidence / "options-input.txt",
                                         inherited_source_chunks=source_chunks.get(folder, ())))
        report = reports[-1]
        source_chunks.setdefault(folder, set()).update(
            tuple(coordinate) for coordinate in report["pause_changed_chunks"] + report["changed_chunks"])
    def hashes(root):
        return {Path(key).as_posix(): value for key, value in snapshot(root).items()}
    for epoch, folder in enumerate(folders, 1):
        previous = evidence / "before" if epoch == 1 else shots / f"after-{epoch - 1}"
        current, changed = shots / f"before-{epoch}", f"{folder}/session.lock"
        left, right = hashes(previous), hashes(current)
        if any(right.get(key) != value for key, value in left.items() if key != changed):
            raise RuntimeError("Source reopening changed existing disk bytes before Save")
        if set(right) - set(left) - {changed} or changed not in right or (current / changed).stat().st_size != 8:
            raise RuntimeError("Source reopening only creates/replaces actual session token")
    if hashes(shots / f"after-{count}") != hashes(evidence / "after"):
        raise RuntimeError("Native multi-epoch terminal cleanup wrote source save bytes")
    parser_path = Path(__file__).resolve().parents[1] / "world/test_source_saving.py"
    spec = importlib.util.spec_from_file_location("reload_independent_nbt", parser_path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    saved = parser.inflate(parser.read(shots / "after-1/alpha/level.dat"), 31)["Data"][1]["Player"]
    loaded = parser.Nbt(parser.read(shots / "loaded-player-2.nbt")).root()
    def payload(kind, value):
        if kind == 10:
            return {key: typed(item) for key, item in value.items()}
        if kind == 9:
            child, items = value
            return [child if items else 0, [payload(child, item) for item in items]]
        return value
    def typed(tag):
        return [tag[0], payload(tag[0], tag[1])]
    # The observer's generic encoder canonicalizes an empty List's child byte.
    # Compare complete stable-field semantics; no nonempty child type/value is normalized.
    if saved[0] != 10 or saved[1].get("Score") != [3, 7] or loaded.get("Score") != [3, 7]:
        raise RuntimeError("Declared saved Player Score7 must survive actual source read/save/reopen")
    for key in ("Inventory", "Dimension", "Score"):
        if key not in saved[1] or key not in loaded or typed(saved[1][key]) != typed(loaded[key]):
            raise RuntimeError(f"Actual reopened LocalPlayer semantic {key}")
    return {"mode": "reload-cycle", "stages": reports,
            "loaded_player_sha256": hashlib.sha256(parser.read(shots / "loaded-player-2.nbt")).hexdigest(),
            "scope": "native actual source same/different World UI reload and multi-epoch cleanup; no original whole-frame equivalence"}


def string(value):
    data = value.encode("ascii")
    return struct.pack(">H", len(data)) + data


def tag(kind, key, value):
    return bytes([kind]) + string(key) + value


def metadata(display, played, version, partial=False, player_score=None, cactus_player=False,
             torch_player=False, fluid_player=False, ladder_player=False):
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
    if player_score is not None:
        if partial:
            raise RuntimeError("Distinct authored source Player fixture arms")
        player = tag(3, "Dimension", struct.pack(">i", 0))
        # Source saved Pos uses Entity.y, including the Player's float offset.
        # Feet at the authored grass surface y64 therefore require this value.
        saved_y = 64.0 + struct.unpack(">f", struct.pack(">f", 1.62))[0]
        player += tag(9, "Pos", b"\x06" + struct.pack(">i3d", 3, .5, saved_y, .5))
        player += tag(9, "Motion", b"\x06" + struct.pack(">i3d", 3, 0., 0., 0.))
        player += tag(9, "Rotation", b"\x05" + struct.pack(">i2f", 2, 0., 0.))
        player += tag(2, "Health", struct.pack(">h", 20))
        player += tag(3, "Score", struct.pack(">i", player_score))
        player += tag(9, "Inventory", b"\x0a\x00\x00\x00\x00")
        data += tag(10, "Player", player + b"\x00")
    if cactus_player or torch_player or fluid_player or ladder_player:
        if (partial or player_score is not None or
                sum(bool(flag) for flag in (cactus_player, torch_player, fluid_player, ladder_player)) != 1):
            raise RuntimeError("Distinct authored special Block Player fixture arm")
        saved_y = (65.0 if fluid_player or ladder_player else 64.0) + struct.unpack(">f", struct.pack(">f", 1.62))[0]
        player = tag(3, "Dimension", struct.pack(">i", 0))
        player += tag(9, "Pos", b"\x06" + struct.pack(">i3d", 3, 3.5, saved_y, .5))
        player += tag(9, "Motion", b"\x06" + struct.pack(">i3d", 3, 0., 0., 0.))
        pitch = 15. if ladder_player else 26. if fluid_player else 2. if torch_player else 0.
        player += tag(9, "Rotation", b"\x05" + struct.pack(">i2f", 2, 0., pitch))
        player += tag(1, "OnGround", b"\x01")
        player += tag(2, "Health", struct.pack(">h", 20))
        player += tag(3, "Score", struct.pack(">i", 0))
        player += tag(9, "Inventory", b"\x0a\x00\x00\x00\x00")
        data += tag(10, "Player", player + b"\x00")
    return b"\x0a\x00\x00" + tag(10, "Data", data + b"\x00") + b"\x00"


def cactus_pose_input(path):
    """Independently decode the exact saved pose supplied to the source reader."""
    parser_path = Path(__file__).resolve().parents[1] / "world/test_source_saving.py"
    spec = importlib.util.spec_from_file_location("cactus_pose_nbt", parser_path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    decoded = parser.inflate(parser.read(path), 31)
    player = decoded['Data'][1]['Player'][1]
    position = [struct.pack('>d', value).hex() for value in
                (3.5, 64.0 + struct.unpack('>f', struct.pack('>f', 1.62))[0], .5)]
    rotation = ['00000000', '00000000']
    wanted_position = [int(value, 16) for value in position]
    if (player['Dimension'] != [3, 0] or player['Pos'] != [9, [6, wanted_position]] or
            player['Rotation'] != [9, [5, [0, 0]]] or
            player['Motion'] != [9, [6, [0, 0, 0]]] or
            player['OnGround'] != [1, 1] or player['Health'] != [2, 20] or
            player['Inventory'] != [9, [10, []]]):
        raise RuntimeError("Exact independently decoded saved Cactus camera input")
    return {'position_bits': position, 'rotation_bits': rotation,
            'feet_y_bits': struct.pack('>d', 64.).hex(),
            'offset_bits': struct.pack('>f', 1.62).hex()}


def torch_scene_input(level_path, region_path):
    """Independently decode the actual pre-bootstrap Player and Chunk operands."""
    parser_path = Path(__file__).resolve().parents[1] / "world/test_source_saving.py"
    spec = importlib.util.spec_from_file_location("torch_input_nbt", parser_path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    player = parser.inflate(parser.read(level_path), 31)['Data'][1]['Player'][1]
    position = [struct.pack('>d', value).hex() for value in
                (3.5, 64.0 + struct.unpack('>f', struct.pack('>f', 1.62))[0], .5)]
    rotation = ['00000000', '40000000']
    if (player['Dimension'] != [3, 0] or
            player['Pos'] != [9, [6, [int(value, 16) for value in position]]] or
            player['Rotation'] != [9, [5, [int(value, 16) for value in rotation]]] or
            player['Motion'] != [9, [6, [0, 0, 0]]] or player['OnGround'] != [1, 1] or
            player['Health'] != [2, 20] or player['Inventory'] != [9, [10, []]]):
        raise RuntimeError("Exact independently decoded saved Torch camera input")
    raw = parser.read(region_path)
    location = struct.unpack_from('>I', raw, 0)[0]
    sector, count = location >> 8, location & 255
    at = sector * 4096
    if sector < 2 or count < 1 or (sector + count) * 4096 > len(raw):
        raise RuntimeError("Actual authored Torch Chunk allocation")
    length = struct.unpack_from('>I', raw, at)[0]
    if not 1 < length <= count * 4096 - 4 or raw[at + 4] != 2:
        raise RuntimeError("Actual authored Torch Chunk compression extent")
    level = parser.inflate(raw[at + 5:at + 4 + length], 15)['Level'][1]
    base = (3 << 11) | (3 << 7)
    if (level['xPos'] != [3, 0] or level['zPos'] != [3, 0] or
            level['TerrainPopulated'] != [1, 1] or
            level['Blocks'][0] != 7 or len(level['Blocks'][1]) != 32768 or
            level['Blocks'][1][base + 63:base + 66] != bytes([2, 12, 50]) or
            level['Data'][0] != 7 or len(level['Data'][1]) != 16384 or
            level['Data'][1][(base + 64) // 2] != 0x50 or
            level['SkyLight'][1][(base + 64) // 2] != 0xf0 or
            level['HeightMap'][1][3 * 16 + 3] != 65):
        raise RuntimeError("Independently decoded saved Grass/Sand/Torch5 source operands")
    return {'position_bits': position, 'rotation_bits': rotation,
            'feet_y_bits': struct.pack('>d', 64.).hex(),
            'offset_bits': struct.pack('>f', 1.62).hex(),
            'sand': [3, 64, 3, 12], 'torch': [3, 65, 3, 50, 5]}


def torch_receipts(stdout, pose_input):
    rows = stdout.splitlines()
    pose_expected = ("WORLD_TORCH_POSE input=saved-player source-loaded=1 camera=player position=" +
                     ','.join(pose_input['position_bits']) + " rotation=" +
                     ','.join(pose_input['rotation_bits']) + " feet=" + pose_input['feet_y_bits'] +
                     " offset=" + pose_input['offset_bits'] + " hit=3,65,3")
    if [line for line in rows if line.startswith("WORLD_TORCH_POSE ")] != [pose_expected]:
        raise RuntimeError("Actual Torch Player/camera must equal the independently decoded saved input")
    if [line for line in rows if line.startswith("WORLD_TORCH_PARTICLES ")] != [
            "WORLD_TORCH_PARTICLES source-callback=1 smoke=1 flame=1 same-manager=1 "
            "world-random-unchanged=1"]:
        raise RuntimeError("Actual Torch callback must publish Smoke and Flame through the same DN")
    particles = [line for line in rows if line.startswith("WORLD_TORCH_PARTICLE_DRAW ")]
    drawn = re.fullmatch(
        r"WORLD_TORCH_PARTICLE_DRAW entry=world-draw smoke=1 flame=1 same-identities=1 returned=1 "
        r"visited=(?P<visited>[0-9]+) partial=(?P<partial>[0-9a-f]{8}) "
        r"performance-mode=(?P<performance>-?[0-9]+) px-I=(?P<clock>-?[0-9]+) "
        r"deadline=(?P<deadline>-?[0-9]+) timer-unchanged=1 loop-counts-unchanged=1 "
        r"loop-clocks-unchanged=1 retired=1",
        particles[0]) if len(particles) == 1 else None
    if drawn is None or int(drawn['visited']) < 2:
        raise RuntimeError("Both published identities must be visited by one returned entered World draw")
    performance, clock, deadline = (int(drawn[key]) for key in ('performance', 'clock', 'deadline'))
    target = 120 if performance == 1 else 40 if performance == 2 else 200
    derived_deadline = 0 if performance == 0 else (clock + 1_000_000_000 // target)
    derived_deadline = (derived_deadline + (1 << 63)) % (1 << 64) - (1 << 63)
    if (not -(1 << 31) <= performance < (1 << 31) or
            not -(1 << 63) <= clock < (1 << 63) or
            not -(1 << 63) <= deadline < (1 << 63) or deadline != derived_deadline):
        raise RuntimeError("Entered World draw must use the source Settings/PX deadline")
    summary = [line for line in rows if line.startswith("WORLD_TORCH ")]
    receipt = re.fullmatch(
        r"WORLD_TORCH input=pre-bootstrap-region sand=12 torch=50 at=3,65,3 pass=0 "
        r"metadata=5 vertices=60 torch-vertices=30 exact-position-uv-color=1 light-before=(?P<light>[0-9]+) "
        r"frames=(?P<frames>[0-9]+) reuse-frames=(?P<reuse>[0-9]+) "
        r"recompile-frames=(?P<recompile>[0-9]+) list=(?P<list>[0-9]+) "
        r"same-world-and-registry=1 returned-world-hud=1 reused-definition=1 "
        r"world-light-notification=1 dirty-recompiled=1 source-bounds-preserved=1",
        summary[0]) if len(summary) == 1 else None
    if (receipt is None or not 0 <= int(receipt['light']) <= 15 or
            not 1 <= int(receipt['recompile']) <= 16 or not 1 <= int(receipt['reuse']) <= 16 or
            not 4 <= int(receipt['frames']) <= 48 or int(receipt['list']) <= 0):
        raise RuntimeError("Torch exact section/UV/color, returned World/HUD and real dirty/recompile receipt")
    steps = [line for line in rows if line.startswith("WORLD_TORCH_STEP ")]
    if len(steps) != int(receipt['reuse']):
        raise RuntimeError("Bounded actual Torch reuse observation inventory")
    for attempt, step in enumerate(steps, 1):
        observed = re.fullmatch(rf"WORLD_TORCH_STEP stage=reuse attempt={attempt} same-list=1 "
                                r"same-definition=([01]) same-bytes=([01])", step)
        final = attempt == len(steps)
        if (observed is None or (observed[1] == '1') != final or final and observed[2] != '1'):
            raise RuntimeError("Every Torch section must be validated through actual immutable list reuse")
    cleanup = [line for line in rows if line.startswith("WORLD_SELECTION mode=load-torch cleanup=")]
    if len(cleanup) != 1 or re.fullmatch(
            r"WORLD_SELECTION mode=load-torch cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
        raise RuntimeError("Loaded Torch checked native ownership retirement")


def standing_scene_input(level_path, region_path, block_id):
    """Decode saved Block/standing cells and the source-loaded Player pose."""
    if block_id not in (9, 11, 65):
        raise RuntimeError("Declared Water/Lava/Ladder registration")
    ladder = block_id == 65
    parser_path = Path(__file__).resolve().parents[1] / "world/test_source_saving.py"
    spec = importlib.util.spec_from_file_location("fluid_input_nbt", parser_path)
    decoder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(decoder)
    player = decoder.inflate(decoder.read(level_path), 31)['Data'][1]['Player'][1]
    position = [struct.pack('>d', value).hex() for value in
                (3.5, 65.0 + struct.unpack('>f', struct.pack('>f', 1.62))[0], .5)]
    rotation = [struct.pack('>f', value).hex() for value in (0., 15. if ladder else 26.)]
    if (player['Dimension'] != [3, 0] or
            player['Pos'] != [9, [6, [int(value, 16) for value in position]]] or
            player['Rotation'] != [9, [5, [int(value, 16) for value in rotation]]] or
            player['Motion'] != [9, [6, [0, 0, 0]]] or player['OnGround'] != [1, 1] or
            player['Health'] != [2, 20] or player['Inventory'] != [9, [10, []]]):
        raise RuntimeError("Independently decoded saved fluid Player camera input")
    raw = decoder.read(region_path)
    location = struct.unpack_from('>I', raw, 0)[0]
    sector, count = location >> 8, location & 255
    at = sector * 4096
    if sector < 2 or count < 1 or (sector + count) * 4096 > len(raw):
        raise RuntimeError("Actual authored fluid Chunk allocation")
    length = struct.unpack_from('>I', raw, at)[0]
    if not 1 < length <= count * 4096 - 4 or raw[at + 4] != 2:
        raise RuntimeError("Actual authored fluid Chunk compression extent")
    chunk = decoder.inflate(raw[at + 5:at + 4 + length], 15)['Level'][1]
    if (chunk['xPos'] != [3, 0] or chunk['zPos'] != [3, 0] or
            chunk['TerrainPopulated'] != [1, 1] or chunk['Blocks'][0] != 7 or
            len(chunk['Blocks'][1]) != 32768 or chunk['Data'][0] != 7 or
            len(chunk['Data'][1]) != 16384):
        raise RuntimeError("Actual source fluid saved Chunk fields")
    cells = [(3, 64, 0, 1, 0), (3, 64, 3, 12, 0), (3, 65, 3, block_id, 2 if ladder else 0)]
    if ladder:
        cells.append((3, 65, 4, 1, 0))
    for x, y, z, block, meta in cells:
        index = (x << 11) | (z << 7) | y
        if (chunk['Blocks'][1][index] != block or
                (chunk['Data'][1][index // 2] >> ((index & 1) * 4)) & 15 != meta):
            raise RuntimeError("Independent saved fluid/standing cells and metadata")
    base, platform = (3 << 11) | (3 << 7), (3 << 11)
    if (chunk['SkyLight'][1][(base + 64) // 2] != (0xf0 if ladder else 0xc0 if block_id == 9 else 0) or
            chunk['HeightMap'][1][3 * 16 + 3] != (65 if ladder else 66) or
            chunk['SkyLight'][1][(platform + 64) // 2] != 0xf0 or
            chunk['HeightMap'][1][3] != 65):
        raise RuntimeError("Declared source-opacity-based fluid and platform saved light inputs")
    if ladder:
        wall = (3 << 11) | (4 << 7)
        if (chunk['SkyLight'][1][(wall + 64) // 2] != 0 or
                chunk['HeightMap'][1][4 * 16 + 3] != 66):
            raise RuntimeError('Declared opaque Ladder backing and its saved skylight inputs')
    return {'position_bits': position, 'rotation_bits': rotation,
            'feet_y_bits': struct.pack('>d', 65.).hex(),
            'offset_bits': struct.pack('>f', 1.62).hex(), 'cells': cells,
            'block': [3, 65, 3, block_id, 2 if ladder else 0],
            'pick': [3, 65, 3, 65] if ladder else [3, 64, 3, 12]}


def fluid_receipts(stdout, mode, pose_input, reference):
    rows = stdout.splitlines()
    wanted_pose = (f"WORLD_FLUID_POSE mode={mode} input=saved-player source-loaded=1 camera=player "
                   "position=" + ','.join(pose_input['position_bits']) + " rotation=" +
                   ','.join(pose_input['rotation_bits']) + " feet=" + pose_input['feet_y_bits'] +
                   " offset=" + pose_input['offset_bits'] + " hit=3,64,3 block=12")
    if [line for line in rows if line.startswith("WORLD_FLUID_POSE ")] != [wanted_pose]:
        raise RuntimeError("Source-read saved fluid Player/camera and Sand pick through liquid")
    summary = [line for line in rows if line.startswith("WORLD_FLUID ")]
    receipt = re.fullmatch(
        rf"WORLD_FLUID mode={mode} input=pre-bootstrap-region block={reference['id']} "
        rf"metadata={reference['metadata']} pass={reference['pass']} fluid-vertices={reference['vertices']} "
        r"exact-original-position-uv=1 color-matches=(?P<colors>[0-9]+) "
        rf"source-colors={reference['vertices']} frames=(?P<frames>[0-9]+) "
        r"reuse-frames=(?P<reuse>[0-9]+) recompile-frames=(?P<recompile>[0-9]+) "
        r"list=(?P<list>[0-9]+) source-time=(?P<time>-?[0-9]+) "
        r"sky-subtraction=(?P<sky>-?[0-9]+) same-world-and-registry=1 returned-world-hud=1 "
        r"sand-pick-through-fluid=1 reused-definition=1 world-light-notification=1 "
        r"dirty-recompiled=1 source-bounds-preserved=1",
        summary[0]) if len(summary) == 1 else None
    if (receipt is None or not 3 <= int(receipt['frames']) <= 48 or
            not 1 <= int(receipt['reuse']) <= 16 or not 1 <= int(receipt['recompile']) <= 16 or
            int(receipt['list']) <= 0 or not -(1 << 63) <= int(receipt['time']) < (1 << 63) or
            not -(1 << 31) <= int(receipt['sky']) < (1 << 31)):
        raise RuntimeError("Returned loaded fluid section/reuse/WorldAccess/cleanup receipts")
    words = [line for line in rows if line.startswith("WORLD_FLUID_WORD ")]
    if len(words) != reference['vertices']:
        raise RuntimeError("Every actual loaded fluid vertex must be retained")
    color_matches = 0
    for vertex, line in enumerate(words):
        actual = re.fullmatch(rf"WORLD_FLUID_WORD mode={mode} vertex={vertex} "
                              r"words=([0-9a-f]{8}(?:,[0-9a-f]{8}){7})", line)
        if actual is None:
            raise RuntimeError("Typed loaded fluid packed-word receipt")
        values = actual[1].split(',')
        original = reference['words'][vertex * 8:(vertex + 1) * 8]
        if values[:5] != original[:5]:
            raise RuntimeError("Exact unchanged-original fluid position/UV words")
        color_matches += values[5] == original[5]
    if color_matches != int(receipt['colors']):
        raise RuntimeError("Independently counted packed source-color matches")
    # Loaded light evolves normally. Raw colors remain retained even when its
    # brightness operands differ from the direct original profile's inputs.
    steps = [line for line in rows if line.startswith("WORLD_FLUID_STEP ")]
    if len(steps) != int(receipt['reuse']):
        raise RuntimeError("Actual bounded fluid definition-reuse inventory")
    for attempt, line in enumerate(steps, 1):
        step = re.fullmatch(rf"WORLD_FLUID_STEP mode={mode} stage=reuse attempt={attempt} "
                            r"same-list=1 same-definition=([01]) same-bytes=([01])", line)
        final = attempt == len(steps)
        if step is None or (step[1] == '1') != final or final and step[2] != '1':
            raise RuntimeError("Every validated fluid section before immutable definition reuse")
    cleanup = [line for line in rows if line.startswith(f"WORLD_SELECTION mode={mode} cleanup=")]
    if len(cleanup) != 1 or re.fullmatch(
            rf"WORLD_SELECTION mode={mode} cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
        raise RuntimeError("Loaded fluid checked ownership terminal retirement")


def ladder_receipts(stdout, pose_input, reference):
    rows = stdout.splitlines()
    pose = ('WORLD_LADDER_POSE input=saved-player source-loaded=1 camera=player position=' +
            ','.join(pose_input['position_bits']) + ' rotation=' +
            ','.join(pose_input['rotation_bits']) + ' feet=' + pose_input['feet_y_bits'] +
            ' offset=' + pose_input['offset_bits'] + ' hit=3,65,3 block=65')
    if [row for row in rows if row.startswith('WORLD_LADDER_POSE ')] != [pose]:
        raise RuntimeError('Source-read saved Ladder Player/camera and actual Ladder pick')
    summaries = [row for row in rows if row.startswith('WORLD_LADDER ')]
    receipt = re.fullmatch(
        r'WORLD_LADDER input=pre-bootstrap-region block=65 metadata=2 pass=0 vertices=102 '
        r'ladder-vertices=6 exact-original-position-uv=1 color-matches=(?P<colors>[0-9]+) '
        r'source-colors=6 frames=(?P<frames>[0-9]+) reuse-frames=(?P<reuse>[0-9]+) '
        r'recompile-frames=(?P<recompile>[0-9]+) list=(?P<list>[0-9]+) '
        r'same-world-and-registry=1 returned-world-hud=1 ladder-pick=1 reused-definition=1 '
        r'world-light-notification=1 dirty-recompiled=1 source-bounds-preserved=1',
        summaries[0]) if len(summaries) == 1 else None
    if (receipt is None or not 3 <= int(receipt['frames']) <= 48 or
            not 1 <= int(receipt['reuse']) <= 16 or not 1 <= int(receipt['recompile']) <= 16 or
            int(receipt['list']) <= 0 or reference['vertices'] != 6 or
            reference['id'] != 65 or reference['metadata'] != 2):
        raise RuntimeError('Returned Ladder section/reuse/dirty-recompile inventory')
    vertices = [row for row in rows if row.startswith('WORLD_LADDER_WORD ')]
    if len(vertices) != 6:
        raise RuntimeError('Every actual loaded Ladder vertex must be retained')
    colors = 0
    for i, row in enumerate(vertices):
        match = re.fullmatch(rf'WORLD_LADDER_WORD vertex={i} words=([0-9a-f]{{8}}(?:,[0-9a-f]{{8}}){{7}})', row)
        if match is None:
            raise RuntimeError('Typed loaded Ladder packed-word receipt')
        words = match[1].split(',')
        original = reference['words'][i * 8:(i + 1) * 8]
        if words[:5] != original[:5]:
            raise RuntimeError('Exact unchanged-original Ladder position/UV words')
        colors += words[5] == original[5]
    if colors != int(receipt['colors']):
        raise RuntimeError('Independently counted loaded Ladder color matches')
    steps = [row for row in rows if row.startswith('WORLD_LADDER_STEP ')]
    if len(steps) != int(receipt['reuse']):
        raise RuntimeError('Actual bounded Ladder definition-reuse inventory')
    for i, row in enumerate(steps, 1):
        step = re.fullmatch(rf'WORLD_LADDER_STEP stage=reuse attempt={i} same-list=1 '
                            r'same-definition=([01]) same-bytes=([01])', row)
        final = i == len(steps)
        if step is None or (step[1] == '1') != final or final and step[2] != '1':
            raise RuntimeError('Every qualified Ladder section before immutable reuse')
    cleanup = [row for row in rows if row.startswith('WORLD_SELECTION mode=load-ladder cleanup=')]
    if len(cleanup) != 1 or re.fullmatch(
            r'WORLD_SELECTION mode=load-ladder cleanup=1 checks=[1-9][0-9]*', cleanup[0]) is None:
        raise RuntimeError('Loaded Ladder checked ownership retirement')


def chunk_record(chunk_x, chunk_z, marker=57, cactus=False, torch=False, fluid_id=0, ladder=False):
    blocks = bytearray(32768)
    data = bytearray(16384)
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
        blocks[127] = marker
        skylight[127 // 2] &= 15
        heights[0] = 128
        if cactus:
            # Sand is the highest opaque block; Cactus retains full skylight.
            base = (3 << 11) | (3 << 7)
            blocks[base + 64], blocks[base + 65] = 12, 81
            skylight[(base + 64) // 2] = 0xf0
            heights[3 * 16 + 3] = 65
        if torch:
            if cactus:
                raise RuntimeError("Independent Cactus and Torch saved input scopes")
            base = (3 << 11) | (3 << 7)
            blocks[base + 64], blocks[base + 65] = 12, 50
            data[(base + 64) // 2] = 0x50
            skylight[(base + 64) // 2] = 0xf0
            heights[3 * 16 + 3] = 65
        if fluid_id:
            if fluid_id not in (9, 11) or cactus or torch:
                raise RuntimeError("Independent saved Water/Lava input arm")
            # Ordinary saved standing platform and Sand-supported fluid.
            # Actual UU.q opacity is3/255, independently pinned before this
            # fixture; later source lighting/flow remains ordinary execution.
            platform = (3 << 11) | (0 << 7)
            base = (3 << 11) | (3 << 7)
            blocks[platform + 64] = 1
            skylight[(platform + 64) // 2] = 0xf0
            heights[3] = 65
            blocks[base + 64], blocks[base + 65] = 12, fluid_id
            skylight[(base + 64) // 2] = 0xc0 if fluid_id == 9 else 0
            heights[3 * 16 + 3] = 66
        if ladder:
            if cactus or torch or fluid_id:
                raise RuntimeError('Independent saved Ladder input arm')
            platform, base, wall = (3 << 11), (3 << 11) | (3 << 7), (3 << 11) | (4 << 7)
            blocks[platform + 64] = blocks[wall + 65] = 1
            blocks[base + 64], blocks[base + 65] = 12, 65
            data[(base + 64) // 2] = 0x20
            skylight[(platform + 64) // 2] = skylight[(base + 64) // 2] = 0xf0
            skylight[(wall + 64) // 2] = 0
            heights[3] = heights[3 * 16 + 3] = 65
            heights[4 * 16 + 3] = 66
    level = tag(3, "xPos", struct.pack(">i", chunk_x))
    level += tag(3, "zPos", struct.pack(">i", chunk_z))
    level += tag(4, "LastUpdate", struct.pack(">q", 0))
    for name, values in (("Blocks", blocks), ("Data", data),
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


def saved_regions(directory, generate, marker=57, cactus=False, torch=False, fluid_id=0, ladder=False):
    # The default UI profile reads all 289 existing Chunks. The optional
    # generation profile retains one saved marker Chunk and generates its
    # surroundings through the same source path.
    coordinates = [(0, 0)] if generate else [(x, z) for x in range(-8, 9)
                                            for z in range(-8, 9)]
    regions = {}
    for x, z in coordinates:
        body, sectors = chunk_record(x, z, marker, cactus=cactus, torch=torch, fluid_id=fluid_id,
                                     ladder=ladder)
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
                                 "load-terminal", "load-partial", "repeat", "load-light", "load-cactus", "load-torch", "load-water", "load-lava", "load-ladder", "animals", "creatures",
                                 "creatures-slime", "creatures-creeper", "remaining-mobs",
                                 "remaining-wolf", "remaining-zombie", "remaining-skeleton",
                                 "save-title", "save-title-session", "reload-cycle"))
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
        cactus = mode == "load-cactus"
        torch = mode == "load-torch"
        ladder = mode == 'load-ladder'
        fluid_id = 9 if mode == 'load-water' else 11 if mode == 'load-lava' else 0
        reload_cycle = mode == "reload-cycle"
        save_title = mode in ("save-title", "save-title-session") or reload_cycle
        evidence = Path(tempfile.mkdtemp(prefix=mode + "-", dir=artifacts))
        with tempfile.TemporaryDirectory(prefix="mc-rq-loop-") as work:
            appdata = Path(work)
            game = appdata / ".minecraft"
            if mode == "repeat" or cactus or torch or fluid_id or ladder or animal or creature or remaining or save_title:
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
            elif mode in ("populated", "double", "load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or cactus or torch or fluid_id or ladder or animal or creature or remaining or save_title:
                worlds = [
                    ("older", "Old world", 1303000000000, 19132),
                    ("bravo", "", 1304000000000, 0 if reload_cycle else 19132),
                    ("alpha", "Alpha world", 1304000000000,
                     19132 if mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or cactus or torch or fluid_id or ladder or animal or creature or remaining or save_title else 0),
                ]
            else:
                worlds = []
            for folder, display, played, version in worlds:
                directory = saves / folder
                directory.mkdir()
                filename = "level.dat_old" if folder == "bravo" else "level.dat"
                (directory / filename).write_bytes(
                    gzip.compress(metadata(display, played, version,
                                           partial=mode == "load-partial" and folder == "alpha",
                                           player_score=7 if reload_cycle and folder == "alpha" else None,
                                           cactus_player=cactus and folder == "alpha",
                                           torch_player=torch and folder == "alpha",
                                           fluid_player=bool(fluid_id) and folder == "alpha",
                                           ladder_player=ladder and folder == "alpha"),
                                  mtime=0))
                (directory / "preserve-chunk.dat").write_bytes(b"authored untouched chunk")
            if mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or cactus or torch or fluid_id or ladder or animal or creature or remaining or save_title:
                regions = saves / "alpha" / "region"
                regions.mkdir()
                saved_regions(regions, generate=mode == "generate", cactus=cactus, torch=torch,
                              fluid_id=fluid_id, ladder=ladder)
            if reload_cycle:
                regions = saves / "older" / "region"
                regions.mkdir()
                saved_regions(regions, generate=False, marker=56)
            before = snapshot(saves)
            before_files = save_title_files(saves, evidence / "before")
            if cactus:
                pose_input = cactus_pose_input(evidence / "before/alpha/level.dat")
                (evidence / "cactus-input.json").write_text(json.dumps(pose_input, indent=2) + "\n", encoding="utf-8")
            if torch:
                pose_input = torch_scene_input(evidence / "before/alpha/level.dat",
                                               evidence / "before/alpha/region/r.0.0.mcr")
                (evidence / "torch-input.json").write_text(json.dumps(pose_input, indent=2) + "\n", encoding="utf-8")
            if fluid_id:
                pose_input = standing_scene_input(evidence / 'before/alpha/level.dat',
                                               evidence / 'before/alpha/region/r.0.0.mcr', fluid_id)
                (evidence / 'fluid-input.json').write_text(json.dumps(pose_input, indent=2) + '\n', encoding='utf-8')
                spec = importlib.util.spec_from_file_location('fluid_original_projection',
                                                              Path(__file__).with_name('test_chunk_compile.py'))
                fluid_projection = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(fluid_projection)
                fluid_reference = [row for row in fluid_projection.fluid_reference_cases(2)
                                   if row['id'] == fluid_id]
                if len(fluid_reference) != 1:
                    raise RuntimeError('One exact executed-original loaded fluid profile')
                original_path = evidence / 'fluid-original-inputs-and-words.txt'
                fluid_projection.write_fluid_reference(original_path, fluid_reference,
                                                        marker='FLUID_LOADED_V1')
            if ladder:
                pose_input = standing_scene_input(evidence / 'before/alpha/level.dat',
                                                 evidence / 'before/alpha/region/r.0.0.mcr', 65)
                (evidence / 'ladder-input.json').write_text(json.dumps(pose_input, indent=2) + '\n',
                                                         encoding='utf-8', newline='\n')
                spec = importlib.util.spec_from_file_location('ladder_original_projection',
                                                              Path(__file__).with_name('test_chunk_compile.py'))
                block_projection = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(block_projection)
                ladder_reference = [row for row in block_projection.ladder_reference_cases(2)
                                    if row['metadata'] == 2]
                if len(ladder_reference) != 1:
                    raise RuntimeError('One exact executed-original loaded Ladder profile')
                original_path = evidence / 'ladder-original-inputs-and-words.txt'
                block_projection.write_fluid_reference(original_path, ladder_reference,
                                                        marker='LADDER_LOADED_V1')
            environment = os.environ.copy()
            environment["APPDATA"] = str(appdata)
            if fluid_id:
                environment['MC_WORLD_SELECTION_FLUID_REFERENCE'] = str(original_path)
            if ladder:
                environment['MC_WORLD_SELECTION_LADDER_REFERENCE'] = str(original_path)
            if save_title:
                environment["MC_WORLD_SAVE_TITLE_SESSION"] = str(saves / "alpha/session.lock")
                environment["MC_WORLD_SAVE_TITLE_SAVES"] = str(saves)
                environment["MC_WORLD_SAVE_TITLE_ENTRIES"] = str(evidence / "entries")
            if reload_cycle:
                shots = evidence / "native-snapshots"
                shots.mkdir()
                environment["MC_WORLD_RELOAD_SAVES"] = str(saves)
                environment["MC_WORLD_SAVE_TITLE_SNAPSHOTS"] = str(shots)
            capture = artifacts / (mode + ".png")
            process_first = time.time_ns() // 1000000
            result = retained_native_run(
                [str(executable), str(assets), mode, str(capture)], environment, evidence,
                saves, before_files, mode, process_first,
                args.timeout if args.timeout is not None else
                (600 if mode == "generate" else 240 if animal or creature or remaining or save_title else 110))
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
            if cactus:
                pose_rows = [line for line in result.stdout.splitlines() if line.startswith("WORLD_CACTUS_POSE ")]
                pose_expected = ("WORLD_CACTUS_POSE input=saved-player source-loaded=1 camera=player position=" +
                                 ','.join(pose_input['position_bits']) + " rotation=" +
                                 ','.join(pose_input['rotation_bits']) + " feet=" + pose_input['feet_y_bits'] +
                                 " offset=" + pose_input['offset_bits'] + " hit=3,65,3")
                if pose_rows != [pose_expected]:
                    raise RuntimeError("Actual source-read Player/camera pose differs from independently decoded saved input")
                rows = [line for line in result.stdout.splitlines() if line.startswith("WORLD_CACTUS ")]
                receipt = re.fullmatch(
                    r"WORLD_CACTUS input=pre-bootstrap-region sand=12 cactus=81 at=3,65,3 pass=0 "
                    r"vertices=60 inset-vertices=24 frames=(?P<frames>[0-9]+) reuse-frames=(?P<reuse>[0-9]+) "
                    r"recompile-frames=(?P<recompile>[0-9]+) list=(?P<list>[0-9]+) "
                    r"same-world-and-registry=1 returned-world-hud=1 reused-definition=1 "
                    r"world-light-notification=1 dirty-recompiled=1 source-bounds-preserved=1",
                    rows[0]) if len(rows) == 1 else None
                if (receipt is None or not 1 <= int(receipt['recompile']) <= 16 or
                        not 1 <= int(receipt['reuse']) <= 16 or
                        not 3 <= int(receipt['frames']) <= 48 or int(receipt['list']) <= 0):
                    raise RuntimeError("Loaded Cactus did not prove actual section geometry, returned frames, reuse and dirty recompile")
                steps = [line for line in result.stdout.splitlines() if line.startswith("WORLD_CACTUS_STEP ")]
                if len(steps) != int(receipt['reuse']):
                    raise RuntimeError("Bounded actual Cactus reuse observation inventory")
                for attempt, step in enumerate(steps, 1):
                    observed = re.fullmatch(
                        rf"WORLD_CACTUS_STEP stage=reuse attempt={attempt} same-list=1 "
                        r"same-definition=([01]) same-bytes=([01])", step)
                    wanted_same = attempt == len(steps)
                    if (observed is None or (observed[1] == '1') != wanted_same or
                            wanted_same and observed[2] != '1'):
                        raise RuntimeError("Every validated Cactus section must precede an actual immutable list reuse")
                cleanup = [line for line in result.stdout.splitlines()
                           if line.startswith("WORLD_SELECTION mode=load-cactus cleanup=")]
                if len(cleanup) != 1 or re.fullmatch(
                        r"WORLD_SELECTION mode=load-cactus cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
                    raise RuntimeError("Loaded Cactus native terminal ownership cleanup receipt")
            if torch:
                torch_receipts(result.stdout, pose_input)
            if fluid_id:
                fluid_receipts(result.stdout, mode, pose_input, fluid_reference[0])
            if ladder:
                ladder_receipts(result.stdout, pose_input, ladder_reference[0])
            after = snapshot(saves)
            if save_title:
                decoded = (reload_decode(evidence, result.stdout, process_first) if reload_cycle else
                           save_title_decode(evidence / "before", evidence / "entries", evidence / "after",
                                             mode, result.stdout, process_first))
                cleanup = [line for line in result.stdout.splitlines()
                           if line.startswith(f"WORLD_SELECTION mode={mode} cleanup=")]
                if len(cleanup) != 1 or re.fullmatch(
                        rf"WORLD_SELECTION mode={mode} cleanup=1 checks=[1-9][0-9]*", cleanup[0]) is None:
                    raise RuntimeError("Save/title native terminal ownership cleanup receipt")
                (evidence / "report.json").write_text(json.dumps(decoded, indent=2) + "\n", encoding="utf-8")
            elif mode in ("load", "generate", "load-terminal", "load-partial", "repeat", "load-light") or cactus or torch or fluid_id or ladder or animal or creature or remaining:
                if any(after.get(name) != digest for name, digest in before.items()):
                    raise RuntimeError("World loading changed pre-existing fixture bytes")
                if not (saves / "alpha" / "session.lock").is_file():
                    raise RuntimeError("Actual source SaveHandler did not create its session lock")
                if (fluid_id or ladder) and set(after) - set(before) != {str(Path("alpha") / "session.lock")}:
                    raise RuntimeError("Loaded Block scene created files beyond its actual session lock")
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
    if all(mode == "load-cactus" for mode in modes):
        print("Native loaded Cactus: authored saved Sand/Cactus, actual 60-vertex section, "
              "returned World/HUD frames, list reuse and WorldAccess dirty recompile passed; "
              "no original pixels or general play claim")
    elif all(mode == "load-torch" for mode in modes):
        print("Native loaded Torch: saved Sand/Torch5 and Player camera, exact30 Torch vertices/UV/white color, "
              "returned World/HUD and selection outline, list reuse and WorldAccess dirty recompile passed; "
              "no original pixels or general play claim")
    elif all(mode in ('load-water', 'load-lava') for mode in modes):
        print("Native loaded fluid: saved Water/Lava and Player pose, original position/UV words, "
              "observed color matches, actual returned World/HUD, Sand pick through fluid, "
              "list reuse and WorldAccess dirty recompile; no original pixel or general play claim")
    elif all(mode == 'load-ladder' for mode in modes):
        print('Native loaded Ladder: saved mounted Ladder and Player pose, original position/UV words, '
              'actual picking/outline, returned World/HUD, mesh reuse, WorldAccess dirty recompile '
              'and checked ownership; no original pixel or general play claim')
    elif all(mode == "reload-cycle" for mode in modes):
        print("Native actual same/different World reload: ten source UI epochs, failed successor, actual entry snapshots, "
              "N/DN replacements, disk/player readback and all-epoch terminal ownership; no original parity claim")
    elif all(mode in ("save-title", "save-title-session") for mode in modes):
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
