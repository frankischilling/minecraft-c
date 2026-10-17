"""Generated small containers and single-entry compounds for local reference checks."""

from dataclasses import dataclass
import struct
import zlib


INVALID = "invalid compressed stream"
TRUNCATED = "truncated compressed stream"
ZIP_ERROR = "java.util.zip.ZipException"
EOF_ERROR = "java.io.EOFException"


def mutf(units: list[int]) -> bytes:
    encoded = bytearray()
    for unit in units:
        if 1 <= unit <= 127:
            encoded.append(unit)
        elif unit <= 2047:
            encoded.extend((0xC0 | (unit >> 6), 0x80 | (unit & 63)))
        else:
            encoded.extend((0xE0 | (unit >> 12), 0x80 | ((unit >> 6) & 63), 0x80 | (unit & 63)))
    return struct.pack(">H", len(encoded)) + encoded


def named(kind: int, payload: bytes, name: bytes = b"") -> bytes:
    return bytes([kind]) + struct.pack(">H", len(name)) + name + payload


def compound(child: bytes = b"", name: bytes = b"") -> bytes:
    return named(10, child + b"\0", name)


def raw_deflate(data: bytes) -> bytes:
    compressor = zlib.compressobj(level=6, wbits=-15)
    return compressor.compress(data) + compressor.flush()


def gzip_member(data: bytes, *, extra: bytes | None = None, name: bytes | None = None,
                comment: bytes | None = None, fhcrc: bool = False, text: bool = False) -> bytes:
    flags = int(text) | (2 if fhcrc else 0) | (4 if extra is not None else 0) \
        | (8 if name is not None else 0) | (16 if comment is not None else 0)
    header = b"\x1f\x8b\x08" + bytes([flags]) + b"\0\0\0\0\0\xff"
    if extra is not None:
        header += struct.pack("<H", len(extra)) + extra
    if name is not None:
        header += name + b"\0"
    if comment is not None:
        header += comment + b"\0"
    if fhcrc:
        header += struct.pack("<H", zlib.crc32(header) & 65535)
    payload = raw_deflate(data)
    return header + payload + struct.pack("<II", zlib.crc32(data), len(data))


def changed(data: bytes, index: int, mask: int = 1) -> bytes:
    result = bytearray(data)
    result[index] ^= mask
    return bytes(result)


@dataclass(frozen=True)
class Fixture:
    label: str
    format: str
    wire: bytes
    decoded: bytes | None
    consumed: int | None
    root: bytes | None
    c_error: str | None = None
    jdk_error: str | None = None
    original_error: str | None = None
    nbt_error: str | None = None
    departure: str | None = None


def root_fixtures() -> list[tuple[str, bytes]]:
    return [("empty", compound()),
            ("int", compound(named(3, struct.pack(">i", -2147483648), b"value"), b"root")),
            ("long", compound(named(4, struct.pack(">q", -9223372036854775808), b"n"))),
            ("string-units", compound(named(8, mutf([0, 127, 128, 2047, 2048, 65535, 0xD800, 0xDC00]), b"s"))),
            ("byte-array", compound(named(7, struct.pack(">i", 256) + bytes(range(256)), b"data"))),
            ("list-strings", compound(named(9, b"\x08\0\0\0\3" + mutf([]) + mutf([0]) + mutf([0xD800]), b"list"))),
            ("nested", compound(compound(named(1, b"\x80", b"b"), b"inner"))),
            ("repeated-bytes", compound(named(7, struct.pack(">i", 4096) + b"a" * 4096, b"data")))]


def payload_fixtures() -> list[tuple[str, bytes]]:
    return root_fixtures() + [("empty-payload", b""), ("binary", bytes(range(256))),
                              ("repeat-payload", b"abcd" * 2048)]


