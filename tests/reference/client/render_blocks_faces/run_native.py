"""Compare real native CV face/NW state with the unchanged original profile."""
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
    out = Path(tempfile.mkdtemp(prefix="cv-faces-", dir=args.work_dir.resolve()))
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "original CV face finite inventory")
    inputs = [executable, HERE / "run_native.py", HERE / "capture_reference.py",
              HERE / "finite.json", HERE / "source-pins.json", HERE / "FaceObservation.java",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable)], out, "native", 90, os.environ.copy(), out.resolve())
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"CV faces: [1-9][0-9]* checks; 1656 rows and 24 float colors",
                            text.splitlines()[-1]), "native CV face completion missing")
        observed = rows(text)
        report["mismatches"] = [{"index": i, "native": actual, "original": original}
                                for i, (actual, original) in enumerate(zip(observed, expected))
                                if actual != original]
        ensure(observed == expected,
               "native/original CV face/color state differs; see run.json mismatches")
        report.update(status="PASS", rows=1680,
                      scope="six actual CV face bodies and NW float RGB leaf; same canonical "
                            "Block bounds/CV fields/NW backing, including source failure prefixes; "
                            "no DK, World, draw, GL or full terrain frame claim")
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "CV face native inputs changed")
    print("CV faces: 1680 exact original rows passed; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
