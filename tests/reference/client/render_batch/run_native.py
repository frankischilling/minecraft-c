"""Compare whole entered TG/DK bodies with unchanged-original field/state rows."""
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
    parser.add_argument("--device", action="store_true")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="render-batch-", dir=args.work_dir.resolve()))
    original = json.loads((HERE / "finite.json").read_text())
    ensure(rows("\n".join([*original["cpu"], *original["device"]])) == original,
           "original batch fixture inventory")
    expected = {"cpu": original["cpu"], "device": original["device"] if args.device else []}
    inputs = [executable, HERE / "run_native.py", HERE / "capture_reference.py",
              HERE / "finite.json", HERE / "source-pins.json", HERE / "BatchObservation.java",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "device": args.device}
    try:
        command = [str(executable), *(["--device"] if args.device else [])]
        receipt = capture(command, out, "native", 100, os.environ.copy(), out.resolve())
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        count = 17 if args.device else 0
        ensure(re.fullmatch(rf"Source batches: [1-9][0-9]* checks; CPU=78; device={count}",
                            text.splitlines()[-1]), "normal native batch completion")
        observed = rows(text, count)
        report["actual_rows"] = observed
        for kind in ("cpu", "device"):
            for index, (actual, wanted) in enumerate(zip(observed[kind], expected[kind])):
                ensure(actual == wanted,
                       f"batch {kind} row{index} differs: native {actual}; original {wanted}")
        report.update(status="PASS", cpu_rows=78, device_rows=count,
                      scope="whole TG/DK leaves; exact source fields/cursors/list-state rows; "
                            "native strict-FP and failed Push/Translate/replay-prefix controls; "
                            "no terrain/whole-frame claim")
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native batch inputs changed")
    print(f"Source batches: 78 CPU and{count} device original rows; {out / 'run.json'}")


if __name__ == "__main__":
    main()
