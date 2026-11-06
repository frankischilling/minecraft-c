"""Run or audit finite READY Block/cache CPU work. No scene/FPS claim."""
import argparse
from functools import lru_cache
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
RECEIPT = ROOT / "tests/benchmarks/static_cpu_windows.json"
VARIANTS = ("baseline", "candidate")
COUNTS = (513, 1024, 4096)
PATTERNS = ("mixed", "one-bucket")
MASK64 = (1 << 64) - 1
PUBLIC_BLOCK_CONTROLS = 1032


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return digest(json.dumps(value, sort_keys=True, separators=(",", ":"),
                             allow_nan=False, ensure_ascii=True).encode("ascii"))


def fields(line, prefix, names):
    cells = line.split()
    require(cells and cells[0] == prefix and len(cells) == len(names) + 1,
            "complete typed native row required")
    result = {}
    for cell, name in zip(cells[1:], names):
        key, separator, value = cell.partition("=")
        require(separator == "=" and key == name and value, "native field order differs")
        result[key] = value
    return result


@lru_cache(maxsize=1)
def expected_keys():
    """Independent reconstruction of authored key sequence/checksums."""
    values = {}
    for pattern in PATTERNS:
        value, collision = 1469598103934665603, 0
        for index in range(max(COUNTS)):
            if pattern == "mixed":
                bits = (index * 2654435761) & 0xFFFFFFFF
            else:
                while True:
                    hashed = collision ^ (collision >> 16)
                    hashed = (hashed * 0x7FEB352D) & 0xFFFFFFFF
                    hashed ^= hashed >> 15
                    hashed = (hashed * 0x846CA68B) & 0xFFFFFFFF
                    hashed ^= hashed >> 16
                    if not hashed & 1023:
                        break
                    collision += 1
                    require(collision <= 0xFFFFFFFF, "collision enumeration exhausted")
                bits = collision
                collision += 1
            value = ((value ^ bits) * 1099511628211) & MASK64
            if index + 1 in COUNTS:
                values[pattern, index + 1] = value
    return values


def parse_block(stdout, expected_controls=1011):
    lines = stdout.splitlines()
    require(lines[:4] == ["16 achievements", "151 recipes"] * 2 and len(lines) == 5,
            "complete genuine Registry control prefix required")
    row = fields(lines[4], "BLOCK_STATIC_GETTER_BENCH", (
        "side", "phase", "rounds", "fields", "calls", "ticks", "frequency", "checksum",
        "controls", "live-cells", "source-order"))
    require(row.pop("side") == "client" and row.pop("phase") == "READY",
            "actual client READY getter required")
    row = {name: int(value) for name, value in row.items()}
    require(all(0 <= value <= MASK64 for value in row.values()), "bounded integer fields required")
    require(0 < row["rounds"] <= MASK64 // 4656 and row["fields"] == 96 and
            row["calls"] == row["rounds"] * 96 and row["checksum"] == row["rounds"] * 4656,
            "complete getter work/checksum required")
    require(row["controls"] == expected_controls and row["live-cells"] == row["source-order"] == 1,
            "all untimed semantic/live-cell controls required")
    require(row["ticks"] > 0 and row["frequency"] > 0, "positive elapsed timer required")
    row["ns_per_get"] = row["ticks"] * 1e9 / row["frequency"] / row["calls"]
    return [row]


