#!/usr/bin/env python3
"""Compare finite original plant bodies with native GAME World projections.

The original dependency fixture supplies prepared World services. Comparison
includes all block/metadata cells in the declared native extent, both Random
continuations and selected deterministic drop fields. Original World service
traces remain retained but are not equated with native World implementation.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[4]
HELPER = ROOT / "tests/reference/world/PlantLifecycleObservation.java"
REQUESTS = ROOT / "tests/reference/world/plants/requests.tsv"
HASHES = {"client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
          "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
FIELDS = ("before", "after", "supplied_random_tail", "world_random_tail", "drops")
EXTENT = [0, 31, 0, 127, 0, 31]
ORIGINAL_EXTENT = [-16, 31, 0, 127, -16, 31]
INT_MIN, INT_MAX = -(1 << 31), (1 << 31) - 1


def pin(path):
    raw = path.read_bytes()
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key: " + key)
        result[key] = value
    return result


def read(path):
    def invalid_constant(value):
        raise ValueError("non-finite JSON number: " + value)

    return json.loads(path.read_text(encoding="ascii"), object_pairs_hook=unique_object,
                      parse_constant=invalid_constant)


def exact_keys(value, keys, label):
    if type(value) is not dict or value.keys() != set(keys):
        raise ValueError("invalid " + label + " keys")


def integer(value, minimum, maximum, label):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError("invalid " + label + " integer")


def fixed_hex(value, width, label):
    if type(value) is not str or re.fullmatch("[0-9a-f]{" + str(width) + "}", value) is None:
        raise ValueError("invalid " + label + " hex")


def integer_vector(value, expected, label):
    if type(value) is not list or any(type(item) is not int for item in value) or value != expected:
        raise ValueError("invalid " + label)


def random_tail(value):
    exact_keys(value, ("next_long_hex", "next_int31", "next_double_bits", "next_gaussian_bits"),
               "Random continuation")
    integer(value["next_int31"], 0, 30, "Random nextInt(31)")
    for field in ("next_long_hex", "next_double_bits", "next_gaussian_bits"):
        fixed_hex(value[field], 16, "Random " + field)


def drop_fields(value):
    if type(value) is not list or len(value) > 4096:
        raise ValueError("invalid drop list")
    for drop in value:
        exact_keys(drop, ("stack", "pickup_delay", "position_bits"), "drop")
        stack = drop["stack"]
        if type(stack) is not list or len(stack) != 4:
            raise ValueError("invalid drop stack")
        # Both writers expose the original four signed Java int fields and
        # signed pickup delay, without a count, ID or damage narrowing.
        for field in stack:
            integer(field, INT_MIN, INT_MAX, "drop stack")
        integer(drop["pickup_delay"], INT_MIN, INT_MAX, "drop pickup delay")
        position = drop["position_bits"]
        if type(position) is not list or len(position) != 3:
            raise ValueError("invalid drop position")
        for bits in position:
            fixed_hex(bits, 16, "drop position")


def header(value, original):
    if type(value) is not dict:
        raise ValueError("invalid observation object")
    keys = {"schema", "side", "extent", "sparse_default", "cases"}
    projected = original and "projection_only" in value
    if original:
        keys.update(("jar_sha256", "requests_sha256", "scope", "not_measured",
                     "prepared_loaded_chunks", "projection_only" if projected else "fixture_source_sha256"))
    exact_keys(value, keys, "observation")
    schema = "plant-dependency-fixture-v1" if original else "native-plant-lifecycle-v1"
    if (type(value["schema"]) is not str or value["schema"] != schema or
            type(value["side"]) is not str or value["side"] not in HASHES):
        raise ValueError("unexpected observation schema or side")
    integer_vector(value["extent"], ORIGINAL_EXTENT if original and not projected else EXTENT,
                   "observation extent")
    integer_vector(value["sparse_default"], [0, 0], "sparse default")
    if type(value["cases"]) is not list or not 1 <= len(value["cases"]) <= 64:
        raise ValueError("invalid observation case inventory")
    if original:
        if value["jar_sha256"] != HASHES[value["side"]]:
            raise ValueError("original archive identity")
        fixed_hex(value["requests_sha256"], 64, "request digest")
        if any(type(value[field]) is not str for field in ("scope", "not_measured")):
            raise ValueError("invalid original scope")
        integer_vector(value["prepared_loaded_chunks"], [0, 1, 0, 1], "prepared Chunk extent")
        if projected:
            if value["projection_only"] is not True:
                raise ValueError("invalid original projection marker")
        else:
            fixed_hex(value["fixture_source_sha256"], 64, "fixture source digest")
    return projected


def case_fields(row, original, projected):
    keys = set(FIELDS) | {"outcome", "request" if original else "name"}
    if original and not projected:
        keys.update(("receiver_class", "method_class", "method", "descriptor", "trace", "setters"))
    if not original and type(row) is dict and "failure" in row:
        keys.add("failure")
    exact_keys(row, keys, "case")
    outcome = row["outcome"]
    if type(outcome) is not dict:
        raise ValueError("invalid outcome")
    status = outcome.get("status")
    if status == "returned":
        exact_keys(outcome, ("status",), "returned outcome")
        if "failure" in row:
            raise ValueError("returned case has failure diagnostic")
    elif original and status == "threw":
        exact_keys(outcome, ("status", "class", "message"), "thrown outcome")
        if (type(outcome["class"]) is not str or
                (outcome["message"] is not None and type(outcome["message"]) is not str)):
            raise ValueError("invalid thrown outcome")
    elif not original and status == "native_failed":
        exact_keys(outcome, ("status",), "failed native outcome")
        if type(row.get("failure")) is not dict:
            raise ValueError("missing native failure diagnostic")
    else:
        raise ValueError("unexpected outcome status")
    if original:
        if type(row["request"]) is not dict or type(row["request"].get("name")) is not str:
            raise ValueError("invalid original request name")
        return row["request"]["name"]
    if type(row["name"]) is not str:
        raise ValueError("invalid native case name")
    return row["name"]


def projection(row, original, extent=None):
    result = {field: row[field] for field in FIELDS}
    extent = extent if extent is not None else ORIGINAL_EXTENT if original else EXTENT
    for field in ("before", "after"):
        cells = result[field]
        if type(cells) is not list:
            raise ValueError("invalid sparse cell list")
        previous = None
        for cell in cells:
            if (type(cell) is not list or len(cell) != 5 or any(type(value) is not int for value in cell) or
                    not (extent[0] <= cell[0] <= extent[1] and extent[2] <= cell[1] <= extent[3] and
                         extent[4] <= cell[2] <= extent[5]) or
                    not (0 <= cell[3] <= 255 and 0 <= cell[4] <= 15) or
                    cell[3:] == [0, 0]):
                raise ValueError("invalid sparse cell projection")
            coordinate = (cell[0], cell[2], cell[1])
            if previous is not None and coordinate <= previous:
                raise ValueError("unordered or duplicate sparse cell")
            previous = coordinate
        if original:
            cells = [cell for cell in cells if 0 <= cell[0] <= 31 and 0 <= cell[2] <= 31]
        result[field] = cells
    random_tail(result["supplied_random_tail"])
    random_tail(result["world_random_tail"])
    drop_fields(result["drops"])
    return result


def compare(original, native, names):
    projected = header(original, True)
    header(native, False)
    if original["side"] != native["side"]:
        raise ValueError("side mismatch")
    if ([case_fields(row, True, projected) for row in original["cases"]] != names or
            [case_fields(row, False, False) for row in native["cases"]] != names):
        raise ValueError("complete request order mismatch")
    mismatches = []
    for expected, actual in zip(original["cases"], native["cases"], strict=True):
        name = actual["name"]
        if expected["outcome"]["status"] != "returned" or actual["outcome"]["status"] != "returned":
            mismatches.append({"case": name, "field": "outcome", "expected": expected["outcome"],
                               "actual": actual["outcome"], "failure": actual.get("failure")})
        left, right = projection(expected, True, original["extent"]), projection(actual, False)
        for field in FIELDS:
            if left[field] != right[field]:
                mismatches.append({"case": name, "field": field, "expected": left[field],
                                   "actual": right[field]})
    return mismatches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--client", required=True, type=Path)
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="capture-", dir=args.work_dir)).resolve()
    report = {"schema": 1, "scope": __doc__, "tested_commit": args.tested_commit,
              "passed": False, "processes": [], "sides": {}}

    def run(name, argv):
        started = time.monotonic()
        record = {"argv": [str(value) for value in argv], "cwd": str(ROOT)}
        report["processes"].append(record)
        try:
            child = subprocess.run(record["argv"], cwd=ROOT, capture_output=True, timeout=180)
            stdout, stderr = child.stdout, child.stderr
            record.update(exit_code=child.returncode, timed_out=False)
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b"", error.stderr or b""
            record.update(exit_code=None, timed_out=True)
        (work / (name + ".stdout")).write_bytes(stdout)
        (work / (name + ".stderr")).write_bytes(stderr)
        record.update(seconds=time.monotonic() - started,
                      stdout=pin(work / (name + ".stdout")), stderr=pin(work / (name + ".stderr")))
        if record["exit_code"] != 0:
            raise RuntimeError(name + " failed: " + stderr.decode("utf-8", errors="replace"))

    try:
        jars = {"client": args.client.resolve(strict=True), "server": args.server.resolve(strict=True)}
        paths = {"native_executable": args.executable.resolve(strict=True), "java_observer": HELPER,
                 "driver": Path(__file__).resolve(), "requests": REQUESTS,
                 "build_definition": ROOT / "CMakeLists.txt", **jars}
        paths.update({str(path.relative_to(ROOT)): path
                      for base in (ROOT / "src", ROOT / "include", ROOT / "tests")
                      for path in sorted(base.rglob("*"))
                      if path.suffix in (".c", ".h")})
        report["inputs"] = {name: pin(path) for name, path in paths.items()}
        for side in jars:
            if report["inputs"][side]["sha256"] != HASHES[side]:
                raise ValueError("original JAR hash mismatch: " + side)
        names = [line.split("\t")[0] for line in REQUESTS.read_text(encoding="utf-8").splitlines()
                 if line and not line.startswith("#")]
        if not names or len(names) > 64 or len(set(names)) != len(names):
            raise ValueError("bounded unique request inventory")
        classes = work / "classes"
        classes.mkdir()
        run("java-version", [args.java, "-version"])
        run("javac-version", [args.javac, "-version"])
        run("compile", [args.javac, "-d", classes, HELPER])
        for side, jar in jars.items():
            original_path, native_path = work / (side + "-original.json"), work / (side + "-native.json")
            run(side + "-original", [args.java, "-Xint", "-cp", classes, "PlantLifecycleObservation",
                                      side, jar, work / (side + "-generated"), REQUESTS, original_path])
            run(side + "-native", [args.executable.resolve(), side, REQUESTS,
                                    str(work / (side + "-worlds")).encode("utf-8").hex(), native_path])
            original, native = read(original_path), read(native_path)
            mismatches = compare(original, native, names)
            report["sides"][side] = {"cases": len(names), "mismatches": mismatches,
                                      "original": pin(original_path), "native": pin(native_path)}
        report["inputs_unchanged"] = all(pin(path) == report["inputs"][name] for name, path in paths.items())
        if not report["inputs_unchanged"]:
            raise ValueError("inputs changed during comparison")
        report["passed"] = all(not side["mismatches"] for side in report["sides"].values())
        for side, result in report["sides"].items():
            print(f"{side}: {result['cases']} cases, {len(result['mismatches'])} mismatching fields")
            for mismatch in result["mismatches"]:
                print("  " + mismatch["case"] + ": " + mismatch["field"])
        return 0 if report["passed"] else 1
    except Exception as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print("Capture: " + str(work), file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
