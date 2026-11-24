"""Direct genuine FD.n contracts and optional unchanged-original frame/hash comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib


SCENARIOS = ("empty_players", "air_qhigh", "stone_q0", "air_ambient", "sound_failure", "P_mutation",
             "blank_grass_remote", "raw_null", "tick_entry_null", "snow_HD", "ice_HD", "lightning_HD",
             "normal_alias", "random_null", "native_h_ignored")
STREAMS = ("raw", "nibbles", "height", "membership", "P")
RESOURCES = {
    "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
    "lang/stats_US.lang": b"# Authored direct random-Chunk tick resource\n",
    "achievement/map.txt": b"",
}


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as source:
        for data in iter(lambda: source.read(1024 * 1024), b""):
            result.update(data)
    return result.hexdigest()


def tag(kind, name, payload):
    encoded = name.encode("utf-8")
    return bytes([kind]) + struct.pack(">H", len(encoded)) + encoded + payload


def block_bytes(index):
    if index in (2, 8, 12):
        return bytes([1]) * 32768
    data = bytearray(32768)
    if 9 <= index <= 11:
        for x in range(16):
            for z in range(16):
                data[(x << 11) | (z << 7) | 63] = 1
    return bytes(data)


def height_bytes(index):
    return bytes([128 if index in (2, 8, 12) else 64 if 9 <= index <= 11 else 0]) * 256


def chunk(index, x, z):
    arrays = (("Blocks", block_bytes(index)), ("Data", bytes(16384)), ("SkyLight", bytes(16384)),
              ("BlockLight", bytes(16384)), ("HeightMap", height_bytes(index)))
    fields = [tag(3, "xPos", struct.pack(">i", x)), tag(3, "zPos", struct.pack(">i", z)),
              tag(4, "LastUpdate", bytes(8))]
    fields += [tag(7, name, struct.pack(">i", len(data)) + data) for name, data in arrays]
    fields += [tag(1, "TerrainPopulated", b"\1"), tag(9, "Entities", b"\x0a\0\0\0\0"),
               tag(9, "TileEntities", b"\x0a\0\0\0\0")]
    return tag(10, "", tag(10, "Level", b"".join(fields) + b"\0") + b"\0")


def authored_world(path, index):
    path.mkdir(parents=True)
    if index in (0, 6):
        return
    region = path / "region"
    region.mkdir()
    radius = 10 if 9 <= index <= 11 else 9
    points = [(0, 0)] if index == 12 else [(x, z) for x in range(-radius, radius + 1) for z in range(-radius, radius + 1)]
    grouped = {}
    for x, z in points:
        grouped.setdefault((x // 32, z // 32), []).append((x, z))
    for (rx, rz), coordinates in grouped.items():
        header = bytearray(8192)
        sectors = []
        for x, z in coordinates:
            payload = b"\2" + zlib.compress(chunk(index, x, z))
            record = struct.pack(">I", len(payload)) + payload
            if len(record) > 4096:
                raise AssertionError("authored Chunk exceeds one Region sector")
            struct.pack_into(">I", header, 4 * ((x & 31) + 32 * (z & 31)), ((2 + len(sectors)) << 8) | 1)
            sectors.append(record + bytes(4096 - len(record)))
        (region / f"r.{rx}.{rz}.mcr").write_bytes(header + b"".join(sectors))


def array_hash(arrays):
    result = hashlib.sha256()
    for array in arrays:
        result.update(struct.pack(">i", -1 if array is None else len(array)))
        if array is not None:
            result.update(array)
    return result.hexdigest()


def source_frames(path):
    text = path.read_text(encoding="utf-8")
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        data = [json.loads(line) for line in text.splitlines() if line.strip()]
    if isinstance(data, dict):
        data = data.get("frame_records", data.get("original_rows", data.get("frames", data.get("rows"))))
    if not isinstance(data, list):
        raise AssertionError("original complete frame transport required")
    frames = [row for row in data if row.get("event") == "frame"]
    if [row["index"] for row in frames] != list(range(14)):
        raise AssertionError("fourteen complete original frame identities required")
    return frames


def event_rows(frame):
    counts = {"SOUND": 0, "CELL": 0, "RANGE": 0}
    result = []
    for event in frame["events"]:
        cells = event.split()
        kind = cells[0]
        if kind not in counts:
            raise AssertionError(f"unqualified original observer event: {event}")
        values = cells[1:]
        if kind == "SOUND":
            if values[0] != "ambient.cave.cave":
                raise AssertionError("actual ambient literal differs")
            values = values[1:]
        result.append(" ".join(["RANDOM_" + kind, str(frame["index"]), str(counts[kind]), *values]))
        counts[kind] += 1
    return result


def check_native(frames, states, events, hashes, directory):
    if [int(row.split()[1]) for row in frames] != list(range(15)) or len(states) != 15:
        raise AssertionError("fifteen native whole-body profiles required")
    by_event = {index: [] for index in range(15)}
    for event in events:
        by_event[int(event.split()[1])].append(event)
    for index, row in enumerate(frames):
        cells = row.split()
        hd = 9 <= index <= 11
        size = 441 if hd else 361 if index else 0
        unique = 1 if index in (6, 12) else size
        failed = index in (4, 5, 7, 8, 13)
        if (len(cells) != 16 or cells[2] != SCENARIOS[index] or int(cells[3]) != (-1 if failed else 1)
                or int(cells[8]) != (0 if index in (0, 5) else 361) or int(cells[9]) != size
                or int(cells[10]) != unique or int(cells[11]) != 0 or int(cells[12]) != (1 if index == 11 else 0)):
            raise AssertionError(f"whole original-body native frame shape/result/ownership differs: {row}")
        state = states[index].split()
        if state[0] != "NATIVE_RANDOM_STATE" or int(state[1]) != index or len(state) != 18:
            raise AssertionError("complete native counter/ownership receipt required")
        expected_pc = {4: 443, 5: 172, 7: 944, 8: 982, 13: 470}.get(index, 994)
        if int(state[2]) != expected_pc or int(state[-1]) != (-2147483648 if index == 14 else 1013904223):
            raise AssertionError("source boundary or actual untouched h differs")
        if not failed and int(state[7]) != (28880 if index else 0):
            raise AssertionError("every completed active Chunk must perform all eighty samples")
        if index == 6 and int(state[8]) != 28880:
            raise AssertionError("Blank raw Grass samples must enter actual remote WP body")
        rows = by_event[index]
        kinds = {kind: [event for event in rows if event.startswith("RANDOM_" + kind + " ")] for kind in ("SOUND", "CELL", "RANGE")}
        if [len(kinds[kind]) for kind in ("SOUND", "CELL", "RANGE")] != list(map(int, cells[13:16])):
            raise AssertionError("actual observer counts/events differ")
        if index in (3, 4, 5):
            expected_sound = f"RANDOM_SOUND {index} 0 4012000000000000 4051a00000000000 3ff8000000000000 3f333333 3f68cb07"
            if kinds["SOUND"] != [expected_sound]:
                raise AssertionError("actual nearby Player ambient sound arguments differ")
        if index in (9, 10):
            if len(kinds["CELL"]) != 28 or kinds["RANGE"]:
                raise AssertionError("twenty-eight actual snow/ice notifying writes required")
            if any(int(event.split()[4]) != (64 if index == 9 else 63) for event in kinds["CELL"]):
                raise AssertionError("source snow/ice Y differs")
        mapping = (directory / f"case{index}-membership.bin").read_bytes()
        if len(mapping) != size * 24:
            raise AssertionError("complete actual captured identity mapping required")
        coordinates = [(x, z) for x in range(-(10 if hd else 9), (10 if hd else 9) + 1)
                       for z in range(-(10 if hd else 9), (10 if hd else 9) + 1)] if size else []
        mapping_expected = b"".join(struct.pack(">6i", x, z, 1 if index == 6 else 0,
                                               0 if index in (6, 12) else x,
                                               0 if index in (6, 12) else z,
                                               1 if index in (6, 12) else ordinal + 1)
                                    for ordinal, (x, z) in enumerate(coordinates))
        if mapping != mapping_expected:
            raise AssertionError("requested coordinates must preserve actual Chunk aliases and own fields")
        p_bytes = (directory / f"case{index}-P.bin").read_bytes()
        pairs = list(struct.iter_unpack(">2i", p_bytes))
        if len(pairs) != (0 if index in (0, 5) else 361) or (pairs and (len(set(pairs)) != 361 or set(pairs) != {(x, z) for x in range(-9, 10) for z in range(-9, 10)})):
            raise AssertionError("complete actual Player-radius-nine Set membership required")
        if pairs and pairs[0] != (-2, -2):
            raise AssertionError("pinned real JDK HashSet first acquired Chunk differs")
        zero_nibble = array_hash([bytes(16384)] * (unique * 3))
        expected_height = array_hash([height_bytes(index)] * unique)
        if hashes[index]["nibbles"] != zero_nibble or hashes[index]["height"] != expected_height:
            raise AssertionError("actual source Nibble/height arrays differ")
        raw_values = []
        for x, z in coordinates[:1] if index in (6, 12) else coordinates:
            if index == 7 and (x, z) == (-2, -2):
                raw_values.append(None)
                continue
            raw = bytearray(bytes([2]) * 32768 if index == 6 else block_bytes(index))
            if index in (9, 10):
                for event in kinds["CELL"]:
                    wx, wy, wz = map(int, event.split()[3:6])
                    if (wx // 16, wz // 16) == (x, z):
                        raw[((wx & 15) << 11) | ((wz & 15) << 7) | wy] = 78 if index == 9 else 79
            raw_values.append(raw)
        if hashes[index]["raw"] != array_hash(raw_values):
            raise AssertionError("live actual raw bytes differ from declared terrain and observed notifying writes")
    if frames[14].split()[3:] != frames[1].split()[3:] or hashes[14] != hashes[1]:
        raise AssertionError("mutable World.h must be ignored by the source LCG")
    return by_event


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--observations", type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="world-random-tick-", dir=args.work_dir.resolve()))
    resources = out / "resources"
    for name, data in RESOURCES.items():
        target = resources / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    worlds = out / "worlds"
    for index in range(15):
        authored_world(worlds / f"case{index}", index)
    executable = args.executable.resolve(strict=True)
    inputs = [executable, Path(__file__), *sorted(resources.rglob("*.*")), *sorted(worlds.rglob("*.mcr"))]
    if args.observations:
        inputs.append(args.observations.resolve(strict=True))
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "original_comparison": None}
    transports = []
    try:
        command = [str(executable), str(resources), str(worlds), str(out)]
        result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=270)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        report["command"] = command
        lines = result.stdout.decode("ascii").splitlines()
        footer = re.fullmatch(r"World random tick: ([1-9][0-9]*) checks;14 paired profiles and native h-ignored control", lines[-1]) if lines else None
        if result.returncode or result.stderr or footer is None or any(line not in ("151 recipes", "16 achievements") for line in lines[:-1]):
            raise AssertionError(f"native direct FD.n failed ({result.returncode}); {result.stderr.decode(errors='replace')}; {out}")
        rows = (out / "native.txt").read_text(encoding="ascii").splitlines()
        frames = [row for row in rows if row.startswith("RANDOM_FRAME ")]
        states = [row for row in rows if row.startswith("NATIVE_RANDOM_STATE ")]
        events = [row for row in rows if row.startswith(("RANDOM_SOUND ", "RANDOM_CELL ", "RANDOM_RANGE "))]
        failures = [row for row in rows if row.startswith("RANDOM_FAILURE ")]
        if len(rows) != len(frames) + len(states) + len(events) + len(failures) or [int(row.split()[1]) for row in failures] != [4, 5, 7, 8, 13]:
            raise AssertionError("complete typed native output inventory required")
        hashes = {}
        transport_report = {}
        for index in range(15):
            hashes[index] = {}
            for name in STREAMS:
                path = out / f"case{index}-{name}.bin"
                if path.stat().st_size > 32 * 1024 * 1024:
                    raise AssertionError("finite observation transport bound exceeded")
                value = digest(path)
                hashes[index][name] = value
                transport_report[path.name] = {"sha256": value, "bytes": path.stat().st_size}
                transports.append(path)
        by_event = check_native(frames, states, events, hashes, out)
        hash_rows = ["RANDOM_HASH " + str(index) + " " + " ".join(hashes[index][name] for name in STREAMS) for index in range(15)]
        (out / "actual.json").write_text(json.dumps({"frame_rows": frames, "hash_rows": hash_rows, "event_rows": events, "native_state_rows": states, "failure_rows": failures}, indent=2) + "\n")
        if args.observations:
            source = source_frames(args.observations)
            for index, frame in enumerate(source):
                if frame["row"] != frames[index] or frame["hash_row"] != hash_rows[index] or event_rows(frame) != by_event[index]:
                    raise AssertionError(f"unchanged-original direct FD.n profile{index} differs; captures {out}")
                if frame["h_unchanged"] != 1013904223:
                    raise AssertionError("original canonical h input changed")
                if index in (5, 7, 8, 13):
                    expected_failure = "java.util.ConcurrentModificationException" if index == 5 else "java.lang.NullPointerException"
                    if frame["failure"] != expected_failure:
                        raise AssertionError("actual original source failure class differs")
            report["original_comparison"] = {"frame_rows": 14, "hash_rows": 14, "event_rows": sum(len(event_rows(frame)) for frame in source),
                "scope": "Complete actual FD.n frame/hash/event comparison; declared Observer callbacks are controlled inputs. Profile4 pairs reached sound/RNG/cooldown effects with a native IO adapter failure and excludes Java Throwable equivalence. No outer FD.l weather/spawn or default Renderer/audio claim."}
        report.update(status="PASS", checks=int(footer.group(1)), profiles=15, observations=transport_report,
                      cleanup_scope="Random-call and captured-Chunk retirement are proved with b=NULL, World.r=NULL and n[1]=true/m[1]=NULL intact. After call retirement, passive whole-Store admission explicitly refuses b=NULL with UNSUPPORTED_STATE/'Store entity retirement Chunk ownership' and preserves the graph; r=NULL has no analogous Store refusal. Only final fixture teardown restores its retained array/Random/global storage before ordinary World/Store disposal. Unrepaired whole-Store terminal closure with null block bytes is outside this qualification.",
                      scope="Direct actual FD.n on genuine GAME Worlds/READY LocalPlayer/Math/Registry,361 real cached Chunk identities (HD441 padding), all28880 samples per completed nonempty frame; source-setSeed/g/Q/h inputs and separate World.r/shared/private RNG progression; actual installed Blank and one-LM aliases; real HD canonical Tundra/Plains managers,28snow/ice writes and Lightning constructor/publication; reentrant actual P.clear and retryable held failure cleanup; native source-derived integration oracle uses only declared terrain plus separately qualified Random/HashSet primitives, never executes snapshots")
        for path in transports:
            path.resolve().relative_to(out.resolve())
            path.unlink()
        report["observation_transport"] = "Successful temporary binary observation streams consumed to SHA256 and removed; exact hashes/byte counts retained. All transports remain on any comparison failure."
        print(f"Direct World random tick: {footer.group(1)} checks; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        unchanged = before == report["input_after_sha256"]
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n")
        if not unchanged:
            raise AssertionError("authored random-tick inputs changed")


if __name__ == "__main__":
    main()