def parse_cache(stdout, controls_only=False):
    lines = stdout.splitlines()
    require(lines and lines[0] ==
            "CACHE_CONTROLS checks=780 immutable=1 sizes-through=4096 patterns=4 collision-keys=1",
            "all 780 validation/immutability controls required")
    require(len(lines) == (1 if controls_only else 25), "complete cache transcript required")
    if controls_only:
        return []
    result, cursor, sink = [], 1, 0
    for pattern in PATTERNS:
        for count in COUNTS:
            warmup = fields(lines[cursor], "CACHE_WARMUP", ("count", "pattern", "calls", "immutable"))
            require(warmup == {"count": str(count), "pattern": pattern, "calls": "4", "immutable": "1"},
                    "all four unchanged warmups required")
            cursor += 1
            pairs = count * (count - 1) // 2
            calls = 50000000 // pairs + 1
            for sample in range(3):
                row = fields(lines[cursor], "CACHE_CPU", (
                    "count", "pattern", "sample", "calls", "keys", "baseline-pairs-per-call",
                    "cpu-ns", "wall-ticks", "frequency", "immutable", "sink"))
                require(row.pop("pattern") == pattern, "cache pattern/order differs")
                row = {name: int(value) for name, value in row.items()}
                require(all(0 <= value <= MASK64 for value in row.values()), "bounded integer fields required")
                for _ in range(calls):
                    sink = (sink * 1099511628211 + 1) & MASK64
                require(row["count"] == count and row["sample"] == sample and row["calls"] == calls and
                        row["keys"] == expected_keys()[pattern, count] and
                        row["baseline-pairs-per-call"] == pairs and row["sink"] == sink,
                        "complete key/work/sink sequence required")
                require(row["immutable"] == 1 and row["cpu-ns"] >= 0 and row["wall-ticks"] > 0 and
                        row["frequency"] > 0, "immutable finite complete sample required")
                row["pattern"] = pattern
                row["qpc_ms_per_call"] = row["wall-ticks"] * 1000 / row["frequency"] / calls
                result.append(row)
                cursor += 1
    return result


def summary(values, unit):
    require(values and all(math.isfinite(x) and x > 0 for x in values), "finite samples required")
    return {"n": len(values), "median_" + unit: statistics.median(values),
            "mean_" + unit: statistics.mean(values), "min_" + unit: min(values),
            "max_" + unit: max(values)}


def close(actual, expected):
    if isinstance(actual, dict):
        require(isinstance(expected, dict) and actual.keys() == expected.keys(), "summary fields differ")
        for name in actual:
            close(actual[name], expected[name])
    elif isinstance(actual, list):
        require(isinstance(expected, list) and len(actual) == len(expected), "summary rows differ")
        for left, right in zip(actual, expected):
            close(left, right)
    elif isinstance(actual, (int, float)):
        require(type(expected) in (int, float) and math.isfinite(expected) and
                math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-9), "computed timing differs")
    else:
        require(actual == expected, "summary identity differs")


