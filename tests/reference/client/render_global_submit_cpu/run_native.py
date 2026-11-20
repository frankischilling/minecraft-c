"""Compare public native N submission's pre-U failures with original prefixes."""
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
sys.path.insert(0, str(HERE))
from capture_reference import rows
from fixtures import encode
from capture import capture, digest, ensure, require_success


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="submit-cpu-", dir=args.work_dir.resolve()))
    picking_path = HERE.parents[2] / "tools/client/test_picking.py"
    spec = importlib.util.spec_from_file_location("submit_cpu_resources", picking_path)
    picking = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(picking)
    for name, data in picking.RESOURCES.items():
        p = out / "resources" / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    cases = out / "cases.txt"
    cases.write_text(encode(), encoding="ascii", newline="\n")
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "original submission CPU inventory")
    inputs = [executable, picking_path, cases, HERE / "run_native.py", HERE / "fixtures.py",
              HERE / "capture_reference.py", HERE / "finite.json", HERE / "source-pins.json",
              HERE / "SubmitObservation.java", HERE / "SubmitController.java",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    resources = inventory(out / "resources")
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable), str(out / "resources"), str(cases)], out, "native", 60,
                          os.environ.copy(), out.resolve())
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        actual = rows(text)
        report["mismatches"] = [{"index": i, "native": a, "original": b}
                                for i, (a, b) in enumerate(zip(actual, expected)) if a != b]
        ensure(actual == expected, "native/original submission prefixes differ; see run.json")
        ensure(re.fullmatch(r"Source submission CPU: [1-9][0-9]* checks; 12 pre-U prefixes",
                            text.splitlines()[-1]), "native normal completion missing")
        report.update(status="PASS", rows=12,
                      scope="public N submission failures before U302: original R/list/counter/H "
                            "effects, completed R-store counts and exception frames; native "
                            "retained views, immutable failures, guards and owner retirement; "
                            "no World, Display, query, terrain draw or whole-frame claim")
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"] and resources == inventory(out / "resources")
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8",
                                      newline="\n")
        ensure(same, "native submission CPU inputs changed")
    print("Source submission CPU: 12 original prefixes; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
