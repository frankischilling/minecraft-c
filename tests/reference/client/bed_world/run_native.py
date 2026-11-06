"""Compare real native Bed NW output with the finite original CPU packet."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success
from packet import read_reference, rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--reference-file", type=Path, default=HERE / "selected.txt")
    args = parser.parse_args(argv)
    ensure(args.reference_file.name == "selected.txt", "source-bound packet filename")
    expected = read_reference(args.reference_file.parent)
    spec = importlib.util.spec_from_file_location("bed_native_inputs", ROOT / "tests/tools/client/test_picking.py")
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="bed-world-", dir=args.work_dir.resolve()))
    for name, data in inputs.RESOURCES.items():
        path = output / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs.authored_world(output / "world")
    executable = args.executable.resolve(strict=True)
    paths = [executable, Path(__file__), HERE / "packet.py", args.reference_file,
             ROOT / "tests/tools/client/test_picking.py", HERE.parent / "controller/capture.py"]
    before = {str(path): digest(path) for path in paths}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable), str(output / "resources"), str(output / "world")],
                          output, "native", 90)
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt["stdout_file"]).read_text(encoding="ascii")
        match = re.fullmatch(r"Bed World: ([1-9][0-9]*) checks;192 original geometry rows and retained native failure controls",
                             text.splitlines()[-1])
        ensure(match is not None, "complete positive native ownership/failure-control marker")
        actual = rows(text)
        ensure(actual == expected, "exact192 original/native Bed NW rows differ")
        report.update(status="PASS", checks=int(match[1]), compared_rows=192)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in paths}
        if report["input_after_sha256"] != before:
            report["status"] = "FAIL"
            report["input_changed"] = True
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    ensure(report["status"] == "PASS", "native Bed input integrity")
    print("Bed:192 exact original/native NW rows; " + str(output / "report.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
