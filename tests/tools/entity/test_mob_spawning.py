"""Authored genuine World/Mob spawn contracts, with optional local original observations."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib


RESOURCES = {
    "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
    "lang/stats_US.lang": b"# Authored spawn statistics resource\n",
    "achievement/map.txt": b"",
}
HOSTILES = ("cn", "uz", "fr", "gb")
ANIMALS = ("wh", "dl", "bx", "ww", "gi")
ELIG_IDS = [index for index in range(59) if index not in (52, 56, 57)]
CHUNK_IDS = list(range(16)) + [28, 29]
POINTS = ((0, 0), (1, -1), (-2049, 4097), (46341, -46341),
          (2147483647, -2147483648), (65536, 65536), (-1, -1))
MASK = (1 << 48) - 1
MULTIPLIER = 0x5DEECE66D


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tag(kind, name, payload):
    encoded = name.encode("utf-8")
    return bytes([kind]) + struct.pack(">H", len(encoded)) + encoded + payload


def chunk(x, z):
    fields = [tag(3, "xPos", struct.pack(">i", x)), tag(3, "zPos", struct.pack(">i", z)),
              tag(4, "LastUpdate", struct.pack(">q", 0))]
    for name, size in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                       ("BlockLight", 16384), ("HeightMap", 256)):
        fields.append(tag(7, name, struct.pack(">i", size) + bytes(size)))
    fields += [tag(1, "TerrainPopulated", b"\1"), tag(9, "Entities", b"\x0a\0\0\0\0"),
               tag(9, "TileEntities", b"\x0a\0\0\0\0")]
    return tag(10, "", tag(10, "Level", b"".join(fields) + b"\0") + b"\0")


def authored_world(path, points=((0, 0),)):
    region = path / "region"
    region.mkdir(parents=True)
    groups = {}
    for x, z in points:
        groups.setdefault((x // 32, z // 32), []).append((x, z))
    for (rx, rz), positions in groups.items():
        header = bytearray(8192)
        sectors = []
        for x, z in positions:
            payload = b"\2" + zlib.compress(chunk(x, z))
            record = struct.pack(">I", len(payload)) + payload
            if len(record) > 4096:
                raise AssertionError("authored Chunk exceeds its one-sector bound")
            struct.pack_into(">I", header, 4 * ((x & 31) + 32 * (z & 31)), ((2 + len(sectors)) << 8) | 1)
            sectors.append(record + bytes(4096 - len(record)))
        (region / f"r.{rx}.{rz}.mcr").write_bytes(header + b"".join(sectors))


def rng_seed(seed):
    return (seed ^ MULTIPLIER) & MASK


def bounded(state, bound):
    draws = 0
    while True:
        state = (state * MULTIPLIER + 11) & MASK
        draws += 1
        bits = state >> 17
        if bound & (bound - 1) == 0:
            return state, (bound * bits) >> 31, draws
        value = bits % bound
        if bits - value + bound - 1 < (1 << 31):
            return state, value, draws


def native_invariants(groups):
    if [int(row.split()[1]) for row in groups["ELIG_STATE"]] != ELIG_IDS:
        raise AssertionError("56 supported genuine hostile predicate identities required")
    for row in groups["ELIG_STATE"]:
        cells = row.split()
        if len(cells) != 13:
            raise AssertionError("complete predicate observation shape")
        index, scene, result = int(cells[1]), int(cells[3]), int(cells[4])
        if index < 44:
            kind = HOSTILES[index // 11]
            expected = (0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0)[scene]
            seed, _, _ = bounded(rng_seed(4096 if scene == 0 else 0), 32)
            if scene != 0:
                seed, _, _ = bounded(seed, 8)
            subtraction = 11 if scene == 8 else 0
        else:
            kind = "uw"
            expected = {0: 0, 1: 0, 2: 0, 3: 1, 4: 1, 5: 0, 6: 0, 7: 1, 9: 1, 10: -1, 11: -1, 12: 0}[scene]
            seed = rng_seed(1 if scene == 1 else 0)
            if scene not in (0, 10):
                seed, _, _ = bounded(seed, 10)
            subtraction = 0
        if (cells[2], result, cells[5], cells[6], int(cells[7]), cells[9], cells[12]) != (
                kind, expected, f"{seed:012x}", f"{rng_seed(89):012x}", subtraction, "0", "1"):
            raise AssertionError(f"source predicate result/RNG/World fields differ: {row}")
    for prefix, count, kinds, scenes in (("ANIMAL_STATE", 50, ANIMALS, 10), ("WATER_STATE", 9, ("xt",), 9)):
        rows = groups[prefix]
        if [int(row.split()[1]) for row in rows] != list(range(count)):
            raise AssertionError(f"complete {prefix} identity order")
        for index, row in enumerate(rows):
            cells = row.split()
            scene = index % scenes
            expected = ((0, 0, 1, 0, 0, 0, 1, 1, 1, 1) if prefix == "ANIMAL_STATE" else (1, 1, 1, 1, 0, 1, 1, 0, 1))[scene]
            if (len(cells) != 13 or cells[2] != kinds[index // scenes] or int(cells[3]) != scene
                    or int(cells[4]) != expected or cells[5] != f"{rng_seed(0):012x}"
                    or cells[6] != f"{rng_seed(89):012x}" or cells[9] != "0" or cells[12] != "1"
                    or int(cells[7]) != (15 if prefix == "ANIMAL_STATE" and scene == 9 else 0)):
                raise AssertionError(f"source animal/water result/RNG/light fields differ: {row}")
    if [int(row.split()[1]) for row in groups["CHUNK_RANDOM"]] != CHUNK_IDS:
        raise AssertionError("18 actual normal/installed-Blank Chunk identities")
    for row in groups["CHUNK_RANDOM"]:
        cells = row.split()
        index, blank, x, z = map(int, cells[1:5])
        salt, world_seed = int(cells[5], 16), int(cells[6], 16)
        def i32(value):
            return (value + (1 << 31)) % (1 << 32) - (1 << 31)
        summed = (world_seed + i32(i32(x * x) * 4987142) + i32(x * 5947611)
                  + i32(z * z) * 4392871 + i32(z * 389711)) & ((1 << 64) - 1)
        seed = rng_seed(summed ^ salt)
        after, value, draws = bounded(seed, 10)
        if cells[7:] != [f"{seed:012x}", str(value), f"{after:012x}"]:
            raise AssertionError(f"independent Java-width Chunk seed/rejection result differs: {row}")
        if index >= 28 and (draws != 2 or value != 9):
            raise AssertionError("actual two-advance nextInt rejection required")
        if blank and (x, z) != (0, 0):
            raise AssertionError("only actual installed native Blank coordinates admitted")
    if groups["THUNDER"] != ["THUNDER 0 3f666666 0", "THUNDER 1 3f666667 1", "THUNDER 2 7fc00000 0"]:
        raise AssertionError("actual GZ double-promoted thunder threshold")
    if len(groups["CHUNK_FAILURE"]) != 3 or [int(row.split()[1]) for row in groups["CHUNK_FAILURE"]] != [0, 1, 3]:
        raise AssertionError("three reachable original Chunk source-null prefixes")
    if len(groups["SPAWN_FAILURE"]) != 2 or len(groups["SPAWN_CONTROL"]) != 2:
        raise AssertionError("complete source/native failure and ownership controls")


def compare_original(groups, original):
    counts = {}
    for prefix, key, identities in (("ELIG_STATE", "state_rows", ELIG_IDS),
                                     ("CHUNK_RANDOM", "chunk_rows", CHUNK_IDS),
                                     ("THUNDER", "thunder_rows", range(3)),
                                     ("ANIMAL_STATE", "animal_rows", range(50)),
                                     ("WATER_STATE", "water_rows", range(9))):
        if key not in original:
            if prefix in ("ANIMAL_STATE", "WATER_STATE"):
                continue
            raise AssertionError(f"missing unchanged-original {key}")
        selected = {int(row.split()[1]): row for row in original[key]}
        expected = [selected[index] for index in identities]
        if groups[prefix] != expected:
            for actual, source in zip(groups[prefix], expected):
                if actual != source:
                    raise AssertionError(f"{prefix} exact original difference\nsource: {source}\nnative: {actual}")
            raise AssertionError(f"{prefix} original row count differs")
        counts[key] = len(expected)
    return {"matched_rows": counts, "source_checkpoint_count": len(original["checkpoints"]),
            "source_only": ["UW scenario8: original fresh null-loader provider reaches LI; native constructor unavailable",
                            "controlled FailureWorld second/third light calls; no native virtual-callee seam",
                            "LI leaves at arbitrary coordinates; native installed Blank is (0,0)",
                            "LI null-World field; embedded native fallback retains its actual World",
                            "direct protected BG path-weight component entries; no standalone native entry"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--observations", type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="mob-spawning-", dir=args.work_dir.resolve()))
    for name, data in RESOURCES.items():
        target = out / "resources" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    worlds = out / "worlds"
    for index in ELIG_IDS:
        authored_world(worlds / f"case{index}")
    for prefix, count in (("animal", 50), ("water", 9)):
        for index in range(count):
            authored_world(worlds / f"{prefix}{index}")
    authored_world(worlds / "chunks0", POINTS)
    authored_world(worlds / "chunks1")
    authored_world(worlds / "control0")
    authored_world(worlds / "control1", ((0, 0), (1, 0)))
    executable = args.executable.resolve(strict=True)
    paths = [executable, Path(__file__), *sorted((out / "resources").rglob("*.*")), *sorted(worlds.rglob("*.mcr"))]
    if args.observations:
        paths.append(args.observations.resolve(strict=True))
    before = {str(path.resolve()): digest(path) for path in paths}
    report = {"status": "FAIL", "input_before_sha256": before, "original_comparison": None}
    try:
        actual = out / "native.txt"
        command = [str(executable), str(out / "resources"), str(worlds), str(actual)]
        result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=170)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        report["command"] = command
        lines = result.stdout.decode("ascii").splitlines()
        footer = re.fullmatch(r"Mob spawning: ([1-9][0-9]*) checks; 115 genuine predicates and 18 exact Chunk leaves", lines[-1]) if lines else None
        if result.returncode or result.stderr or footer is None or any(line not in ("151 recipes", "16 achievements") for line in lines[:-1]):
            raise AssertionError(f"Mob spawn native failure ({result.returncode}); {result.stderr.decode(errors='replace')}; {out}")
        prefixes = ("ELIG_STATE", "CHUNK_RANDOM", "THUNDER", "ANIMAL_STATE", "WATER_STATE", "CHUNK_FAILURE", "SPAWN_FAILURE", "SPAWN_CONTROL")
        groups = {prefix: [] for prefix in prefixes}
        for row in actual.read_text(encoding="ascii").splitlines():
            groups[row.split()[0]].append(row)
        native_invariants(groups)
        if args.observations:
            report["original_comparison"] = compare_original(groups, json.loads(args.observations.read_text()))
        report.update(status="PASS", checks=int(footer.group(1)), predicates=115, chunk_leaves=18,
                      source_null_chunk_controls=3, native_result=str(actual),
                      scope="Authored genuine GAME World/Registry/Chunk/Entity/Math graph, all eleven concrete Mob predicates; source-setSeed inputs; private, World and shared Math progression checked separately; native allocation and source-null failures retain reached state without retry repair; real publication/occurrence retirement; direct installed Blank only; no AI, save, tick evolution, renderer, arbitrary Blank construction or controlled virtual-World failure equivalence claim")
        print(f"Mob spawning: {footer.group(1)} checks; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in paths}
        unchanged = before == report["input_after_sha256"]
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n")
        if not unchanged:
            raise AssertionError("Mob spawn fixture inputs changed")


if __name__ == "__main__":
    main()
