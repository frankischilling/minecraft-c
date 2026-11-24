"""Require native completion and compare every captured original World color row."""
import argparse
import json
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
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="world-colors-", dir=args.work_dir))
    executable = args.executable.resolve(strict=True)
    inputs = [executable, HERE / "finite.json", HERE / "run_native.py",
              HERE / "capture_reference.py", HERE.parents[1] / "client/controller/capture.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        expected = json.loads((HERE / "finite.json").read_text())["rows"]
        receipt = capture([str(executable)], out, "native", 60)
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"[1-9][0-9]* World color checks passed", text.splitlines()[-1]),
               "native completion missing")
        observed = rows(text)
        if observed != expected:
            first = next(index for index, pair in enumerate(zip(observed, expected))
                         if pair[0] != pair[1])
            ensure(False, "original/native World color rows differ at row " + str(first) +
                   ": expected " + expected[first] + "; observed " + observed[first])
        report.update(status="PASS", rows=432, native=receipt)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "native inputs changed")
    print("World colors: native controls and 432 original rows; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
