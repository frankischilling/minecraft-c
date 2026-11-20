"""Compare canonical camera fields exactly and native GL matrices under a declared policy."""
import argparse
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
sys.path.insert(0, str(HERE))
from capture_reference import rows

ABSOLUTE_COEFFICIENT_LIMIT = 2e-6


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    ensure(spec is not None and spec.loader is not None, "authored fixture module")
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def number(bits):
    return struct.unpack(">f", bytes.fromhex(bits))[0]


def fields(text):
    found = [line.split()[:7] for line in text.splitlines() if line.startswith("CAMERA_FIELDS ")]
    ensure(len(found) == 36, "camera field row count differs")
    for row, identity in zip(found, [(m, s) for m in range(12) for s in range(3)]):
        ensure(len(row) == 7 and tuple(map(int, row[1:3])) == identity,
               "camera field row identity differs")
        ensure(re.fullmatch(r"[0-9a-f]{8}", row[3]) and
               all(re.fullmatch(r"[0-9a-f]{16}", v) for v in row[4:]),
               "camera field encoding differs")
    return [" ".join(row) for row in found]


def compare(actual, expected):
    cases = []
    for got, wanted in zip(actual, expected):
        a, b = got.split(), wanted.split()
        ensure(a[:5] == b[:5], "actual camera k/D/identity differs: " + got)
        coefficients = []
        for index, (v, w) in enumerate(zip(a[5:], b[5:])):
            x, y = number(v), number(w)
            ensure(math.isfinite(x) and math.isfinite(y), "finite matrix profile required")
            coefficients.append({"matrix": "projection" if index < 16 else "modelview",
                                 "index": index % 16, "native_bits": v,
                                 "original_bits": w, "absolute_difference": abs(x - y)})
        worst = max(coefficients, key=lambda c: c["absolute_difference"])
        cases.append({"mode": int(a[1]), "step": int(a[2]),
                      "exact_coefficients": sum(c["native_bits"] == c["original_bits"] for c in coefficients),
                      "worst": worst, "coefficients": coefficients})
    return cases


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="renderer-camera-", dir=args.work_dir.resolve()))
    picker_path = HERE.parents[2] / "tools/client/test_picking.py"
    picker = module("renderer_camera_authored_world", picker_path)
    for name, data in picker.RESOURCES.items():
        path = out / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    picker.authored_world(out / "world")
    resource_before = {p.relative_to(out / "resources").as_posix(): digest(p)
                       for p in (out / "resources").rglob("*") if p.is_file()}
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "original matrix fixture invalid")
    expected_fields = fields("\n".join(json.loads((HERE / "fields.json").read_text())["rows"]))
    inputs = [executable, picker_path, HERE / "run_native.py", HERE / "capture_reference.py",
              HERE / "RendererCameraObservation.java", HERE / "finite.json", HERE / "fields.json",
              HERE / "profile.json", HERE / "source-pins.json", SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "resource_before_sha256": resource_before,
              "matrix_policy": {"absolute_coefficient_limit": ABSOLUTE_COEFFICIENT_LIMIT,
                                "bit_exact_driver_claim": False},
              "original_profile": json.loads((HERE / "profile.json").read_text())}
    try:
        result = capture([str(executable), str(out / "resources"), str(out / "world")],
                         out, "native", 90, os.environ.copy(), out.resolve())
        report["native"] = result
        require_success(result, empty_stderr=True)
        text = (out / result["stdout_file"]).read_text()
        ensure(re.fullmatch(r"Source camera: [1-9][0-9]* checks; 36 camera matrices and real collision rays",
                            text.splitlines()[-1]), "native camera completion missing")
        actual = rows(text)
        report["matrix_cases"] = compare(actual, expected)
        report["maximum_absolute_difference"] = max(c["worst"]["absolute_difference"] for c in report["matrix_cases"])
        ensure(fields(text) == expected_fields, "native/original camera FOV/E/F/G bits differ")
        ensure(report["maximum_absolute_difference"] <= ABSOLUTE_COEFFICIENT_LIMIT,
               "native/original matrix coefficient exceeded declared 2e-6 policy")
        report.update(status="PASS", camera_rows=36, exact_field_rows=36)
    finally:
        report["resource_after_sha256"] = {p.relative_to(out / "resources").as_posix(): digest(p)
                                          for p in (out / "resources").rglob("*") if p.is_file()}
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"] and resource_before == report["resource_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native camera inputs/resources changed")
    print("Source camera: 36 exact field rows and finite GL matrix policy passed; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
