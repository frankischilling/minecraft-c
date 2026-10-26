"""Bounded structural and Code verification; never load original classes."""

import hashlib
from pathlib import Path
import re
import struct
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, count):
        require(0 <= count <= len(self.data) - self.pos, "truncated classfile")
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def u1(self):
        return self.take(1)[0]

    def u2(self):
        return struct.unpack(">H", self.take(2))[0]

    def u4(self):
        return struct.unpack(">I", self.take(4))[0]


def parse_class(data):
    require(0 < len(data) <= 262144, "classfile byte bound")
    r = Reader(data)
    require(r.u4() == 0xCAFEBABE, "classfile magic")
    minor, major = r.u2(), r.u2()
    cp = [None] * r.u2()
    require(len(cp) > 1, "constant pool extent")
    i = 1
    while i < len(cp):
        tag = r.u1()
        if tag == 1:
            cp[i] = (tag, r.take(r.u2()))
        elif tag in (3, 4, 5, 6):
            r.take(8 if tag in (5, 6) else 4)
            cp[i] = (tag,)
            if tag in (5, 6):
                i += 1
                require(i < len(cp), "wide constant extent")
        elif tag in (7, 8, 16, 19, 20):
            cp[i] = (tag, r.u2())
        elif tag in (9, 10, 11, 12, 17, 18):
            cp[i] = (tag, r.u2(), r.u2())
        elif tag == 15:
            cp[i] = (tag, r.u1(), r.u2())
        else:
            raise ValueError("constant pool tag")
        i += 1

    def constant(index, tag):
        require(0 < index < len(cp) and cp[index] is not None and cp[index][0] == tag,
                "constant pool type/index")
        return cp[index]

    def text(index):
        raw = constant(index, 1)[1]
        require(all(0 < byte < 128 for byte in raw), "structural ASCII name")
        return raw.decode("ascii")

    def name(index):
        return text(constant(index, 7)[1])

    def attributes(reader):
        result = []
        for _ in range(reader.u2()):
            key = text(reader.u2())
            result.append((key, reader.take(reader.u4())))
        return result

    access, this, parent = r.u2(), r.u2(), r.u2()
    interfaces = [name(r.u2()) for _ in range(r.u2())]

    def members():
        result = []
        for _ in range(r.u2()):
            value = {"access": r.u2(), "name": text(r.u2()), "descriptor": text(r.u2())}
            for key, body in attributes(r):
                if key != "Code":
                    continue
                require("code_sha256" not in value, "duplicate Code")
                c = Reader(body)
                stack, locals_ = c.u2(), c.u2()
                code = c.take(c.u4())
                handlers = []
                for _ in range(c.u2()):
                    start, end, handler, kind = (c.u2() for _ in range(4))
                    require(start < end <= len(code) and handler < len(code), "handler range")
                    handlers.append({"start": start, "end": end, "handler": handler,
                                     "catch": name(kind) if kind else None})
                attributes(c)
                require(c.pos == len(body), "Code trailing bytes")
                value.update(code_length=len(code), code_sha256=sha(code),
                             code_attribute_sha256=sha(body), max_stack=stack,
                             max_locals=locals_, exception_table=handlers)
            result.append(value)
        return result

    fields, methods = members(), members()
    attributes(r)
    require(r.pos == len(data), "classfile trailing bytes")
    return {"name": name(this), "super": name(parent) if parent else None,
            "interfaces": interfaces, "major": major, "minor": minor, "access": access,
            "class_sha256": sha(data), "class_bytes": len(data), "fields": fields,
            "methods": methods}


def validate_jar(path: Path, side: str, pins: dict):
    require(pins["schema"] == "CLIENT_STATS_CLASS_CODE_PINS_V1", "class pin schema")
    expected = pins["jars"][side]
    require(path.is_file() and path.stat().st_size == expected["bytes"], side + " JAR size")
    require(sha(path.read_bytes()) == expected["sha256"], side + " JAR digest")
    methods = 0
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        require(len(names) == len(set(names)), "duplicate archive names")
        require(not any(name in names for name in ("StatsFileObservation.class",)), "original/helper name collision")
        for cls in expected["classes"]:
            entry = cls["entry"]
            require(re.fullmatch(r"[A-Za-z0-9_$/]+\.class", entry) is not None and
                    ".." not in entry and not entry.startswith("/"), "class entry name")
            require(archive.getinfo(entry).file_size == cls["class_bytes"] <= 262144,
                    "class entry extent")
            actual = parse_class(archive.read(entry))
            require(actual == {k: v for k, v in cls.items() if k != "entry"},
                    side + " class/Code pin: " + cls["name"])
            methods += sum("code_sha256" in m for m in actual["methods"])
    return {"bytes": expected["bytes"], "sha256": expected["sha256"],
            "classes": len(expected["classes"]), "Code_methods": methods}
