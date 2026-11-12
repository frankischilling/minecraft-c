"""Retain direct Slime K execution and exact native cleanup controls."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
MODES = ("ready", "sticky-math", "scope-drift", "native-close", "legacy-release",
         "owned-release", "owned-final-release", "child-allocation")


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def observation(line, mode):
    match = re.fullmatch(
        r"SLIME_REMOVAL_EXECUTION_OBSERVATION mode=" + re.escape(mode) +
        r" prepared=([01]) completed=([01]) raw=([0-9]+) native=([0-9]+)"
        r" math_failed=([01]) callbacks=([0-9]+) children=([0-9]+) dead=([01])"
        r" invariants=([01]) cleanup=([01])", line)
    if match is None:
        return None
    keys = ("prepared", "completed", "raw", "native", "math_failed", "callbacks",
            "children", "dead", "invariants", "cleanup")
    return dict(zip(keys, map(int, match.groups())))


def legacy_observation(line):
    match = re.fullmatch(
        r"SLIME_REMOVAL_LEGACY_OBSERVATION mode=legacy-release prepared=([01]) raw=([0-9]+)"
        r" cleanup_status=([0-9]+) pc=([0-9]+) pending=([01]) draws=([0-9]+)"
        r" callbacks=([0-9]+) children=([0-9]+) dead=([01]) invariants=([01]) cleanup=([01])", line)
    if match is None:
        return None
    keys = ("prepared", "raw", "cleanup_status", "pc", "pending", "draws", "callbacks",
            "children", "dead", "invariants", "cleanup")
    return dict(zip(keys, map(int, match.groups())))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=MODES, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="slime-removal-owned-", dir=args.work_dir)).resolve()
    report = {"status": "FAIL", "mode": args.mode,
              "scope": "Actual direct UW K; no natural LS tick or original VM comparison"}
    child = before = None
    guarded = []

    def pins():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in guarded}

    def fail(name, error):
        report[name] = {"type": type(error).__name__, "message": str(error)}
        report["status"] = "FAIL"

    try:
        executable = args.executable.resolve(strict=True)
        guarded = [executable, Path(__file__).resolve(),
                   ROOT / "tests/world/runtime/test_slime_removal_execution.c",
                   ROOT / "tests/world/runtime/test_entity_tick.c",
                   ROOT / "tests/reference/client/statistics/support.py"]
        before = pins()
        report["guard_before"] = before
        support = load("slime_owned_capture", "tests/reference/client/statistics/support.py")
        root = out / "worlds"
        root.mkdir()
        command = [str(executable), args.mode, str(root).encode("utf-8").hex()]
        report["command"] = command
        child = support.run_bounded(command, out, "native", timeout=240, limit=1024 ** 2,
                                    env=os.environ.copy())
        report["capture"] = child.capture_report
        lines = child.stdout.decode("utf-8", errors="strict").splitlines()
        require(child.returncode == 0 and child.stderr == b"", "Native Slime control failed")
        legacy = args.mode == "legacy-release"
        prefix = "SLIME_REMOVAL_LEGACY" if legacy else "SLIME_REMOVAL_EXECUTION"
        require(len(lines) == 5 and lines[0] == prefix + "_BEGIN mode=" + args.mode and
                lines[1:3] == ["16 achievements", "151 recipes"],
                "Unexpected Slime initialization/output protocol")
        row = legacy_observation(lines[3]) if legacy else observation(lines[3], args.mode)
        require(row is not None, "Missing exact Slime observation")
        report["observation"] = row
        if legacy:
            expected = {"prepared": 1, "pc": 132, "pending": 1, "draws": 2,
                        "callbacks": 2, "children": 2, "dead": 0, "invariants": 1, "cleanup": 1}
            require(all(row[key] == value for key, value in expected.items()) and
                    row["raw"] != 0 and row["cleanup_status"] != 0,
                    "Public legacy K hid a native stop or changed its retained prefix")
        else:
            ready = args.mode == "ready"
            sticky = args.mode == "sticky-math"
            final_release = args.mode == "owned-final-release"
            allocation = args.mode == "child-allocation"
            expected = {"prepared": 1, "completed": int(ready),
                        "math_failed": int(sticky),
                        "callbacks": 4 if (ready or final_release) else 0 if sticky else 1 if allocation else 2,
                        "children": 4 if (ready or final_release) else 0 if sticky else 1 if allocation else 2,
                        "dead": int(ready or final_release),
                        "invariants": 1, "cleanup": 1}
            require(all(row[key] == value for key, value in expected.items()),
                    "Slime Source/native prefix or cleanup observation mismatch")
            require((row["raw"] != 0) if allocation else row["raw"] == 0,
                    "Slime actual raw Entity return channel mismatch")
            require((row["native"] == 0) if (ready or sticky or allocation) else row["native"] != 0,
                    "Slime native-only refusal channel mismatch")
        qualification = "legacy-release" if legacy else "execution-owned"
        footer = re.fullmatch(r"PASS: ([1-9][0-9]*) " + qualification +
                              r" Slime removal checks;mode=" + re.escape(args.mode), lines[4])
        require(footer is not None, "Missing Slime completion footer")
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
                require(before == report["guard_after"], "Slime source/executable guard changed")
            except BaseException as error:
                fail("guard_failure", error)
        if child is not None:
            try:
                controls = load("slime_owned_retention", "tests/tools/world/test_world_query_controls.py")
                controls.verify_retained(child, {name: (out / name).read_bytes() for name in
                                                ("native.stdout", "native.stderr", "native.capture.json")})
                report["retained_capture_verified"] = True
            except BaseException as error:
                fail("retention_failure", error)
        with (out / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(report, indent=2) + "\n")
    print(f"{report['status']}: actual Slime K execution; retained {out / 'report.json'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
