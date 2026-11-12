#!/usr/bin/env python3
"""Authored native close-ownership controller; no JAR, image or GPU is used.

This controller is source-only until its separately built C contract passes.
It uses the existing reviewed bounded capture helper, including explicit
reader-fault reporting. All rejection paths use checks that survive python -O.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys


CAPTURE_SHA256 = "1ac10e26456a6a8c9ba21751f49499549ec8ed322874c0d5c07be74c5fed4fa1"
CASES = ("stream-success", "stream-retry", "font-success", "font-retry")
RESOURCE = b"authored non-PNG close ownership input\n"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pin(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def expected_rows():
    rows = []
    # The C control independently compares the actual enum constants. Numeric
    # values below pin this source protocol: stream BUSY=7, font BUSY=3, OK=0.
    for name in CASES:
        count = 2 if name.endswith("retry") else 1
        font = name.startswith("font")
        for entry in range(1, count + 1):
            rows.extend((f"CLOSE {name} {entry} 0", f"NESTED_STREAM {name} 7 1"))
            if font:
                rows.extend((f"NESTED_FONT {name} 3 1", f"NESTED_RETAIN {name} 3"))
        rows.append(f"CASE_OK {name} {count} {count} {count if font else 0} {count if font else 0}")
    return rows


def validate(result):
    capture = result.capture_report
    require(result.returncode == 0, "native close control exit must be exactly zero")
    require(result.stderr == b"", "native close control stderr must be empty")
    require(capture.get("reader_errors") == {}, "capture reader errors")
    require(capture.get("pipe_capture_complete") is True, "incomplete capture")
    require(capture.get("reaped") is True, "native child was not reaped")
    require(capture.get("timed_out") is False, "native child timed out")
    require(capture.get("output_overflow") is False, "native output overflow")
    text = result.stdout.decode("ascii", errors="strict")
    # Match logical LF/CRLF rows only; do not strip banners or diagnostics.
    require(text.endswith("\n"), "missing final newline")
    logical = text.replace("\r\n", "\n")
    require("\r" not in logical, "non-row CR in stdout")
    rows = logical[:-1].split("\n")
    require(len(rows) > 0, "empty stdout")
    require(rows[:-1] == expected_rows(), "callback/close/hold protocol mismatch")
    footer = re.fullmatch(r"Source close callbacks: ([1-9][0-9]*) checks;4 genuine-filesystem cases", rows[-1])
    require(footer is not None, "missing positive-check native footer")
    return int(footer.group(1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    require(executable.is_file(), "executable must be an existing regular file")
    helper = (args.source_root / "tests/reference/client/statistics/support.py").resolve(strict=True)
    require(pin(helper)["sha256"] == CAPTURE_SHA256, "unreviewed bounded capture helper")
    driver = Path(__file__).resolve(strict=True)
    before = {str(path): pin(path) for path in (executable, helper, driver)}
    work = args.work_dir.resolve()
    # The caller supplies a fresh capture root; no old evidence is overwritten.
    work.mkdir(parents=True, exist_ok=False)
    resources = work / "resources"
    resources.mkdir()
    resource = resources / "not-image.bin"
    require(resource.write_bytes(RESOURCE) == len(RESOURCE), "short authored resource write")
    spec = importlib.util.spec_from_file_location("close_capture", helper)
    require(spec is not None and spec.loader is not None, "capture helper import spec")
    module = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    spec.loader.exec_module(module)
    env = dict(os.environ)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    report = {"status": "FAIL", "before": before, "authored_resource": pin(resource),
              "cases": list(CASES), "native_or_original_GL_claim": False}
    code = 1
    try:
        result = module.run_bounded([str(executable), str(resources)], work, "native-close",
                                    timeout=30, limit=65536, env=env)
        report["capture"] = result.capture_report
        report["checks"] = validate(result)
        report["status"] = "PASS"
        code = 0
    except Exception as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        if hasattr(error, "capture_report"):
            report["capture"] = error.capture_report
    finally:
        try:
            report["after"] = {str(path): pin(path) for path in (executable, helper, driver)}
            require(report["after"] == before, "executable/controller/capture helper changed")
            require(pin(resource) == report["authored_resource"], "authored resource changed")
        except Exception as error:
            report["pin_error"] = {"type": type(error).__name__, "message": str(error)}
            report["status"] = "FAIL"
            code = 1
        data = json.dumps(report, indent=2) + "\n"
        with (work / "report.json").open("x", encoding="utf-8", newline="\n") as target:
            require(target.write(data) == len(data), "short controller report write")
    if code:
        print("Close callback control failed; retained report: " + str(work / "report.json"), file=sys.stderr)
    else:
        print(f"Close callback control: {report['checks']} checks;4 genuine-filesystem cases")
    return code


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("Close callback control: " + str(error), file=sys.stderr)
        raise SystemExit(1)
