"""Retain finite WorldAccess notification inputs and actual native receipts."""
import argparse
import copy
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
TIMEOUT = 180
LIMIT = 1024 * 1024
COUNTS = {"lease-cell": (2, 1, 0), "lease-range": (2, 1, 0),
          "live-cell": (1, 3, 0), "live-range": (1, 3, 0), "equal-range": (1, 3, 0),
          "empty-cell": (1, 0, 0), "empty-range": (1, 0, 0), "admission": (8, 0, 0),
          "nodes": (6, 1, 4), "capture-refusal": (2, 1, 0),
          "native-stop-cell": (2, 1, 0), "native-stop-range": (2, 1, 0),
          "scope-drift": (2, 1, 0)}


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def protocol(mode, checks=1):
    entries, callbacks, probes = COUNTS[mode]
    return [f"NOTIFICATION_BEGIN schema=1 mode={mode}".encode(), b"16 achievements",
            b"151 recipes", f"NOTIFICATION_CASE {mode} BEGIN".encode(),
            f"NOTIFICATION_CASE {mode} END entries={entries} callbacks={callbacks} probes={probes}".encode(),
            b"NOTIFICATION_OPEN neighbors/hooks/full-notify/original-UI",
            f"PASS: {checks} actual owned notification checks;mode={mode}".encode()]


def completed(child, mode, command):
    require(type(child.returncode) is int and child.returncode == 0 and
            type(child.stdout) is bytes and type(child.stderr) is bytes and child.stderr == b"",
            "Native notification failed")
    capture = child.capture_report
    require(type(capture) is dict and capture.get("command") == command and child.args == command,
            "Actual notification command mismatch")
    for name, value in (("exit", 0), ("timeout_seconds", TIMEOUT), ("limit", LIMIT),
                        ("stdout_bytes", len(child.stdout)), ("stderr_bytes", 0)):
        require(type(capture.get(name)) is int and capture[name] == value,
                "Wrong native notification capture count/status: " + name)
    for name, value in (("reaped", True), ("pipe_capture_complete", True),
                        ("timed_out", False), ("output_overflow", False)):
        require(capture.get(name) is value, "Incomplete notification capture: " + name)
    require(capture.get("reader_errors") == {} and child.stdout.endswith(b"\n") and
            len(child.stdout) <= LIMIT, "Notification capture extent/error")
    normalized = child.stdout.replace(b"\r\n", b"\n")
    require(b"\r" not in normalized, "Malformed notification newline")
    lines = normalized.split(b"\n")
    require(len(lines) == 8 and lines[:-2] == protocol(mode)[:-1] and lines[-1] == b"",
            "Wrong exact notification protocol")
    footer = re.fullmatch(rb"PASS: ([1-9][0-9]{0,4}) actual owned notification checks;mode=" +
                          mode.encode(), lines[-2])
    require(footer is not None and int(footer.group(1)) <= 65536, "Missing notification completion")
    return int(footer.group(1))


