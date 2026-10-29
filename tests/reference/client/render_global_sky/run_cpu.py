"""Require native completion and compare the four original early sky outcomes."""
import argparse
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_prefix import rows
from capture import capture, digest, ensure, require_success


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="sky-prefix-", dir=args.work_dir))
    executable = args.executable.resolve(strict=True)
    inputs = [executable, HERE / "prefix.json", HERE / "run_cpu.py",
              HERE / "capture_prefix.py", HERE.parent / "controller/capture.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        expected = json.loads((HERE / "prefix.json").read_text())["rows"]
        receipt = capture([str(executable)], out, "native", 45)
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"Source sky CPU: [1-9][0-9]* checks", text.splitlines()[-1]),
               "native completion missing")
        ensure(rows(text) == expected, "original/native early sky outcomes differ")
        report.update(status="PASS", rows=4, native=receipt)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "native inputs changed")
    print("Sky CPU: native ownership controls and four exact original outcomes; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
