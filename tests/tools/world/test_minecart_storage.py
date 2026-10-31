#!/usr/bin/env python3
"""Retain actual native Minecart storage contracts and inspect every saved record.

The C fixture exercises owners and in-memory storage transitions. This driver
independently decodes its six retained GAME regions. It makes no original-game
comparison and does not describe the staged ceiling failure as a loader failure.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import uuid
import zlib


ROOT = Path(__file__).absolute().parents[3]
SCENARIO = "entity-minecart-storage"
FIXTURE = "tests/entity/minecart/test_storage.c"
INCLUDED_FIXTURE = "tests/world/runtime/tile_entities/test_tile_entities.c"


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def ordinary(path, *, directory=False):
    path = Path(os.path.abspath(path))
    for part in (path, *path.parents):
        value = part.lstat()
        require(not stat.S_ISLNK(value.st_mode) and
                not (getattr(value, "st_file_attributes", 0) & 0x400),
                "redirected path: " + str(part))
    value = path.stat()
    require(stat.S_ISDIR(value.st_mode) if directory else stat.S_ISREG(value.st_mode),
            "ordinary directory/file required: " + str(path))
    return path


def pin_bytes(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def pin_file(path):
    path = ordinary(path)
    digest, length = hashlib.sha256(), 0
    with path.open("rb") as incoming:
        for block in iter(lambda: incoming.read(1024 * 1024), b""):
            length += len(block)
            digest.update(block)
    return {"bytes": length, "sha256": digest.hexdigest()}


def save_new(path, raw):
    with path.open("xb") as output:
        require(output.write(raw) == len(raw), "complete exclusive write: " + str(path))
        output.flush()


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=True, sort_keys=True, indent=2) + "\n").encode("ascii")


def file_census(directory):
    """Keep all observed members and errors, even if another entry is unreadable."""
    files, errors = {}, []
    directory = Path(os.path.abspath(directory))

    def visit(parent):
        names = []
        try:
            ordinary(parent, directory=True)
            with os.scandir(parent) as entries:
                for entry in entries:
                    names.append(entry.name)
        except (OSError, Failure) as error:
            errors.append(str(parent) + ": " + str(error))
        for name in sorted(names):
            child = parent / name
            try:
                value = child.lstat()
                require(not stat.S_ISLNK(value.st_mode) and
                        not (getattr(value, "st_file_attributes", 0) & 0x400),
                        "redirected member: " + str(child))
                if stat.S_ISDIR(value.st_mode):
                    visit(child)
                else:
                    require(stat.S_ISREG(value.st_mode), "non-file member: " + str(child))
                    files[child.relative_to(directory).as_posix()] = child
            except (OSError, Failure) as error:
                errors.append(str(child) + ": " + str(error))
    visit(directory)
    return files, errors


def source_paths(executable):
    paths = {"executable": Path(os.path.abspath(executable)),
             "python": Path(os.path.abspath(sys.executable)),
             "project/CMakeLists.txt": ROOT / "CMakeLists.txt",
             "project/" + FIXTURE: ROOT / FIXTURE,
             "project/" + INCLUDED_FIXTURE: ROOT / INCLUDED_FIXTURE,
             "project/tests/tools/world/test_minecart_storage.py": Path(__file__).absolute()}
    errors = []
    for relative in ("include", "src"):
        members, failures = file_census(ROOT / relative)
        errors.extend(failures)
        for name, path in members.items():
            if path.suffix in (".c", ".h"):
                paths["project/" + relative + "/" + name] = path
    return dict(sorted(paths.items())), errors


def measure_paths(paths):
    measured, errors = {}, []
    for name, path in sorted(paths.items()):
        try:
            measured[name] = {"path": str(path), **pin_file(path)}
        except (OSError, Failure) as error:
            errors.append(name + ": " + str(error))
    return measured, errors


def fresh_work(base, protected):
    base = ordinary(base, directory=True)
    require(not ROOT.is_relative_to(base), "work parent must not contain project source")
    if base.is_relative_to(ROOT):
        first = base.relative_to(ROOT).parts[0]
        require(first in (".local", "build") or first.startswith("build-"),
                "repository work parent must be private or a build directory")
        if first == ".local":
            require(base == ROOT / ".local", "private captures must be fresh direct .local children")
    require(not base.is_relative_to(Path(os.path.abspath(sys.executable)).parent),
            "work parent overlaps the Python installation")
    work = base / ("minecart-storage-" + uuid.uuid4().hex)
    for path in protected:
        require(not path.is_relative_to(work) and not work.is_relative_to(path),
                "work overlaps an admitted input")
    work.mkdir(exist_ok=False)
    return work


def run_native(command, work, record):
    """One launch; wait on its actual handle without a timeout or output cap."""
    streams, child = {}, None
    record.update({"argv": command, "cwd": str(work), "stdin": "DEVNULL",
                   "created": False, "reaped": False, "exit_code": None,
                   "streams": {}, "errors": []})
    try:
        for channel in ("stdout", "stderr"):
            path = work / ("native." + channel)
            streams[channel] = path.open("xb")
            record["streams"][channel] = {"path": str(path), "opened": True, "closed": False}
        child = subprocess.Popen(command, cwd=work, stdin=subprocess.DEVNULL,
                                 stdout=streams["stdout"], stderr=streams["stderr"],
                                 shell=False)
        record["created"], record["pid"] = True, child.pid
        record["exit_code"] = child.wait()
        record["reaped"] = True
    finally:
        # An interrupted caller still waits for this same child to finish. No
        # cancellation, relaunch, truncation or replacement child is used.
        if child is not None and not record["reaped"]:
            try:
                record["exit_code"] = child.wait()
                record["reaped"] = True
            except BaseException as error:
                record["errors"].append("same-handle natural wait: " + str(error))
        for channel, output in streams.items():
            try:
                output.flush()
            except OSError as error:
                record["errors"].append(channel + " flush: " + str(error))
            try:
                output.close()
                record["streams"][channel]["closed"] = output.closed
            except OSError as error:
                record["errors"].append(channel + " close: " + str(error))
        record["all_owned_closures_known"] = (
            record["reaped"] and set(record["streams"]) == {"stdout", "stderr"} and
            all(row["closed"] for row in record["streams"].values()) and not record["errors"])
        if record["all_owned_closures_known"]:
            for channel, row in record["streams"].items():
                try:
                    row["pin"] = pin_file(Path(row["path"]))
                except (OSError, Failure) as error:
                    record["errors"].append(channel + " closed-stream pin: " + str(error))
    require(record["all_owned_closures_known"], "native child/stream closure is unknown")
    require(not record["errors"] and all("pin" in row for row in record["streams"].values()),
            "complete closed native stream pins: " + repr(record["errors"]))
    require(record["exit_code"] == 0, "native child exit: " + str(record["exit_code"]))


def utf16_units(raw):
    """Decode the NBT modified-UTF8 units without combining surrogate pairs."""
    units, offset = [], 0
    while offset < len(raw):
        first = raw[offset]
        if first <= 0x7f:
            width, value = 1, first
        elif 0xc0 <= first <= 0xdf:
            width, value = 2, first & 0x1f
        elif 0xe0 <= first <= 0xef:
            width, value = 3, first & 0x0f
        else:
            raise Failure("invalid modified-UTF8 leading byte")
        require(width <= len(raw) - offset, "complete modified-UTF8 unit")
        for index in range(1, width):
            byte = raw[offset + index]
            require(byte & 0xc0 == 0x80, "modified-UTF8 continuation")
            value = (value << 6) | (byte & 0x3f)
        units.append(value)
        offset += width
    return tuple(units)


def units(text):
    return tuple(ord(character) for character in text)


class Tag:
    def __init__(self, kind, name, value):
        self.kind, self.name, self.value = kind, name, value


def decode_nbt(raw):
    """Keep tag types, ordered compounds, UTF16 units and raw floating bits."""
    offset = 0

    def take(length):
        nonlocal offset
        require(0 <= length <= len(raw) - offset, "complete declared NBT span")
        result = raw[offset:offset + length]
        offset += length
        return result

    def number(code):
        return struct.unpack(code, take(struct.calcsize(code)))[0]

    def text():
        return utf16_units(take(number(">H")))

    def payload(kind, name):
        require(1 <= kind <= 10, "original NBT tag domain")
        if kind in (1, 2, 3, 4):
            value = number({1: ">b", 2: ">h", 3: ">i", 4: ">q"}[kind])
        elif kind in (5, 6):
            value = take(4 if kind == 5 else 8)
        elif kind == 7:
            value = take(number(">i"))
        elif kind == 8:
            value = text()
        elif kind == 9:
            child, length = number(">B"), number(">i")
            require(length >= 0 and (1 <= child <= 10 or child == length == 0),
                    "valid declared NBT list")
            # Every admitted child consumes at least one actual payload byte.
            require(length <= len(raw) - offset, "list count fits remaining payload")
            value = (child, [payload(child, ()) for _ in range(length)])
        else:
            value = []
            while True:
                child = number(">B")
                if child == 0:
                    break
                value.append(payload(child, text()))
        return Tag(kind, name, value)

    require(number(">B") == 10, "NBT Compound root")
    result = payload(10, text())
    require(result.name == () and offset == len(raw), "one complete unnamed NBT document")
    return result


def members(tag):
    require(tag.kind == 10, "Compound field owner")
    result = {}
    for child in tag.value:
        require(child.name not in result, "unique fixture Compound field name")
        result[child.name] = child
    return result


def projection(tag):
    if tag.kind in (5, 6, 7):
        value = {"hex": tag.value.hex()}
    elif tag.kind == 8:
        value = list(tag.value)
    elif tag.kind == 9:
        value = {"element_type": tag.value[0],
                 "items": [projection(item) for item in tag.value[1]]}
    elif tag.kind == 10:
        value = [projection(item) for item in tag.value]
    else:
        value = tag.value
    return {"type": tag.kind, "name_utf16": list(tag.name), "value": value}


def inflate(raw, window):
    decoder = zlib.decompressobj(window)
    result = decoder.decompress(raw) + decoder.flush()
    require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
            "one complete compressed member with no trailing bytes")
    return result


def region_document(path):
    raw = ordinary(path).read_bytes()
    require(len(raw) >= 8192 and len(raw) % 4096 == 0, "complete sector-aligned region")
    offsets, stamps = struct.unpack_from(">1024I", raw), struct.unpack_from(">1024I", raw, 4096)
    require(offsets[0] != 0 and all(value == 0 for value in offsets[1:]),
            "only region slot0 is populated")
    require(stamps[0] == 0xfedcba98 and all(value == 0 for value in stamps[1:]),
            "explicit timestamp and empty timestamp slots")
    offset, sectors = offsets[0] >> 8, offsets[0] & 255
    require(offset >= 2 and sectors > 0 and (offset + sectors) * 4096 <= len(raw),
            "complete allocated Chunk sectors")
    start = offset * 4096
    length = struct.unpack_from(">I", raw, start)[0]
    require(1 < length <= sectors * 4096 - 4 and raw[start + 4] == 2,
            "complete real zlib Chunk envelope")
    payload = inflate(raw[start + 5:start + 4 + length], zlib.MAX_WBITS)
    return decode_nbt(payload), {"region": pin_bytes(raw), "decoded_nbt": pin_bytes(payload),
                                 "slot": 0, "timestamp": stamps[0]}


def check_disk(root, report):
    """Check saved current records separately from unsaved native C transitions."""
    count = 0

    def check(condition, message):
        nonlocal count
        count += 1
        report["independent_disk_checks"] = count
        require(condition, message)

    def scalar(fields, name, kind, value):
        tag = fields.get(units(name))
        check(tag is not None and tag.kind == kind and tag.value == value,
              "exact typed saved field: " + name)

    def vector(fields, name, kind, bits):
        tag = fields.get(units(name))
        check(tag is not None and tag.kind == 9 and tag.value[0] == kind and
              [item.value.hex() for item in tag.value[1]] == bits,
              "complete raw-bit saved vector: " + name)

    def cart_record(record, saved):
        fields = members(record)
        expected = {"Pos", "Motion", "Rotation", "FallDistance", "Fire", "Air", "OnGround",
                    "Type", "Items", "id", "RootExtra", "Item", "Items\0"}
        if not saved:
            expected.update(("PushX", "PushZ", "Fuel"))
        check(set(fields) == {units(name) for name in expected},
              "complete current Minecart field domain and inactive-field policy")
        scalar(fields, "id", 8, units("Minecart"))
        scalar(fields, "Type", 3, 1)
        vector(fields, "Pos", 6, ["4020000000000000", "4050000000000000", "4020000000000000"])
        vector(fields, "Motion", 6, ["0000000000000000"] * 3)
        vector(fields, "Rotation", 5, ["00000000"] * 2)
        scalar(fields, "FallDistance", 5, bytes.fromhex("00000000"))
        scalar(fields, "Fire", 2, 0)
        scalar(fields, "Air", 2, 300)
        scalar(fields, "OnGround", 1, 0)
        for name, value in (("RootExtra", 111), ("Item", 222), ("Items\0", 333)):
            scalar(fields, name, 4, value)
        if not saved:
            scalar(fields, "PushX", 6, bytes.fromhex("7ff8000000000000"))
            scalar(fields, "PushZ", 6, bytes.fromhex("8000000000000000"))
            scalar(fields, "Fuel", 2, -7)
        items = fields[units("Items")]
        check(items.kind == 9 and items.value[0] == 10, "actual Compound Items list")
        expected_items = ([(0, 3, 10, 1, 33), (1, 3, 10, 1, 22)] if saved else
                          [(0, 1, 12, 1, 11), (1, 2, 8, 1, 22), (None, 3, 10, 1, 33),
                           (27, 4, -9, 2, 55), (-1, 5, -10, 2, 66)])
        check(len(items.value[1]) == len(expected_items), "all actual Items records, without resurrection")
        for item, (slot, identifier, amount, count_kind, marker) in zip(items.value[1], expected_items):
            child = members(item)
            names = {"id", "Count", "Damage", "Occurrence"}
            if slot is not None:
                names.add("Slot")
            check(set(child) == {units(name) for name in names},
                  "exact occurrence fields without Stack-source fallback")
            if slot is not None:
                scalar(child, "Slot", 1, slot)
            scalar(child, "id", 2, identifier)
            scalar(child, "Count", count_kind, amount)
            scalar(child, "Damage", 2, 6)
            scalar(child, "Occurrence", 4, marker)

    files, failures = file_census(root)
    check(not failures, "complete ordinary retained World inventory: " + repr(failures))
    expected_regions = {f"storage0/world{slot}/region/r.0.0.mcr" for slot in range(4)} | {
        f"storage1/world{slot}/region/r.0.0.mcr" for slot in range(2)}
    check({name for name in files if name.endswith(".mcr")} == expected_regions,
          "exact six final regions, including unsaved staged-failure input")
    for basename in ("level.dat", "session.lock"):
        check({name for name in files if name.endswith("/" + basename)} == {
            f"storage{epoch}/world{slot}/" + basename for epoch in range(2) for slot in range(4)},
            "exact eight World owners: " + basename)
    report["saved_regions"] = []
    report["world_metadata"] = []
    for epoch in range(2):
        for slot in range(4):
            prefix = f"storage{epoch}/world{slot}/"
            lock, level = root / (prefix + "session.lock"), root / (prefix + "level.dat")
            check(ordinary(lock).read_bytes() == struct.pack(">q", 137), "exact restored session token")
            data = members(decode_nbt(inflate(ordinary(level).read_bytes(), 16 + zlib.MAX_WBITS)))
            check(units("Data") in data and data[units("Data")].kind == 10, "real level.dat Data Compound")
            info = members(data[units("Data")])
            for name, kind, value in (("RandomSeed", 4, 23), ("SpawnY", 3, 64),
                                      ("LevelName", 8, units("Storage")), ("version", 3, 19132)):
                scalar(info, name, kind, value)
            report["world_metadata"].append({"path": prefix, "level": pin_file(level),
                                              "session": pin_file(lock)})
            if epoch == 1 and slot >= 2:
                continue
            name = prefix + "region/r.0.0.mcr"
            document, seals = region_document(root / name)
            fields = members(document)
            check(set(fields) == {units("Level")}, "complete Chunk root field domain")
            fields = members(fields[units("Level")])
            scalar(fields, "xPos", 3, 0)
            scalar(fields, "zPos", 3, 0)
            for array_name, length in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                                       ("BlockLight", 16384), ("HeightMap", 256)):
                array = fields.get(units(array_name))
                check(array is not None and array.kind == 7 and len(array.value) == length,
                      "complete live Chunk array: " + array_name)
            entities, tiles = fields[units("Entities")], fields[units("TileEntities")]
            check(entities.kind == 9 and entities.value[0] == 10 and len(entities.value[1]) == 1,
                  "exact one actual Minecart record")
            check(tiles.kind == 9 and not tiles.value[1], "empty complete Tile record traversal")
            record = entities.value[1][0]
            cart_record(record, epoch == 0)
            report["saved_regions"].append({"path": name, **seals,
                "route": "full-loader/save/reload/save" if epoch == 0 else
                         "seed retained after explicit staged preservation ceiling failure",
                "minecart": projection(record)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path,
                        help="existing ordinary parent for a fresh retained capture directory")
    args = parser.parse_args()
    work = root = None
    paths, before = {}, {}
    retained_paths = {}
    report = {"schema": "MINECART_STORAGE_NATIVE_DRIVER_A_V1", "scenario": SCENARIO,
              "scope": "constructed native inventory/NBT/storage contracts and retained GAME disk records",
              "original_comparison": False, "passed": False, "errors": [], "process": {},
              "native_checks": None, "native_source_cases": None, "independent_disk_checks": 0,
              "unsaved_generation_transitions": "native C assertions only; no independent disk observation",
              "staged_failure_scope": "explicit public preservation ceiling after factory/Chunk adoption; not a full-loader failure"}
    try:
        paths, failures = source_paths(args.executable)
        require(not failures, "source inventory: " + repr(failures))
        before, failures = measure_paths(paths)
        report["inputs_before"] = before
        require(not failures, "input pins: " + repr(failures))
        work = fresh_work(args.work_dir, paths.values())
        inputs = work / "inputs"
        inputs.mkdir(exist_ok=False)
        for name, path in paths.items():
            if name.startswith("project/"):
                target = inputs / name
                target.parent.mkdir(parents=True, exist_ok=True)
                retained_paths[name] = target
                raw = ordinary(path).read_bytes()
                require(pin_bytes(raw) == {key: before[name][key] for key in ("bytes", "sha256")},
                        "input changed while retaining source: " + name)
                save_new(target, raw)
        report["retained_inputs_before"], failures = measure_paths(retained_paths)
        require(not failures, "retained source pins: " + repr(failures))
        for name, row in report["retained_inputs_before"].items():
            require(all(row[key] == before[name][key] for key in ("bytes", "sha256")),
                    "retained source differs from admitted input: " + name)
        root = work / "world-\u00e9-\u4e16\u754c"
        require(not root.exists() and not root.is_symlink(), "fresh Unicode World root")
        command = [str(paths["executable"]), SCENARIO, str(root).encode("utf-8").hex()]
        run_native(command, work, report["process"])
        stdout_path, stderr_path = work / "native.stdout", work / "native.stderr"
        stdout, stderr = stdout_path.read_bytes(), stderr_path.read_bytes()
        require(not stderr, "complete native stderr must be empty")
        require(stdout.endswith(b"\n"), "complete final native stdout LF")
        lines = [line[:-1] if line.endswith(b"\r") else line for line in stdout[:-1].split(b"\n")]
        require(lines[:-1] == [b"16 achievements", b"151 recipes"] * 4,
                "only the actual four Registry bootstrap pairs precede the footer")
        match = re.fullmatch(rb"Minecart storage: ([1-9][0-9]*) checks, 14 source cases", lines[-1])
        require(match is not None, "one complete final actual native check/case footer")
        report["native_checks"], report["native_source_cases"] = int(match.group(1)), 14
        report["registry_console"] = [line.decode("ascii") for line in lines[:-1]]
        files, failures = file_census(root)
        require(not failures, "World inventory before decode: " + repr(failures))
        report["world_files_before"], failures = measure_paths(files)
        require(not failures, "World file pins before decode: " + repr(failures))
        check_disk(root, report)
    except (Exception, KeyboardInterrupt) as error:
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        # Every previously known input is remeasured even if a later source,
        # disk, parser or child check failed. A fresh census also finds additions.
        try:
            after_paths, failures = source_paths(args.executable)
            report["errors"].extend("after source census: " + value for value in failures)
            known = dict(paths)
            known.update(after_paths)
            after, failures = measure_paths(known)
            report["inputs_after"] = after
            report["errors"].extend("after input pin: " + value for value in failures)
            require(set(after_paths) == set(paths), "complete source membership changed")
            require(before == after, "executable/interpreter/source input bytes changed")
            report["inputs_unchanged"] = True
        except Exception as error:
            report["errors"].append("input afterpass: " + str(error))
        if work is not None:
            retained_files, failures = file_census(work / "inputs")
            report["errors"].extend("retained input census: " + value for value in failures)
            retained_known = dict(retained_paths)
            retained_known.update(retained_files)
            report["retained_inputs_after"], failures = measure_paths(retained_known)
            report["errors"].extend("retained input pin: " + value for value in failures)
            if "retained_inputs_before" in report:
                if (set(retained_files) != set(retained_paths) or
                        report["retained_inputs_after"] != report["retained_inputs_before"]):
                    report["errors"].append("retained input membership or bytes changed")
            for channel, row in report["process"].get("streams", {}).items():
                if row.get("closed") and "pin" in row:
                    try:
                        require(pin_file(Path(row["path"])) == row["pin"],
                                "closed native stream changed: " + channel)
                    except Exception as error:
                        report["errors"].append("stream afterpass: " + str(error))
            if (root is not None and root.exists() and
                    report["process"].get("all_owned_closures_known") is True):
                files, failures = file_census(root)
                report["errors"].extend("World inventory: " + value for value in failures)
                # Keep before-known files in this independent afterpass even
                # if a later directory census fails or a member disappears.
                known_world_files = {name: Path(row["path"]) for name, row in
                                     report.get("world_files_before", {}).items()}
                known_world_files.update(files)
                report["world_files_after"], failures = measure_paths(known_world_files)
                report["errors"].extend("World file pin: " + value for value in failures)
                if "world_files_before" in report:
                    if (set(files) != set(report["world_files_before"]) or
                            report["world_files_after"] != report["world_files_before"]):
                        report["errors"].append("retained World membership or bytes changed during decode")
            elif root is not None:
                report["world_files_deferred"] = "native child or output writer closure is unknown"
            report["passed"] = not report["errors"]
            save_new(work / "report.json", json_bytes(report))
            print("Capture: " + str(work))
    if report["passed"]:
        print("Minecart storage driver: " + str(report["native_checks"]) +
              " native checks, 14 source cases; " + str(report["independent_disk_checks"]) +
              " independent disk checks")
        return 0
    print("FAIL: " + "; ".join(report["errors"]), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
