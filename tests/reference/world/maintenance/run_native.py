"""Strict native-only driver, usable by CTest without a JVM or original assets."""
import argparse
import json
from pathlib import Path
import re
import tempfile

from capture import capture, digest, ensure, require_success

FINAL = re.compile(rb"Provider maintenance: ([1-9][0-9]{0,9}) checks")


def validate_report(path):
    path = Path(path)
    report = json.loads(path.read_text())
    require_success(report, empty_stderr=True)
    ensure(report["kind"] == "native_maintenance" and report["checks"] > 0, report)
    stdout = path.parent / report["stdout_file"]
    stderr = path.parent / report["stderr_file"]
    ensure(digest(stdout) == report["stdout_sha256"], "native stdout changed")
    ensure(digest(stderr) == report["stderr_sha256"] and stderr.stat().st_size == 0, "native stderr changed")
    lines = stdout.read_bytes().splitlines()
    match = FINAL.fullmatch(lines[-1]) if lines else None
    ensure(match is not None and int(match.group(1)) == report["checks"], "native final marker missing or changed")
    return report, stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    if args.work_dir is not None:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        args.output = Path(tempfile.mkdtemp(prefix="provider-maintenance-", dir=args.work_dir))
    else:
        args.output.mkdir(parents=True, exist_ok=False)
    resources = args.output / "resources"
    (resources / "lang").mkdir(parents=True)
    (resources / "achievement").mkdir()
    (resources / "achievement/map.txt").write_bytes(b"")
    (resources / "lang/en_US.lang").write_bytes(b"tile.stone.name=Authored stone\n")
    (resources / "lang/stats_US.lang").write_bytes(
        b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
        b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n")
    command = [str(args.executable.resolve()), str(resources.resolve()), str(args.output.resolve())]
    report = capture(command, args.output, "native", 180)
    require_success(report, empty_stderr=True)
    lines = (args.output / report["stdout_file"]).read_bytes().splitlines()
    match = FINAL.fullmatch(lines[-1]) if lines else None
    ensure(match is not None, "native final positive check-count marker missing")
    report.update(kind="native_maintenance", checks=int(match.group(1)),
                  executable_sha256=digest(args.executable))
    (args.output / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    validate_report(args.output / "run.json")
    print("Native maintenance: " + str(report["checks"]) + " checks; report=" + str((args.output / "run.json").resolve()))


if __name__ == "__main__":
    main()
