"""Replay declared recenter/JO fields; optionally run real device integration."""
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
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*")
            if p.is_file() and p.name != "session.lock"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--device", action="store_true")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="recenter-jo-", dir=args.work_dir.resolve()))
    picking_path = HERE.parents[2] / "tools/client/test_picking.py"
    spec = importlib.util.spec_from_file_location("recenter_authored_inputs", picking_path)
    picking = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(picking)
    for name, data in picking.RESOURCES.items():
        p = out / "resources" / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    if args.device:
        picking.authored_world(out / "world")
    cases = out / "cases.txt"
    cases.write_text(encode(), encoding="ascii", newline="\n")
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "original recenter/JO row inventory")
    inputs = [executable, picking_path, cases, HERE / "run_native.py", HERE / "fixtures.py",
              HERE / "capture_reference.py", HERE / "finite.json", HERE / "source-pins.json",
              HERE / "RecenterObservation.java", HERE.parent / "controller/capture.py",
              HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    resources = inventory(out / "resources")
    world = inventory(out / "world")
    report = {"status": "FAIL", "device": args.device, "input_before_sha256": before}
    try:
        command = [str(executable), str(out / "resources"), str(out / "world"), str(cases)]
        if not args.device:
            command.append("--cpu-only")
        receipt = capture(command, out, "native", 120, os.environ.copy(), out.resolve())
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        actual = rows(text)
        report["mismatches"] = [{"index": i, "native": a, "original": b}
                                for i, (a, b) in enumerate(zip(actual, expected)) if a != b]
        ensure(actual == expected, "native/original recenter/JO rows differ; see run.json")
        ensure(re.fullmatch(r"Source recenter/JO: [1-9][0-9]* checks; 31 rows; device=" +
                            str(int(args.device)), text.splitlines()[-1]), "native completion")
        report.update(status="PASS", rows=31,
                      scope="whole original recenter prepared no-move/failure fields and JO/JDK "
                            "traces/partial arrays; device mode separately adds real native "
                            "rebuild, moving DK lists and retained two-move failure prefix")
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = (before == report["input_after_sha256"] and resources == inventory(out / "resources")
                and world == inventory(out / "world"))
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8",
                                      newline="\n")
        ensure(same, "native recenter/JO inputs changed")
    print("Source recenter/JO: 31 original rows; device=" + str(int(args.device)) +
          "; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
