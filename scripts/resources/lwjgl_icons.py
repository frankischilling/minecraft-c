"""Bounded import-only extraction of the verified SDK's two window icons.

This module does not load Java, execute class files, extract class files to disk,
or run at native-client startup. Its output is ordinary private RGBA resources.
"""

from __future__ import annotations

import hashlib
from pathlib import Path
import zipfile


SDK_SHA256 = "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a"
CLASS_PATH = "org/lwjgl/LWJGLUtil.class"
CLASS_SHA256 = "26a1799fe5ff7e1edda2eb76e6be1490d7cfd96db752e9ad38c603db14f995b8"
CODE_SHA256 = {
    "<clinit>": "4515f87171653f3c09b9fce383c397ebccb48159b969e868832b32d8f9e556eb",
    "loadIcon": "1ca15f4ae8a5847fd1d36169028bc0f4c44b19a21e628abe07d8697e557a5bd8",
}
ATTRIBUTE_SHA256 = {
    "<clinit>": "d7e7f10855954546576f821ee4eae27d444fab0204beddbcc72a527170ec1c7d",
    "loadIcon": "b19f13e8b1f7876d47a8f3673dc3e6138bfc1e987ec1334c86c4c1b913d660b7",
}
MAX_JAR_BYTES = 16 * 1024 * 1024
MAX_CLASS_BYTES = 65536
MAX_ENTRIES = 8192