def audit_cohort(cohort):
    kind, rounds = cohort["kind"], cohort["rounds"]
    if "status" in cohort:
        require(cohort["status"] == "PASS", "failed cohort cannot receive timing credit")
    require(kind in ("block", "cache") and type(rounds) is int and 1 <= rounds <= 12,
            "bounded known cohort required")
    order = [list(VARIANTS if r % 2 == 0 else reversed(VARIANTS)) for r in range(rounds)]
    require(cohort["declared_order"] == order and len(cohort["processes"]) == rounds * 2,
            "every declared fresh process/order must be retained")
    values, signatures, executables = {}, {}, {}
    for index, process in enumerate(cohort["processes"]):
        round_, position = divmod(index, 2)
        variant = order[round_][position]
        require((process["round"], process["position"], process["variant"]) ==
                (round_ + 1, position + 1, variant), "process order differs")
        require(process["exit"] == 0 and not process.get("timeout") and process["stderr"] == "",
                "native failure retained; cohort invalid")
        require(math.isfinite(process["seconds"]) and process["seconds"] >= 0, "finite host time required")
        for stream in ("stdout", "stderr"):
            require(digest(process[stream].encode("ascii")) == process[stream + "_sha256"],
                    "exact captured stream binding differs")
        executable = process["exe_sha256"]
        require(re.fullmatch("[0-9a-f]{64}", executable), "complete executable identity required")
        executables.setdefault(variant, executable)
        require(executables[variant] == executable, "executable changed within cohort")
        rows = (parse_block(process["stdout"], cohort.get("block_controls", 1011))
                if kind == "block" else parse_cache(process["stdout"]))
        if kind == "block":
            row = rows[0]
            for name in ("ticks", "frequency", "calls", "ns_per_get"):
                if name in process:
                    close(row[name], process[name])
            values.setdefault(variant, []).append(row["ns_per_get"])
            signatures[round_, variant] = {k: v for k, v in row.items() if k not in ("ticks", "frequency", "ns_per_get")}
        else:
            signature = []
            for row in rows:
                key = row["count"], row["pattern"], variant
                values.setdefault(key, []).append(row["qpc_ms_per_call"])
                signature.append({k: v for k, v in row.items()
                                  if k not in ("cpu-ns", "wall-ticks", "frequency", "qpc_ms_per_call")})
            signatures[round_, variant] = signature
            if "rows" in process:
                require(len(process["rows"]) == len(rows), "retained parsed sample inventory differs")
                for raw, saved in zip(rows, process["rows"]):
                    projected = {k.replace("-", "_"): v for k, v in raw.items()
                                 if k not in ("immutable", "baseline-pairs-per-call")}
                    close(projected, saved)
    for round_ in range(rounds):
        require(signatures[round_, "baseline"] == signatures[round_, "candidate"],
                "matched work/checksums differ")
    if kind == "block":
        result = {variant: summary(values[variant], "ns") for variant in VARIANTS}
        result["median_change_percent"] = (result["candidate"]["median_ns"] /
                                           result["baseline"]["median_ns"] - 1) * 100
    else:
        result = []
        for pattern in PATTERNS:
            for count in COUNTS:
                row = {"count": count, "pattern": pattern}
                row.update({variant: summary(values[count, pattern, variant], "ms") for variant in VARIANTS})
                row["median_change_percent"] = (row["candidate"]["median_ms"] /
                                                row["baseline"]["median_ms"] - 1) * 100
                result.append(row)
    if "summary" in cohort:
        close(result, cohort["summary"])
    return result


def safe_identity(value):
    return (isinstance(value, str) and value and "\\" not in value and ":" not in value and
            not PurePosixPath(value).is_absolute() and ".." not in PurePosixPath(value).parts)


def c_tokens(text):
    """Preserve string/character literals while dropping comments/whitespace."""
    pattern = r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|//[^\n]*|/\*.*?\*/'
    text = re.sub(pattern, lambda match: match[1] or "", text, flags=re.S)
    return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_0-9]+|[^\s]', text)


