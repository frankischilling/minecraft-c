"""Retain actual Math invocation proofs and canonical VALUE forwarding controls."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from types import SimpleNamespace

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
MODES = ("require", "note", "forward")
TIMEOUT = 90
LIMIT = 1024 * 1024


def require(value, message):
    if not value:
        raise ValueError(message)


def completed(child, mode, command):
    require(type(child.returncode) is int and child.returncode == 0 and
            type(child.stdout) is bytes and type(child.stderr) is bytes and child.stderr == b"",
            "Actual Math proof control failed")
    capture = child.capture_report
    require(type(capture) is dict and capture.get("command") == command and child.args == command,
            "Actual Math proof command mismatch")
    for name, value in (("exit", 0), ("timeout_seconds", TIMEOUT), ("limit", LIMIT),
                        ("stdout_bytes", len(child.stdout)), ("stderr_bytes", 0)):
        require(type(capture.get(name)) is int and capture[name] == value,
                "Wrong Math proof capture count/status: " + name)
    for name, value in (("reaped", True), ("pipe_capture_complete", True),
                        ("timed_out", False), ("output_overflow", False)):
        require(capture.get(name) is value, "Incomplete Math proof capture: " + name)
    require(capture.get("reader_errors") == {} and child.stdout.endswith(b"\n") and
            len(child.stdout) <= LIMIT, "Math proof capture extent/error")
    normalized = child.stdout.replace(b"\r\n", b"\n")
    require(b"\r" not in normalized, "Malformed Math proof newline")
    lines = normalized.split(b"\n")
    require(len(lines) == 3 and lines[-1] == b"" and
            lines[0] == ("MATH_PROOF_BEGIN schema=1 mode=" + mode).encode(),
            "Wrong exact Math proof sequence")
    footer = re.fullmatch(rb"PASS: ([1-9][0-9]{0,4}) actual Math proof checks;mode=" +
                          mode.encode(), lines[1])
    require(footer is not None and int(footer.group(1)) <= 65536, "Missing Math proof completion")
    return int(footer.group(1))


def self_test():
    # Python-only transport controls; these are never native Math proof producers.
    refused = 0
    for mode in MODES:
        command = ["native", mode]
        stdout = (f"MATH_PROOF_BEGIN schema=1 mode={mode}\n"
                  f"PASS: 1 actual Math proof checks;mode={mode}\n").encode()
        capture = dict(command=command, exit=0, timeout_seconds=TIMEOUT, limit=LIMIT,
                       stdout_bytes=len(stdout), stderr_bytes=0, reaped=True,
                       pipe_capture_complete=True, timed_out=False, output_overflow=False,
                       reader_errors={})
        good = SimpleNamespace(args=command, returncode=0, stdout=stdout, stderr=b"",
                               capture_report=capture)
        require(completed(good, mode, command) == 1, "Positive Math proof transport")
        bad = []
        for name, value in (("returncode", False), ("returncode", 1),
                            ("stdout", stdout[:-1]), ("stdout", stdout + b"extra\n"),
                            ("stdout", stdout.replace(b"schema=1", b"schema=2")),
                            ("stderr", b"failure\n")):
            item = copy.deepcopy(good)
            setattr(item, name, value)
            item.capture_report.update(stdout_bytes=len(item.stdout), stderr_bytes=len(item.stderr))
            bad.append(item)
        for name, value in (("command", ["other"]), ("exit", False), ("reaped", False),
                            ("pipe_capture_complete", False), ("timed_out", True),
                            ("output_overflow", True), ("reader_errors", {"stdout": "lost"}),
                            ("limit", 0), ("timeout_seconds", 1)):
            item = copy.deepcopy(good)
            item.capture_report[name] = value
            bad.append(item)
        for item in bad:
            try:
                completed(item, mode, command)
            except ValueError:
                refused += 1
            else:
                raise ValueError("Malformed Math proof capture accepted")
    require(refused == len(MODES) * 15, "Every malformed Math transport refused")
    print(f"PASS: {len(MODES)} Math proof protocols; {refused} malformed receipts refused")


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def pin(path):
    require(path.is_file() and not path.is_symlink(), "Regular Math fixture file required")
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--mode", choices=MODES)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    require(args.mode is not None and args.executable is not None and args.work_dir is not None,
            "Math proof mode/executable/work directory required")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="math-proof-", dir=args.work_dir.resolve()))
    print(json.dumps({"retained_work": str(out), "stage": "PREPARING"}), flush=True)
    report = {"status": "FAIL", "mode": args.mode,
              "scope": "Actual require returns and VALUE forwarding; no Source replay or full caller claim"}
    child = before = None
    guarded = []

    def pins():
        return {str(path): pin(path) for path in guarded}

    def fail(name, error):
        report[name] = {"type": type(error).__name__, "message": str(error)}
        report["status"] = "FAIL"

    try:
        executable = args.executable.resolve(strict=True)
        guarded = [executable, Path(__file__).resolve()] + [ROOT / name for name in (
            "tests/world/runtime/world_access/test_math_proof_execution.c",
            "src/core/math/classes.c", "src/core/math/execution_internal.h",
            "src/world/runtime/runtime_execution.c", "src/world/runtime/runtime_execution_internal.h",
            "tests/reference/client/statistics/support.py")]
        before = pins()
        report["guard_before"] = before
        support = load("math_proof_actual_capture", "tests/reference/client/statistics/support.py")
        command = [str(executable), args.mode]
        report["command"] = command
        child = support.run_bounded(command, out, "native", timeout=TIMEOUT, limit=LIMIT,
                                    env=os.environ.copy())
        report["capture"] = child.capture_report
        report["checks"] = completed(child, args.mode, command)
        report["status"] = "PASS"
    except BaseException as error:
        fail("failure", error)
        if getattr(error, "capture_report", None) is not None:
            report["failed_capture"] = error.capture_report
    finally:
        if before is not None:
            try:
                report["guard_after"] = pins()
                require(before == report["guard_after"], "Math proof fixture/executable changed")
            except BaseException as error:
                fail("guard_failure", error)
        if child is not None:
            try:
                controls = load("math_proof_actual_retention", "tests/tools/world/test_world_query_controls.py")
                controls.verify_retained(child, {name: (out / name).read_bytes() for name in
                                                ("native.stdout", "native.stderr", "native.capture.json")})
                report["retained_capture_verified"] = True
            except BaseException as error:
                fail("retention_failure", error)
        with (out / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(report, indent=2) + "\n")
    print(f"{report['status']}: actual Math proof control; retained {out / 'report.json'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
