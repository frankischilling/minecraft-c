"""Retain genuine Player action execution controls with authored resources."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
TIMEOUT = 180
LIMIT = 1024 * 1024
DROP_MODES = ("scatter", "normal", "selected", "null-stack", "nullable-world", "absent",
              "math-missing-scatter", "math-missing-normal", "math-limit-scatter",
              "math-limit-normal", "preflight", "lease-source", "callback-capture",
              "native-close", "scope-drift", "scope-capacity", "fresh-world")
DEATH_MODES = ("empty", "full40", "notch41", "fail-apple", "math-missing", "math-limit",
               "nullable-world", "preflight", "lease-source", "native-stop",
               "callback-capture", "scope-capacity") + tuple("fail-" + str(i) for i in range(1, 41))


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main(kind="drop"):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=DROP_MODES if kind == "drop" else DEATH_MODES, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="player-" + kind + "-owned-",
                                dir=args.work_dir.resolve()))
    print(json.dumps({"retained_work": str(out), "stage": "PREPARING"}), flush=True)
    report = {"status": "FAIL", "mode": args.mode,
              "scope": "Authored actual Player owners; no original runtime comparison"}
    scalar = load("player_execution_pins", "tests/tools/world/test_block_scalar_execution.py")
    support = load("player_execution_capture", "tests/reference/client/statistics/support.py")
    resources = load("player_execution_resources", "tests/reference/client/player_statistics/support.py")
    child = before = None
    guarded = []

    def pins():
        return {str(path): scalar.pin(path) for path in guarded}

    def fail(name, error):
        report[name] = {"type": type(error).__name__, "message": str(error)}
        report["status"] = "FAIL"

    try:
        executable = args.executable.resolve(strict=True)
        guarded = [executable, Path(__file__).resolve(),
                   ROOT / ("tests/entity/player/test_" + kind + "_execution.c"),
                   ROOT / "tests/client/statistics/test_player_dispatch.c",
                   ROOT / "tests/reference/client/player_statistics/support.py",
                   ROOT / "tests/reference/client/statistics/support.py"]
        before = pins()
        report["guard_before"] = before
        resource_root, game = out / "resources", out / "game"
        resources.resources_at(resource_root)
        game.mkdir()
        command = [str(executable), args.mode, str(resource_root), str(game)]
        report["command"] = command
        child = support.run_bounded(command, out, "native", timeout=TIMEOUT, limit=LIMIT,
                                    env=os.environ.copy())
        report["capture"] = child.capture_report
        require(type(child.returncode) is int and child.returncode == 0 and child.stderr == b"",
                "Actual Player execution control failed")
        lines = child.stdout.replace(b"\r\n", b"\n").split(b"\n")
        require(lines[-1] == b"" and lines[0] ==
                ("PLAYER_" + kind.upper() + "_EXECUTION_BEGIN mode=" + args.mode).encode(),
                "Player execution mode/banner mismatch")
        footer = re.fullmatch(rb"PASS: ([1-9][0-9]*) execution-owned Player " + kind.encode() +
                              rb" checks;mode=" + args.mode.encode(), lines[-2])
        require(footer is not None, "Missing Player execution completion")
        report["checks"] = int(footer.group(1))
        report["status"] = "PASS"
    except BaseException as error:
        fail("failure", error)
        if getattr(error, "capture_report", None) is not None:
            report["failed_capture"] = error.capture_report
    finally:
        if before is not None:
            try:
                report["guard_after"] = pins()
                require(before == report["guard_after"], "Player fixture/executable changed")
            except BaseException as error:
                fail("guard_failure", error)
        if child is not None:
            try:
                controls = load("player_execution_retention", "tests/tools/world/test_world_query_controls.py")
                controls.verify_retained(child, {name: (out / name).read_bytes() for name in
                                                ("native.stdout", "native.stderr", "native.capture.json")})
                report["retained_capture_verified"] = True
            except BaseException as error:
                fail("retention_failure", error)
        with (out / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(report, indent=2) + "\n")
    print(f"{report['status']}: actual Player {kind} execution; retained {out / 'report.json'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