def audit_receipt(receipt, check_sources=True):
    require(receipt["schema"] == 1 and receipt["kind"] == "finite_static_cpu_windows",
            "known receipt schema required")
    serialized = json.dumps(receipt)
    require(not re.search(r"[A-Za-z]:\\|/home/|/Users/", serialized), "machine-private path leaked")
    require([c["name"] for c in receipt["captures"]] ==
            ["block_ready_final", "cache_prototype1", "cache_prototype2", "cache_final3"],
            "all measured cohorts must remain distinct")
    require([c["kind"] for c in receipt["captures"]] == ["block", "cache", "cache", "cache"] and
            all(c["rounds"] == 4 for c in receipt["captures"]), "historical cohort scope differs")
    require(receipt["captures"][0].get("block_controls", 1011) == 1011,
            "historical1011 and current public1032 controls must remain distinct")
    require([c["qualification"] for c in receipt["captures"]] == [
        "FINAL_FINITE_READY_GETTER_CPU", "RETAINED_PROTOTYPE_COLLISION_REGRESSION",
        "RETAINED_PROTOTYPE_COLLISION_REGRESSION", "FINAL_FINITE_CPU_WITH_COLLISION_TRADEOFF"],
        "historical qualification/tradeoffs must remain explicit")
    result = {}
    for capture in receipt["captures"]:
        require(canonical({k: v for k, v in capture.items() if k != "projection_sha256"}) ==
                capture["projection_sha256"], "capture projection integrity differs")
        provenance = capture["provenance"]
        for name in ("capture_hashes", "products_sha256", "selected_inputs_sha256"):
            require(provenance[name] and all(safe_identity(k) and re.fullmatch("[0-9a-f]{64}", v)
                                            for k, v in provenance[name].items()),
                    "portable complete provenance required")
        libraries = provenance["ordered_library_tail"]
        require(libraries and all(safe_identity(row["identity"]) and
                                  re.fullmatch("[0-9a-f]{64}", row["sha256"]) for row in libraries),
                "complete ordered frozen library tail required")
        compiles = provenance["actual_compile_records"]
        require(compiles and all(row["exit"] == 0 and
                                 set(("/O2", "/Ob2", "-std:c17", "/W4", "/WX", "/fp:strict")).issubset(row["flags"])
                                 for row in compiles), "actual strict successful compilation records required")
        result[capture["name"]] = audit_cohort(capture)
        for process in capture["processes"]:
            require(process["exe_sha256"] == provenance["products_sha256"][process["variant"] + ".exe"],
                    "captured executable/build identity differs")
    cache = receipt["captures"][1:]
    require(all(c["provenance"]["products_sha256"]["observer.obj"] ==
                "f6d58d6d966b3d8ed5801d9f67d8e71eb05f2b19b3781b14e0ed85b5cc485487" for c in cache),
            "exact shared cache observer required across all prototypes")
    require([c["provenance"]["observer_reused"] for c in cache] == [False, True, True],
            "observer compile versus exact-object reuse must remain distinct")
    failure = receipt["retained_block_fixture_failure"]
    require(failure["qualification"] == "RETAINED_FAILURE_NO_TIMING_CREDIT" and
            failure["baseline_exit"] == 1 and failure["candidate_status"] == "NOT_RUN" and
            failure["diagnostic_projection"] and failure["projection_policy"],
            "original failed fixture prefix must remain retained and qualified")
    public = receipt["public_caller_smoke"]
    require(public["qualification"] == "SEMANTIC_SMOKE_NO_TIMING_CREDIT" and
            public["result"]["cache_exit"] == public["result"]["block_exit"] == 0,
            "public caller smoke must remain separate from historical timings")
    for name, stream in public["persisted_streams"].items():
        require(name in ("block.stdout", "cache.stdout") and stream["encoding"] == "utf-16le-bom",
                "declared PowerShell persisted text encoding required")
        raw = b"\xff\xfe" + stream["text"].encode("utf-16le")
        require(len(raw) == stream["bytes"] and digest(raw) == stream["sha256"],
                "exact persisted public smoke stream binding differs")
    parse_block(public["persisted_streams"]["block.stdout"]["text"], PUBLIC_BLOCK_CONTROLS)
    parse_cache(public["persisted_streams"]["cache.stdout"]["text"], controls_only=True)
    require(set(receipt["publication_sources"]) == {
        "tests/benchmarks/block_static_getter.c", "tests/benchmarks/cache_validation.c"},
        "both public source mappings required")
    captured_sources = {"tests/benchmarks/block_static_getter.c":
                        "87f7ea883fe1caa2a90e61c66268d8a958b46e51bbe22e84da65502433f3969a",
                        "tests/benchmarks/cache_validation.c":
                        "d01e7c0476d8f2b070d90497cb8d787794a1312efbfdbc134e38fd32dbbdb90c"}
    for name, projected in receipt["publication_sources"].items():
        require(safe_identity(name) and name.startswith("tests/benchmarks/") and
                re.fullmatch("[0-9a-f]{64}", projected["sha256"]), "portable source projection required")
        require(projected["captured_source_sha256"] == captured_sources[name],
                "captured caller identity differs")
        if check_sources:
            require(digest((ROOT/name).read_bytes()) == projected["sha256"], "published caller changed")
            text = (ROOT/name).read_text()
            if name.endswith("cache_validation.c"):
                require(canonical(c_tokens(text)) == projected["all_c_tokens_sha256"],
                        "cache publication token projection differs")
            else:
                start = text.index("    for (size_t round = 0; round < rounds; ++round) {")
                end = text.index("    CHECK(block_bench_counter(&last));")
                require(canonical(c_tokens(text[start:end])) == projected["timed_getter_loop_tokens_sha256"],
                        "Block timed-loop token projection differs")
    return result


