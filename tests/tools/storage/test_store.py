"""Owned-world store integration: frozen original inputs plus explicit C policies.

Every filesystem path belongs to a TemporaryDirectory. Original-derived checks
reuse the public level/chunk observations; no JDK, personal world, decoder or
terrain-feature parity is inferred from the constructed ownership/failure cases.
The persistent ASCII protocol is also usable by copied-world live drivers.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import queue
import struct
import subprocess
import sys
import tempfile
import threading
import zlib

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts/reference/storage"))
try:
    import compare_level as level
    import export_level as level_export
    import compare_chunk_storage as chunks
    import export_chunk_storage as chunk_export
finally:
    sys.path.pop(0)

DATA_LIMIT = 2 * 1024 * 1024 - 256
LINE_LIMIT = 4 * 1024 * 1024
RESPONSE_LIMIT = 8 * 1024 * 1024
INPUT_LIMIT, OUTPUT_LIMIT = 32 * 1024 * 1024, 128 * 1024 * 1024
OK, INVALID_ARGUMENT, MISSING, IO_ERROR, INVALID_DATA, UNSUPPORTED, SESSION_MISMATCH, LIMIT = range(8)
IDENTITY_MAP = bytes(range(256))
DEFAULT_LIMITS = [512, 64 * 1024 * 1024, 64 * 1024 * 1024, 128 * 1024 * 1024,
                  1000000, 1000000, 64, 64 * 1024 * 1024, 64 * 1024 * 1024,
                  128 * 1024 * 1024, 1024]


def require(value, message):
    if not value:
        raise AssertionError(message)


def same(left, right):
    """Exact JSON types, dictionary keys and ordered list values."""
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(same(left[key], right[key]) for key in left)
    if type(left) is list:
        return len(left) == len(right) and all(same(a, b) for a, b in zip(left, right))
    return left == right


def bounded_int(value, low, high):
    require(type(value) is int and low <= value <= high, "exact bounded integer")


def raw_hex(value, limit=DATA_LIMIT):
    require(type(value) is str and len(value) <= limit * 2 and len(value) % 2 == 0 and
            all(char in "0123456789abcdef" for char in value), "bounded lowercase hex")
    return bytes.fromhex(value)


def bits(value):
    require(type(value) is int and -(1 << 63) <= value < 1 << 64, "64-bit value")
    return f"{value & ((1 << 64) - 1):016x}"


def token(raw):
    require(type(raw) is bytes and len(raw) <= DATA_LIMIT, "bounded payload")
    return raw.hex() if raw else "-"


def validate_response(raw):
    require(type(raw) is bytes and len(raw) <= RESPONSE_LIMIT and raw.endswith(b"\n") and
            raw.count(b"\n") == 1 and all(byte < 128 for byte in raw), "bounded complete ASCII response")
    value = level.strict_json(raw)
    require(type(value) is dict and set(value) == {"status", "error", "selected", "stores", "usage",
            "info", "metadata_hex", "handle", "chunk", "growth", "outputs_preserved", "acquire"}, "complete response")
    bounded_int(value["status"], 0, 8)
    error = value["error"]
    require(type(error) is dict and set(error) == {"status", "operation", "system_error"}, "complete diagnostic")
    bounded_int(error["status"], 0, 8)
    require(error["status"] == value["status"], "status matches diagnostic")
    bounded_int(error["system_error"], 0, 0xffffffff)
    if value["status"] == OK:
        require(error["operation"] is None and error["system_error"] == 0, "successful diagnostic reset")
    else:
        require(type(error["operation"]) is str and 0 < len(error["operation"]) <= 256,
                "static bounded failure operation")
    bounded_int(value["selected"], 0, 1)
    require(type(value["stores"]) is list and len(value["stores"]) == 2, "two owner slots")
    for store in value["stores"]:
        require(type(store) is dict and set(store) == {"open", "chunks"} and type(store["open"]) is bool,
                "exact store summary")
        bounded_int(store["chunks"], 0, 512)
        require(store["open"] or store["chunks"] == 0, "closed owner has no chunks")
    usage = value["usage"]
    require(type(usage) is dict and set(usage) == {"count", "bytes"}, "complete shared cache usage")
    bounded_int(usage["count"], 0, 256)
    bounded_int(usage["bytes"], 1, 128 * 1024 * 1024)
    info = value["info"]
    if info is not None:
        require(type(info) is dict and set(info) == {"seed", "spawn", "time", "last_played", "size_on_disk",
                "level_name", "version", "rain_time", "raining", "thunder_time", "thundering",
                "has_player", "player_dimension"}, "complete level view")
        for name in ("seed", "time", "last_played", "size_on_disk"):
            require(len(raw_hex(info[name], 8)) == 8, "complete long bits")
        raw_hex(info["level_name"], 65535)
        require(len(info["level_name"]) % 4 == 0, "UTF16 units retained")
        require(type(info["spawn"]) is list and len(info["spawn"]) == 3, "three spawn coordinates")
        for coordinate in info["spawn"]:
            bounded_int(coordinate, -(1 << 31), (1 << 31) - 1)
        for name in ("version", "rain_time", "thunder_time", "player_dimension"):
            bounded_int(info[name], -(1 << 31), (1 << 31) - 1)
        for name in ("raining", "thundering", "has_player"):
            require(type(info[name]) is bool, "exact level boolean")
        require(value["metadata_hex"] is not None, "metadata owns the info view")
    require((info is None) == (value["metadata_hex"] is None), "info and retained document agree")
    if value["metadata_hex"] is not None:
        chunks.parse_raw(raw_hex(value["metadata_hex"]))
    chunk = value["chunk"]
    if chunk is not None:
        bounded_int(value["handle"], 0, 1024)
        require(type(chunk) is dict and set(chunk) == {"coordinates", "blank", "flags", "last_save_time", "data"},
                "complete chunk view")
        require(type(chunk["coordinates"]) is list and len(chunk["coordinates"]) == 2, "two chunk coordinates")
        for coordinate in chunk["coordinates"]:
            bounded_int(coordinate, -(1 << 31), (1 << 31) - 1)
        require(type(chunk["blank"]) is bool and type(chunk["flags"]) is dict and
                set(chunk["flags"]) == set("cnopq") and all(type(bit) is bool for bit in chunk["flags"].values()),
                "exact runtime flags")
        require(len(raw_hex(chunk["last_save_time"], 8)) == 8, "complete save time bits")
        if chunk["blank"]:
            require(value["handle"] == 0 and chunk["data"] is None, "borrowed blank fallback")
        else:
            require(value["handle"] > 0 and type(chunk["data"]) is dict and
                    set(chunk["data"]) == {"blocks", "metadata", "skylight", "block_light", "heightmap", "minimum_height"},
                    "full normal data")
            for name, length in (("blocks", 32768), ("metadata", 16384), ("skylight", 16384),
                                 ("block_light", 16384), ("heightmap", 256)):
                array = chunk["data"][name]
                require(type(array) is dict and set(array) == {"length", "values"}, "complete array")
                bounded_int(array["length"], length, length)
                require(len(raw_hex(array["values"], length)) == length, "all array bytes")
            bounded_int(chunk["data"]["minimum_height"], -(1 << 31), (1 << 31) - 1)
    else:
        require(value["handle"] is None, "no unpublished handle")
    if value["growth"] is not None:
        bounded_int(value["growth"], 0, 0xffffffff)
        require(value["status"] == OK, "growth only on success")
    require(value["outputs_preserved"] is None or type(value["outputs_preserved"]) is bool,
            "exact output-retention proof")
    acquisition = value["acquire"]
    if acquisition is not None:
        require(type(acquisition) is dict and set(acquisition) == {"status", "loader_status", "entries",
                "insertions", "pending", "callbacks"}, "complete real coordinator state")
        bounded_int(acquisition["status"], 0, 5)
        bounded_int(acquisition["loader_status"], 0, 8)
        require(type(acquisition["entries"]) is list and len(acquisition["entries"]) <= 128, "finite map")
        keys = []
        for row in acquisition["entries"]:
            require(type(row) is list and len(row) == 2, "map row")
            bounded_int(row[0], -(1 << 31), (1 << 31) - 1)
            bounded_int(row[1], 0, 1024)
            keys.append(row[0])
        require(len(set(keys)) == len(keys), "unique packed keys")
        for name, count in (("insertions", 256), ("pending", 128)):
            require(type(acquisition[name]) is list and len(acquisition[name]) <= count, "finite ordered storage")
            for element in acquisition[name]:
                bounded_int(element, 0 if name == "insertions" else -(1 << 31),
                            1024 if name == "insertions" else (1 << 31) - 1)
        require(type(acquisition["callbacks"]) is list and len(acquisition["callbacks"]) <= 9, "bounded empty registrations")
        for row in acquisition["callbacks"]:
            require(type(row) is dict and set(row) == {"kind", "handle", "active", "slice"} and
                    row["kind"] in ("tiles", "entities") and type(row["active"]) is bool, "complete registration")
            bounded_int(row["handle"], 1, 1024)
            require(row["active"] is True, "coordinator activates before registration")
            if row["kind"] == "tiles":
                require(row["slice"] is None, "tile callback")
            else:
                bounded_int(row["slice"], 0, 7)
    return value


class Session:
    """One bounded persistent process; usable by owned copied-world drivers."""
    def __init__(self, executable):
        self.errors = tempfile.TemporaryFile()
        self.child = subprocess.Popen([str(Path(executable).resolve())], stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE, stderr=self.errors)
        self.lines = queue.Queue()
        self.total_output = self.total_input = self.commands = 0
        def reader():
            try:
                while True:
                    raw = self.child.stdout.readline(RESPONSE_LIMIT + 1)
                    if not raw:
                        self.lines.put(None)
                        return
                    self.total_output += len(raw)
                    require(len(raw) <= RESPONSE_LIMIT and raw.endswith(b"\n") and self.total_output <= OUTPUT_LIMIT,
                            "bounded complete output stream")
                    self.lines.put(raw)
            except BaseException as error:
                self.lines.put(error)
        self.reader = threading.Thread(target=reader, daemon=True)
        self.reader.start()

    def request(self, text, status=OK):
        raw = text.encode("ascii") + b"\n"
        self.commands += 1
        self.total_input += len(raw)
        require(len(raw) <= LINE_LIMIT + 1 and self.commands <= 4096 and self.total_input <= INPUT_LIMIT and
                b"\n" not in raw[:-1], "bounded input")
        failures = []
        def write():
            try:
                self.child.stdin.write(raw)
                self.child.stdin.flush()
            except BaseException as error:
                failures.append(error)
        writer = threading.Thread(target=write, daemon=True)
        writer.start()
        writer.join(15)
        require(not writer.is_alive() and not failures, "complete bounded input write")
        response = self.lines.get(timeout=15)
        require(type(response) is bytes, "exactly one C response: " + repr(response))
        value = validate_response(response)
        require(value["status"] == status, "expected status for " + text[:100] + ": " + repr(value["error"]))
        if value["outputs_preserved"] is not None:
            require(value["outputs_preserved"] is (status != OK), "failure preserves result output")
        return value

    def open(self, path, slot=0, token_value=23, players=False, block_map=IDENTITY_MAP, status=OK):
        path_bytes = str(path).encode("utf8") if isinstance(path, Path) else path
        require(type(path_bytes) is bytes and 0 < len(path_bytes) <= 32768 and b"\0" not in path_bytes,
                "owned bounded path")
        return self.request(f"OPEN {slot} {path_bytes.hex()} {bits(token_value)} {int(players)} {block_map.hex()}", status)

    def initialize(self, raw=None, clock=1234, status=OK):
        raw = level.encode(level.compound("", [level.compound("Data", [])])) if raw is None else raw
        return self.request(f"INIT_INFO {bits(clock)} {token(raw)}", status)

    def limits(self, **changes):
        names = ("chunks", "nbt_input", "nbt_output", "nbt_allocation", "tags", "elements", "depth",
                 "compression_input", "compression_output", "compression_allocation", "members")
        require(set(changes) <= set(names), "known limit names")
        return self.request("LIMITS " + " ".join(str(changes.get(name, value)) for name, value in zip(names, DEFAULT_LIMITS)))

    def __enter__(self):
        return self

    def __exit__(self, kind, value, traceback):
        try:
            if kind is None:
                self.request("QUIT")
                self.child.stdin.close()
                require(self.child.wait(timeout=5) == 0, "successful adapter exit")
                self.reader.join(5)
                require(not self.reader.is_alive() and self.lines.get(timeout=1) is None and self.lines.empty(),
                        "no omitted/unsolicited response")
                self.errors.seek(0)
                require(self.errors.read(32769) == b"", "no sanitizer/internal stderr")
        finally:
            if self.child.poll() is None:
                self.child.kill()
                self.child.wait(timeout=5)
            if not self.child.stdin.closed:
                self.child.stdin.close()
            self.child.stdout.close()
            self.errors.close()


def fixture_inputs():
    metadata = level_export.load_fixture(ROOT / "tests/reference/storage/level_fixtures.json")
    chunk_fixture = chunk_export.load_fixture(ROOT / "tests/reference/storage/chunk_storage_fixtures.json")
    output = []
    for side in ("client", "server"):
        source = next(row for row in metadata["contexts"] if row["side"] == side)
        observed = level.strict_json(level_export.unpacked(source["observation"]))
        before = {path: level_export.unpacked(row) for path, row in source["before"].items()}
        chunk_source = next(row for row in chunk_fixture["contexts"] if row["side"] == side)
        chunk_observed = chunks.strict_json(chunk_export.unpacked(chunk_source["record"]))
        artifacts = {path: chunk_export.unpacked(row, chunks.MAX_NBT) for path, row in chunk_source["artifacts"].items()}
        output.append((side, observed, before, chunk_observed, artifacts))
    return output


def info_projection(original):
    return {"seed": original["random_seed_hex"], "spawn": original["spawn"], "time": original["time_hex"],
            "last_played": original["last_played_hex"], "size_on_disk": original["size_on_disk_hex"],
            "level_name": original["level_name_utf16"], "version": original["version"],
            "rain_time": original["rain_time"], "raining": original["raining"],
            "thunder_time": original["thunder_time"], "thundering": original["thundering"],
            "has_player": original["player_present"], "player_dimension": original["dimension"]}


def inventory(root):
    return {path.relative_to(root).as_posix(): path.read_bytes() for path in sorted(root.rglob("*")) if path.is_file()}


def read_nbt(raw):
    return chunks.parse_raw(raw)


def disk_info(root):
    return read_nbt(zlib.decompress((root / "level.dat").read_bytes(), 31))


def install_record(root, dimension, x, z, raw):
    """Construct transport input only, without reproducing the writer allocator."""
    packed = zlib.compress(raw)
    sectors = (len(packed) + 5 + 4095) // 4096
    require(0 < sectors < 256, "bounded constructed record")
    image = bytearray(8192 + sectors * 4096)
    index = (x & 31) + 32 * (z & 31)
    struct.pack_into(">I", image, index * 4, 2 * 256 + sectors)
    struct.pack_into(">I", image, 4096 + index * 4, 0x89abcdef)
    struct.pack_into(">I", image, 8192, len(packed) + 1)
    image[8196] = 2
    image[8197:8197 + len(packed)] = packed
    path = root / ("DIM-1/region" if dimension == -1 else "region") / f"r.{x // 32}.{z // 32}.mcr"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(image)
    return path


def read_record(root, dimension, x, z):
    path = root / ("DIM-1/region" if dimension == -1 else "region") / f"r.{x // 32}.{z // 32}.mcr"
    raw = path.read_bytes()
    index = (x & 31) + 32 * (z & 31)
    location = struct.unpack_from(">I", raw, index * 4)[0]
    offset, count = (location >> 8) * 4096, location & 255
    require(count > 0 and offset >= 8192 and offset + count * 4096 <= len(raw), "valid emitted location")
    length = struct.unpack_from(">I", raw, offset)[0]
    require(1 < length <= count * 4096 - 4 and raw[offset + 4] in (1, 2), "valid emitted record")
    unpacked = zlib.decompress(raw[offset + 5:offset + 4 + length], 31 if raw[offset + 4] == 1 else 15)
    return read_nbt(unpacked), struct.unpack_from(">I", raw, 4096 + index * 4)[0]


def check_sessions(executable, directory, originals):
    del originals
    root = directory / "session-世界"
    with Session(executable) as session:
        first = session.open(root, token_value=-(1 << 63) + 42)
        require(first["info"] is None and first["usage"]["count"] == 0, "open does not load/save chunks or metadata")
        require((root / "data").is_dir() and not (root / "players").exists(), "false flag still creates data")
        lock = root / "session.lock"
        expected = struct.pack(">q", -(1 << 63) + 42)
        require(lock.read_bytes() == expected, "exact explicit big-endian session token")
        lock.write_bytes(expected + b"x" * (2 * 1024 * 1024))
        session.request("SESSION")
        require(lock.stat().st_size == 2 * 1024 * 1024 + 8, "prefix read does not truncate trailing bytes")
        for raw in (b"", expected[:7], struct.pack(">q", 3)):
            lock.write_bytes(raw)
            session.request("SESSION", SESSION_MISMATCH)
            require(lock.read_bytes() == raw, "failed check does not rewrite token")
        lock.unlink()
        session.request("SESSION", SESSION_MISMATCH)
        require(not lock.exists(), "missing lock remains missing")
        second = session.open(root, slot=1, token_value=99, players=True)
        require(second["selected"] == 1 and (root / "players").is_dir(), "second actual owner and optional players")
        session.request("SELECT 0")
        session.request("SESSION", SESSION_MISMATCH)
        session.request("CLOSE")
        require(lock.read_bytes() == struct.pack(">q", 99), "close never removes another token")
        session.request("SELECT 1")
        session.request("SESSION")
        session.request("CLOSE")
        require(lock.exists() and not (root / "level.dat").exists(), "no implicit metadata save or lock deletion")
        session.open(b"\xff", status=INVALID_ARGUMENT)
        session.request("CHECK")


def check_metadata_originals(executable, directory, originals):
    for side, observed, files, _, _ in originals:
        with Session(executable) as session:
            for row in observed["handlers"]:
                name = row["case"]
                root = directory / (side + "-metadata-" + name)
                prefix = "handler-" + name + "/world/"
                root.mkdir()
                for path, raw in files.items():
                    if path.startswith(prefix):
                        target = root / path.removeprefix(prefix)
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(raw)
                session.open(root)
                before = inventory(root)
                status = OK if row["load"]["status"] == "info" else INVALID_DATA if name == "both-bad" else MISSING
                result = session.request("LOAD_INFO", status)
                require(inventory(root) == before, "metadata reads preserve complete input files")
                if status == OK:
                    require(same(result["info"], info_projection(row["load"]["info"])), "full original-derived handler fields: " + name)
                else:
                    require(result["info"] is None, "no silent new-world fallback")
                session.request("CLOSE")
            for row in observed["codecs"]:
                root = directory / (side + "-codec-" + row["case"])
                root.mkdir()
                (root / "level.dat").write_bytes(files["codec-" + row["case"] + "/input.dat"])
                session.open(root)
                expected = row["constructor"]
                status = OK if expected["status"] == "ok" else INVALID_DATA
                result = session.request("LOAD_INFO", status)
                if status == OK:
                    require(same(result["info"], info_projection(expected["info"])), "full codec/default/exact-type input")
                session.request("CLOSE")


def check_metadata_save_and_recovery(executable, directory, originals):
    _, _, files, _, _ = originals[0]
    root = directory / "metadata-save"
    root.mkdir()
    original = files["codec-full/input.dat"]
    (root / "level.dat").write_bytes(original)
    with Session(executable) as session:
        session.open(root)
        loaded = session.request("LOAD_INFO")
        before_info, before_root = loaded["info"], read_nbt(bytes.fromhex(loaded["metadata_hex"]))
        saved = session.request("SAVE_INFO " + bits(1234))
        require(saved["info"]["last_played"] == before_info["last_played"] and saved["info"]["version"] == 19132,
                "live loaded LastPlayed is separate from explicit output clock/version")
        require((root / "level.dat_old").read_bytes() == original and not (root / "level.dat_new").exists(),
                "checked rotation preserves exact previous complete file")
        first_tree = disk_info(root)
        data = level.child(first_tree, "Data")
        require(level.child(data, "LastPlayed")["value"] == 1234 and level.child(data, "version")["value"] == 19132,
                "explicit metadata serialization clock/version")
        for parent, source in ((first_tree, before_root), (data, level.child(before_root, "Data"))):
            unknown = "UnknownRoot" if parent is first_tree else "UnknownData"
            require(same(level.normalized(level.child(parent, unknown)), level.normalized(level.child(source, unknown))),
                    "unknown metadata retained")
        require(same(level.normalized(level.child(data, "Player")),
                     level.normalized(level.child(level.child(before_root, "Data"), "Player"))), "opaque Player retained")
        retained_files = inventory(root)
        (root / "level.dat").write_bytes(b"bad-primary")
        (root / "level.dat_old").write_bytes(b"bad-backup")
        corrupt_files = inventory(root)
        rejected = session.request("LOAD_INFO", INVALID_DATA)
        require(same(rejected["info"], saved["info"]) and rejected["metadata_hex"] == saved["metadata_hex"]
                and inventory(root) == corrupt_files, "failed metadata reload preserves prior owned view and every input byte")
        for path, raw in retained_files.items():
            (root / path).write_bytes(raw)
        stable = saved["info"]
        baseline = inventory(root)
        session.request("PLAYER_DROP")
        refused = session.request("SAVE_INFO " + bits(7), UNSUPPORTED)
        require(same(refused["info"], stable) and inventory(root) == baseline, "Player drop failure leaves views and files unchanged")
        session.request("LOAD_INFO")
        before_foreign = inventory(root)
        session.initialize(zlib.decompress(original, 31), status=UNSUPPORTED)
        require(inventory(root) == before_foreign, "foreign equal-valued Player cannot replace owned identity")
        # Saving directly from the retained view exercises string/Player rebinding after old document free.
        session.request("SAVE_INFO " + bits(-1))
        again = session.request("SAVE_INFO " + bits(99))
        require(again["info"]["level_name"] == before_info["level_name"] and again["info"]["has_player"],
                "successive saves retain owned UTF16 and Player views")
        require(level.child(level.child(disk_info(root), "Data"), "LastPlayed")["value"] == 99, "second explicit clock")
        # Both current and old must be regular before old deletion is attempted.
        for obstacle in ("level.dat", "level.dat_old", "level.dat_new"):
            if (root / obstacle).exists():
                (root / obstacle).unlink()
            (root / obstacle).mkdir()
            before = inventory(root)
            view = session.request("INFO")
            failed = session.request("SAVE_INFO " + bits(100), IO_ERROR)
            after = inventory(root)
            require(same(failed["info"], view["info"]) and failed["metadata_hex"] == view["metadata_hex"], "rotation failure preserves live views")
            for path, raw in before.items():
                if path != "level.dat_new":
                    require(after[path] == raw, "rotation preflight preserves recovery/current files")
            (root / obstacle).rmdir()
            if obstacle != "level.dat_new":
                require((root / "level.dat_new").is_file(), "completed new metadata survives preflight failure")
            # Restore native files from valid independently retained bytes for next obstacle.
            for name in ("level.dat", "level.dat_old"):
                (root / name).write_bytes(original)
        session.request("CLOSE")
    fresh = directory / "fresh-info"
    with Session(executable) as session:
        session.open(fresh)
        session.initialize(zlib.decompress(original, 31), status=UNSUPPORTED)
        require(not (fresh / "level.dat").exists(), "cannot introduce foreign opaque Player")
        tree = read_nbt(zlib.decompress(original, 31))
        level.child(tree, "Data")["value"] = [x for x in level.child(tree, "Data")["value"] if x["name_utf16"] != level.u16("Player")]
        raw = level.encode(tree)
        frozen = bytes(raw)
        result = session.initialize(raw, clock=10)
        require(raw == frozen and not result["info"]["has_player"] and result["info"]["level_name"] == before_info["level_name"],
                "initializer input immutable and copied strings owned")
        require(level.child(disk_info(fresh), "UnknownRoot") is None, "fresh info initialization does not pretend to import unknown baseline tree")
    io_root = directory / "metadata-io-fallback"
    (io_root / "level.dat").mkdir(parents=True)
    (io_root / "level.dat_old").write_bytes(files["codec-defaults/input.dat"])
    with Session(executable) as session:
        session.open(io_root)
        fallback = session.request("LOAD_INFO")
        require(fallback["info"]["seed"] == bits(0) and (io_root / "level.dat").is_dir(),
                "primary I/O failure permits valid backup without replacing obstacle")


def check_chunks_and_accounting(executable, directory, originals):
    _, _, _, observed, artifacts = originals[0]
    original_raw = artifacts[observed["cases"][0]["input"]["artifact"]]
    root = directory / "chunk-created"
    with Session(executable) as session:
        session.open(root)
        session.initialize()
        session.request("INFO_EDIT size_on_disk " + bits((1 << 63) - 2))
        info = session.request("SAVE_INFO " + bits(5))
        disk_before = (root / "level.dat").read_bytes()
        total = int(info["info"]["size_on_disk"], 16)
        previous = []
        for dimension, x, z in ((0, 0, 0), (-1, -32, -33), (0, -(1 << 31), (1 << 31) - 1),
                                (-1, (1 << 31) - 1, -(1 << 31))):
            tree = read_nbt(original_raw)
            level.child(level.child(tree, "Level"), "xPos")["value"] = x
            level.child(level.child(tree, "Level"), "zPos")["value"] = z
            raw = level.encode(tree)
            retained = session.request(f"RETAIN {dimension} {token(raw)}")
            original_state = chunks.projection(observed["cases"][0]["static"]["chunk_before_write"])
            original_state["coordinates"] = [x, z]
            require(same(retained["chunk"], original_state), "retain copies every original-derived source byte/field")
            handle = retained["handle"]
            session.request(f"EDIT {handle} o 1")
            session.request(f"EDIT {handle} c 1")
            session.request(f"EDIT {handle} p 1")
            session.request(f"EDIT {handle} r {bits(-71)}")
            edited = session.request(f"EDIT {handle} minimum_height -7")
            written = session.request(f"WRITE {handle} {bits(1234)} 4294967295")
            require(same(written["chunk"], edited["chunk"]), "write leaves every runtime flag/time/data byte unchanged")
            growth = written["growth"]
            total = (total + growth) & ((1 << 64) - 1)
            require(written["info"]["size_on_disk"] == bits(total), "growth accounted with Java long wrap")
            serialized, timestamp = read_record(root, dimension, x, z)
            require(timestamp == 0xffffffff and level.child(level.child(serialized, "Level"), "LastUpdate")["value"] == 1234,
                    "explicit region timestamp and LastUpdate")
            loaded = session.request(f"LOAD {dimension} {x} {z}")
            require(loaded["handle"] != handle and loaded["chunk"]["coordinates"] == [x, z] and
                    loaded["chunk"]["data"]["minimum_height"] == 0, "new stable identity and decoded transient minimum")
            require(loaded["chunk"]["data"] | {"minimum_height": -7} == edited["chunk"]["data"], "all five arrays preserved")
            require(loaded["chunk"]["flags"] == {"c": False, "n": edited["chunk"]["flags"]["n"], "o": False, "p": False, "q": False}
                    and loaded["chunk"]["last_save_time"] == bits(0), "load transient defaults")
            previous.append((handle, edited["chunk"]))
            for old_handle, expected in previous:
                require(same(session.request(f"CHUNK {old_handle}")["chunk"], expected), "later loads do not invalidate earlier owners")
        require((root / "level.dat").read_bytes() == disk_before, "chunk writes do not implicitly save live SizeOnDisk")
        usage = session.request("USAGE")["usage"]
        session.request("CLOSE")
        session.request(f"CHUNK {previous[0][0]}", INVALID_ARGUMENT)
        require(same(session.request("USAGE")["usage"], usage), "store free does not clear shared region owner")
        require((root / "level.dat").read_bytes() == disk_before and (root / "session.lock").exists(), "free has no hidden save/lock deletion")
        session.request("CLEAR")
        require(session.request("USAGE")["usage"]["count"] == 0, "explicit region clear")
        shared = []
        for index in (0, 1):
            session.open(directory / ("shared-world-" + str(index)), slot=index)
            session.initialize()
            retained = session.request("RETAIN 0 " + token(original_raw))
            shared.append(retained["handle"])
            session.request(f"WRITE {retained['handle']} {bits(index)} 0")
        shared_usage = session.request("USAGE")["usage"]
        require(shared_usage["count"] == 2, "two simultaneous stores retain independent global region paths")
        session.request("SELECT 0")
        session.request("CLOSE")
        session.request("SELECT 1")
        require(same(session.request("USAGE")["usage"], shared_usage) and
                session.request(f"CHUNK {shared[1]}")["chunk"] is not None,
                "closing first owner preserves shared cache and second owner's identities")


def check_original_chunk_imports(executable, directory, originals):
    compared = refused = 0
    for side, _, _, observed, artifacts in originals:
        with Session(executable) as session:
            for index, row in enumerate(observed["cases"]):
                root = directory / (side + "-import-" + row["name"])
                root.mkdir()
                raw = artifacts[row["input"]["artifact"]]
                path = install_record(root, 0, 1, 0, raw)
                before = path.read_bytes()
                table = bytes.fromhex(observed["registry_before"]["map"]["values"])
                session.open(root, block_map=table)
                status = OK if row["name"] in chunks.SUPPORTED else INVALID_DATA
                loaded = session.request("LOAD 0 1 0", status)
                require(path.read_bytes() == before, "load never rewrites source region")
                if status == OK:
                    expected = chunks.projection(row["region"]["chunk_before_write"])
                    require(same(loaded["chunk"], expected), "full original-derived postload shape/flags/normalization: " + row["name"])
                    compared += 1
                else:
                    require(loaded["stores"][0]["chunks"] == 0 and loaded["handle"] is None, "bad record never retained")
                    refused += 1
                session.request("CLOSE")
                session.request("CLEAR")
    require(compared == 18 and refused == 38, "all paired finite original-derived imports and explicit malformed-array departures")


def check_acquisition_and_opaque(executable, directory, originals):
    _, _, _, observed, artifacts = originals[0]
    raw_for = {row["name"]: artifacts[row["input"]["artifact"]] for row in observed["cases"]}
    root = directory / "acquisition"
    install_record(root, 0, 1, 0, raw_for["valid_saved"])
    opaque_root = directory / "opaque"
    install_record(opaque_root, 0, 1, 0, raw_for["unknown_entity"])
    malformed_root = directory / "malformed"
    install_record(malformed_root, 0, 1, 0, raw_for["short_all_arrays"])
    with Session(executable) as session:
        session.open(root)
        session.initialize()
        loaded = session.request("ACQUIRE 0 1 0 " + bits(1234))
        handle = loaded["handle"]
        require(loaded["chunk"]["flags"]["c"] is True and loaded["chunk"]["last_save_time"] == bits(1234), "real load stamp then activation")
        callbacks = loaded["acquire"]["callbacks"]
        require([x["kind"] for x in callbacks] == ["tiles"] + ["entities"] * 8 and
                [x["slice"] for x in callbacks] == [None] + list(range(8)) and
                all(x["handle"] == handle for x in callbacks), "nine actual ordered empty registrations")
        require(loaded["acquire"]["entries"] == [[65536, handle]] and loaded["acquire"]["insertions"] == [handle], "real requested-key publication")
        hit = session.request("ACQUIRE 0 1 0 " + bits(9999))
        require(hit["handle"] == handle and hit["stores"][0]["chunks"] == 1 and
                hit["chunk"]["last_save_time"] == bits(1234) and hit["acquire"]["callbacks"] == [] and
                same(hit["acquire"]["entries"], loaded["acquire"]["entries"]), "hit retains identity, count, stamp and registration")
        missing = session.request("ACQUIRE 0 2 0 " + bits(7))
        require(missing["handle"] == 0 and missing["chunk"]["blank"] and missing["stores"][0]["chunks"] == 1 and
                missing["acquire"]["loader_status"] == MISSING and missing["acquire"]["callbacks"] == [],
                "missing alone permits real blank fallback without invented generation")
        session.request("ACQUIRE -1 1 0 " + bits(0), UNSUPPORTED)
        session.request("CACHE_RESET")
        session.request("CLOSE")
        for path in (opaque_root, malformed_root):
            session.open(path)
            failed = session.request("ACQUIRE 0 1 0 " + bits(1234), UNSUPPORTED if path == opaque_root else INVALID_DATA)
            require(failed["acquire"]["entries"] == [] and failed["acquire"]["insertions"] == [] and
                    failed["stores"][0]["chunks"] == 0 and failed["acquire"]["callbacks"] == [],
                    "fatal/opaque load cannot publish, activate or retain a result")
            session.request("CLOSE")
        # C opaque-preservation policy intentionally differs from original factory discard.
        for name in ("unknown_entity", "unknown_tile", "valid_boat_and_sign"):
            path = directory / ("passthrough-" + name)
            source = read_nbt(raw_for[name])
            original_coords = [level.child(level.child(source, "Level"), key)["value"] for key in ("xPos", "zPos")]
            install_record(path, 0, *original_coords, raw_for[name])
            session.open(path)
            session.initialize()
            loaded = session.request("LOAD 0 " + " ".join(map(str, original_coords)))
            handle = loaded["handle"]
            written = session.request(f"WRITE {handle} {bits(-1)} 17")
            require(same(written["chunk"], loaded["chunk"]), "opaque transport preserves q/r and all runtime state")
            output, _ = read_record(path, 0, *original_coords)
            expected = copy.deepcopy(source)
            level.child(level.child(expected, "Level"), "LastUpdate")["value"] = -1
            require(same(level.normalized(output), level.normalized(expected)), "opaque/unknown source preserved except explicit LastUpdate")
            session.request(f"BYTE {handle} blocks 0 255")
            before = inventory(path)
            info_before = session.request("INFO")["info"]
            failed = session.request(f"WRITE {handle} {bits(0)} 0", UNSUPPORTED)
            require(inventory(path) == before and failed["growth"] is None and same(failed["info"], info_before),
                    "unsafe opaque edit refuses output without disk/accounting changes")
            session.request("RETAIN 0 " + token(raw_for[name]), UNSUPPORTED)
            session.request("CLOSE")
        # Wrong Level coordinates are retained and the writer routes by returned coordinates.
        path = directory / "mislocation"
        input_path = install_record(path, 0, 1, 0, raw_for["wrong_location"])
        input_before = input_path.read_bytes()
        session.open(path)
        session.initialize()
        loaded = session.request("LOAD 0 1 0")
        require(loaded["chunk"]["coordinates"] == [-7, 19], "mislocation is not repaired")
        session.request(f"WRITE {loaded['handle']} {bits(8)} 9")
        require(input_path.read_bytes() == input_before, "write addresses stored coordinates, preserving requested slot")
        _, stamp = read_record(path, 0, -7, 19)
        require(stamp == 9, "stored-coordinate output slot exists")


def check_limits_and_failures(executable, directory, originals):
    _, _, files, observed, artifacts = originals[0]
    raw = artifacts[observed["cases"][0]["input"]["artifact"]]
    with Session(executable) as session:
        # The backup fits these budgets while the primary does not: fallback
        # would return success and therefore cannot accidentally satisfy the test.
        backup = files["codec-defaults/input.dat"]
        for label, changes in (("count", {"chunks": 0}),
                               ("nbt", {"nbt_input": len(zlib.decompress(backup, 31))}),
                               ("allocation", {"nbt_allocation": 0}),
                               ("compression", {"compression_input": len(backup)})):
            path = directory / ("limits-" + label)
            path.mkdir()
            (path / "level.dat").write_bytes(files["codec-full/input.dat"])
            (path / "level.dat_old").write_bytes(files["codec-defaults/input.dat"])
            install_record(path, 0, 1, 0, raw)
            session.limits(**changes)
            session.open(path)
            if label != "count":
                session.request("LOAD_INFO", LIMIT)
                require(session.request("INFO")["info"] is None, "resource failure cannot select older metadata")
            else:
                session.request("LOAD 0 1 0", LIMIT)
                fatal = session.request("ACQUIRE 0 1 0 " + bits(0), LIMIT)
                require(fatal["acquire"]["entries"] == [] and fatal["stores"][0]["chunks"] == 0, "resource failure is fatal before publication")
            session.request("CLOSE")
        session.limits(depth=0)
        invalid = directory / "invalid-depth"
        session.open(invalid, status=INVALID_ARGUMENT)
        require(not invalid.exists(), "invalid context rejected before filesystem effects")
        session.limits(members=0)
        session.open(directory / "invalid-members", status=INVALID_ARGUMENT)
        session.limits()
        output_path = directory / "save-output-budget"
        output_path.mkdir()
        (output_path / "level.dat").write_bytes(files["codec-full/input.dat"])
        session.limits(nbt_output=0)
        session.open(output_path)
        before_view = session.request("LOAD_INFO")
        before_files = inventory(output_path)
        failed = session.request("SAVE_INFO " + bits(1), LIMIT)
        require(same(failed["info"], before_view["info"]) and failed["metadata_hex"] == before_view["metadata_hex"]
                and inventory(output_path) == before_files, "serialization limit preserves owned state and files")
        session.request("CLOSE")
        session.limits()
        bad_map = bytearray(IDENTITY_MAP)
        bad_map[1] = 2
        session.open(directory / "invalid-map", block_map=bytes(bad_map), status=INVALID_ARGUMENT)
        path = directory / "save-failures"
        session.open(path)
        retained = session.request("RETAIN 0 " + token(raw))
        session.request(f"WRITE {retained['handle']} {bits(1)} 1", UNSUPPORTED)
        session.initialize()
        before = inventory(path)
        (path / "session.lock").write_bytes(b"bad")
        expected = inventory(path)
        view = session.request("INFO")["info"]
        result = session.request(f"WRITE {retained['handle']} {bits(1)} 1", SESSION_MISMATCH)
        require(inventory(path) == expected and result["growth"] is None and same(result["info"], view),
                "failed chunk session check preserves files/growth/live accounting")
        session.request("SAVE_INFO " + bits(1), SESSION_MISMATCH)
        require(inventory(path) == expected and set(expected) == set(before), "failed metadata check creates no recovery file")
        session.request("LOAD 7 0 0", UNSUPPORTED)
        session.request("RETAIN -2 " + token(raw), UNSUPPORTED)
        session.request("CHUNK 1024", INVALID_ARGUMENT)
        session.request("CHECK")


def check_protocol(executable, directory, originals):
    invalid = (b"\n", b" USAGE\n", b"USAGE \n", b"USAGE\r\n", b"UNKNOWN\n", b"USAGE", b"\xff\n",
               b"SELECT 2\n", b"SELECT 00\n", b"LOAD 0 -0 0\n", b"LOAD 0 2147483648 0\n",
               b"INIT_INFO 0 -\n", b"OPEN 0 00 0000000000000000 0 " + b"00" * 256 + b"\n",
               b"OPEN 0 ff 0000000000000000 0 0\n", b"X" * (LINE_LIMIT + 1) + b"\n")
    for raw in invalid:
        result = subprocess.run([str(executable.resolve())], input=raw, capture_output=True, timeout=15)
        require(result.returncode == 2 and result.stdout == b"" and result.stderr == b"", "malformed request fails before output")
    result = subprocess.run([str(executable.resolve())], input=b"QUIT\nUSAGE\n", capture_output=True, timeout=15)
    require(result.returncode == 2 and len(result.stdout.splitlines()) == 1, "trailing commands after QUIT rejected")
    result = subprocess.run([str(executable.resolve())], input=b"USAGE\n" * 4097, capture_output=True, timeout=15)
    lines = result.stdout.splitlines(keepends=True)
    require(result.returncode == 2 and len(lines) == 4096 and result.stderr == b"", "finite command count")
    first = validate_response(lines[0])
    for line in lines:
        require(same(validate_response(line), first), "all bounded-prefix responses intact")
    for mutate in (lambda x: x.update(status=False), lambda x: x["usage"].update(count=0.0),
                   lambda x: x["stores"][0].update(open=0), lambda x: x["error"].update(status=0.0)):
        value = copy.deepcopy(first)
        mutate(value)
        try:
            validate_response((json.dumps(value) + "\n").encode())
        except AssertionError:
            pass
        else:
            raise AssertionError("same public response validator accepted bool/int/float coercion")
    oversized_total = (b"RETAIN 0 " + b"00" * DATA_LIMIT + b"\n") * 9
    result = subprocess.run([str(executable.resolve())], input=oversized_total, capture_output=True, timeout=30)
    lines = result.stdout.splitlines(keepends=True)
    require(result.returncode == 2 and len(lines) == 8 and result.stderr == b"", "32MiB input cap with complete preceding responses")
    require(all(validate_response(line)["status"] == INVALID_DATA for line in lines), "budget prefix preserves semantic failures")
    # Exercise the actual output quota without retaining128MiB in Python RAM.
    # All repeated complete lines must remain byte-identical; only the final
    # failed response may be a prefix. The child must report failure, not pass.
    observed, artifacts = originals[0][3:]
    raw = artifacts[observed["cases"][0]["input"]["artifact"]]
    path = directory / "output-budget"
    wire = (f"OPEN 0 {str(path).encode('utf8').hex()} {bits(0)} 0 {IDENTITY_MAP.hex()}\n"
            f"RETAIN 0 {token(raw)}\n" + "CHUNK 1\n" * 1024).encode("ascii")
    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        child = subprocess.run([str(executable.resolve())], input=wire, stdout=output, stderr=errors, timeout=30)
        require(child.returncode == 1 and OUTPUT_LIMIT - 1024 <= output.tell() <= OUTPUT_LIMIT,
                "128MiB output bound is an explicit process failure")
        errors.seek(0)
        require(errors.read(32769) == b"world store probe contract or resource failure\n", "output-bound diagnostic")
        output.seek(0)
        validate_response(output.readline(RESPONSE_LIMIT + 1))
        validate_response(output.readline(RESPONSE_LIMIT + 1))
        repeated = output.readline(RESPONSE_LIMIT + 1)
        validate_response(repeated)
        complete = 1
        while True:
            line = output.readline(RESPONSE_LIMIT + 1)
            if not line:
                break
            if line.endswith(b"\n"):
                require(line == repeated, "complete output-budget prefix retained")
                complete += 1
            else:
                require(repeated.startswith(line) and not output.read(1), "last failed response is only a bounded prefix")
        require(complete >= 700, "finite substantial output prefix actually exercised")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args(argv)
    originals = fixture_inputs()
    checks = (check_sessions, check_metadata_originals, check_metadata_save_and_recovery,
              check_chunks_and_accounting, check_original_chunk_imports, check_acquisition_and_opaque,
              check_limits_and_failures)
    with tempfile.TemporaryDirectory(prefix="mc-world-store-") as temporary:
        for check in checks:
            check(args.executable, Path(temporary), originals)
        check_protocol(args.executable, Path(temporary), originals)
    print("world store: 7 owned integration scenarios;52 paired original-derived metadata inputs;56 paired chunk imports;bounded strict transport guards")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
