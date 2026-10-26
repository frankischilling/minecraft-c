"""Run the genuine native spawn/reset or position-vector contract."""
import argparse
import json
from pathlib import Path
import re
import tempfile

from capture import capture, digest, ensure, require_success


def main(arguments=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--mode", choices=["spawn", "spawn-reference", "position-probe"], default="spawn")
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--work-dir", type=Path)
    options = parser.parse_args(arguments)
    if options.work_dir is not None:
        options.work_dir.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="player-spawn-", dir=options.work_dir))
    else:
        output = options.output
        output.mkdir(parents=True, exist_ok=False)
    executable = options.executable.resolve(strict=True)
    before = digest(executable)
    command = [str(executable), options.mode, str((output / "worlds").resolve()).encode("utf-8").hex()]
    report = capture(command, output, "native", 180)
    require_success(report, empty_stderr=True)
    after = digest(executable)
    ensure(before == after, "native executable changed during execution")
    stdout = (output / "native.stdout").read_text()
    marker = r"PASS: ([1-9][0-9]*) " + ("position vector checks" if options.mode == "position-probe" else "Player spawn reset checks")
    match = re.fullmatch(marker, stdout.splitlines()[-1]) if stdout.splitlines() else None
    ensure(match is not None, "native final marker missing")
    if options.mode == "spawn-reference":
        from capture_reference import rows
        ensure(len(rows(stdout)) == 42, "native spawn rows differ")
    report.update(executable_sha256=before, executable_before_sha256=before,
                  executable_after_sha256=after, executable_unchanged=True,
                  mode=options.mode, checks=int(match.group(1)))
    (output / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Native Player spawn reset: " + str(report["checks"]) + " checks; report=" + str(output / "run.json"))


if __name__ == "__main__":
    main()
