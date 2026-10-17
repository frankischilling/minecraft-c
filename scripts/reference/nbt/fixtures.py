"""Small generated inputs for the original NBT classes and the C codec."""

from dataclasses import dataclass
import struct


def u16hex(text: str) -> str:
    return text.encode("utf-16-be", "surrogatepass").hex()


def mutf(text: str) -> bytes:
    raw = text.encode("utf-16-be", "surrogatepass")
    encoded = bytearray()
    for index in range(0, len(raw), 2):
        unit = int.from_bytes(raw[index:index + 2], "big")
        if 1 <= unit <= 127:
            encoded.append(unit)
        elif unit <= 2047:
            encoded.extend((0xC0 | (unit >> 6), 0x80 | (unit & 63)))
        else:
            encoded.extend((0xE0 | (unit >> 12), 0x80 | ((unit >> 6) & 63),
                            0x80 | (unit & 63)))
    if len(encoded) > 65535:
        raise ValueError("fixture exceeds writeUTF encoded-byte limit")
    return struct.pack(">H", len(encoded)) + encoded


def named(kind: int, payload: bytes = b"", name: str = "") -> bytes:
    return bytes([kind]) + (b"" if kind == 0 else mutf(name) + payload)


@dataclass(frozen=True)
class Fixture:
    label: str
    wire: bytes
    mode: str = "generic"
    java_error: str | None = None
    c_error: str | None = None
    departure: str | None = None
    compound_order: bool = False

    def request(self) -> str:
        return f"{self.mode}\t{self.wire.hex()}"


@dataclass(frozen=True)
class Construction:
    label: str
    kind: int
    name: str
    value: str
    java_error: str | None = None
    compound_order: bool = False
    heterogeneous: bool = False
    expected_wire: bytes | None = None

    def request(self) -> str:
        return f"construct\t{self.kind}\t{u16hex(self.name)}\t{self.value}"


