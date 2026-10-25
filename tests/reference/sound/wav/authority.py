"""Check original archive, class and method identities without extracting code."""

import hashlib
import json
from pathlib import Path
import struct
import zipfile


def need(condition, message):
    if not condition:
        raise ValueError(message)


def identity(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


class Reader:
    def __init__(self, data):
        self.data = data
        self.at = 0

    def take(self, size):
        need(0 <= size <= len(self.data) - self.at, "complete classfile field")
        result = self.data[self.at:self.at + size]
        self.at += size
        return result

    def u1(self):
        return self.take(1)[0]

    def u2(self):
        return struct.unpack(">H", self.take(2))[0]

    def u4(self):
        return struct.unpack(">I", self.take(4))[0]


def method_codes(data):
    """Return hashes/exception tables; never return original instruction bytes."""
    reader = Reader(data)
    need(reader.u4() == 0xcafebabe, "classfile magic")
    minor, major = reader.u2(), reader.u2()
    count = reader.u2()
    constants = [None] * count
    index = 1
    while index < count:
        tag = reader.u1()
        if tag == 1:
            constants[index] = reader.take(reader.u2())
        elif tag in (3, 4):
            reader.take(4)
        elif tag in (5, 6):
            reader.take(8)
            index += 1
        elif tag in (7, 8, 16, 19, 20):
            value = reader.u2()
            if tag == 7:
                constants[index] = value
        elif tag in (9, 10, 11, 12, 17, 18):
            reader.take(4)
        elif tag == 15:
            reader.take(3)
        else:
            raise ValueError("unsupported constant-pool tag")
        index += 1

    def text(at):
        need(0 < at < len(constants) and isinstance(constants[at], bytes), "UTF8 constant")
        return constants[at].decode("ascii")

    def attributes(owner):
        result = []
        for _ in range(owner.u2()):
            name = text(owner.u2())
            result.append((name, owner.take(owner.u4())))
        return result

    reader.take(6)
    reader.take(reader.u2() * 2)
    for _ in range(reader.u2()):
        reader.take(6)
        attributes(reader)
    methods = {}
    for _ in range(reader.u2()):
        reader.u2()
        name, descriptor = text(reader.u2()), text(reader.u2())
        for kind, body in attributes(reader):
            if kind != "Code":
                continue
            code = Reader(body)
            stack, locals_count = code.u2(), code.u2()
            instructions = code.take(code.u4())
            handlers = []
            for _ in range(code.u2()):
                start, end, target, caught = code.u2(), code.u2(), code.u2(), code.u2()
                need(start <= end <= len(instructions) and target < len(instructions), "Code handler range")
                catch = None
                if caught:
                    need(caught < len(constants) and isinstance(constants[caught], int), "Class constant")
                    catch = text(constants[caught])
                handlers.append({"start": start, "end": end, "target": target, "catch": catch})
            attributes(code)
            need(code.at == len(body), "complete Code attribute")
            key = name + descriptor
            need(key not in methods, "unique method descriptor")
            methods[key] = {"code_bytes": len(instructions),
                            "code_sha256": hashlib.sha256(instructions).hexdigest(),
                            "max_stack": stack, "max_locals": locals_count,
                            "exception_handlers": handlers}
    attributes(reader)
    need(reader.at == len(data), "complete classfile")
    return {"major": major, "minor": minor, "methods": methods}


def verify_archive(path):
    pins = json.loads(Path(__file__).with_name("pins.json").read_text(encoding="utf-8"))
    raw = path.read_bytes()
    need(identity(raw) == pins["archive"], "verified supplied client archive identity")
    classes = {}
    with zipfile.ZipFile(path) as archive:
        for name, expected in pins["classes"].items():
            data = archive.read(name)
            need(identity(data) == expected["identity"], "original class identity: " + name)
            actual = method_codes(data)
            need(actual == expected["code"], "original complete Code/table identity: " + name)
            classes[name] = expected
    return {"archive": pins["archive"], "classes": classes}