def collect(kind, executables, rounds, getter_rounds, artifacts):
    require(kind in ("block", "cache") and 1 <= rounds <= 12 and getter_rounds > 0,
            "bounded known collection required")
    require(not artifacts.exists(), "new artifact directory required; preserve all prior attempts")
    artifacts.mkdir(parents=True)
    hashes = {name: digest(path.read_bytes()) for name, path in executables.items()}
    report = {"status": "FAIL", "kind": kind, "rounds": rounds, "processes": [],
              "declared_order": [list(VARIANTS if r % 2 == 0 else reversed(VARIANTS)) for r in range(rounds)],
              "scope": "Fresh finite CPU comparison only; no FPS/scene or historical-binary requalification."}
    if kind == "block":
        report["block_controls"] = PUBLIC_BLOCK_CONTROLS
    try:
        for round_, variants in enumerate(report["declared_order"], 1):
            for position, variant in enumerate(variants, 1):
                require(hashes == {name: digest(path.read_bytes()) for name, path in executables.items()},
                        "executable changed before process")
                prefix = f"round-{round_}-{position}-{variant}"
                command = [str(executables[variant]), str(getter_rounds) if kind == "block" else "measure"]
                started = time.monotonic()
                record = {"round": round_, "position": position, "variant": variant,
                          "exe_sha256": hashes[variant], "timeout": False}
                try:
                    child = subprocess.run(command, capture_output=True, timeout=300)
                    stdout, stderr, exit_ = child.stdout, child.stderr, child.returncode
                except subprocess.TimeoutExpired as error:
                    stdout, stderr, exit_ = error.stdout or b"", error.stderr or b"", None
                    record["timeout"] = True
                except OSError as error:
                    stdout, stderr, exit_ = b"", b"", None
                    record["launch_error"] = str(error)
                for stream, data in (("stdout", stdout), ("stderr", stderr)):
                    (artifacts/(prefix + "." + stream)).write_bytes(data)
                    try:
                        record[stream] = data.decode("ascii")
                    except UnicodeDecodeError:
                        record[stream] = data.decode("ascii", errors="backslashreplace")
                        record["non_ascii_stream_projection"] = True
                    record[stream + "_sha256"] = digest(data)
                record.update(exit=exit_, seconds=time.monotonic()-started)
                report["processes"].append(record)
                (artifacts/"report.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
                require(exit_ == 0 and not record["timeout"], "native failure retained")
                require(not record.get("non_ascii_stream_projection"), "non-ASCII native output retained without credit")
                require(record["stderr"] == "", "native stderr retained without credit")
                if kind == "block":
                    parse_block(record["stdout"], PUBLIC_BLOCK_CONTROLS)
                else:
                    parse_cache(record["stdout"])
                require(hashes == {name: digest(path.read_bytes()) for name, path in executables.items()},
                        "executable changed after process")
        # Admission is checked from every retained process before PASS is set.
        candidate_report = dict(report)
        candidate_report.pop("status")
        report["summary"] = audit_cohort(candidate_report)
        report["status"] = "PASS"
        return report
    finally:
        (artifacts/"report.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, default=RECEIPT)
    parser.add_argument("--kind", choices=("block", "cache"))
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--candidate", type=Path)
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--rounds", type=int, default=4)
    parser.add_argument("--getter-rounds", type=int, default=10000)
    args = parser.parse_args()
    if any((args.baseline, args.candidate, args.artifacts, args.kind)):
        require(all((args.baseline, args.candidate, args.artifacts, args.kind)), "complete collection arguments required")
        result = collect(args.kind, {"baseline": args.baseline.resolve(), "candidate": args.candidate.resolve()},
                         args.rounds, args.getter_rounds, args.artifacts.resolve())
        print(json.dumps(result["summary"], indent=2))
    else:
        print(json.dumps(audit_receipt(json.loads(args.receipt.read_text(encoding="utf-8"))), indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, ValueError, OSError, UnicodeError) as error:
        print("static CPU benchmark: " + str(error), file=sys.stderr)
        raise SystemExit(1)
