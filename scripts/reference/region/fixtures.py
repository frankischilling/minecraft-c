"""Deterministic small region inputs; importing this module performs no I/O.

Each case has a label, immutable file bytes and ordered local-coordinate
queries. Callers must make a separate owned copy for every query because the
original region constructor can change short or unaligned input files.
"""

import struct
import zlib

SECTOR = 4096


def _gzip(raw):
    """One gzip member with a fixed timestamp, flags, XFL and OS byte."""
    encoder = zlib.compressobj(9, zlib.DEFLATED, -15)
    payload = encoder.compress(raw) + encoder.flush()
    return (bytes.fromhex("1f8b08000000000002ff") + payload
            + struct.pack("<II", zlib.crc32(raw) & 0xFFFFFFFF, len(raw) & 0xFFFFFFFF))


def named(kind, name, payload):
    return bytes([kind]) + struct.pack(">H", len(name)) + name + payload


def root(x, z):
    fields = named(3, b"xPos", struct.pack(">i", x)) + named(3, b"zPos", struct.pack(">i", z))
    return named(10, b"", named(10, b"Level", fields + b"\0") + b"\0")


def entry(data, x, z, start, count, timestamp=0):
    index = x + z * 32
    struct.pack_into(">I", data, index * 4, ((start << 8) | count) & 0xFFFFFFFF)
    struct.pack_into(">I", data, 4096 + index * 4, timestamp)


def record(data, sector, payload, selector=2, length=None):
    if length is None:
        length = len(payload) + 1
    body = struct.pack(">I", length & 0xFFFFFFFF) + bytes([selector]) + payload
    data[sector * SECTOR:sector * SECTOR + len(body)] = body


def cases():
    """Return the complete 36-case, 49-query corpus as new Python objects."""
    result = []

    def add(label, data, queries=((0, 0),)):
        if len(data) >= 65536:
            raise ValueError("region fixture must be smaller than 64 KiB")
        result.append({"label": label, "bytes": bytes(data), "queries": list(queries)})

    add("empty-header", bytes(8192), [(0, 0), (31, 31), (-1, 0), (32, 0)])
    mapped = bytearray(SECTOR * 5)
    for x, z, sector, selector, raw, stamp in [(0, 0, 2, 1, root(-1, -33), 0x80000000),
                                              (31, 0, 3, 2, root(31, 0), 0xFFFFFFFF),
                                              (0, 1, 4, 2, root(0, 1), 0x12345678)]:
        entry(mapped, x, z, sector, 1, stamp)
        record(mapped, sector, _gzip(raw) if selector == 1 else zlib.compress(raw), selector)
    add("coordinate-and-timestamp-mapping", mapped,
        [(0, 0), (31, 0), (0, 1), (1, 0), (32, 0), (-1, 1), (-1, 0), (0, -1), (31, 31)])
    raw = root(-1, -33)
    compressed = zlib.compress(raw)
    for label, start, count in [("same-sector-overlap", 2, 1), ("range-overlap", 2, 2)]:
        data = bytearray(SECTOR * 4)
        entry(data, 0, 0, start, count)
        entry(data, 1, 0, 2 if count == 1 else 3, 1)
        record(data, 2, compressed)
        if count == 2:
            record(data, 3, zlib.compress(root(99, 99)))
        add(label, data, [(0, 0), (1, 0)])
    for label, start, count in [("past-end", 3, 1), ("zero-count", 2, 0),
                                ("header-sector-zero", 0, 1), ("negative-offset-zero-count", 0xFFFFFF, 0),
                                ("negative-offset-one-count", 0xFFFFFF, 1),
                                ("high-bit-offset", 0x800000, 1), ("count-255-past-end", 2, 255)]:
        data = bytearray(SECTOR * 3)
        entry(data, 0, 0, start, count)
        record(data, 2, compressed)
        add(label, data)
    data = bytearray(SECTOR * 3)
    entry(data, 0, 0, 1, 1)
    record(data, 1, compressed)
    add("header-sector-one-readable", data)
    for length in [-1, 0, 1, 4092, 4093, 4095, 4096, 4097]:
        data = bytearray(SECTOR * 3)
        entry(data, 0, 0, 2, 1)
        record(data, 2, compressed if length != 1 else b"", length=length)
        add("zlib-declared-length-" + str(length), data)
    for selector in [0, 1, 2, 3, 255]:
        data = bytearray(SECTOR * 3)
        entry(data, 0, 0, 2, 1)
        record(data, 2, b"", selector=selector, length=1)
        add("empty-payload-selector-" + str(selector), data)
    for label, selector, payload in [("zlib-under-gzip-selector", 1, compressed),
                                      ("gzip-under-zlib-selector", 2, _gzip(raw))]:
        data = bytearray(SECTOR * 3)
        entry(data, 0, 0, 2, 1)
        record(data, 2, payload, selector=selector)
        add(label, data)
    data = bytearray(SECTOR * 3)
    entry(data, 0, 0, 2, 1)
    bad = bytearray(_gzip(raw)); bad[-8] ^= 1
    record(data, 2, bad, selector=1)
    add("gzip-bad-unread-crc", data)
    for length in [0, 1, 4095, 4096, 4097, 8191]:
        data = bytearray(length)
        if length:
            data[0] = 0xA5
        add("short-file-" + str(length), data)
    add("unaligned-header-overwrite-17", b"\xA5" * 17 + b"\0" * 8192)
    add("unaligned-header-overwrite-1", b"\x80\0\x02\x01" + b"\0" * (8193 - 4))
    if len(result) != 36 or sum(len(item["queries"]) for item in result) != 49:
        raise ValueError("incomplete region fixture corpus")
    return result
