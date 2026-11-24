"""Retain bounded execution-owned World scalar controls and their real inputs."""
import argparse
import copy
import gzip
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
TIMEOUT = 180
LIMIT = 1024 * 1024
COUNTS = {
    "values": (27, 0, 0), "metadata": (29, 0, 0), "blank": (12, 0, 0),
    "admission": (19, 2, 9), "loader-error": (2, 0, 0),
    "completion-block": (2, 1, 0), "completion-metadata": (2, 1, 0),
    "completion-set": (2, 1, 0), "open-descendant": (2, 1, 0),
}


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def pin(path):
    require(path.is_file() and not path.is_symlink(), "Regular literal file required")
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def protocol(mode, checks=1):
    calls, callbacks, probes = COUNTS[mode]
    return [f"BLOCK_SCALAR_BEGIN schema=1 mode={mode}".encode(), b"16 achievements",
            b"151 recipes", f"BLOCK_SCALAR_CASE {mode} BEGIN".encode(),
            f"BLOCK_SCALAR_CASE {mode} END calls={calls} callbacks={callbacks} probes={probes}".encode(),
            b"BLOCK_SCALAR_OPEN external-cold-graph/full-Arrow",
            f"PASS: {checks} actual owned block scalar checks;mode={mode}".encode()]


def completed(child, mode, command):
    capture = child.capture_report
    require(type(child.returncode) is int and child.returncode == 0 and
            type(child.stdout) is bytes and type(child.stderr) is bytes and child.stderr == b"",
            "Native exit/stream failure")
    require(type(capture) is dict and capture.get("command") == command and child.args == command,
            "Actual native command mismatch")
    for name, value in (("exit", 0), ("timeout_seconds", TIMEOUT), ("limit", LIMIT),
                        ("stdout_bytes", len(child.stdout)), ("stderr_bytes", 0)):
        require(type(capture.get(name)) is int and capture[name] == value,
                "Native capture count/status mismatch: " + name)
    for name, value in (("reaped", True), ("pipe_capture_complete", True),
                        ("timed_out", False), ("output_overflow", False)):
        require(capture.get(name) is value, "Incomplete native capture: " + name)
    require(capture.get("reader_errors") == {} and child.stdout.endswith(b"\n"),
            "Native capture error/final newline")
    normalized = child.stdout.replace(b"\r\n", b"\n")
    require(b"\r" not in normalized and len(child.stdout) <= LIMIT,
            "Malformed native transport extent")
    lines = normalized.split(b"\n")
    require(len(lines) == 8 and lines[-1] == b"" and lines[:-2] == protocol(mode)[:-1],
            "Wrong exact scalar mode/control sequence")
    footer = re.fullmatch(rb"PASS: ([1-9][0-9]{0,4}) actual owned block scalar checks;mode=" +
                          mode.encode(), lines[-2])
    require(footer is not None and int(footer.group(1)) <= 65536, "Missing scalar completion")
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
        require(completed(good, mode, command) == 1, "Positive scalar protocol")
        cases = []
        for name, value in (("returncode", False), ("returncode", 1),
                            ("stdout", stdout[:-1]), ("stdout", stdout + b"extra\n"),
                            ("stdout", stdout.replace(b"calls=", b"calls=9", 1)),
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
                raise ValueError("Malformed scalar capture accepted")
    require(refused == len(COUNTS) * 15, "Every malformed scalar receipt refused")
    print(f"PASS: {len(COUNTS)} scalar protocols; {refused} malformed receipts refused")


def prepared(mode):
    saving = load("scalar_real_seed", "tests/tools/world/test_source_saving.py")
    construction = load("scalar_real_construction", "tests/tools/world/test_source_world_construction.py")
    collision = load("scalar_real_values", "tests/tools/world/collision_query_inputs.py")
    if mode == "loader-error":
        info, image = gzip.compress(construction.metadata(), mtime=0), construction.malformed_region()
    elif mode.startswith("completion-") or mode == "open-descendant":
        info, image = saving.seed("item-chest")
    else:
        info, image = collision.values(saving)
    return {"world/level.dat": bytes(info), "world/region/r.0.0.mcr": bytes(image),
            "world/data/idcounts.dat": construction.raw_ids(),
            "world/preserve.bin": b"owned scalar unrelated input\0\xff"}


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
            "Native scalar mode/executable/work directory required")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="block-scalar-", dir=args.work_dir.resolve()))
    print(json.dumps({"retained_work": str(out), "stage": "PREPARING"}), flush=True)
    report = {"status": "FAIL", "mode": args.mode,
              "scope": "Finite native scalar controls; external cold graph/full Arrow remain open"}
    child = before = inputs_before = controls = owned = None
    finished = False
    guarded = []

    def fail(name, error):
        report[name] = {"type": type(error).__name__, "message": str(error)}
        report["status"] = "FAIL"

    def guards():
        return {str(path): pin(path) for path in guarded}

    try:
        executable = args.executable.resolve(strict=True)
        relative = ("tests/world/runtime/world_access/test_block_scalar_execution.c",
                    "src/world/runtime/blocks_execution_internal.h", "src/world/runtime/blocks.c",
                    "tests/world/runtime/world_access/test_collision_query_execution.c",
                    "tests/world/runtime/tile_entities/test_tile_entities.c",
                    "tests/tools/world/test_source_saving.py",
                    "tests/tools/world/test_source_world_construction.py",
                    "tests/tools/world/collision_query_inputs.py",
                    "tests/tools/world/test_world_query_controls.py",
                    "tests/reference/client/statistics/support.py")
        guarded = [executable, Path(__file__).resolve()] + [ROOT / name for name in relative]
        guarded += [ROOT / "tests/reference/client" / name / "provenance.json" for name in
                    ("application_owner", "biped", "render_resources", "display_sdk",
                     "loading_draw", "resource_lookup_common")]
        before = guards()
        report["guard_before"] = before
        controls = load("scalar_real_guards", "tests/tools/world/test_world_query_controls.py")
        support = load("scalar_real_capture", "tests/reference/client/statistics/support.py")
        owned = out / "owned"
        require(str(owned).isascii(), "Actual fixture requires ASCII root")
        (owned / "snapshots").mkdir(parents=True)
        for relative, data in prepared(args.mode).items():
            require(type(data) is bytes and len(data) <= LIMIT, "Bounded literal scalar input")
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
        finished = True
        report["status"] = "PASS"
    except BaseException as error:
        fail("failure", error)
        if getattr(error, "capture_report", None) is not None:
            report["failed_capture"] = error.capture_report
    finally:
        if before is not None:
            try:
                report["guard_after"] = guards()
                require(before == report["guard_after"], "Scalar source/executable/provenance changed")
            except BaseException as error:
                fail("guard_failure", error)
        if inputs_before is not None:
            try:
                controls.terminal_literal_guard(report, owned, inputs_before, finished)
            except BaseException as error:
                fail("literal_failure", error)
        if child is not None:
            try:
                byte_map = {name: (out / name).read_bytes() for name in
                            ("native.stdout", "native.stderr", "native.capture.json")}
                controls.verify_retained(child, byte_map)
                report["retained_capture_verified"] = True
            except BaseException as error:
                fail("retention_failure", error)
        report["native_protocol_complete"] = finished
        with (out / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(report, indent=2) + "\n")
    print(f"{report['status']}: actual World scalar control; retained {out / 'report.json'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