def self_test():
    refused = 0
    for mode in COUNTS:
        command = ["native", mode, "owned"]
        stdout = b"\n".join(protocol(mode)) + b"\n"
        capture = dict(command=command, exit=0, timeout_seconds=TIMEOUT, limit=LIMIT,
                       stdout_bytes=len(stdout), stderr_bytes=0, reaped=True,
                       pipe_capture_complete=True, timed_out=False, output_overflow=False,
                       reader_errors={})
        good = SimpleNamespace(args=command, returncode=0, stdout=stdout, stderr=b"",
                               capture_report=capture)
        require(completed(good, mode, command) == 1, "Positive notification protocol")
        cases = []
        for name, value in (("returncode", False), ("returncode", 1),
                            ("stdout", stdout[:-1]), ("stdout", stdout + b"extra\n"),
                            ("stdout", stdout.replace(b"entries=", b"entries=9", 1)),
                            ("stderr", b"failed\n")):
            item = copy.deepcopy(good)
            setattr(item, name, value)
            item.capture_report.update(stdout_bytes=len(item.stdout), stderr_bytes=len(item.stderr))
            cases.append(item)
        for name, value in (("command", ["other"]), ("exit", False), ("reaped", False),
                            ("pipe_capture_complete", False), ("timed_out", True),
                            ("output_overflow", True), ("reader_errors", {"stdout": "lost"}),
                            ("limit", 0), ("timeout_seconds", 1)):
            item = copy.deepcopy(good)
            item.capture_report[name] = value
            cases.append(item)
        for item in cases:
            try:
                completed(item, mode, command)
            except ValueError:
                refused += 1
            else:
                raise ValueError("Malformed notification capture accepted")
    require(refused == len(COUNTS) * 15, "Every malformed notification receipt refused")
    print(f"PASS: {len(COUNTS)} notification protocols; {refused} malformed receipts refused")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--mode", choices=tuple(COUNTS))
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    require(args.mode is not None and args.executable is not None and args.work_dir is not None,
            "Notification mode/executable/work directory required")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="notification-", dir=args.work_dir.resolve()))
    print(json.dumps({"retained_work": str(out), "stage": "PREPARING"}), flush=True)
    report = {"status": "FAIL", "mode": args.mode,
              "scope": "Finite notification leaf; neighbors/hooks/full notify remain open"}
    scalar = load("notification_scalar_inputs", "tests/tools/world/test_block_scalar_execution.py")
    controls = load("notification_real_guards", "tests/tools/world/test_world_query_controls.py")
    support = load("notification_real_capture", "tests/reference/client/statistics/support.py")
    child = before = inputs_before = owned = None
    finished = False
    guarded = []

    def fail(name, error):
        report[name] = {"type": type(error).__name__, "message": str(error)}
        report["status"] = "FAIL"

    def guards():
        return {str(path): scalar.pin(path) for path in guarded}

    try:
        executable = args.executable.resolve(strict=True)
        guarded = [executable, Path(__file__).resolve()] + [ROOT / name for name in (
            "tests/world/runtime/world_access/test_notification_execution.c",
            "tests/world/runtime/world_access/test_block_scalar_execution.c",
            "tests/world/runtime/world_access/test_collision_query_execution.c",
            "tests/world/runtime/tile_entities/test_tile_entities.c",
            "src/world/runtime/mutation/notifications.c",
            "src/world/runtime/mutation/notifications_execution_internal.h",
            "tests/tools/world/test_block_scalar_execution.py",
            "tests/tools/world/test_world_query_controls.py",
            "tests/tools/world/test_source_saving.py",
            "tests/tools/world/test_source_world_construction.py",
            "tests/tools/world/collision_query_inputs.py",
            "tests/reference/client/statistics/support.py")]
        before = guards()
        report["guard_before"] = before
        owned = out / "owned"
        require(str(owned).isascii(), "Actual fixture requires ASCII path")
        (owned / "snapshots").mkdir(parents=True)
        for relative, data in scalar.prepared("values").items():
            path = owned / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("xb") as stream:
                stream.write(data)
        inputs_before = controls.fixture_files(owned)
        report["literal_before"] = inputs_before
        command = [str(executable), args.mode, str(owned)]
        report["command"] = command
        child = support.run_bounded(command, out, "native", timeout=TIMEOUT, limit=LIMIT,
                                    env=os.environ.copy())
        report["capture"] = child.capture_report
        report["checks"] = completed(child, args.mode, command)
        report["status"] = "PASS"
        finished = True
    except BaseException as error:
        fail("failure", error)
        if getattr(error, "capture_report", None) is not None:
            report["failed_capture"] = error.capture_report
    finally:
        if before is not None:
            try:
                report["guard_after"] = guards()
                require(before == report["guard_after"], "Notification source/executable changed")
            except BaseException as error:
                fail("guard_failure", error)
        if inputs_before is not None:
            try:
                controls.terminal_literal_guard(report, owned, inputs_before, finished)
            except BaseException as error:
                fail("literal_failure", error)
        if child is not None:
            try:
                controls.verify_retained(child, {name: (out / name).read_bytes() for name in
                                                ("native.stdout", "native.stderr", "native.capture.json")})
                report["retained_capture_verified"] = True
            except BaseException as error:
                fail("retention_failure", error)
        report["native_protocol_complete"] = finished
        with (out / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(report, indent=2) + "\n")
    print(f"{report['status']}: actual notification control; retained {out / 'report.json'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
