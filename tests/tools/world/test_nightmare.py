"""Run genuine CQ nightmare parents over authored saved Chunks; no reference JVM needed."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib

from test_random_tick import RESOURCES, tag


ROOT = Path(__file__).resolve().parents[3]
PROFILES = ROOT / "tests/reference/world/nightmare/profiles.tsv"
PLANES = {"blocks": 32768, "metadata": 16384, "sky": 16384, "block_light": 16384, "height": 256}


def need(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def authored_world(path):
    region = path / "region"
    region.mkdir(parents=True)
    grouped = {}
    for x in range(-4, 5):
        for z in range(-4, 5):
            grouped.setdefault((x // 32, z // 32), []).append((x, z))
    for (rx, rz), positions in grouped.items():
        header = bytearray(8192)
        records = []
        for x, z in positions:
            fields = [tag(3, "xPos", struct.pack(">i", x)), tag(3, "zPos", struct.pack(">i", z)),
                      tag(4, "LastUpdate", bytes(8))]
            for name, length in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                                 ("BlockLight", 16384), ("HeightMap", 256)):
                fields.append(tag(7, name, struct.pack(">i", length) + bytes(length)))
            fields += [tag(1, "TerrainPopulated", b"\1"), tag(9, "Entities", b"\x0a\0\0\0\0"),
                       tag(9, "TileEntities", b"\x0a\0\0\0\0")]
            nbt = tag(10, "", tag(10, "Level", b"".join(fields) + b"\0") + b"\0")
            payload = b"\2" + zlib.compress(nbt)
            record = struct.pack(">I", len(payload)) + payload
            need(len(record) <= 4096, "one bounded authored empty-Chunk sector")
            struct.pack_into(">I", header, 4 * ((x & 31) + 32 * (z & 31)), ((2 + len(records)) << 8) | 1)
            records.append(record + bytes(4096 - len(record)))
        (region / f"r.{rx}.{rz}.mcr").write_bytes(header + b"".join(records))


def author_inputs(directory, profiles=None):
    profiles = PROFILES if profiles is None else profiles
    values = [[int(word) for word in line.split()] for line in profiles.read_text(encoding="ascii").splitlines()]
    need(len(values) == 28 and all(len(row) == 8 and row[0] == index
                                 for index, row in enumerate(values)), "complete ordered declared CQ profiles")
    resources = directory / "resources"
    for name, value in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)
    worlds = directory / "worlds"
    for index in range(len(values) + 1):
        authored_world(worlds / f"case{index}")
    output = directory / "native"
    output.mkdir()
    return resources, worlds, output, values


def decode_plane(value, length):
    need(type(value) is dict and set(value) == {"length", "runs"} and value["length"] == length
         and type(value["runs"]) is list, "complete native plane extent")
    result = bytearray(length)
    end = 0
    previous_value = 0
    for run in value["runs"]:
        need(type(run) is list and len(run) == 3 and all(type(word) is int for word in run),
             "typed lossless native plane run")
        start, count, byte = run
        need(0 <= start < length and count > 0 and start >= end and start + count <= length
             and 1 <= byte <= 255 and not (start == end and byte == previous_value),
             "ordered disjoint maximal nonzero native plane runs")
        result[start:start + count] = bytes([byte]) * count
        end = start + count
        previous_value = byte
    return bytes(result)


def read_state(path):
    value = json.loads(path.read_text(encoding="utf-8"))
    chunks = value["chunks"]
    need([(row["x"], row["z"]) for row in chunks] == [(x, z) for x in range(-4, 5) for z in range(-4, 5)],
         "all81normalChunks in coordinate order")
    for chunk in chunks:
        need(set(chunk) == {"x", "z", "planes"} and set(chunk["planes"]) == set(PLANES),
             "complete actual Chunk plane inventory")
        for name, length in PLANES.items():
            chunk[name] = hashlib.sha256(decode_plane(chunk["planes"][name], length)).hexdigest()
        del chunk["planes"]
    return value


def native_rows(directory, profiles):
    rows = []
    for index, profile in enumerate(profiles):
        row = json.loads((directory / f"case{index}.json").read_text(encoding="utf-8"))
        need(row["case"] == index and row["input"] == profile, "actual complete native profile arguments")
        row["before"] = read_state(directory / f"case{index}-before.json")
        row["after"] = read_state(directory / f"case{index}-after.json")
        source_finder = {"searches", "heap_count", "heap_capacity", "map_count", "map_capacity",
                         "map_threshold", "map_modifications"}
        native_finder = {"native_query_count", "native_points", "native_cell_queries", "native_work"}
        need(type(row["finder"]) is dict and set(row["finder"]) == source_finder | native_finder,
             "closed source finder projection and separately declared native telemetry")
        row["native_finder_metrics"] = {name: row["finder"].pop(name) for name in sorted(native_finder)}
        need(type(row["returned"]) is bool and
             ((type(row["result"]) is bool and row["source_pc"] == 653 and row["native_status"] == 0)
              if row["returned"] else row["result"] is None and row["native_status"] != 0),
             "source return is separate from native refusal or caught printing")
        for before, after in zip(row["before"]["players"], row["after"]["players"]):
            need(before["random"] == after["random"] and after["bounds_same"] and
                 after["bed_same"] and after["spawn_same"] and after["private_bN_same"],
                 "actual Player Random and retained source identities remain unchanged")
        if index in (0, 1, 5, 7, 9, 10, 11, 12):
            need(row["returned"] and row["result"] is False, "actual completed no-nightmare parent")
        if index in (13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24):
            need(row["returned"] and row["result"] is True, "actual completed successful nightmare parent")
        if index in (2, 3, 4):
            need(not row["returned"] and row["exception"] == "java/lang/NullPointerException" and
                 row["source_pc"] == (87 if index == 4 else 12), "actual nullable source prefix")
        if index in (6, 8):
            need(not row["returned"] and row["print_pending"] and row["source_pc"] == 362,
                 "caught source Exception remains at the real printer, without a false result")
        if index == 23:
            need(not row["returned"] and row["exception"] == "java/util/ConcurrentModificationException",
                 "actual captured Iterator.next modification failure")
        if index == 25:
            need(not row["returned"] and row["exception"] == "java/lang/IllegalStateException" and
                 len(row["after"]["entities"]) == len(row["before"]["entities"]) + 1,
                 "actual authored callback Throwable retains its publication prefix")
        if index in (9, 10, 11, 12):
            need(row["counters"]["attempts_started"] == row["counters"]["attempts_returned"] == 20,
                 "all20real attempts execute on rejection")
        if index == 21:
            mobs = [entity for entity in row["after"]["entities"] if entity["id"] >= 1000]
            need([entity["class"] for entity in mobs] == ["cn", "fr"] and
                 mobs[0]["rider_id"] == mobs[1]["id"] and mobs[1]["mount_id"] == mobs[0]["id"],
                 "actual Spider/Skeleton jockey publication and owning mount relationship")
        need(row["finder"] is not None and
             row["finder"]["searches"] == row["counters"]["paths"],
             "one same actual retained finder accounts for every returned search")
        rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="nightmare-", dir=args.work_dir.resolve()))
    resources, worlds, output, profiles = author_inputs(directory)
    executable = args.executable.resolve(strict=True)
    paths = {executable, Path(__file__).resolve(), PROFILES,
             ROOT / "tests/world/runtime/test_nightmare.c",
             ROOT / "tests/world/runtime/test_world_random_tick.c",
             ROOT / "tests/support/java_random.h", Path(__file__).with_name("test_random_tick.py"),
             *resources.rglob("*.*"), *worlds.rglob("*.mcr")}
    for source in (ROOT / "src", ROOT / "include"):
        paths.update(path for path in source.rglob("*") if path.suffix in (".c", ".h", ".inc"))
    before = {str(path): digest(path) for path in sorted(paths)}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Authored native full-parent/control observations over genuine World/Player/Mob/Path owners. All81Chunkplanes are lossless; independent default-constructor entropy remains raw and is not rebound. These native controls add no original comparison; caught printer362 rows remain prefixes, not source returns."}
    try:
        command = [str(executable), str(resources), str(worlds), str(PROFILES), str(output)]
        report["command"] = command
        with (directory / "stdout.txt").open("wb") as stdout, (directory / "stderr.txt").open("wb") as stderr:
            child = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                   timeout=1200)
        report["returncode"] = child.returncode
        native_stdout = (directory / "stdout.txt").read_bytes()
        native_stderr = (directory / "stderr.txt").read_bytes()
        need(child.returncode == 0 and not native_stderr, f"native nightmare fixture failed: {directory}")
        lines = native_stdout.decode("ascii").splitlines()
        footer = re.fullmatch(r"Nightmare: ([1-9][0-9]*) checks;28 genuine profiles", lines[-1]) if lines else None
        observations = [line for line in lines if line.startswith("NIGHTMARE_NATIVE ")]
        limits = "NIGHTMARE_LIMITS literal_points=0 source_pc=410 checked_cleanup=1 source_replay=0"
        need(footer is not None and len(observations) == 28 and
             lines.count(limits) == 1 and
             all(line in ("151 recipes", "16 achievements", limits) or line in observations for line in lines[:-1]),
             "complete native constructor/observer output")
        actual = native_rows(output, profiles)
        (directory / "actual.json").write_text(json.dumps(actual, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", checks=int(footer.group(1)), profiles=28,
                      full_returns=sum(row["returned"] for row in actual),
                      successful_nightmares=sum(row["result"] is True for row in actual),
                      real_attempts=sum(row["counters"]["attempts_started"] for row in actual),
                      native_literal_point_limit_control=True,
                      actual_sha256=digest(directory / "actual.json"))
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        report.setdefault("returncode", None)
        raise
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in sorted(paths)}
        unchanged = before == report["input_after_sha256"]
        if not unchanged:
            report["status"] = "FAIL"
        (directory / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        need(unchanged, "native source, executable or authored inputs changed during observation")
    print(f"Nightmare controls: {report['checks']} checks; {directory / 'run.json'}")


if __name__ == "__main__":
    main()
