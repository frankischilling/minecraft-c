"""Replay all seven genuine-device N profiles, including the occlusion branch."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import rows
from capture import capture, digest, ensure, require_success


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="terrain-submit-", dir=args.work_dir.resolve()))
    cases = json.loads((HERE / "inputs.json").read_text())["cases"]
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(len(cases) == 7 and len(expected) == 35, "complete original device cohort")
    inputs = [executable, HERE / "finite.json", HERE / "inputs.json", HERE / "source-pins.json",
              HERE / "SubmitObservation.java", HERE / "capture_reference.py", HERE / "run_native.py",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": []}
    try:
        observed = []
        for i, case in enumerate(cases):
            original = expected[5 * i:5 * (i + 1)]
            ensure(rows("\n".join(original), case["name"]) == original, "original row inventory")
            receipt = capture([str(executable), str(i)], output, "native" + str(i), 30, os.environ.copy(), output.resolve())
            profile = {"name": case["name"], "capture": receipt}
            report["profiles"].append(profile)
            require_success(receipt, empty_stderr=True)
            text = (output / receipt["stdout_file"]).read_text()
            ensure(re.fullmatch(r"Source N submit: " + re.escape(case["name"]) + r"; [1-9][0-9]* checks", text.splitlines()[-1]),
                   "normal native completion")
            actual = rows(text, case["name"])
            profile["actual_rows"] = actual
            profile["format"] = [line for line in text.splitlines() if line.startswith("SUBMIT_FORMAT ")]
            capability = [line for line in text.splitlines() if line.startswith("SUBMIT_CAPABILITY ")]
            profile["capability"] = capability
            ensure(capability == (["SUBMIT_CAPABILITY " + case["name"] + " 1"] if case.get("actual_capability") else []),
                   "actual supported-context occlusion capability differs; no forced branch or visibility substitute")
            profile["mismatches"] = [{"index": j, "native": a, "original": b}
                                      for j, (a, b) in enumerate(zip(actual, original)) if a != b]
            ensure(actual == original, "native/original complete device rows differ")
            observed += actual
        ensure(observed == expected, "all35 original rows including exact pixels")
        report.update(status="PASS", rows=35, profiles_completed=7)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable and fixture inputs unchanged")
    print("Source N submit: 35 exact original rows, seven device profiles; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
