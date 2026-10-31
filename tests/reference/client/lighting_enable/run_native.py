"""Compare whole native U.b state with the finite original device observations."""

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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    cases = json.loads((HERE / "inputs.json").read_text(encoding="utf-8"))["cases"]
    expected = json.loads((HERE / "finite.json").read_text(encoding="utf-8"))["rows"]
    ensure(len(cases) == 5 and len(expected) == 86, "five original profiles and 86 rows")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="lighting-enable-", dir=args.work_dir)).resolve(strict=True)
    inputs = [executable, HERE / "finite.json", HERE / "inputs.json", HERE / "source-pins.json",
              HERE / "run_native.py", HERE / "capture_reference.py",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "scope": "86 exact original state/parameter rows: one no-context failure profile "
                       "and five whole U.b calls on actual native contexts. No lit pixels or geometry."}
    try:
        observed = []
        offset = 0
        for index, case in enumerate(cases):
            count = 17 * case["calls"] if case["device"] else 1
            original = expected[offset:offset + count]
            ensure(rows("\n".join(original), case) == original, "complete original profile inventory")
            offset += count
            receipt = capture([str(executable), str(index)], output, "native" + str(index),
                              30, os.environ.copy(), output)
            profile = {"name": case["name"], "capture": receipt}
            report["profiles"].append(profile)
            require_success(receipt, empty_stderr=True)
            text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
            lines = text.splitlines()
            marker = r"Source U\.b lighting: " + re.escape(case["name"]) + r"; [1-9][0-9]* checks"
            ensure(lines and re.fullmatch(marker, lines[-1]), "completed native profile and checked cleanup")
            ensure(sum(re.fullmatch(marker, line) is not None for line in lines) == 1,
                   "one native profile completion")
            actual = rows(text, case)
            profile["mismatches"] = [{"index": row_index, "native": native, "original": original_row}
                                     for row_index, (native, original_row) in enumerate(zip(actual, original))
                                     if native != original_row]
            ensure(actual == original, "native/original U.b state or parameters differ")
            observed.extend(actual)
            profile.update(status="PASS", rows=len(actual))
        ensure(offset == 86 and observed == expected, "all 86 original rows exactly")
        report.update(status="PASS", rows=86, profiles_completed=5, whole_calls=5)
    except Exception as error:
        report["failure"] = str(error)
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable, original fixture and replay inputs unchanged")
    print("Source U.b lighting: 86 exact original state/parameter rows, five profiles; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