def read_fixtures() -> list[Fixture]:
    result = [Fixture("end", b"\0"), Fixture("end-trailing", b"\0\xff\0")]
    scalar_formats = {1: (">b", [-128, -1, 0, 1, 127]),
                      2: (">h", [-32768, -1, 0, 1, 32767]),
                      3: (">i", [-2147483648, -1, 0, 1, 2147483647]),
                      4: (">q", [-9223372036854775808, -1, 0, 1, 9223372036854775807])}
    for kind, (fmt, values) in scalar_formats.items():
        for value in values:
            result.append(Fixture(f"scalar-{kind}-{value}", named(kind, struct.pack(fmt, value), "v")))
    float_bits = [0, 0x80000000, 1, 0x80000001, 0x007FFFFF, 0x00800000,
                  0x3F800000, 0x7F7FFFFF, 0x7F800000, 0xFF800000,
                  0x7F800001, 0x7FC00000, 0x7FFFFFFF, 0xFF800001, 0xFFC00000, 0xFFFFFFFF]
    double_bits = [0, 0x8000000000000000, 1, 0x8000000000000001,
                   0x000FFFFFFFFFFFFF, 0x0010000000000000, 0x3FF0000000000000,
                   0x7FEFFFFFFFFFFFFF, 0x7FF0000000000000, 0xFFF0000000000000,
                   0x7FF0000000000001, 0x7FF8000000000000, 0x7FFFFFFFFFFFFFFF,
                   0xFFF0000000000001, 0xFFF8000000000000, 0xFFFFFFFFFFFFFFFF]
    for kind, width, values in [(5, 4, float_bits), (6, 8, double_bits)]:
        for value in values:
            result.append(Fixture(f"float-{kind}-{value:0{width * 2}x}",
                                  named(kind, value.to_bytes(width, "big"), "f")))
    texts = ["", "\0", "\1", "\x7f", "\x80", "\u07ff", "\u0800", "\uffff",
             "\ud800", "\udbff", "\udc00", "\udfff", "\ud800\udc00", "\udbff\udfff",
             "a\0z\u0800\ud800x\udc00"]
    for index, text in enumerate(texts):
        result.append(Fixture(f"text-value-{index}", named(8, mutf(text), "value")))
        result.append(Fixture(f"text-name-{index}", named(1, b"\x7f", text)))
    for length in [0, 1, 5, 255]:
        payload = bytes(index & 255 for index in range(length))
        result.append(Fixture(f"byte-array-{length}", named(7, struct.pack(">i", length) + payload)))
    for kind in [0, 1, 8, 10, 11, 127, 128, 255]:
        result.append(Fixture(f"empty-list-type-{kind}", named(9, bytes([kind]) + b"\0\0\0\0")))
    for kind, payload in [(0, b""), (1, b"\x80\0\x7f"),
                          (2, struct.pack(">hhh", -32768, 0, 32767)),
                          (8, mutf("") + mutf("\0") + mutf("\ud800")),
                          (10, b"\0\0\0")]:
        result.append(Fixture(f"list-of-{kind}", named(9, bytes([kind]) + struct.pack(">i", 3) + payload)))
    duplicate = named(1, b"\1", "same") + named(3, struct.pack(">i", 2), "same")
    result += [Fixture("compound-empty", named(10, b"\0")),
               Fixture("compound-single", named(10, named(8, mutf("text"), "a") + b"\0", "root")),
               Fixture("compound-duplicate", named(10, duplicate + b"\0")),
               Fixture("compound-early-end-trailing", named(10, b"\0") + named(1, b"\1", "tail"))]
    children = [named(8, mutf(key), key) for key in ["z", "Aa", "BB", "a", "\0", "\ud800"]]
    for index, sequence in enumerate([children, children[::-1]]):
        result.append(Fixture(f"compound-multikey-{index}", named(10, b"".join(sequence) + b"\0"),
                              compound_order=True))
    nested = named(10, b"".join(children) + b"\0", "inner")
    result.append(Fixture("compound-nested-multikey", named(10, nested + b"\0"), compound_order=True))
    result.append(Fixture("compound-list-nested", named(9, b"\x0a\0\0\0\2" +
                          b"".join(children) + b"\0" + b"\0"), compound_order=True))
    result += [Fixture("compound-helper-empty", named(10, b"\0"), mode="compound"),
               Fixture("compound-helper-named", named(10, nested + b"\0", "root"), mode="compound",
                       compound_order=True)]
    for kind, payload in [(0, b""), (1, b"\1"), (8, mutf("text")), (9, b"\1\0\0\0\0")]:
        result.append(Fixture(f"compound-helper-reject-{kind}", named(kind, payload), mode="compound",
                              java_error="java.io.IOException", c_error="root is not a compound"))
    # DataInputStream accepts these non-shortest encodings and rewrites them through writeUTF.
    for label, encoded in [("raw-nul", b"\0"), ("overlong-nul-3", b"\xe0\x80\x80"),
                           ("overlong-ascii-2", b"\xc1\x81"),
                           ("overlong-ascii-3", b"\xe0\x81\x81"),
                           ("overlong-two-byte-3", b"\xe0\x82\x80")]:
        result.append(Fixture(f"mutf-value-{label}", named(8, struct.pack(">H", len(encoded)) + encoded)))
        result.append(Fixture(f"mutf-name-{label}", b"\1" + struct.pack(">H", len(encoded)) + encoded + b"\1"))
    for label, encoded in [("continuation", b"\x80"), ("incomplete-two", b"\xc0"),
                           ("bad-continuation-two", b"\xc0A"), ("incomplete-three", b"\xe0\x80"),
                           ("bad-continuation-three", b"\xe0\x80A"),
                           ("four-byte-utf8", b"\xf0\x90\x80\x80"), ("invalid-ff", b"\xff")]:
        payload = struct.pack(">H", len(encoded)) + encoded
        for where in ["value", "name"]:
            wire = named(8, payload) if where == "value" else b"\1" + payload + b"\1"
            result.append(Fixture(f"mutf-invalid-{where}-{label}", wire,
                                  java_error="java.io.UTFDataFormatException", c_error="invalid modified UTF-8"))
    for kind in [11, 12, 127, 128, 255]:
        result.append(Fixture(f"unsupported-tag-{kind}", bytes([kind]) + b"\0\0",
                              java_error="java.lang.NullPointerException", c_error="invalid tag type"))
        result.append(Fixture(f"unsupported-list-{kind}", named(9, bytes([kind]) + b"\0\0\0\1"),
                              java_error="java.lang.NullPointerException", c_error="invalid tag type"))
    for count in [-1, -2147483648]:
        result.append(Fixture(f"negative-byte-array-{count}", named(7, struct.pack(">i", count)),
                              java_error="java.lang.NegativeArraySizeException", c_error="invalid length"))
        result.append(Fixture(f"negative-list-{count}", named(9, b"\1" + struct.pack(">i", count)),
                              c_error="invalid length", departure="C rejects negative list lengths; original reads an empty list"))
    valid_truncation = named(10, named(7, b"\0\0\0\3abc", "array") +
                             named(9, b"\x08\0\0\0\1" + mutf("\ud800"), "list") + b"\0", "root")
    for length in range(len(valid_truncation)):
        result.append(Fixture(f"truncated-compound-{length}", valid_truncation[:length],
                              java_error="java.io.EOFException", c_error="truncated input"))
    result.append(Fixture("byte-array-length-exceeds-input", named(7, b"\0\0\0\4abc"),
                          java_error="java.io.EOFException", c_error="truncated input"))
    return result


