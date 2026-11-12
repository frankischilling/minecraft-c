"""Require a completed native test and compare its finite original state rows."""
import argparse
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="singleplayer-tick-", dir=args.work_dir))
    executable = args.executable.resolve(strict=True)
    expected = json.loads((HERE / "finite.json").read_text())
    inputs = [executable, HERE / "finite.json", HERE / "run_native.py",
              HERE / "capture_reference.py", HERE.parent / "controller/capture.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable)], out, "native", 45)
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"[1-9][0-9]* singleplayer tick/reset checks passed",
                            text.splitlines()[-1]), "native completion missing")
        # Import the sibling observer by path to avoid the old controller module's same name.
        import importlib.util
        spec = importlib.util.spec_from_file_location("tick_capture_reference", HERE / "capture_reference.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        actual = module.rows(text)
        ensure(actual == expected["rows"], "original/native tick/reset state rows differ")
        report.update(status="PASS", rows=len(actual), native=receipt)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "native/fixture/helper inputs changed")
    print("Singleplayer tick/reset: completed native checks and 60 original rows; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
