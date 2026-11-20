"""Strict native-only driver, usable by CTest without a JVM or original assets."""
import argparse
import json
from pathlib import Path
import re
import tempfile

from capture import capture, digest, ensure, require_success

FINAL = re.compile(rb"Player action: ([1-9][0-9]{0,9}) checks")


def validate_report(path):
    path = Path(path)
    report = json.loads(path.read_text())
    require_success(report, empty_stderr=True)
    ensure(report["kind"] == "native_player_action" and report["checks"] > 0, report)
    stdout = path.parent / report["stdout_file"]
    stderr = path.parent / report["stderr_file"]
    ensure(digest(stdout) == report["stdout_sha256"], "native stdout changed")
    ensure(digest(stderr) == report["stderr_sha256"] and stderr.stat().st_size == 0, "native stderr changed")
    lines = stdout.read_bytes().splitlines()
    match = FINAL.fullmatch(lines[-1]) if lines else None
    ensure(match is not None and int(match.group(1)) == report["checks"], "native final marker missing or changed")
    return report, stdout


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--reference", action="store_true")
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--work-dir", type=Path)
    args = parser.parse_args(argv)
    if args.work_dir is not None:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        args.output = Path(tempfile.mkdtemp(prefix="player-action-", dir=args.work_dir))
    else:
        args.output.mkdir(parents=True, exist_ok=False)
    command = [str(args.executable.resolve()), "player-action-reference" if args.reference else "player-action",
               str((args.output / "worlds").resolve()).encode("utf-8").hex()]
    before = digest(args.executable)
    report = capture(command, args.output, "native", 180)
    after = digest(args.executable)
    report.update(executable_before_sha256=before, executable_after_sha256=after)
    ensure(before == after, "native executable changed during execution")
    require_success(report, empty_stderr=True)
    lines = (args.output / report["stdout_file"]).read_bytes().splitlines()
    match = FINAL.fullmatch(lines[-1]) if lines else None
    ensure(match is not None, "native final positive check-count marker missing")
    report.update(kind="native_player_action", checks=int(match.group(1)),
                  executable_sha256=digest(args.executable))
    (args.output / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    validate_report(args.output / "run.json")
    print("Native Player action: " + str(report["checks"]) + " checks; report=" + str((args.output / "run.json").resolve()))


if __name__ == "__main__":
    main()