def construction_fixtures() -> list[Construction]:
    result = [Construction("constructed-end", 0, "ignored", "", expected_wire=b"\0")]
    for kind, value, payload in [(1, "-128", b"\x80"), (2, "-32768", b"\x80\0"),
                                 (3, "-2147483648", b"\x80\0\0\0"),
                                 (4, "-9223372036854775808", b"\x80" + b"\0" * 7),
                                 (7, "0080ff", b"\0\0\0\3\0\x80\xff"),
                                 (8, "0000d800dc00", mutf("\0\ud800\udc00"))]:
        result.append(Construction(f"constructed-{kind}", kind, "name", value,
                                   expected_wire=named(kind, payload, "name")))
    for kind, values in [(5, ["7f800001", "7fc00000", "7fffffff", "ff800001", "ffc00000", "ffffffff"]),
                         (6, ["7ff0000000000001", "7ff8000000000000", "7fffffffffffffff",
                              "fff0000000000001", "fff8000000000000", "ffffffffffffffff"])]:
        canonical = bytes.fromhex("7fc00000" if kind == 5 else "7ff8000000000000")
        for value in values:
            result.append(Construction(f"constructed-nan-{kind}-{value}", kind, "", value,
                                       expected_wire=named(kind, canonical)))
    for label, text in [("ascii-65535", "a" * 65535), ("ascii-65536", "a" * 65536),
                        ("nul-65534", "\0" * 32767), ("nul-65535", "a" + "\0" * 32767),
                        ("nul-65536", "\0" * 32768), ("three-byte-65535", "\u0800" * 21845),
                        ("three-byte-65536", "a" + "\u0800" * 21845)]:
        too_long = label.endswith("65536")
        for where in ["name", "value"]:
            result.append(Construction(f"constructed-{where}-{label}", 8,
                                       text if where == "name" else "",
                                       u16hex(text if where == "value" else ""),
                                       java_error="java.io.UTFDataFormatException" if too_long else None,
                                       expected_wire=None if too_long else
                                       named(8, mutf(text if where == "value" else ""),
                                             text if where == "name" else "")))
    result += [Construction("constructed-list-empty", 9, "", "", expected_wire=named(9, b"\1\0\0\0\0")),
               Construction("constructed-list-end", 9, "", "0,0", expected_wire=named(9, b"\0\0\0\0\2")),
               Construction("constructed-list-short", 9, "", "2:-32768,2:32767",
                            expected_wire=named(9, b"\2\0\0\0\2\x80\0\x7f\xff")),
               Construction("constructed-list-mixed", 9, "", "2:1,3:2", heterogeneous=True,
                            expected_wire=named(9, b"\2\0\0\0\2\0\1\0\0\0\2")),
               Construction("constructed-compound-empty", 10, "", "", expected_wire=named(10, b"\0")),
               Construction("constructed-compound-duplicate", 10, "", "duplicate",
                            expected_wire=named(10, named(3, b"\0\0\0\2", "same") + b"\0")),
               Construction("constructed-compound-multikey", 10, "", "multikey", compound_order=True)]
    return result


def packet_fixtures() -> list[tuple[str, str, str | None]]:
    return [(f"packet-{index}", text, None) for index, text in enumerate([
        "", "\0", "A", "\x80", "\u07ff", "\u0800", "\uffff", "\ud800", "\udc00", "\ud800\udc00",
        "x" * 32767])] + [("packet-too-long", "x" * 32768, "java.io.IOException")]
