"""Require native completion and compare all original pause screen observations."""
import argparse
import json
from pathlib import Path
import re
import sys
import tempfile
import os

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
    out = Path(tempfile.mkdtemp(prefix="pause-screen-", dir=args.work_dir))
    executable = args.executable.resolve(strict=True)
    inputs = [executable, HERE / "finite.json", HERE / "run_native.py",
              HERE / "capture_reference.py", HERE.parent / "controller/capture.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        expected = json.loads((HERE / "finite.json").read_text())["rows"]
        resources = out / "resources"
        (resources / "lang").mkdir(parents=True)
        (resources / "lang/en_US.lang").write_text("gui.achievements=Achievements\ngui.stats=Statistics\n", encoding="ascii")
        (resources / "lang/stats_US.lang").write_text("# authored empty statistics translations\n", encoding="ascii")
        resource_before = {p.relative_to(resources).as_posix(): digest(p) for p in resources.rglob("*") if p.is_file()}
        report["resource_before_sha256"] = resource_before
        receipt = capture([str(executable), str(resources.resolve())], out, "native", 45)
        resource_after = {p.relative_to(resources).as_posix(): digest(p) for p in resources.rglob("*") if p.is_file()}
        report["resource_after_sha256"] = resource_after
        ensure(resource_before == resource_after, "native component modified authored resources")
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"[1-9][0-9]* pause screen checks passed", text.splitlines()[-1]),
               "native completion missing")
        ensure(rows(text) == expected, "original/native pause state differs")
        report.update(status="PASS", rows=68, native=receipt)
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "native inputs changed")
    print("Pause screen: native controls and 68 exact original rows; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
