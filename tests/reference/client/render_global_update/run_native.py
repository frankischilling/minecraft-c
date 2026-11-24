"""Compare actual native selector calls with 42 unchanged-JAR JDI outcomes."""
import argparse
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import updates
from capture import capture, digest, ensure, require_success


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="render-global-update-", dir=args.work_dir.resolve()))
    executable = args.executable.resolve(strict=True)
    native_source = HERE.parents[2] / "client/world/test_render_global_update.c"
    inputs = [executable, native_source, HERE / "run_native.py", HERE / "capture_reference.py",
              HERE / "inputs.json", HERE / "finite.json", HERE / "source-pins.json",
              HERE / "UpdateObservation.java", HERE / "UpdateController.java",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "42 actual N selector outcomes; genuine clean DK return7 or dirty "
                       "null-World EW112 failure, canonical owners and static fields; "
                       "no successful dirty terrain compilation or geometry claim"}
    try:
        ensure(digest(HERE / "inputs.json") in native_source.read_text(),
               "native prepared input cohort binding missing")
        fixture = json.loads((HERE / "finite.json").read_text())
        expected = updates("\n".join(fixture["rows"]))
        receipt = capture([str(executable)], out, "native", 60)
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"[1-9][0-9]* N update checks passed", text.splitlines()[-1]),
               "native selector controls did not complete")
        observed = updates(text)
        report["actual_rows"] = observed
        if observed != expected:
            row = next(i for i, pair in enumerate(zip(observed, expected)) if pair[0] != pair[1])
            actual_cells, original_cells = observed[row].split(), expected[row].split()
            cell = next(i for i, pair in enumerate(zip(actual_cells, original_cells))
                        if pair[0] != pair[1])
            names = ["tag", "index", "name", "result", "exception", "queue", "u", "p", "E",
                     "A", "B count", "dk.b", "lm.a", "N PC", "compile attempts", "returns"]
            ensure(False, f"selector row{row} {names[cell]} differs: original "
                   f"{original_cells[cell]}; native {actual_cells[cell]}")
        report.update(status="PASS", original_rows=42)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "selector inputs changed")
    print("N update: 42 exact original rows and native ownership controls; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
