"""Authored dispatch projections; use the reviewed stats capture unchanged."""
import importlib.util
import re
import struct
from pathlib import Path

LABELS = ("null", "general-null-client", "root-null-client", "child-null-client",
    "general-null-I", "root-null-I", "child-null-I", "general-zero", "general-negative",
    "general-large", "general-wrap", "blocked-parent", "root-null-u", "root-zero",
    "root-negative-present", "child-zero-parent-negative", "child-negative-present",
    "next-null-u", "next-negative", "next-zero-present")
EFFECT = b"EFFECT\twater\t8\t205\t0\t0\t1\t0\t0\t1024\t256\t256\t256\t256\t1\t1"
WATER_ARRAY_SITES = ((14, 16, "g"), (23, 25, "h"), (32, 34, "i"), (41, 43, "j"))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_water_instruction_sites(data):
    """Read the supplied, already-pinned original Code; never execute/rewrite it."""
    cursor = 0

    def take(count):
        nonlocal cursor
        require(0 <= count <= len(data) - cursor, "water class extent")
        part = data[cursor:cursor + count]
        cursor += count
        return part

    def u2():
        return struct.unpack(">H", take(2))[0]

    def u4():
        return struct.unpack(">I", take(4))[0]

    require(u4() == 0xcafebabe, "water class magic")
    take(4)
    pool = [None] * u2()
    index = 1
    while index < len(pool):
        tag = take(1)[0]
        if tag == 1:
            pool[index] = (tag, take(u2()))
        elif tag in (3, 4):
            take(4)
        elif tag in (5, 6):
            take(8)
            index += 1
        elif tag in (7, 8, 16, 19, 20):
            pool[index] = (tag, u2())
        elif tag in (9, 10, 11, 12, 17, 18):
            pool[index] = (tag, u2(), u2())
        elif tag == 15:
            take(3)
        else:
            raise ValueError("water constant-pool tag")
        index += 1

    def constant(index, tag):
        require(0 < index < len(pool) and pool[index] is not None and pool[index][0] == tag,
                "water constant-pool reference")
        return pool[index]

    def text(index):
        return constant(index, 1)[1].decode("ascii")

    def attributes():
        result = []
        for _ in range(u2()):
            name = text(u2())
            result.append((name, take(u4())))
        return result

    take(6)
    take(u2() * 2)
    for _ in range(u2()):
        take(6)
        attributes()
    constructor = None
    for _ in range(u2()):
        u2()
        name, descriptor = text(u2()), text(u2())
        for kind, body in attributes():
            if name == "<init>" and descriptor == "()V" and kind == "Code":
                require(constructor is None and len(body) >= 8, "water constructor Code")
                size = struct.unpack(">I", body[4:8])[0]
                constructor = body[8:8 + size]
                require(len(constructor) == size, "water Code extent")
    require(constructor is not None and len(constructor) == 52, "actual water constructor length")

    def field_at(pc, name, descriptor):
        require(constructor[pc] == 0xb5, "water putfield instruction at" + str(pc))
        ref = constant(struct.unpack(">H", constructor[pc + 1:pc + 3])[0], 9)
        owner = text(constant(ref[1], 7)[1])
        pair = constant(ref[2], 12)
        require((owner, text(pair[1]), text(pair[2])) == ("vs", name, descriptor),
                "water publication field at" + str(pc))

    sites = []
    for allocation, publication, name in WATER_ARRAY_SITES:
        require(constructor[allocation:allocation + 2] == bytes((0xbc, 6)),
                "water float newarray instruction at" + str(allocation))
        field_at(publication, name, "[F")
        sites.append({"newarray": allocation, "putfield": publication, "field": name})
    field_at(48, "k", "I")
    require(constructor[51] == 0xb1, "water constructor return51")
    return {"arrays": sites, "counter_putfield": 48, "return": 51}


def run(command, directory, stem, timeout=120, env=None):
    path = Path(__file__).resolve().parents[1] / "statistics/support.py"
    spec = importlib.util.spec_from_file_location("mc_player_stats_capture", path)
    require(spec is not None and spec.loader is not None, "reviewed stats capture unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = module.run_bounded(command, directory, stem, timeout=timeout, env=env)
    require(result.capture_report.get("reader_errors") == {}, "capture reader errors")
    return result


def resources_at(root):
    resources = {
        "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n"
                           b"achievement.get=Achievement get!\n",
        "lang/stats_US.lang": b"# Authored Player statistic resource\n",
        "achievement/map.txt": b"",
    }
    for name, data in resources.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def rows(payload, native, fault=False):
    lines = payload.splitlines()  # Preserve actual banner text; no strip/rewrite.
    selected = [line for line in lines if line.startswith(b"STAT\t")]
    require([line for line in lines if line.startswith(b"EFFECT\t")] == [EFFECT],
            "actual ad constructor fields/independent zero arrays")
    require(len(selected) == (0 if fault else len(LABELS)), "dispatch projection cardinality")
    for row, label in zip(selected, LABELS):
        require(re.fullmatch(rb"STAT\t" + label.encode("ascii") +
            rb"\t(?:OK|NULL)(?:\t-?[0-9]+){13}", row) is not None,
            "dispatch projection syntax: " + repr(row))
    remainder = [line for line in lines if not line.startswith((b"STAT\t", b"EFFECT\t"))]
    # Actual ad construction reads Block.B before jl186 on both profiles.
    expected_banners = [b"16 achievements", b"151 recipes"] * ((1 if fault else 4) if native else 1)
    require(remainder[:-1] == expected_banners, "actual constructor-profile banners")
    if native:
        match = re.fullmatch(rb"Player statistics: ([1-9][0-9]*) checks", remainder[-1])
        require(match is not None, "missing completed positive native checks")
        return selected, int(match.group(1))
    require(remainder[-1] == b"PLAYER_STATISTICS_ORIGINAL clean", "original lifetime marker")
    return selected, None
