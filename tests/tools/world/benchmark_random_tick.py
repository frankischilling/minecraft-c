"""Run or audit finite genuine-World random-Chunk CPU work; no FPS claim."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
MASK = (1 << 48) - 1


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bounded(state, bound):
    while True:
        state = (state * 0x5DEECE66D + 11) & MASK
        bits = state >> 17
        value = bits % bound
        if bound & (bound - 1) == 0:
            return state, (bound * bits) >> 31
        if (bits - value + bound - 1) & 0xFFFFFFFF < 0x80000000:
            return state, value


def parse_rows(lines, workload):
    require(workload in ("air_qhigh", "stone_q0"), "known authored workload required")
    require(len(lines) == 16, "two warmups and fourteen complete samples required")
    g, random, q = 17, 0x5DEECE66D, 6001 if workload == "air_qhigh" else 0
    rows = []
    for ordinal, line in enumerate(lines):
        cells = line.split()
        require(len(cells) == 17 and cells[0] == workload, "complete native row required")
        require([int(cells[1]), int(cells[2])] == [ordinal, int(ordinal >= 2)], "sample order differs")
        times = [float(value) for value in cells[3:5]]
        require(all(math.isfinite(value) and value >= 0 for value in times), "finite timing required")
        require([int(cells[5], 16), int(cells[7], 16)] == [g, random], "source state prefix differs")
        q = max(0, q - 1)
        snow, ambient = 0, 361 if q == 0 else 0
        for _ in range(361):
            if q == 0:
                g = (g * 3 + 1013904223) & 0xFFFFFFFF
            random, _ = bounded(random, 100000)
            random, ticket = bounded(random, 16)
            if ticket == 0:
                snow += 1
                g = (g * 3 + 1013904223) & 0xFFFFFFFF
            for _ in range(80):
                g = (g * 3 + 1013904223) & 0xFFFFFFFF
        require([int(cells[6], 16), int(cells[8], 16)] == [g, random], "source RNG/LCG differs")
        require(list(map(int, cells[9:])) ==
                [q, 361, 28880, 722, 28880 + ambient + snow, ambient, snow, 0],
                "all Chunk/sample/Random/ambient/weather work required")
        rows.append({"ms": times[0], "retirement_ms": times[1],
                     "timed": ordinal >= 2, "state": cells[:3] + cells[5:]})
    return rows


def summary(values):
    require(bool(values), "nonempty finite samples required")
    ordered = sorted(values)
    return {"count": len(values), "min": min(values), "median": statistics.median(values),
            "mean": statistics.mean(values),
            "p95_nearest_rank": ordered[math.ceil(.95 * len(ordered)) - 1], "max": max(values)}


def audit_cohort(cohort):
    rounds = cohort["rounds"]
    require(isinstance(rounds, int) and 1 <= rounds <= 12, "bounded round count required")
    expected = []
    for round_ in range(rounds):
        for index, workload in enumerate(("air_qhigh", "stone_q0"), 1):
            order = ("baseline", "candidate") if (round_ + index) % 2 else ("candidate", "baseline")
            expected.extend((round_, workload, variant, position) for position, variant in enumerate(order))
    actual = [(row["round"], row["workload"], row["variant"], row["position_in_pair"])
              for row in cohort["processes"]]
    require(actual == expected, "every prespecified fresh process/order must be retained")
    values, traces, strata = {}, {}, {}
    for process in cohort["processes"]:
        require(process["exit"] == 0 and not process.get("timeout"), "native failure retained; cohort invalid")
        key = process["workload"], process["variant"]
        rows = parse_rows(process["rows"], key[0])
        measured = [row["ms"] for row in rows if row["timed"]]
        values.setdefault(key, []).extend(measured)
        strata.setdefault((*key, process["position_in_pair"]), []).extend(measured)
        traces[(*key, process["round"])] = [row["state"] for row in rows]
    result = {}
    for workload in ("air_qhigh", "stone_q0"):
        for round_ in range(rounds):
            require(traces[workload, "baseline", round_] == traces[workload, "candidate", round_],
                    "matched source/state/work prefixes differ")
        result[workload] = {variant: summary(values[workload, variant])
                            for variant in ("baseline", "candidate")}
        result[workload]["order_strata"] = {
            f"{variant}-{'first' if position == 0 else 'second'}": summary(samples)
            for (name, variant, position), samples in strata.items() if name == workload}
    return result


def audit_receipt(receipt):
    require(receipt["schema"] == 1, "known CPU receipt schema required")
    controls = receipt["controls"]
    require(controls["baseline_raw_transcript"] == controls["candidate_raw_transcript"],
            "independent original/raw helper control transcript differs")
    raw_bytes = ("\r\n".join(controls["baseline_raw_transcript"]) + "\r\n").encode("ascii")
    require(hashlib.sha256(raw_bytes).hexdigest() == controls["raw_transcript_sha256"],
            "captured Windows raw transcript binding differs")
    require(sum(row.startswith("PAIR ") for row in controls["baseline_raw_transcript"]) == 44,
            "complete original paired control inventory required")
    require(controls["baseline_world_rows"] == controls["candidate_world_rows"],
            "whole-World callback/failure/state transcript differs")
    rows = controls["baseline_world_rows"]
    frames = [row.split() for row in rows if row.startswith("RANDOM_FRAME ")]
    states = [row.split() for row in rows if row.startswith("NATIVE_RANDOM_STATE ")]
    failures = [row.split() for row in rows if row.startswith("RANDOM_FAILURE ")]
    require([int(row[1]) for row in frames] == list(range(15)) and
            [int(row[1]) for row in states] == list(range(15)), "all fifteen native profiles required")
    require([int(row[1]) for row in failures] == [4, 5, 7, 8, 13], "retained failed source prefixes required")
    hashes = controls["baseline_world_outputs_sha256"]
    require(len(hashes) == 76 and hashes == controls["candidate_world_outputs_sha256"],
            "all complete source observation output hashes must match")
    world_bytes = ("\n".join(rows) + "\n").encode("ascii")
    require(hashlib.sha256(world_bytes).hexdigest() == hashes["native.txt"],
            "complete World transcript binding differs")
    require(all(len(value) == 64 and all(c in "0123456789abcdef" for c in value)
                for value in hashes.values()), "complete SHA256 bindings required")
    for row in states:
        index = int(row[1])
        pc = {4: 443, 5: 172, 7: 944, 8: 982, 13: 470}.get(index, 994)
        require(len(row) == 18 and int(row[2]) == pc, "source failure/return boundary differs")
        if index not in (4, 5, 7, 8, 13):
            require(int(row[7]) == (28880 if index else 0), "complete native sample count differs")
    require([item["name"] for item in receipt["cohorts"]] == ["initial", "confirmation"],
            "initial and separate confirmation must remain distinct")
    result = {}
    for cohort in receipt["cohorts"]:
        computed = audit_cohort(cohort)
        for workload in ("air_qhigh", "stone_q0"):
            for variant in ("baseline", "candidate"):
                captured = cohort["captured_statistics"][workload][variant + "_ms"]
                for field, value in computed[workload][variant].items():
                    require(math.isclose(value, captured[field], rel_tol=1e-12, abs_tol=1e-9),
                            "captured statistics differ from complete raw rows")
        result[cohort["name"]] = computed
    return result


def collect(executables, rounds, artifacts, mode):
    require(not artifacts.exists(), "new artifact directory required; preserve previous attempts")
    artifacts.mkdir(parents=True)
    report = {"status": "FAIL", "processes": [], "rounds": rounds,
              "executables_sha256": {name: digest(path) for name, path in executables.items()}}
    report_path = artifacts / "report.json"
    try:
        for variant, executable in executables.items():
            command = [sys.executable, "-B", str(Path(__file__).with_name("test_random_tick.py")),
                       "--executable", str(executable), "--work-dir", str(artifacts / (variant + "-controls"))]
            result = subprocess.run(command, capture_output=True, timeout=300)
            (artifacts / (variant + "-controls.stdout")).write_bytes(result.stdout)
            (artifacts / (variant + "-controls.stderr")).write_bytes(result.stderr)
            require(result.returncode == 0, variant + " genuine World controls failed")
        require(report["executables_sha256"] == {name: digest(path) for name, path in executables.items()},
                "executable changed after controls")
        if mode == "controls":
            report["status"] = "PASS_CONTROLS_ONLY"
            return report
        spec = importlib.util.spec_from_file_location("random_fixture", Path(__file__).with_name("test_random_tick.py"))
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        resources = artifacts / "resources"
        for name, data in fixture.RESOURCES.items():
            path = resources / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        resources_before = {str(path): digest(path) for path in resources.rglob("*") if path.is_file()}
        for round_ in range(rounds):
            for index, workload in enumerate(("air_qhigh", "stone_q0"), 1):
                order = ("baseline", "candidate") if (round_ + index) % 2 else ("candidate", "baseline")
                for position, variant in enumerate(order):
                    directory = artifacts / f"{len(report['processes']):02}-{workload}-r{round_}-{variant}"
                    fixture.authored_world(directory / "world", index)
                    regions_before = {str(path): digest(path) for path in (directory / "world").rglob("*.mcr")}
                    command = [str(executables[variant]), str(resources), str(directory / "world"),
                               str(directory / "raw.txt"), str(index)]
                    row = {"round": round_, "workload": workload, "variant": variant,
                           "position_in_pair": position, "command": command}
                    report["processes"].append(row)
                    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
                    native = subprocess.run(command, capture_output=True, timeout=30)
                    (directory / "stdout.txt").write_bytes(native.stdout)
                    (directory / "stderr.txt").write_bytes(native.stderr)
                    row["exit"] = native.returncode
                    require(native.returncode == 0, "native failure; preserve artifact directory")
                    row["rows"] = (directory / "raw.txt").read_text().splitlines()
                    require(all(digest(Path(path)) == value for path, value in regions_before.items()),
                            "authored saved Chunk bytes changed")
        report["statistics"] = audit_cohort(report)
        require(report["executables_sha256"] == {name: digest(path) for name, path in executables.items()},
                "executable changed")
        require(all(digest(Path(path)) == value for path, value in resources_before.items()),
                "authored resources changed")
        report["status"] = "PASS_FIXED_WORK_CPU_ONLY"
        return report
    finally:
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--executable", action="append", default=[], help="baseline=PATH or candidate=PATH")
    parser.add_argument("--rounds", type=int, default=3, choices=range(1, 13))
    parser.add_argument("--mode", choices=("controls", "compare"), default="controls")
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    if args.receipt:
        print(json.dumps(audit_receipt(json.loads(args.receipt.read_text())), indent=2))
        return
    executables = dict(item.split("=", 1) for item in args.executable)
    require(set(executables) == {"baseline", "candidate"} and args.artifacts, "two labelled executables and new artifacts required")
    collect({name: Path(value).resolve(strict=True) for name, value in executables.items()},
            args.rounds, args.artifacts.resolve(), args.mode)


if __name__ == "__main__":
    main()
