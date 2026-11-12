"""Compare native lighting class fields with the unchanged original finite profile."""
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
    out = Path(tempfile.mkdtemp(prefix="lighting-", dir=args.work_dir.resolve()))
    spec = importlib.util.spec_from_file_location("lighting_rows", HERE / "capture_reference.py")
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    reference.rows("\n".join(expected))
    inputs = [executable, HERE / "run_native.py", HERE / "capture_reference.py", HERE / "finite.json",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        native = capture([str(executable)],
                         out, "native", 90, os.environ.copy(), out.resolve())
        require_success(native, empty_stderr=True)
        text = (out / native["stdout_file"]).read_text()
        observed = reference.rows(text)
        ensure(observed == expected, "native/original lighting state rows differ")
        ensure(re.fullmatch(r"Source resources CPU: [1-9][0-9]* checks",
                            text.splitlines()[-1]), "native lighting completion missing")
        report.update(status="PASS", rows=3, native=native,
                      scope="actual U cold/eager buffer fields and class-failure persistence; native quota failures remain distinct from original OutOfMemoryError; no U GL or full frame claim")
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "lighting native inputs changed")
    print("Lighting: 3 original rows passed; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
