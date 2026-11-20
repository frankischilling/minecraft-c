"""Compare actual NW normal state and converted vertex bytes with the original."""

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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="nw-normals-", dir=args.work_dir.resolve()))
    cases = json.loads((HERE / "inputs.json").read_text(encoding="utf-8"))["cases"]
    expected = json.loads((HERE / "finite.json").read_text(encoding="utf-8"))["rows"]
    ensure(rows("\n".join(expected), cases) == expected and len(expected) == 30,
           "complete original normal/vertex row inventory")
    inputs = [executable, HERE / "inputs.json", HERE / "finite.json", HERE / "source-pins.json",
              HERE / "NormalObservation.java", HERE / "capture_reference.py", HERE / "run_native.py",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable)], output, "native", 30,
                          os.environ.copy(), output.resolve())
        report["capture"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt["stdout_file"]).read_text()
        marker = re.fullmatch(r"Source NW normals: [1-9][0-9]* checks", text.splitlines()[-1])
        ensure(marker is not None and text.splitlines().count("But..") == 2,
               "completed native method/control calls and two exact source diagnostics")
        actual = rows(text, cases)
        report["actual_rows"] = actual
        ensure(all(line in actual or line == "But.." or line == marker[0]
                   for line in text.splitlines()), "no undeclared native output")
        report["mismatches"] = [{"index": index, "native": native, "original": original}
                                 for index, (native, original) in enumerate(zip(actual, expected))
                                 if native != original]
        ensure(actual == expected, "native/original normal states or vertex words differ")
        report.update(status="PASS", rows=len(actual), diagnostics=2,
                      scope="Whole NW normal method, actual diagnostic/null-out/begin prefixes "
                            "and 48 expanded vertex words. No GL draw or lit pixels.")
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                         encoding="utf-8", newline="\n")
        ensure(same, "native executable and fixture inputs unchanged")
    print("Source NW normals: 30 exact state/vertex rows; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
