"""Compare fixed-work FD.q CPU calls without changing source sampling or entropy."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys
import tempfile

from test_random_display import RESOURCES, advance, authored_world, seed


ROOT = Path(__file__).resolve().parents[3]
WORKLOADS = ("air", "stone")


def need(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hardware_info():
    cpu = platform.processor()
    if sys.platform == "win32":
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
            cpu = winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
    elif sys.platform.startswith("linux"):
        for line in Path("/proc/cpuinfo").read_text(encoding="ascii").splitlines():
            if line.startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    return {"cpu": cpu, "logical_processors": os.cpu_count(), "machine": platform.machine()}


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[max(0, int(len(ordered) * fraction + 0.999999999) - 1)]


def source_pins(executables):
    paths = set(executables.values())
    for directory in ("src", "include"):
        paths.update(path for path in (ROOT / directory).rglob("*")
                     if path.suffix in (".c", ".h", ".inc"))
    paths.update(ROOT / path for path in (
        "tests/world/runtime/test_random_display.c",
        "tests/world/runtime/test_world_random_tick.c",
        "tests/support/java_random.h",
        "tests/tools/world/test_random_display.py",
        "tests/tools/world/test_random_tick.py"))
    paths.update((ROOT / "tests/benchmarks").glob("random_display*"))
    paths.add(Path(__file__).resolve())
    return sorted(paths)


def build_binding(executables):
    directories = {path.parent for path in executables.values()}
    need(len(directories) == 1, "paired executables from one common build required")
    directory = directories.pop()
    ninja = directory / "build.ninja"
    commands = directory / "compile_commands.json"
    rules = directory / "CMakeFiles/rules.ninja"
    need(ninja.is_file(), "Ninja/CMake compilation metadata required")
    lines = ninja.read_text(encoding="utf-8").splitlines()
    objects = {}
    libraries = {}
    selected_lines = {}
    for name, executable in executables.items():
        selected = [line for line in lines if line.startswith(f"build {executable.name}: ")]
        need(len(selected) == 1, "one concrete executable link edge per variant")
        selected_lines[name] = selected[0]
        explicit, implicit = selected[0].split(" | ", 1)
        objects[name] = [directory / word.replace("\\", "/")
                         for word in explicit.split(": ", 1)[1].split()[1:]]
        libraries[name] = [directory / word.replace("\\", "/")
                           for word in implicit.split(" || ", 1)[0].split()]
    need(libraries["detailed"] == libraries["failure"], "identical ordered library tail")
    shared = set(objects["detailed"]) & set(objects["failure"])
    need(len(shared) == 1 and next(iter(shared)).name in ("random_display.c.obj", "random_display.c.o")
         and all(len(values) == 2 for values in objects.values()),
         "one shared compiled observer and one variant adapter per executable")
    metadata = [ninja]
    if commands.is_file():
        raw_compilation = commands.read_bytes()
        compilation_source = {"file": str(commands)}
        metadata.append(commands)
    else:
        # CMake need not export compile_commands.json. Ninja can report its
        # actual commands without compiling or changing the build. Keep both
        # graph files guarded so this report cannot drift during the cohort.
        need(rules.is_file(), "Ninja compilation rules required for command inspection")
        command = ["ninja", "-t", "compdb"]
        result = subprocess.run(command, cwd=directory, stdin=subprocess.DEVNULL,
                                capture_output=True, timeout=60)
        need(result.returncode == 0 and not result.stderr,
             "read-only Ninja compilation inspection failed")
        raw_compilation = result.stdout
        compilation_source = {"command": command, "directory": str(directory)}
        metadata.append(rules)
    compilation = json.loads(raw_compilation)
    selected_commands = [entry for entry in compilation
                         if Path(entry["file"]).name in
                         ("random_display.c", "random_display_detailed.c", "random_display_failure.c")]
    need(len(selected_commands) == 4, "runtime, observer and two adapter compilation commands")
    products = sorted(set([*metadata, *libraries["detailed"],
                           *objects["detailed"], *objects["failure"]]))
    return {"directory": str(directory), "shared_observer": str(next(iter(shared))),
            "link_edges": selected_lines, "compilation": selected_commands,
            "compilation_database": {**compilation_source,
                                     "sha256": hashlib.sha256(raw_compilation).hexdigest()},
            "product_sha256": {str(path): digest(path) for path in products}}, products


def read_rows(path):
    rows = []
    for text in path.read_text(encoding="ascii").splitlines():
        cells = text.split()
        need(len(cells) == 15 and cells[0] in WORKLOADS, "complete native timing row")
        workload, ordinal, timed, ticks, frequency, before, after = cells[:7]
        ordinal, timed, ticks, frequency = map(int, (ordinal, timed, ticks, frequency))
        need(0 <= ordinal < 23 and timed == int(ordinal >= 2) and ticks > 0 and frequency > 0,
             "declared warmup/sample interval")
        need(all(re.fullmatch("[0-9a-f]{12}", word) for word in (before, after)),
             "full 48-bit World Random words")
        started, returned, draws, calls, completed, x, y, z = map(int, cells[7:])
        need((started, returned, draws) == (1000, 1000, 6000) and
             calls == completed == (1000 if workload == "stone" else 0) and
             -15 <= x <= 15 and 49 <= y <= 79 and -15 <= z <= 15,
             "whole source sampler work and final coordinates")
        rows.append({"workload": workload, "ordinal": ordinal, "timed": bool(timed),
                     "ticks": ticks, "frequency": frequency,
                     "milliseconds": ticks * 1000 / frequency,
                     "random_before": before, "random_after": after,
                     "samples_started": started, "samples_returned": returned,
                     "world_draws": draws, "display_calls": calls, "display_returns": completed,
                     "final_coordinates": [x, y, z]})
    need([(row["workload"], row["ordinal"]) for row in rows] ==
         [(workload, ordinal) for workload in WORKLOADS for ordinal in range(23)],
         "every warmup and measured native call retained in source order")
    for workload in WORKLOADS:
        expected = seed(0)
        for row in (row for row in rows if row["workload"] == workload):
            need(int(row["random_before"], 16) == expected, "continuous seeded World trajectory")
            final = advance(expected, 5994)
            draws = []
            for _ in range(6):
                final = advance(final, 1)
                draws.append(final >> 44)
            need(int(row["random_after"], 16) == final and
                 row["final_coordinates"] ==
                 [draws[0] - draws[1], 64 + draws[2] - draws[3], draws[4] - draws[5]],
                 "independent final sampled coordinates and6000draw trajectory")
            expected = final
    return rows


def numeric_projection(rows):
    return [{key: value for key, value in row.items()
             if key not in ("ticks", "frequency", "milliseconds")} for row in rows]


def full_controls(executable, directory):
    directory.mkdir(parents=True)
    command = [sys.executable, "-B", str(ROOT / "tests/tools/world/test_random_display.py"),
               "--executable", str(executable), "--work-dir", str(directory)]
    result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=420)
    (directory / "stdout.txt").write_bytes(result.stdout)
    (directory / "stderr.txt").write_bytes(result.stderr)
    need(result.returncode == 0 and not result.stderr,
         f"full native equivalence controls failed: {directory}")
    receipts = list(directory.glob("world-random-display-*/run.json"))
    need(len(receipts) == 1, "one full fixture invocation receipt")
    receipt = json.loads(receipts[0].read_text(encoding="utf-8"))
    need(receipt["status"] == "PASS" and receipt["direct_profiles"] == 99 and
         receipt["paired_samplers"] == 12 and receipt["native_samplers"] == 3 and
         receipt["input_before_sha256"] == receipt["input_after_sha256"],
         "all existing direct/sampler controls pass without source drift")
    actual = json.loads(receipts[0].with_name("actual.json").read_text(encoding="utf-8"))
    projection = dict(actual)
    # The sampler constructs its own ordinary Random from actual process
    # entropy. The fixture checks each initial/final trajectory independently.
    # Keep raw observations, and compare only fields having equal inputs here.
    projection["SAMPLER_NATIVE"] = [" ".join(row.split()[:-2])
                                     for row in actual["SAMPLER_NATIVE"]]
    projection["SAMPLER_PARTICLE"] = [row for row in actual["SAMPLER_PARTICLE"]
                                       if row.split()[1] != "12"]
    canonical = json.dumps(projection, sort_keys=True, separators=(",", ":")).encode("ascii")
    return {"command": command, "receipt": str(receipts[0]), "sha256": digest(receipts[0]),
            "checks": receipt["checks"], "profiles": 114,
            "direct_events": receipt["direct_events"],
            "sampler_events": receipt["sampler_events"],
            "equal_input_projection_sha256": hashlib.sha256(canonical).hexdigest(),
            "independent_entropy_scope": "Each sampler keeps its raw original local Random words; the native fixture checks each trajectory. These two entropy words per sampler and1000RedTorch particle arguments in native sampler12 have independent inputs across processes and receive no cross-process equality credit. Their counts and source positions remain checked; every other retained output compares exactly."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", action="append", required=True, metavar="VARIANT=PATH")
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=4)
    parser.add_argument("--mode", choices=("controls", "timing"), default="timing")
    args = parser.parse_args()
    executables = {}
    for entry in args.executable:
        name, separator, value = entry.partition("=")
        need(separator and name in ("detailed", "failure") and name not in executables,
             "one executable per distinct comparison variant")
        executables[name] = Path(value).resolve(strict=True)
    need(set(executables) == {"detailed", "failure"} and 1 <= args.rounds <= 16,
         "both variants and bounded declared rounds")
    artifacts = args.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="random-display-", dir=artifacts))
    paths = source_pins(executables)
    binding, products = build_binding(executables)
    paths = sorted(set(paths) | set(products))
    before = {str(path): digest(path) for path in paths}
    report = {"status": "FAIL", "mode": args.mode, "host": platform.platform(),
              "hardware": hardware_info(), "python": sys.version,
              "input_before_sha256": before, "build": binding,
              "controls": {}, "processes": [],
              "scope": "Fixed-work CPU duration of real FD.q begin calls over nine genuine saved cached Chunks. All1000samples/6000Worlddraws execute. Air skips positive Block dispatch; stone executes1000actual no-op display virtuals. Constructor entropy remains natural and is not equated across processes. Setup, scalar oracle, complete plane hashes, assertions, output and outer retirement are untimed. Existing114native profiles cover errors, source mutation, callback order and checked ownership. No new unchanged-original, GPU, scene-FPS or unrestricted-play claim."}
    try:
        for name, executable in executables.items():
            report["controls"][name] = full_controls(executable, out / f"controls-{name}")
        need(report["controls"]["detailed"]["checks"] == report["controls"]["failure"]["checks"],
             "same full native control inventory per variant")
        need(report["controls"]["detailed"]["equal_input_projection_sha256"] ==
             report["controls"]["failure"]["equal_input_projection_sha256"],
             "all equal-input native profile results, diagnostics and ordered events match")
        if args.mode == "timing":
            resources = out / "resources"
            for name, value in RESOURCES.items():
                path = resources / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(value)
            expected = None
            for round_index in range(args.rounds):
                order = ("detailed", "failure") if round_index % 2 == 0 else ("failure", "detailed")
                for name in order:
                    directory = out / f"round{round_index}-{name}"
                    directory.mkdir()
                    worlds = directory / "worlds"
                    for workload in WORKLOADS:
                        authored_world(worlds / workload)
                    protected = sorted([*resources.rglob("*.*"), *worlds.rglob("*.mcr")])
                    input_before = {str(path): digest(path) for path in protected}
                    command = [str(executables[name]), "timing", str(resources), str(worlds),
                               str(directory / "native.txt")]
                    process = {"round": round_index, "variant": name, "command": command,
                               "status": "FAIL", "input_before_sha256": input_before}
                    report["processes"].append(process)
                    result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True,
                                            timeout=180)
                    (directory / "stdout.txt").write_bytes(result.stdout)
                    (directory / "stderr.txt").write_bytes(result.stderr)
                    process["returncode"] = result.returncode
                    need(result.returncode == 0 and not result.stderr,
                         f"native CPU workload failed: {directory}")
                    lines = result.stdout.decode("ascii").splitlines()
                    footer = re.fullmatch(
                        rf"Random display CPU {name}: ([1-9][0-9]*) checks;42 measured calls;42000 samples;252000 World draws",
                        lines[-1]) if lines else None
                    need(footer and all(line in ("151 recipes", "16 achievements")
                                        for line in lines[:-1]), "complete native CPU footer")
                    process["checks"] = int(footer.group(1))
                    process["rows"] = read_rows(directory / "native.txt")
                    projection = numeric_projection(process["rows"])
                    if expected is None:
                        expected = projection
                    need(projection == expected, "all source outputs and World trajectories match")
                    process["input_after_sha256"] = {str(path): digest(path) for path in protected}
                    need(input_before == process["input_after_sha256"], "saved input/resource bytes unchanged")
                    process["status"] = "PASS"
            summaries = {}
            for workload in WORKLOADS:
                summaries[workload] = {}
                for variant in ("detailed", "failure"):
                    values = [row["milliseconds"] for process in report["processes"]
                              if process["variant"] == variant for row in process["rows"]
                              if row["workload"] == workload and row["timed"]]
                    summaries[workload][variant] = {
                        "samples": len(values), "median_ms": statistics.median(values),
                        "mean_ms": statistics.mean(values), "p95_ms": percentile(values, .95),
                        "maximum_ms": max(values)}
                baseline = summaries[workload]["detailed"]["median_ms"]
                summaries[workload]["median_reduction_percent"] = (
                    100 * (baseline - summaries[workload]["failure"]["median_ms"]) / baseline)
            report["summary"] = summaries
        report["status"] = "PASS"
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in paths}
        unchanged = before == report["input_after_sha256"]
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        need(unchanged, "source, harness or executable changed during comparison")
    print(json.dumps({"status": report["status"], "receipt": str(out / "run.json"),
                      "summary": report.get("summary")}, indent=2))


if __name__ == "__main__":
    main()
