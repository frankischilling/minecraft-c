"""Compare all14 closed original polling prefixes with native entered child."""
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
    output = Path(tempfile.mkdtemp(prefix="occlusion-poll-", dir=args.work_dir.resolve()))
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "complete original inventory")
    inputs = [executable, HERE / "finite.json", HERE / "inputs.json", HERE / "source-pins.json",
              HERE / "PollObservation.java", HERE / "capture_reference.py", HERE / "run_native.py",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable)], output, "native", 40, os.environ.copy(), output.resolve())
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"[1-9][0-9]* N closed poll checks passed", text.splitlines()[-1]), "normal native completion")
        actual = rows(text)
        report["actual_rows"] = actual
        report["mismatches"] = [{"index": i, "native": a, "original": b}
                                for i, (a, b) in enumerate(zip(actual, expected)) if a != b]
        ensure(actual == expected, "native/original poll finite rows differ")
        report.update(status="PASS", rows=14)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable and fixture inputs unchanged")
    print("Source query poll: 14 exact original rows; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
