"""Read method Code identities; no rewriting, loading, or decompilation."""

import hashlib
import struct


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def inspect_class(data):
    cursor = 0

    def take(length):
        nonlocal cursor
        result = data[cursor:cursor + length]
        if len(result) != length:
            raise ValueError("truncated classfile")
        cursor += length
        return result

    def u1():
        return take(1)[0]

    def u2():
        return struct.unpack(">H", take(2))[0]

    def u4():
        return struct.unpack(">I", take(4))[0]

    if u4() != 0xCAFEBABE:
        raise ValueError("classfile magic")
    minor, major = u2(), u2()
    pool = [None] * u2()
    index = 1
    while index < len(pool):
        tag = u1()
        if tag == 1:
            # Selected names/descriptors are ASCII. Other constant strings can
            # use modified UTF-8; they are not method identity inputs here.
            pool[index] = take(u2()).decode("utf-8", errors="replace")
        elif tag in (3, 4):
            take(4)
        elif tag in (5, 6):
            take(8)
            index += 1
        elif tag in (7, 8, 16, 19, 20):
            take(2)
        elif tag in (9, 10, 11, 12, 17, 18):
            take(4)
        elif tag == 15:
            take(3)
        else:
            raise ValueError(f"constant-pool tag {tag}")
        index += 1

    def attributes():
        result = []
        for _ in range(u2()):
            name = pool[u2()]
            result.append((name, take(u4())))
        return result

    u2()  # class access
    u2()  # this class
    u2()  # superclass
    take(u2() * 2)
    for _ in range(u2()):
        u2()
        u2()
        u2()
        attributes()
    methods = []
    for _ in range(u2()):
        access, name, descriptor = u2(), pool[u2()], pool[u2()]
        method = {"name": name, "descriptor": descriptor, "access": access}
        for attribute, content in attributes():
            if attribute == "Code":
                if len(content) < 8:
                    raise ValueError("truncated Code attribute")
                length = struct.unpack(">I", content[4:8])[0]
                code = content[8:8 + length]
                if len(code) != length:
                    raise ValueError("truncated bytecode")
                method.update(code_bytes=length, code_sha256=sha256(code))
        methods.append(method)
    return {"class_bytes": len(data), "class_sha256": sha256(data),
            "major": major, "minor": minor, "methods": methods}
