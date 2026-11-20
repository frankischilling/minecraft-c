"""Run native rain body controls and compare every finite original observation."""
import argparse
import importlib.util
import json
import os
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
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="rain-", dir=args.work_dir))
    spec = importlib.util.spec_from_file_location("rain_capture_rows", HERE / "capture_reference.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(module.rows("\n".join(expected)) == expected, "fixture row shape differs")
    inputs = [executable, HERE / "finite.json", HERE / "run_native.py",
              HERE / "capture_reference.py", HERE.parent / "controller_tick/capture_reference.py",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(p): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable)], out, "native", 45, os.environ.copy(), out.resolve())
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"[1-9][0-9]* rain body checks passed", text.splitlines()[-1]),
               "native completion missing")
        ensure(module.rows(text) == expected, "original/native rain observations differ")
        report.update(status="PASS", rows=20, native=receipt)
    finally:
        report["input_after_sha256"] = {str(p): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "native/fixture inputs changed")
    print("Renderer rain: native controls and 20 original rows passed; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
