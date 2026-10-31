"""Replay exact legacy material-tracking observations on the canonical Display."""

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
from capture_material import rows
from capture import capture, digest, ensure, require_success
from reference_profiles import load_references, select_reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    references, reference_paths, reference_hashes = load_references("material", None, None, rows)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="lighting-material-", dir=args.work_dir)).resolve(strict=True)
    inputs = [executable, *reference_paths, HERE / "run_material.py", HERE / "capture_material.py",
              HERE.parent / "controller/capture.py", HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    ensure(all(before.get(path) == value for path, value in reference_hashes.items()),
           "parsed original reference bytes match the initial input snapshot")
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable), "--material"], output, "native", 30, os.environ.copy(), output)
        report["capture"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
        reference, context = select_reference(references, text)
        expected = reference["fixture"]["rows"]
        report.update(context, reference_profile=reference["profile"]["id"])
        ensure(re.fullmatch(r"Source lighting material: 16 rows; [1-9][0-9]* checks", text.splitlines()[-1]),
               "completed actual native controls and checked cleanup")
        actual = rows(text)
        report["mismatches"] = [{"index": index, "native": native, "original": legacy}
                                for index, (native, legacy) in enumerate(zip(actual, expected)) if native != legacy]
        ensure(actual == expected, "native/legacy material tracking differs")
        report.update(status="PASS", rows=16)
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable and original material inputs unchanged")
    print("Source material tracking: 16 exact original state rows; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
