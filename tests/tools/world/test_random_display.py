"""Genuine World display sampling and optional unchanged-original event comparison."""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib

from test_random_tick import RESOURCES, digest, tag


ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "tests/reference/world/random_display"
MASK = (1 << 48) - 1
MULTIPLIER = 0x5DEECE66D


def need(condition, message):
    if not condition:
        raise AssertionError(message)


def seed(value):
    return (value ^ MULTIPLIER) & MASK


def advance(value, count):
    for _ in range(count):
        value = (value * MULTIPLIER + 11) & MASK
    return value


def word(value):
    return f"{value:012x}"


def table(name, count, fields):
    with (REFERENCE / name).open(encoding="ascii", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        need(reader.fieldnames == fields.split(), f"authored {name} schema")
        rows = list(reader)
    need(len(rows) == count and [int(row["case"]) for row in rows] == list(range(count)),
         f"complete authored {name} identities")
    return rows


def authored_world(path):
    """Stored empty GAME Chunks; native preparation applies explicit table inputs."""
    region = path / "region"
    region.mkdir(parents=True)
    grouped = {}
    for x in range(-1, 2):
        for z in range(-1, 2):
            grouped.setdefault((x // 32, z // 32), []).append((x, z))
    for (rx, rz), positions in grouped.items():
        header = bytearray(8192)
        sectors = []
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
            need(len(record) <= 4096, "finite authored empty Chunk sector")
            struct.pack_into(">I", header, 4 * ((x & 31) + 32 * (z & 31)),
                             ((2 + len(sectors)) << 8) | 1)
            sectors.append(record + bytes(4096 - len(record)))
        (region / f"r.{rx}.{rz}.mcr").write_bytes(header + b"".join(sectors))


def event_groups(rows, prefix, count):
    result = {index: [] for index in range(count)}
    for row in rows:
        cells = row.split()
        kind = cells[0].removeprefix(prefix + "_")
        need(kind in ("PARTICLE", "SOUND") and len(cells) == (10 if kind == "PARTICLE" else 9),
             f"complete numeric observer payload: {row}")
        index = int(cells[1])
        need(index in result and int(cells[2]) == len(result[index]), "global observer event order")
        need(all(re.fullmatch("[0-9a-f]{16}", value) for value in cells[4:7]),
             "raw source event position double words")
        need(all(re.fullmatch("[0-9a-f]{16}" if kind == "PARTICLE" else "[0-9a-f]{8}", value)
                 for value in cells[7:]), "raw source event motion/sound words")
        result[index].append(row)
    return result


def native_contracts(displays, samplers, direct_native, sampler_native, direct_events, sampler_events,
                     profiles, inputs):
    need(len(displays) == len(direct_native) == 99 and len(samplers) == len(sampler_native) == 15,
         "all direct/sampler results and ownership diagnostics")
    failed = {3, 9, 11, 89, 90, 92, 93, 94}
    for index, (row, profile) in enumerate(zip(displays, profiles)):
        cells = row.split()
        need(len(cells) == 12 and cells[:3] == ["DISPLAY_STATE", str(index), profile["block"]]
             and cells[4] == profile["scenario"] and int(cells[5]) == (-1 if index in failed else 1),
             f"declared direct source input/result{index}: {row}")
        events = direct_events[index]
        particles = sum(event.startswith("DISPLAY_PARTICLE ") for event in events)
        sounds = len(events) - particles
        need(list(map(int, cells[9:])) == [particles, sounds, len(events)], "direct observer inventory")
        diag = direct_native[index].split()
        need(len(diag) == 7 and diag[:2] == ["DISPLAY_NATIVE", str(index)], "direct typed diagnostic")
        need(cells[6] == "-" if profile["argument_seed"] == "NULL" else
             re.fullmatch("[0-9a-f]{12}", cells[6]) is not None, "actual nullable argument Random")
        old_steps = (0 if index == 9 else 3 if index == 11 else 18) if profile["block"] == "74" else 0
        old = word(advance(seed(int(profile["world_seed"])), old_steps))
        need(cells[8] == old and cells[7] == ("-" if index == 9 else word(seed(123)) if index == 10 else old),
             f"distinct original/late World Random progression{index}")
        if index in (3, 9, 89, 94):
            need(diag[-1] == "0", "genuine source-null failure must retain source classification")
        block = int(profile["block"])
        if block in (1, 67, 73, 75, 61, 93) or (block == 53 and index != 4):
            need(not events, "inherited or inactive source body has no presentation")
        if block == 50 or index == 4:
            need(particles == (1 if profile["callback"] in ("fail_particle", "clear_observers") else 2)
                 and sounds == 0, "actual Torch smoke/flame order and observer mutation")
        if block == 74:
            need(particles == (0 if index in (8, 9) else 1 if index == 11 else 6 if index in (7, 98) else 5),
                 "actual ore exposed-face particle count including source lower-Y quirk")
        if block == 90:
            need(particles == 4 and sounds == (int(profile["argument_seed"]) == 18),
                 "four actual portal attempts and optional source sound")
    for index, row in enumerate(samplers):
        cells = row.split()
        diag = sampler_native[index].split()
        scenario = inputs[index]["scenario"] if index < 12 else (
            "native_local_draws", "native_air_m_null", "native_positive_m_null")[index - 12]
        pc = {6: 31, 7: 125, 10: 125, 11: 100, 14: 115}.get(index, 134)
        returned = 0 if index in (6, 7, 10, 14) else 1 if index == 11 else 1000
        draws = 0 if index == 6 else 6 if index in (7, 10, 14) else 12 if index == 11 else 6000
        started = returned if pc == 134 else returned + 1
        calls = 1000 if index in (1, 2, 3, 9, 12) else 1 if index in (7, 10, 11) else 0
        need(len(cells) == 11 and cells[:3] == ["SAMPLER_STATE", str(index), scenario]
             and int(cells[3]) == (1 if pc == 134 else -1) and cells[6:8] == ["17", "6001"],
             f"whole source sampler input/result{index}")
        need(len(diag) == 10 and diag[:2] == ["SAMPLER_NATIVE", str(index)] and
             list(map(int, diag[2:8])) == [pc, started, returned, draws, calls,
                                         0 if index in (7, 10) else calls],
             f"exact full1000/pending sampler source prefix{index}: {diag}")
        need(diag[9] == word(advance(int(diag[8], 16), 3000 if index == 12 else 0)),
             "independent default local Random must advance only through actual parameter use")
        old = word(advance(seed(0), 6 if index == 9 else draws))
        current = "-" if index == 6 else word(advance(seed(123), 5994)) if index == 9 else old
        need(cells[4:6] == [current, old], "six late World draws/sample and actual field replacement")
        events = sampler_events[index]
        particles = {9: 2000, 10: 1, 11: 2, 12: 1000}.get(index, 0)
        need(cells[8:] == list(map(str, (particles, 0, particles))) and len(events) == particles,
             "whole sampler observer inventory")


def portable_source_path(value):
    if os.name != "nt" and re.match(r"^[A-Za-z]:\\", value):
        return Path("/mnt") / value[0].lower() / value[3:].replace("\\", "/")
    return Path(value)


def original_inputs(observations):
    capture = observations.with_name("capture.json")
    data = json.loads(capture.read_text(encoding="utf-8"))
    need(data["status"] == "PASS" and data["input_before_sha256"] == data["input_after_sha256"]
         and data["helper_before_sha256"] == data["helper_after_sha256"],
         "audited original must have unchanged source/helper inputs")
    pins = {portable_source_path(path): value for path, value in data["input_after_sha256"].items()}
    pins.update({capture.parent / "classes" / path: value
                 for path, value in data["helper_after_sha256"].items()})
    for path, expected in pins.items():
        need(path.is_file() and digest(path) == expected, f"current original source/helper pin: {path}")
    return [capture, *pins], data


def compare_original(source, displays, samplers, direct_events, sampler_events):
    need(source["display_rows"] == displays and source["sampler_rows"] == samplers[:12],
         "actual unchanged-original whole source state rows differ")
    need([row["case"] for row in source["display_records"]] == list(range(99)) and
         [row["case"] for row in source["sampler_records"]] == list(range(12)),
         "all original complete record identities")
    need([event for index in range(99) for event in direct_events[index]] == source["display_event_rows"],
         "actual original ordered direct particle/sound raw words differ")
    need([event for index in range(12) for event in sampler_events[index]] == source["sampler_event_rows"],
         "actual original ordered sampler particle/sound raw words differ")
    for row in source["display_records"]:
        need(not row["argument_is_world_random"], "source direct argument Random identity")
        if row["case"] in (3, 9, 89, 94):
            need(row["failure"] == "java.lang.NullPointerException", "source direct null class")
    for row in source["sampler_records"]:
        need(not row["local_default_entropy_rebound"], "source sampler default entropy unmodified")
        if row["case"] in (6, 7, 11):
            need(row["failure"] == "java.lang.NullPointerException", "original sampler null class")
    return {"display_rows": 99, "sampler_rows": 12,
            "display_events": len(source["display_event_rows"]),
            "sampler_events": len(source["sampler_event_rows"]),
            "scope": "Exact seeded direct rows and all ordered raw IEEE event words; whole1000/6000 sampler state/event rows. Independent default local entropy is never rebound or equated between processes. Native controlled callback IO cases pair reached effects without Java Throwable equivalence; sampler11 pairs the late real getter failure prefix, with the legacy native getter diagnostic retained rather than fabricated Java callee frames."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--observations", type=Path)
    args = parser.parse_args()
    profiles = table("profiles.tsv", 99,
                     "case block scenario metadata argument_seed world_seed x y z scene callback")
    samplers = table("samplers.tsv", 12, "case scenario block x y z scene callback")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="world-random-display-", dir=args.work_dir.resolve()))
    resources = out / "resources"
    for name, value in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)
    worlds = out / "worlds"
    for group, count in (("sampler", 15), ("display", 99)):
        for index in range(count):
            authored_world(worlds / f"{group}{index}")
    executable = args.executable.resolve(strict=True)
    paths = {executable, Path(__file__).resolve(), Path(__file__).with_name("test_random_tick.py").resolve(),
             ROOT / "tests/world/runtime/test_random_display.c", ROOT / "tests/world/runtime/test_world_random_tick.c",
             *resources.rglob("*.*"), *worlds.rglob("*.mcr"),
             *(path for path in REFERENCE.iterdir() if path.is_file())}
    for directory in (ROOT / "src", ROOT / "include", ROOT / "tests/support"):
        paths.update(path for path in directory.rglob("*") if path.suffix in (".c", ".h"))
    source = None
    receipt = None
    if args.observations:
        observation = args.observations.resolve(strict=True)
        extra, receipt = original_inputs(observation)
        paths.update([observation, *extra])
        source = json.loads(observation.read_text(encoding="utf-8"))
    paths = sorted(path.resolve(strict=True) for path in paths)
    before = {str(path): digest(path) for path in paths}
    report = {"status": "FAIL", "input_before_sha256": before, "original_comparison": None}
    try:
        command = [str(executable), str(resources), str(worlds), str(out / "native.txt"),
                   str(REFERENCE / "profiles.tsv"), str(REFERENCE / "samplers.tsv")]
        report["command"] = command
        result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=270)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        lines = result.stdout.decode("ascii").splitlines()
        footer = re.fullmatch(r"Random display: ([1-9][0-9]*) checks;99 direct and12 paired samplers plus3 native controls",
                              lines[-1]) if lines else None
        need(result.returncode == 0 and not result.stderr and footer is not None and
             all(line in ("151 recipes", "16 achievements") for line in lines[:-1]),
             f"native display fixture failed ({result.returncode}); inspect {out}")
        rows = (out / "native.txt").read_text(encoding="ascii").splitlines()
        groups = {name: [] for name in ("DISPLAY_STATE", "SAMPLER_STATE", "DISPLAY_NATIVE", "SAMPLER_NATIVE",
                                       "DISPLAY_PARTICLE", "DISPLAY_SOUND", "SAMPLER_PARTICLE", "SAMPLER_SOUND")}
        for row in rows:
            name = row.split()[0]
            need(name in groups, "complete native observation inventory")
            groups[name].append(row)
        # Preserve the one shared particle/sound ordinal, rather than regrouping by kind.
        display_events = event_groups([row for row in rows if row.startswith(("DISPLAY_PARTICLE ", "DISPLAY_SOUND "))], "DISPLAY", 99)
        sampler_events = event_groups([row for row in rows if row.startswith(("SAMPLER_PARTICLE ", "SAMPLER_SOUND "))], "SAMPLER", 15)
        native_contracts(groups["DISPLAY_STATE"], groups["SAMPLER_STATE"], groups["DISPLAY_NATIVE"],
                         groups["SAMPLER_NATIVE"], display_events, sampler_events, profiles, samplers)
        (out / "actual.json").write_text(json.dumps(groups, indent=2) + "\n", encoding="utf-8", newline="\n")
        if source:
            report["original_comparison"] = compare_original(source, groups["DISPLAY_STATE"],
                groups["SAMPLER_STATE"], display_events, sampler_events)
            report["original_receipt_scope"] = receipt["scope"]
        report.update(status="PASS", checks=int(footer.group(1)), direct_profiles=99, paired_samplers=12,
            native_samplers=3, direct_events=sum(map(len, display_events.values())),
            sampler_events=sum(map(len, sampler_events.values())),
            scope="Actual retained FD.q and eleven concrete display declarations on fresh genuine GAME World/Registry/Store/normal Chunks and exact installed Blank. All1000samples/6000lateWorlddraws are executed on completed sampler routes. Seeded direct argument and World Randoms remain distinct; a separately constructed default local Random has stable callback identity and independently checked3000float advances under whole-sampler RedTorch. Actual query/observer mutation, NULL source prefixes and no-repair call retirement are checked; source controls are restored only at final fixture disposal. Observer particles/sounds qualify presentation arguments, not particle Entity constructors, renderer pixels or audio output.")
        print(f"World random display: {report['checks']} checks; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in paths}
        unchanged = before == report["input_after_sha256"]
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        need(unchanged, "native source/helper/executable/authored fixture inputs changed")


if __name__ == "__main__":
    main()
