"""Owned native-region filesystem checks; only frozen global observations claim original parity.

The remaining cases construct explicit C transport contracts. They do not compare
compressed bytes, SoftReference collection, concurrent writers or failure durability.
"""
from __future__ import annotations

import argparse
import base64
import gzip
import hashlib
import json
from pathlib import Path
import queue
import random
import struct
import subprocess
import tempfile
import threading
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/reference/storage/region_write_fixtures.json"
DATA_LIMIT = 2 * 1024 * 1024
LINE_LIMIT = DATA_LIMIT * 2 + 2048
OUTPUT_LIMIT = 64 * 1024 * 1024
OK, INVALID_ARGUMENT, MISSING, IO_ERROR, INVALID_DATA, LIMIT = 0, 1, 2, 3, 4, 7


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def exact_json(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result
    return json.loads(raw, object_pairs_hook=pairs,
                      parse_constant=lambda _: (_ for _ in ()).throw(AssertionError("nonfinite JSON")))


def bounded_integer(value, low, high):
    require(type(value) is int and low <= value <= high, "exact bounded integer")


def validate_response(raw):
    require(type(raw) is bytes and len(raw) <= LINE_LIMIT and raw.endswith(b"\n") and
            all(byte < 128 for byte in raw), "one bounded ASCII JSON response")
    result = exact_json(raw)
    require(type(result) is dict and set(result) ==
            {"status", "error", "usage", "growth", "data", "outputs_preserved"}, "complete response")
    bounded_integer(result["status"], 0, 8)
    error, usage = result["error"], result["usage"]
    require(type(error) is dict and set(error) == {"status", "operation", "system_error"}, "complete error")
    bounded_integer(error["status"], 0, 8)
    require(error["status"] == result["status"], "status agrees with diagnostic")
    bounded_integer(error["system_error"], 0, 0xffffffff)
    if result["status"] == OK:
        require(error["operation"] is None and error["system_error"] == 0, "successful error reset")
    else:
        require(type(error["operation"]) is str and 0 < len(error["operation"]) <= 256,
                "bounded failure diagnostic")
        require(result["data"] is None and result["growth"] is None, "no failed output published")
    require(type(usage) is dict and set(usage) == {"count", "bytes"}, "complete cache usage")
    bounded_integer(usage["count"], 0, 256)
    bounded_integer(usage["bytes"], 1, 128 * 1024 * 1024)
    if result["growth"] is not None:
        bounded_integer(result["growth"], 0, 0xffffffff)
    if result["data"] is not None:
        require(type(result["data"]) is str and len(result["data"]) <= DATA_LIMIT * 2 and
                len(result["data"]) % 2 == 0 and
                all(c in "0123456789abcdef" for c in result["data"]), "complete lowercase raw-byte hex")
    require(result["outputs_preserved"] is None or type(result["outputs_preserved"]) is bool,
            "typed output-retention proof")
    return result


def raw_token(value):
    require(type(value) is bytes and len(value) <= DATA_LIMIT, "bounded bytes")
    return value.hex() if value else "-"


def root_request(path):
    raw = str(path).encode("utf8") if isinstance(path, Path) else path
    require(type(raw) is bytes and len(raw) <= 32768 and b"\0" not in raw, "bounded root without NUL")
    return "ROOT " + raw_token(raw)


def write_request(x, z, timestamp, raw):
    bounded_integer(x, -(1 << 31), (1 << 31) - 1)
    bounded_integer(z, -(1 << 31), (1 << 31) - 1)
    bounded_integer(timestamp, 0, 0xffffffff)
    return f"WRITE {x} {z} {timestamp} " + raw_token(raw)


def original_mapping():
    """Read original observations independently of the reference driver/exporter."""
    require(FIXTURE.is_file() and FIXTURE.stat().st_size <= 8 * 1024 * 1024, "frozen public region evidence")
    fixture = exact_json(FIXTURE.read_bytes())
    require(fixture["schema"] == 1 and fixture["format"] == "MRW1/MRS1" and
            fixture["provenance"]["mode"] == "original-only", "original-only region corpus")
    results = []
    for side in ("client", "server"):
        packed = fixture["originals"][side]["observed"]
        bounded_integer(packed["bytes"], 1, 32 * 1024 * 1024)
        stream = zlib.decompressobj()
        raw = stream.decompress(base64.b64decode(packed["zlib_base64"], validate=True), 32 * 1024 * 1024 + 1)
        require(stream.eof and not stream.unused_data and not stream.unconsumed_tail and
                len(raw) == packed["bytes"] and hashlib.sha256(raw).hexdigest() == packed["sha256"],
                "complete fingerprinted original protocol")
        record = exact_json(raw)
        require(record["side"] == side and record["mode"] == "observed" and record["jar_sha256"] ==
                fixture["provenance"]["jar_sha256"][side], "original side binding")
        events = record["events"]
        opens = [row for row in events if row["kind"] == "open" and row["label"].startswith("mapping-")]
        state = next(row["state"] for row in events if row["kind"] == "snapshot" and row["label"] == "mapping-open")
        require(len(opens) == 9 and state["cache_count"] == 7, "complete nine-coordinate mapping observation")
        owners = {row["object"]: row["path"] for row in state["held_regions"]}
        mapping = [(tuple(row["coordinates"]), owners[row["object"]].removeprefix("mapping/")) for row in opens]
        require(all(path.startswith("region/r.") and path.endswith(".mcr") for _, path in mapping),
                "original global region paths")
        results.append(mapping)
    require(results[0] == results[1], "paired originals agree on the retained finite mapping")
    return results[0]


class Session:
    def __init__(self, executable):
        self.errors = tempfile.TemporaryFile()
        self.child = subprocess.Popen([str(executable.resolve())], stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE, stderr=self.errors)
        self.lines = queue.Queue()
        self.total_output = 0
        self.closed = False
        def reader():
            try:
                while True:
                    line = self.child.stdout.readline(LINE_LIMIT + 1)
                    if not line:
                        self.lines.put(None)
                        return
                    self.total_output += len(line)
                    require(len(line) <= LINE_LIMIT and line.endswith(b"\n") and
                            self.total_output <= OUTPUT_LIMIT, "bounded complete C stream")
                    self.lines.put(line)
            except BaseException as failure:
                self.lines.put(failure)
        self.reader = threading.Thread(target=reader, daemon=True)
        self.reader.start()

    def request(self, text):
        raw = text.encode("ascii") + b"\n"
        require(len(raw) <= LINE_LIMIT and b"\n" not in raw[:-1], "one bounded request")
        failures = []
        def send():
            try:
                self.child.stdin.write(raw)
                self.child.stdin.flush()
            except BaseException as failure:
                failures.append(failure)
        writer = threading.Thread(target=send, daemon=True)
        writer.start()
        writer.join(15)
        require(not writer.is_alive() and not failures, "complete bounded C input write")
        line = self.lines.get(timeout=15)
        require(line is not None and not isinstance(line, BaseException), "one C response per request")
        return validate_response(line)

    def root(self, path):
        result = self.request(root_request(path))
        require(result["status"] == OK, "root selection is transport-only")
        return result

    def read(self, x, z, status=OK, expected=None):
        result = self.request(f"READ {x} {z}")
        require(result["status"] == status, "expected read status: " + repr(result))
        require(result["outputs_preserved"] is (status != OK), "read output retention contract")
        if status == OK:
            require(bytes.fromhex(result["data"]) == expected, "complete binary read result")
        return result

    def write(self, x, z, raw, timestamp=0x89abcdef, status=OK):
        result = self.request(write_request(x, z, timestamp, raw))
        require(result["status"] == status, "expected write status: " + repr(result))
        require(result["outputs_preserved"] is (status != OK), "write output retention contract")
        return result

    def __enter__(self):
        return self

    def __exit__(self, kind, value, traceback):
        try:
            if kind is None:
                require(self.request("QUIT")["status"] == OK, "normal probe termination")
                self.child.stdin.close()
                require(self.child.wait(timeout=5) == 0, "C process successful exit")
                self.reader.join(5)
                require(not self.reader.is_alive() and self.lines.get(timeout=1) is None and self.lines.empty(),
                        "no omitted or unsolicited response lines")
                self.errors.seek(0)
                require(self.errors.read(32769) == b"", "silent successful C stderr")
            else:
                self.child.kill()
                self.child.wait(timeout=5)
        finally:
            if self.child.poll() is None:
                self.child.kill()
                self.child.wait(timeout=5)
            if not self.child.stdin.closed:
                self.child.stdin.close()
            self.child.stdout.close()
            self.errors.close()


def region_image(records, free_sectors=0):
    """Construct read/import inputs independently; this is not the writer allocator."""
    raw = bytearray(8192)
    for slot, selector, compressed, timestamp in records:
        needed = (len(compressed) + 5 + 4095) // 4096
        offset = len(raw)
        require(1 <= needed <= 255, "small constructed record extent")
        raw.extend(bytes([0xa7]) * (needed * 4096))
        struct.pack_into(">I", raw, slot * 4, (offset // 4096) * 256 + needed)
        struct.pack_into(">I", raw, 4096 + slot * 4, timestamp)
        struct.pack_into(">I", raw, offset, len(compressed) + 1)
        raw[offset + 4] = selector
        raw[offset + 5:offset + 5 + len(compressed)] = compressed
    raw.extend(bytes([0xd3]) * (free_sectors * 4096))
    return bytes(raw)


def slot(raw, x=0, z=0):
    index = (x & 31) + 32 * (z & 31)
    location = struct.unpack_from(">I", raw, index * 4)[0]
    offset = (location >> 8) * 4096
    length = struct.unpack_from(">I", raw, offset)[0]
    require(location and location & 255 and offset >= 8192 and length >= 1 and
            length + 4 <= (location & 255) * 4096 and offset + length + 4 <= len(raw), "complete disk record")
    selector = raw[offset + 4]
    payload = raw[offset + 5:offset + length + 4]
    if selector == 1:
        decoded = gzip.decompress(payload)
    elif selector == 2:
        stream = zlib.decompressobj()
        decoded = stream.decompress(payload) + stream.flush()
        require(stream.eof and not stream.unused_data and not stream.unconsumed_tail,
                "C-written record length names exactly one complete zlib stream")
    else:
        raise AssertionError("supported disk selector")
    return decoded, struct.unpack_from(">I", raw, 4096 + index * 4)[0], offset, length + 4


def preserved_outside(before, after, ranges):
    require(len(before) == len(after), "constructed no-growth rewrite")
    require(all(a == b or any(start <= i < end for start, end in ranges)
                for i, (a, b) in enumerate(zip(before, after))), "all unrelated/dead bytes preserved")


def put_region(root, image, x=0, z=0):
    path = root / "region" / f"r.{x // 32}.{z // 32}.mcr"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(image)
    return path


def check_original_global_paths(executable, directory):
    root = directory / "mapping"
    with Session(executable) as session:
        baseline = session.root(root)["usage"]["bytes"]
        expected = {}
        for (x, z), relative in original_mapping():
            row = session.read(x, z, MISSING)
            expected[relative] = bytes(8192)
            require(row["usage"]["count"] == len(expected), "original live-hit identity membership")
            require((root / relative).read_bytes() == expected[relative], "original-derived filename and empty header")
        actual = {path.relative_to(root).as_posix(): path.read_bytes() for path in root.rglob("*.mcr")}
        require(actual == expected, "complete original-derived global file inventory")
        require(session.request("CHECK")["status"] == OK, "argument/output ownership checks")
        cleared = session.request("CLEAR")
        require(cleared["usage"] == {"count": 0, "bytes": baseline}, "clear releases all retained bytes")
        require(actual == {path.relative_to(root).as_posix(): path.read_bytes() for path in root.rglob("*.mcr")},
                "clear performs no file writes")


def check_cache_and_shared_roots(executable, directory):
    root = directory / "cache"
    with Session(executable) as session:
        baseline = session.root(root)["usage"]["bytes"]
        for i in range(256):
            row = session.read(i * 32, 0, MISSING)
            require(row["usage"]["count"] == i + 1, "each distinct region retained")
        full = session.request("USAGE")["usage"]
        require(session.read(0, 0, MISSING)["usage"] == full, "hit at256 keeps the entire cache")
        row = session.read(256 * 32, 0, MISSING)
        require(row["usage"]["count"] == 1 and baseline < row["usage"]["bytes"] < full["bytes"],
                "257th miss clears the whole cache")
        require(len(list((root / "region").iterdir())) == 257, "cache eviction preserves all files")
        other = directory / "DIM-1"
        session.root(other)
        session.write(0, 0, b"dimension one")
        session.root(root)
        session.write(0, 0, b"dimension zero")
        session.root(other)
        session.read(0, 0, expected=b"dimension one")
        require(session.request("USAGE")["usage"]["count"] == 3, "one cache distinguishes full lexical roots")
        session.root(root)
        session.read(0, 0, expected=b"dimension zero")


def check_raw_unicode_and_growth(executable, directory):
    root = directory / "caf\u00e9-\u4e16\u754c-\U0001f600"
    payloads = [b"", b"\0\xff\r\n" + bytes(range(256)), random.Random(17).randbytes(65537)]
    with Session(executable) as session:
        session.root(root)
        path = root / "region" / "r.-1.-1.mcr"
        for i, payload in enumerate(payloads):
            before = path.stat().st_size if path.exists() else 0
            row = session.write(-1, -1, payload, 0xffffffff - i)
            raw = path.read_bytes()
            require(row["growth"] == len(raw) - before, "new-header/growth accumulated exactly once")
            require(slot(raw, -1, -1)[:2] == (payload, 0xffffffff - i), "Python independently reads C zlib/raw timestamp")
            session.read(-1, -1, expected=payload)
        before = path.read_bytes()
        session.request("CLEAR")
        row = session.write(-1, -1, b"short", 0)
        after = path.read_bytes()
        require(row["growth"] == 0 and len(after) == len(before), "reopened existing header adds no growth; freed run reused")
        require(slot(after, -1, -1)[:2] == (b"short", 0), "zero timestamp is explicit")
        for x, z in ((-32, -33), (-2147483648, 2147483647), (2147483647, -2147483648)):
            value = struct.pack(">ii", x, z) + b"\0\xff"
            session.write(x, z, value)
            actual = (root / "region" / f"r.{x // 32}.{z // 32}.mcr").read_bytes()
            require(slot(actual, x, z)[0] == value, "constructed signed division/local-low-five-bit routing")
            session.read(x, z, expected=value)


def check_interop_and_dead_bytes(executable, directory):
    root = directory / "interop"
    values = [b"gzip\0\xff", bytes(range(256)) * 128, b"", b"last\n"]
    before = region_image([(0, 1, gzip.compress(values[0], mtime=0), 17),
                           (1, 2, zlib.compress(values[1]), 18),
                           (32, 1, gzip.compress(values[2], mtime=0), 19),
                           (33, 2, zlib.compress(values[3]), 20)], free_sectors=2)
    path = put_region(root, before)
    with Session(executable) as session:
        session.root(root)
        for coordinates, value in zip(((0, 0), (1, 0), (0, 1), (1, 1)), values):
            session.read(*coordinates, expected=value)
        require(path.read_bytes() == before, "gzip/zlib reads never rewrite bytes")
        value = b"new\0binary\xff"
        require(session.write(2, 0, value, 0x80000000)["growth"] == 0, "first free sector reused")
        after = path.read_bytes()
        decoded, timestamp, offset, length = slot(after, 2, 0)
        require((decoded, timestamp) == (value, 0x80000000), "new record and timestamp independently decoded")
        preserved_outside(before, after, [(8, 12), (4104, 4108), (offset, offset + length)])
        for coordinates, original in zip(((0, 0), (1, 0), (0, 1), (1, 1)), values):
            session.read(*coordinates, expected=original)
        require(not list(path.parent.glob("*.tmp")), "successful replacement consumes its temporary file")


def check_malformed_and_native_errors(executable, directory):
    good = region_image([(0, 2, zlib.compress(b"valid"), 1)])
    overlap = bytearray(good)
    overlap[4:8] = overlap[:4]
    bad_extent = bytearray(good)
    struct.pack_into(">I", bad_extent, 0, 257)
    unsafe = [b"", bytes(8191), bytes(8192) + b"tail", bytes(overlap), bytes(bad_extent)]
    with Session(executable) as session:
        for i, raw in enumerate(unsafe):
            root = directory / f"unsafe-{i}"
            path = put_region(root, raw)
            session.root(root)
            session.read(0, 0, MISSING if i == 2 else INVALID_DATA)
            require(path.read_bytes() == raw, "malformed read never pads or repairs")
            session.write(0, 0, b"replacement", status=INVALID_DATA)
            require(path.read_bytes() == raw and not list(path.parent.glob("*.tmp")), "unsafe write preserves original file")
        for i, raw in enumerate((region_image([(0, 3, b"opaque", 0)]),
                                 region_image([(0, 2, zlib.compress(b"valid")[:-2], 0)]))):
            root = directory / f"bad-payload-{i}"
            path = put_region(root, raw)
            session.root(root)
            session.read(0, 0, INVALID_DATA)
            require(path.read_bytes() == raw, "invalid selected payload remains untouched")
        blocker = directory / "file-as-root"
        blocker.write_bytes(b"owned blocker")
        session.root(blocker)
        session.read(0, 0, IO_ERROR)
        require(blocker.read_bytes() == b"owned blocker", "mkdir failure never removes a preexisting file")
        session.root(b"\xff")
        session.read(0, 0, INVALID_ARGUMENT)
        session.root(b"")
        session.write(0, 0, b"x", status=INVALID_ARGUMENT)


def check_limits_and_failure_outputs(executable, directory):
    # Seven numbers: cache,image,writer peak,input,output,codec peak,members.
    def limits(session, cache=1 << 20, image=1 << 20, writer=4 << 20,
               source=DATA_LIMIT, output=DATA_LIMIT, allocation=4 << 20, members=4, status=OK):
        row = session.request(f"LIMITS {cache} {image} {writer} {source} {output} {allocation} {members}")
        require(row["status"] == status, "configured resource status")
        return row
    with Session(executable) as session:
        initial = session.request("USAGE")["usage"]
        require(limits(session, cache=0, status=LIMIT)["usage"] == initial, "failed owner creation retains old owner")
        require(limits(session, members=0, status=INVALID_ARGUMENT)["usage"] == initial, "invalid owner limits retain old owner")
        limits(session, cache=initial["bytes"])
        root = directory / "cache-limit"
        session.root(root)
        session.read(0, 0, LIMIT)
        require(not root.exists(), "known path-budget failure precedes filesystem setup")
        limits(session, image=8191)
        session.root(directory / "image-limit")
        session.read(0, 0, LIMIT)
        require(not list((directory / "image-limit").rglob("*.mcr")), "header limit leaves no target file")
        limits(session, image=8192)
        root = directory / "growth-limit"
        session.root(root)
        session.write(0, 0, b"x", status=LIMIT)
        path = root / "region" / "r.0.0.mcr"
        require(path.read_bytes() == bytes(8192), "failed payload growth publishes no changed image")
        session.read(0, 0, MISSING)
        for label, options in (("source", {"source": 3}), ("encoded", {"output": 3}),
                               ("allocation", {"allocation": 0})):
            limits(session, **options)
            root = directory / ("codec-limit-" + label)
            session.root(root)
            session.write(0, 0, b"longer", status=LIMIT)
            require(not root.exists(), "encode failure precedes opening region files")
        limits(session, output=3)
        root = directory / "decode-limit"
        raw = region_image([(0, 2, zlib.compress(b"longer"), 0)])
        path = put_region(root, raw)
        session.root(root)
        session.read(0, 0, LIMIT)
        require(path.read_bytes() == raw, "decode limit preserves file and output sentinel")
        limits(session)
        root = directory / "retained-limit"
        path = root / "region" / "r.0.0.mcr"
        budget = initial["bytes"] + len(str(path).encode("utf8")) + 1 + 8192
        limits(session, cache=budget)
        session.root(root)
        session.read(0, 0, MISSING)
        retained = session.request("USAGE")["usage"]
        session.write(0, 0, b"x", status=LIMIT)
        require(session.request("USAGE")["usage"] == retained and path.read_bytes() == bytes(8192),
                "cache replacement limit preserves retained image/count/bytes")


def check_replace_and_collision_cleanup(executable, directory):
    root = directory / "replace"
    original = region_image([(0, 2, zlib.compress(b"original"), 123)])
    path = put_region(root, original)
    with Session(executable) as session:
        session.root(root)
        session.read(0, 0, expected=b"original")
        retained = session.request("USAGE")["usage"]
        preexisting = path.with_name(path.name + ".mc-0000000000000000.tmp")
        preexisting.write_bytes(b"preexisting; never owned by write")
        path.unlink()
        path.mkdir()
        session.write(0, 0, b"replacement", status=IO_ERROR)
        require(path.is_dir() and preexisting.read_bytes() == b"preexisting; never owned by write" and
                sorted(p.name for p in path.parent.glob("*.tmp")) == [preexisting.name],
                "failed replacement cleans only its newly owned temporary file")
        require(session.request("USAGE")["usage"] == retained, "failed replacement leaves retained image accounting")
        session.read(0, 0, expected=b"original")
        path.rmdir()
        path.write_bytes(original)
        require(session.write(0, 0, b"replacement")["growth"] == 0, "later write can reuse unchanged cached image")
        require(slot(path.read_bytes())[0] == b"replacement" and preexisting.read_bytes() == b"preexisting; never owned by write",
                "successful retry preserves unrelated existing temporary file")
    root = directory / "collisions"
    with Session(executable) as session:
        session.root(root)
        session.read(0, 0, MISSING)
        path = root / "region" / "r.0.0.mcr"
        files = {path.with_name(path.name + f".mc-{i:016x}.tmp"): f"owned fixture {i}".encode() for i in range(64)}
        for name, raw in files.items():
            name.write_bytes(raw)
        row = session.write(0, 0, b"x", status=IO_ERROR)
        require(row["error"]["operation"] == "region temporary file collisions", "bounded64-collision diagnostic")
        require(path.read_bytes() == bytes(8192) and all(name.read_bytes() == raw for name, raw in files.items()),
                "exhausted replacement never touches target or preexisting temporaries")
        require(session.write(0, 0, b"retry")["growth"] == 12288, "failed write does not consume pending header growth")
        require(all(name.read_bytes() == raw for name, raw in files.items()) and
                set(path.parent.glob("*.tmp")) == set(files), "successful retry removes only its own temporary file")


class TransportTests(unittest.TestCase):
    def test_unicode_root_is_utf8_hex_without_argv_conversion(self):
        path = Path("caf\u00e9-\u4e16\u754c-\U0001f600")
        request = root_request(path)
        self.assertEqual(bytes.fromhex(request.split()[1]), str(path).encode("utf8"))
        for value in (b"has\0nul", b"x" * 32769):
            with self.assertRaises(AssertionError): root_request(value)

    def test_request_preserves_unsigned_time_signed_extremes_and_binary_bytes(self):
        self.assertEqual(write_request(-(1 << 31), (1 << 31) - 1, 0xffffffff, b"\0\xff"),
                         "WRITE -2147483648 2147483647 4294967295 00ff")
        self.assertTrue(write_request(0, 0, 0, b"").endswith(" -"))
        for x, timestamp, raw in ((True, 0, b""), (0, -1, b""), (0, 0, "text"), (0, 0, bytes(DATA_LIMIT + 1))):
            with self.assertRaises(AssertionError): write_request(x, 0, timestamp, raw)

    def test_strict_response_rejects_omissions_coercions_and_failed_payloads(self):
        value = {"status": 0, "error": {"status": 0, "operation": None, "system_error": 0},
                 "usage": {"count": 0, "bytes": 1}, "growth": None, "data": "00ff", "outputs_preserved": False}
        self.assertEqual(validate_response(json.dumps(value).encode() + b"\n"), value)
        for name in ("missing", "boolean", "error", "failed_data", "hex", "ownership"):
            row = json.loads(json.dumps(value))
            if name == "missing": del row["usage"]
            elif name == "boolean": row["status"] = True
            elif name == "error": row["error"]["system_error"] = 3
            elif name == "failed_data": row["status"] = row["error"]["status"] = 2; row["error"]["operation"] = "missing"
            elif name == "hex": row["data"] = "AB"
            else: row["outputs_preserved"] = 1
            with self.subTest(name=name), self.assertRaises(AssertionError): validate_response(json.dumps(row).encode() + b"\n")

    def test_original_global_mapping_proof_is_retained_and_finite(self):
        mapping = original_mapping()
        self.assertEqual(len(mapping), 9)
        self.assertEqual(len({path for _, path in mapping}), 7)
        self.assertIn(((-2147483648, 2147483647), "region/r.-67108864.67108863.mcr"), mapping)
        self.assertIn(((-1, -1), "region/r.-1.-1.mcr"), mapping)


def malformed_inputs(executable):
    for raw in (b"\n", b"UNKNOWN\n", b"READ 2147483648 0\n", b"READ -2147483649 0\n",
                b"ROOT 00\n", b"ROOT zz\n", b"ROOT f\n", b"READ 0 0 extra\n",
                b"WRITE 0 0 4294967296 -\n", b"WRITE +0 0 0 -\n", b"USAGE  \n",
                b"READ 0 0", b"ROOT \xff\n"):
        result = subprocess.run([str(executable.resolve())], input=raw, capture_output=True, timeout=10)
        require(result.returncode == 2 and result.stdout == b"", "malformed request rejected before any output")
    result = subprocess.run([str(executable.resolve())], input=b"QUIT\nUSAGE\n", capture_output=True, timeout=10)
    require(result.returncode == 2 and len(result.stdout.splitlines()) == 1, "commands after QUIT cannot be ignored")
    result = subprocess.run([str(executable.resolve())], input=b"USAGE\n" * 4097, capture_output=True, timeout=10)
    require(result.returncode == 2 and len(result.stdout.splitlines()) == 4096,
            "command budget rejects the4097th request without losing prior complete responses")
    for line in result.stdout.splitlines(keepends=True):
        require(validate_response(line)["status"] == OK, "bounded-prefix response integrity")
    oversized = (b"WRITE 0 0 0 " + b"00" * DATA_LIMIT + b"\n") * 4
    result = subprocess.run([str(executable.resolve())], input=oversized, capture_output=True, timeout=15)
    lines = result.stdout.splitlines(keepends=True)
    require(result.returncode == 2 and len(lines) == 3, "16MiB input budget rejects the fourth complete-sized request")
    require(all(validate_response(line)["status"] == INVALID_ARGUMENT for line in lines),
            "input-budget prefix preserves prior NULL-root failure responses")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args(argv)
    with tempfile.TemporaryDirectory(prefix="mc-regions-") as temporary:
        directory = Path(temporary)
        for check in (check_original_global_paths, check_cache_and_shared_roots, check_raw_unicode_and_growth,
                      check_interop_and_dead_bytes, check_malformed_and_native_errors,
                      check_limits_and_failure_outputs, check_replace_and_collision_cleanup):
            check(args.executable, directory)
    malformed_inputs(args.executable)
    print("native regions: 7 owned filesystem scenarios;9 original-derived global mappings;16 malformed/bounded transport guards")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