class IconImportFailure(ValueError):
    """The supplied SDK or a bounded class-file fact was not admitted."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise IconImportFailure(message)


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.position = 0

    def take(self, count: int) -> bytes:
        require(0 <= count <= len(self.data) - self.position, "Truncated SDK class")
        begin = self.position
        self.position += count
        return self.data[begin:self.position]

    def number(self, count: int) -> int:
        return int.from_bytes(self.take(count), "big")


def modified_utf8_units(data: bytes) -> tuple[int, ...]:
    """Decode class-file modified UTF8 to exact UTF16 code units, not scalars."""
    out = []
    at = 0
    while at < len(data):
        first = data[at]
        at += 1
        if 1 <= first < 0x80:
            unit = first
        elif 0xc0 <= first <= 0xdf:
            require(at < len(data) and data[at] & 0xc0 == 0x80, "Invalid modified UTF8 pair")
            unit = ((first & 31) << 6) | (data[at] & 63)
            at += 1
            require(unit >= 0x80 or (first == 0xc0 and unit == 0), "Overlong modified UTF8 pair")
        elif 0xe0 <= first <= 0xef:
            require(at + 1 < len(data) and data[at] & 0xc0 == 0x80 and data[at + 1] & 0xc0 == 0x80,
                    "Invalid modified UTF8 triple")
            unit = ((first & 15) << 12) | ((data[at] & 63) << 6) | (data[at + 1] & 63)
            at += 2
            require(unit >= 0x800, "Overlong modified UTF8 triple")
        else:
            raise IconImportFailure("Invalid class-file modified UTF8 lead")
        out.append(unit)
    return tuple(out)


def extract_class(data: bytes) -> tuple[dict[str, bytes], dict]:
    require(0 < len(data) <= MAX_CLASS_BYTES, "SDK class size bound")
    require(hashlib.sha256(data).hexdigest() == CLASS_SHA256, "Unexpected SDK utility class")
    r = Reader(data)
    require(r.number(4) == 0xcafebabe and r.number(2) == 0 and r.number(2) == 49,
            "Unexpected SDK class version")
    count = r.number(2)
    require(1 < count <= MAX_ENTRIES, "SDK constant-pool bound")
    pool = [None] * count
    index = 1
    while index < count:
        tag = r.number(1)
        if tag == 1:
            pool[index] = (tag, modified_utf8_units(r.take(r.number(2))))
        elif tag in (7, 8, 16):
            pool[index] = (tag, r.number(2))
        elif tag in (9, 10, 11, 12, 18):
            pool[index] = (tag, r.number(2), r.number(2))
        elif tag in (3, 4):
            pool[index] = (tag, r.take(4))
        elif tag in (5, 6):
            pool[index] = (tag, r.take(8))
            index += 1
            require(index < count, "Invalid wide constant-pool entry")
        elif tag == 15:
            pool[index] = (tag, r.number(1), r.number(2))
        else:
            raise IconImportFailure("Unsupported SDK constant-pool tag")
        index += 1

    def item(i: int, tag: int):
        require(0 < i < len(pool) and pool[i] is not None and pool[i][0] == tag,
                "Unexpected SDK constant-pool reference")
        return pool[i]

    def name(i: int) -> str:
        units = item(i, 1)[1]
        require(all(0 < unit < 128 for unit in units), "Non-ASCII structural SDK name")
        return "".join(chr(unit) for unit in units)

    def reference(i: int, tag: int) -> tuple[str, str, str]:
        ref = item(i, tag)
        owner = name(item(ref[1], 7)[1])
        nt = item(ref[2], 12)
        return owner, name(nt[1]), name(nt[2])

    require(r.number(2) == 33 and name(item(r.number(2), 7)[1]) == "org/lwjgl/LWJGLUtil",
            "Unexpected SDK class declaration")
    require(name(item(r.number(2), 7)[1]) == "java/lang/Object", "Unexpected SDK superclass")
    for _ in range(r.number(2)):
        r.take(2)

    def attributes() -> dict[str, bytes]:
        result = {}
        for _ in range(r.number(2)):
            key = name(r.number(2))
            value = r.take(r.number(4))
            require(key not in result, "Duplicate SDK attribute")
            result[key] = value
        return result

    for _ in range(r.number(2)):
        r.take(6)
        attributes()
    code = {}
    for _ in range(r.number(2)):
        r.take(2)
        method, descriptor = name(r.number(2)), name(r.number(2))
        attrs = attributes()
        if method not in CODE_SHA256:
            continue
        require(method not in code and descriptor == ("()V" if method == "<clinit>" else
                "(Ljava/lang/String;)Ljava/nio/ByteBuffer;"), "Unexpected SDK icon method")
        require("Code" in attrs, "Missing SDK icon method body")
        raw = attrs["Code"]
        body = Reader(raw)
        body.take(4)
        instructions = body.take(body.number(4))
        require(hashlib.sha256(raw).hexdigest() == ATTRIBUTE_SHA256[method] and
                hashlib.sha256(instructions).hexdigest() == CODE_SHA256[method],
                "Unexpected SDK icon Code")
        code[method] = instructions
    attributes()
    require(r.position == len(data) and set(code) == set(CODE_SHA256), "Incomplete SDK icon class")

    clinit = code["<clinit>"]
    images = {}
    sites = []
    for pc, edge in ((0, 16), (8, 32)):
        require(clinit[pc] == 0x12 and clinit[pc + 2] == 0xb8 and clinit[pc + 5] == 0xb3,
                "Unexpected SDK icon instruction sites")
        string_index = clinit[pc + 1]
        invoke_index = int.from_bytes(clinit[pc + 3:pc + 5], "big")
        field_index = int.from_bytes(clinit[pc + 6:pc + 8], "big")
        require(reference(invoke_index, 10) == ("org/lwjgl/LWJGLUtil", "loadIcon",
                "(Ljava/lang/String;)Ljava/nio/ByteBuffer;"), "Wrong SDK icon constructor operand")
        require(reference(field_index, 9) == ("org/lwjgl/LWJGLUtil", f"LWJGLIcon{edge}x{edge}",
                "Ljava/nio/ByteBuffer;"), "Wrong SDK icon publication operand")
        units = item(item(string_index, 8)[1], 1)[1]
        require(len(units) == edge * edge * 4, "Wrong SDK RGBA extent")
        pixels = bytes(unit & 255 for unit in units)  # exact charAt/i2b/absolute put
        relative = f"sdk/lwjgl-icon-{edge}.rgba"
        images[relative] = pixels
        sites.append({"literal_pc": pc, "load_icon_pc": pc + 2, "putstatic_pc": pc + 5,
                      "field": f"LWJGLIcon{edge}x{edge}", "path": relative,
                      "bytes": len(pixels), "sha256": hashlib.sha256(pixels).hexdigest()})
    return images, {"sdk_sha256": SDK_SHA256, "class": CLASS_PATH,
                    "class_sha256": CLASS_SHA256, "code_sha256": dict(CODE_SHA256),
                    "code_attribute_sha256": dict(ATTRIBUTE_SHA256), "assets": sites}


def read_sdk(path: Path) -> tuple[dict[str, bytes], dict]:
    # The enclosing importer owns path/reparse/output policy. This bounded
    # second reader also verifies the complete actual archive before/after.
    require(0 < path.stat().st_size <= MAX_JAR_BYTES, "SDK archive size bound")
    def bounded_archive() -> bytes:
        with path.open("rb") as source:
            value = source.read(MAX_JAR_BYTES + 1)
            require(len(value) <= MAX_JAR_BYTES and source.read(1) == b"", "SDK intake byte bound")
        return value

    raw = bounded_archive()
    require(hashlib.sha256(raw).hexdigest() == SDK_SHA256,
            "Unsupported SDK archive SHA256")
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        require(len(entries) <= MAX_ENTRIES and len({e.filename for e in entries}) == len(entries),
                "SDK archive entry bound/duplicate")
        require(CLASS_PATH in {e.filename for e in entries}, "Missing SDK icon owner")
        entry = archive.getinfo(CLASS_PATH)
        require(not entry.is_dir() and not entry.flag_bits & 1 and
                0 < entry.file_size <= MAX_CLASS_BYTES, "Invalid SDK icon class entry")
        with archive.open(entry) as stream:
            data = stream.read(MAX_CLASS_BYTES + 1)
            require(len(data) == entry.file_size and stream.read(1) == b"", "SDK class intake bound")
    images, provenance = extract_class(data)
    require(path.stat().st_size == len(raw) and hashlib.sha256(bounded_archive()).hexdigest() == SDK_SHA256,
            "SDK archive changed during import")
    return images, provenance
