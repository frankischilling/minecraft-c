"""Check the C memory codec against Python's independent gzip/zlib readers."""

from __future__ import annotations

import argparse
import gzip
from pathlib import Path
import random
import struct
import subprocess
import zlib


def gzip_member(payload: bytes, flags: int = 0) -> bytes:
    header = bytearray(b"\x1f\x8b\x08" + bytes([flags]) + b"\0\0\0\0\0\xff")
    if flags & 4:
        header += struct.pack("<H", 5) + b"extra"
    if flags & 8:
        header += b"fixture.bin\0"
    if flags & 16:
        header += b"public synthetic fixture\0"
    if flags & 2:
        header += struct.pack("<H", zlib.crc32(header) & 0xFFFF)
    compressor = zlib.compressobj(wbits=-15)
    return bytes(header) + compressor.compress(payload) + compressor.flush() + struct.pack(
        "<II", zlib.crc32(payload), len(payload) & 0xFFFFFFFF
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    rng = random.Random(173)
    requests: list[tuple[str, str, bytes, bytes | None, int | None]] = []

    def add(label: str, operation: str, value: bytes, expected: bytes | None,
            consumed: int | None = None) -> None:
        requests.append((label, operation, value, expected, consumed))

    payloads = [b"", b"\0", bytes(range(256)), b"abc\0xyz" * 20000]
    payloads += [rng.randbytes(size) for size in (2, 31, 4096, 32768, 65537, 131071)]
    for index, payload in enumerate(payloads):
        for fmt in ("gzip", "zlib"):
            compressed = gzip_member(payload) if fmt == "gzip" else zlib.compress(payload)
            add(f"python-to-c-{fmt}-{index}", f"decode-{fmt}", compressed, payload, len(compressed))
            add(f"c-to-python-{fmt}-{index}", f"encode-{fmt}", payload, payload, len(payload))
            add(f"tail-{fmt}-{index}", f"decode-{fmt}", compressed + b"tail", payload, len(compressed))

    raw = b"public\0gzip payload\xff" * 3
    # Exercise each optional field combination, FTEXT, and header checksums.
    for flags in range(32):
        member = gzip_member(raw, flags)
        assert gzip.decompress(member) == raw
        add(f"optional-{flags}", "decode-gzip", member, raw, len(member))
    first, empty, last = gzip_member(raw[:3]), gzip_member(b""), gzip_member(raw[3:])
    joined = empty + first + empty + last + empty
    assert gzip.decompress(joined) == raw
    add("split-members", "decode-gzip", joined + b"tail", raw, len(joined))
    zfirst, zlast = zlib.compress(raw[:3]), zlib.compress(raw[3:])
    add("zlib-first-stream", "decode-zlib", zfirst + zlast, raw[:3], len(zfirst))

    minimal = gzip_member(b"\x0a\0\0\0")
    for length in range(len(minimal)):
        add(f"truncation-{length}", "decode-gzip", minimal[:length], None)
    for offset in (2, 3, -8, -4):
        malformed = bytearray(minimal)
        malformed[offset] ^= 0x80
        add(f"corrupt-{offset}", "decode-gzip", bytes(malformed), None)
    add("recognized-broken-second", "decode-gzip", minimal + b"\x1f\x8b", None)
    add("wrong-gzip-framing", "decode-gzip", zlib.compress(raw), None)
    add("wrong-zlib-framing", "decode-zlib", gzip_member(raw), None)
    dictionary = b"the test dictionary"
    compressor = zlib.compressobj(zdict=dictionary)
    add("dictionary", "decode-zlib", compressor.compress(raw) + compressor.flush(), None)
    malformed = bytearray(gzip_member(raw, 2))
    malformed[10] ^= 1
    add("bad-header-crc", "decode-gzip", bytes(malformed), None)

    result = subprocess.run(
        [str(args.executable.resolve())],
        input="".join(f"{operation}\t{value.hex()}\n" for _, operation, value, _, _ in requests),
        text=True, capture_output=True, timeout=60, check=False,
    )
    if result.returncode:
        raise RuntimeError(f"C probe exited {result.returncode}: {result.stderr[:2000]}")
    lines = result.stdout.splitlines()
    if len(lines) != len(requests):
        raise AssertionError(f"C probe returned {len(lines)} records for {len(requests)} requests")
    for (label, operation, _, expected, consumed), line in zip(requests, lines):
        fields = line.split("\t")
        if expected is None:
            if len(fields) != 2 or fields[0] != "ERR":
                raise AssertionError(f"{label}: expected failure, got {line[:100]}")
            continue
        if len(fields) != 3 or fields[0] != "OK":
            raise AssertionError(f"{label}: expected success, got {line[:100]}")
        if int(fields[1]) != consumed:
            raise AssertionError(f"{label}: incorrect consumed count")
        output = bytes.fromhex(fields[2])
        if operation.startswith("encode-"):
            fmt = operation.removeprefix("encode-")
            decoder = zlib.decompressobj(31 if fmt == "gzip" else 15)
            decoded = decoder.decompress(output) + decoder.flush()
            if decoded != expected or not decoder.eof or decoder.unused_data:
                raise AssertionError(f"{label}: C container failed independent decode")
            if fmt == "gzip" and (output[:10] != b"\x1f\x8b\x08\0\0\0\0\0\0\xff"
                                   or gzip.decompress(output) != expected):
                raise AssertionError(f"{label}: gzip metadata/reader mismatch")
        elif output != expected:
            raise AssertionError(f"{label}: decompressed bytes differ")
    print(f"Compression interoperability: {len(requests)} records passed; "
          f"Python zlib runtime {zlib.ZLIB_RUNTIME_VERSION}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
