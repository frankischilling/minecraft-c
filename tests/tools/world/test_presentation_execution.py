"""Retain finite coordinate presentation inputs and actual native receipts."""
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
COUNTS = {'sound-scope-drift': (2, 1, 0),
 'sound-native-stop': (2, 1, 0),
 'sound-live': (1, 3, 0),
 'sound-empty': (1, 0, 0),
 'sound-held-live': (1, 3, 0),
 'sound-lease': (2, 1, 0),
 'sound-nodes': (4, 1, 2),
 'sound-capture-refusal': (2, 1, 0),
 'sound-admission': (5, 0, 0),
 'particle-scope-drift': (2, 1, 0),
 'particle-native-stop': (2, 1, 0),
 'particle-live': (1, 3, 0),
 'particle-empty': (1, 0, 0),
 'particle-held-live': (1, 3, 0),
 'particle-lease': (2, 1, 0),
 'particle-nodes': (4, 1, 2),
 'particle-capture-refusal': (2, 1, 0),
 'particle-admission': (5, 0, 0),
 'particle-legacy-scope-drift': (1, 1, 0),
 'particle-bare-scope-drift': (1, 1, 0),
 'particle-legacy-live': (1, 3, 0),
 'particle-bare-live': (1, 3, 0),
 'particle-legacy-lease': (1, 1, 0),
 'particle-bare-lease': (1, 1, 0),
 'particle-legacy-null-lease': (1, 1, 0),
 'particle-bare-null-native-stop': (1, 1, 0),
 'particle-legacy-admission': (7, 0, 0),
 'particle-bare-admission': (7, 0, 0),
 'entity-detailed-scope-drift': (1, 1, 0),
 'entity-detailed-native-stop': (1, 1, 0),
 'entity-detailed-live': (1, 3, 0),
 'entity-detailed-admission': (7, 0, 0),
 'entity-detailed-null-scope-drift': (1, 1, 0),
 'entity-bare-scope-drift': (1, 1, 0),
 'entity-bare-native-stop': (1, 1, 0),
 'entity-bare-live': (1, 3, 0),
 'entity-bare-admission': (7, 0, 0),
 'entity-bare-null-scope-drift': (1, 1, 0)}


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
    return [f"PRESENTATION_BEGIN schema=1 mode={mode}".encode(), b"16 achievements",
            b"151 recipes", f"PRESENTATION_CASE {mode} BEGIN".encode(),
            f"PRESENTATION_CASE {mode} END entries={entries} callbacks={callbacks} probes={probes}".encode(),
            b"PRESENTATION_OPEN recursive-mutation/full-wake/original-UI",
            f"PASS: {checks} actual owned presentation checks;mode={mode}".encode()]


def completed(child, mode, command):
    require(type(child.returncode) is int and child.returncode == 0 and
            type(child.stdout) is bytes and type(child.stderr) is bytes and child.stderr == b"",
            "Native presentation failed")
    capture = child.capture_report
    require(type(capture) is dict and capture.get("command") == command and child.args == command,
            "Actual presentation command mismatch")
    for name, value in (("exit", 0), ("timeout_seconds", TIMEOUT), ("limit", LIMIT),
                        ("stdout_bytes", len(child.stdout)), ("stderr_bytes", 0)):
        require(type(capture.get(name)) is int and capture[name] == value,
                "Wrong native presentation capture count/status: " + name)
    for name, value in (("reaped", True), ("pipe_capture_complete", True),
                        ("timed_out", False), ("output_overflow", False)):
        require(capture.get(name) is value, "Incomplete presentation capture: " + name)
    require(capture.get("reader_errors") == {} and child.stdout.endswith(b"\n") and
            len(child.stdout) <= LIMIT, "Presentation capture extent/error")
    normalized = child.stdout.replace(b"\r\n", b"\n")
    require(b"\r" not in normalized, "Malformed presentation newline")
    lines = normalized.split(b"\n")
    require(len(lines) == 8 and lines[:-2] == protocol(mode)[:-1] and lines[-1] == b"",
            "Wrong exact presentation protocol")
    footer = re.fullmatch(rb"PASS: ([1-9][0-9]{0,4}) actual owned presentation checks;mode=" +
                          mode.encode(), lines[-2])
    require(footer is not None and int(footer.group(1)) <= 65536, "Missing presentation completion")
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
        require(completed(good, mode, command) == 1, "Positive presentation protocol")
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
                raise ValueError("Malformed presentation capture accepted")
    require(refused == len(COUNTS) * 15, "Every malformed presentation receipt refused")
    print(f"PASS: {len(COUNTS)} presentation protocols; {refused} malformed receipts refused")


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
            "Presentation mode/executable/work directory required")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="presentation-", dir=args.work_dir.resolve()))
    print(json.dumps({"retained_work": str(out), "stage": "PREPARING"}), flush=True)
    report = {"status": "FAIL", "mode": args.mode,
              "scope": "Finite coordinate sound/particle leaves; recursive mutation and full wake remain open"}
    scalar = load("presentation_scalar_inputs", "tests/tools/world/test_block_scalar_execution.py")
    controls = load("presentation_real_guards", "tests/tools/world/test_world_query_controls.py")
    support = load("presentation_real_capture", "tests/reference/client/statistics/support.py")
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
            "tests/world/runtime/world_access/test_presentation_execution.c",
            "tests/world/runtime/world_access/test_block_scalar_execution.c",
            "tests/world/runtime/world_access/test_collision_query_execution.c",
            "tests/world/runtime/tile_entities/test_tile_entities.c",
            "src/world/runtime/presentation.c",
            "src/world/runtime/movement.c",
            "src/world/runtime/world_access.c",
            "src/world/runtime/world_access_internal.h",
            "src/world/runtime/execution_children_internal.h",
            "include/mc/world/runtime/movement.h",
            "include/mc/world/runtime/world_access_error.h",
            "src/world/runtime/presentation_execution_internal.h",
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
                require(before == report["guard_after"], "Presentation source/executable changed")
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
    print(f"{report['status']}: actual presentation control; retained {out / 'report.json'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