def fixtures() -> list[Fixture]:
    result = []
    for label, raw in root_fixtures():
        for fmt, wire in [("gzip", gzip_member(raw)), ("zlib", zlib.compress(raw))]:
            result.append(Fixture(f"{fmt}-root-{label}", fmt, wire, raw, len(wire), raw))
    raw = compound()
    member = gzip_member(raw)
    other = root_fixtures()[1][1]
    valid = [("optional-extra", gzip_member(raw, extra=b"\0\xffAB")),
             ("optional-empty-extra", gzip_member(raw, extra=b"")),
             ("optional-name", gzip_member(raw, name=b"level.dat")),
             ("optional-comment", gzip_member(raw, comment=b"synthetic fixture")),
             ("optional-fhcrc", gzip_member(raw, fhcrc=True)),
             ("optional-all", gzip_member(raw, extra=b"AB\1\0", name=b"n", comment=b"c", fhcrc=True, text=True))]
    result += [Fixture("gzip-" + label, "gzip", wire, raw, len(wire), raw) for label, wire in valid]
    for label, wire, decoded, consumed in [
            ("compressed-tail", member + b"tail", raw, len(member)),
            ("lone-magic-byte-tail", member + b"\x1f", raw, len(member)),
            ("decompressed-tail", gzip_member(raw + b"tail"), raw + b"tail", len(gzip_member(raw + b"tail"))),
            ("concat-roots", member + gzip_member(other), raw + other, len(member) + len(gzip_member(other))),
            ("concat-split-root", gzip_member(raw[:2]) + gzip_member(raw[2:]), raw,
             len(gzip_member(raw[:2])) + len(gzip_member(raw[2:]))),
            ("concat-empty-first", gzip_member(b"") + member, raw, len(gzip_member(b"")) + len(member)),
            ("concat-empty-last", member + gzip_member(b""), raw, len(member) + len(gzip_member(b"")))]:
        result.append(Fixture("gzip-" + label, "gzip", wire, decoded, consumed, raw))
    for label, payload, original_error, nbt_error in [
            ("empty-payload", b"", EOF_ERROR, "truncated input"),
            ("incomplete-root", raw[:-1], EOF_ERROR, "truncated input"),
            ("noncompound-root", b"\1\0\0\x7f", "java.io.IOException", "root is not a compound")]:
        for fmt, wire in [("gzip", gzip_member(payload)), ("zlib", zlib.compress(payload))]:
            result.append(Fixture(fmt + "-" + label, fmt, wire, payload, len(wire), None,
                                  original_error=original_error, nbt_error=nbt_error))
    for label, wire in [("crc", changed(member, -8)), ("isize", changed(member, -4)),
                        ("concat-unread-second-crc", member + changed(gzip_member(other), -8)),
                        ("concat-unread-first-crc", changed(member, -8) + gzip_member(other)),
                        ("concat-split-second-crc", gzip_member(raw[:2]) + changed(gzip_member(raw[2:]), -8))]:
        result.append(Fixture("gzip-corrupt-" + label, "gzip", wire, None, None, raw, c_error=INVALID,
                              jdk_error=ZIP_ERROR, departure="Original root read completes before the corrupt trailer is checked"))
    split_first_bad = changed(gzip_member(raw[:2]), -8) + gzip_member(raw[2:])
    result.append(Fixture("gzip-corrupt-split-first-crc", "gzip", split_first_bad, None, None, None,
                          c_error=INVALID, jdk_error=ZIP_ERROR, original_error=ZIP_ERROR))
    for missing in range(1, 10):
        result.append(Fixture(f"gzip-truncated-tail-{missing}", "gzip", member[:-missing], None, None, raw,
                              c_error=TRUNCATED, jdk_error=EOF_ERROR,
                              departure="Original root read completes before missing container bytes are requested"))
    for length in range(10):
        result.append(Fixture(f"gzip-truncated-header-{length}", "gzip", member[:length], None, None, None,
                              c_error=TRUNCATED, jdk_error=EOF_ERROR, original_error=EOF_ERROR))
    header_fhcrc = gzip_member(raw, fhcrc=True)
    invalid_headers = [("method", changed(member, 2, 1)), ("fhcrc", changed(header_fhcrc, 10))]
    for label, wire in invalid_headers:
        result.append(Fixture("gzip-invalid-header-" + label, "gzip", wire, None, None, None,
                              c_error=INVALID, jdk_error=ZIP_ERROR, original_error=ZIP_ERROR))
    for label, wire in [("extra", member[:3] + b"\4" + member[4:10] + b"\5\0ab"),
                        ("name", member[:3] + b"\x08" + member[4:10] + b"unterminated"),
                        ("comment", member[:3] + b"\x10" + member[4:10] + b"unterminated"),
                        ("fhcrc", header_fhcrc[:11])]:
        result.append(Fixture("gzip-truncated-optional-" + label, "gzip", wire, None, None, None,
                              c_error=TRUNCATED, jdk_error=EOF_ERROR, original_error=EOF_ERROR))
    # Current JDK gzip readers ignore reserved bits and incomplete next-member headers.
    reserved = changed(member, 3, 0x20)
    result.append(Fixture("gzip-reserved-header-bit", "gzip", reserved, raw, len(reserved), raw,
                          c_error=INVALID, departure="C rejects reserved gzip flags accepted by the selected JDK readers"))
    for label, tail, c_error in [("truncated", b"\x1f\x8b", TRUNCATED),
                                ("bad-method", changed(member, 2, 1), INVALID),
                                ("bad-fhcrc", changed(header_fhcrc, 10), INVALID)]:
        result.append(Fixture("gzip-recognized-next-header-" + label, "gzip", member + tail, raw, None, raw,
                              c_error=c_error,
                              departure="C requires a recognized additional gzip member header to be valid; selected JDK drain ignores this failed next header"))
    for fmt, wire in [("gzip", zlib.compress(raw)), ("gzip", raw_deflate(raw))]:
        result.append(Fixture("gzip-wrong-framing-" + str(len(result)), fmt, wire, None, None, None,
                              c_error=INVALID, jdk_error=ZIP_ERROR, original_error=ZIP_ERROR))
    zl = zlib.compress(raw)
    for label, wire, decoded, consumed in [("tail", zl + b"tail", raw, len(zl)),
                                            ("concat", zl + zlib.compress(other), raw, len(zl))]:
        result.append(Fixture("zlib-" + label, "zlib", wire, decoded, consumed, raw))
    result.append(Fixture("zlib-corrupt-adler", "zlib", changed(zl, -1), None, None, None,
                          c_error=INVALID, jdk_error=ZIP_ERROR))
    for missing in range(1, 5):
        result.append(Fixture(f"zlib-truncated-tail-{missing}", "zlib", zl[:-missing], None, None, None,
                              c_error=TRUNCATED, jdk_error=EOF_ERROR))
    result.append(Fixture("zlib-wrong-gzip", "zlib", member, None, None, None,
                          c_error=INVALID, jdk_error=ZIP_ERROR))
    return result
