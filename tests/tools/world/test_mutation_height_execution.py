"""Run the owned incremental height contract in a fresh World directory."""

import argparse
import json
from pathlib import Path
import re
import sys
import time

from test_runtime import CAPTURE, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"passed": False, "processes": []}
    try:
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = [
            ROOT / "tests/world/runtime/test_mutation_height_execution.c",
            ROOT / "tests/world/runtime/test_mutations.c",
            ROOT / "src/world/runtime/mutation/chunk.c",
            ROOT / "src/world/runtime/mutation/height.c",
            ROOT / "src/world/runtime/mutation/notifications.c",
            args.executable,
        ]
        report["inputs"] = {path.name: CAPTURE.file_fingerprint(path) for path in paths}
        world = work / "world-é-世界"
        command = [str(args.executable.resolve()), "world-mutation-height-execution",
                   str(world).encode("utf-8").hex()]
        stdout, stderr = CAPTURE.run(command, work=work, name="native",
                                     records=report["processes"], deadline=time.monotonic() + 60)
        normalized = stdout.replace(b"\r\n", b"\n")
        lines = normalized[:-1].split(b"\n")
        CAPTURE.require(not stderr and b"\r" not in normalized and normalized.endswith(b"\n") and
                        len(lines) == 3 and
                        lines[:2] == [b"16 achievements", b"151 recipes"] and
                        re.fullmatch(rb"PASS: [1-9][0-9]* owned incremental height checks "
                                     rb"\(world-mutation-height-execution\)", lines[2]) is not None,
                        "Complete owned height fixture result")
        CAPTURE.require(report["inputs"] ==
                        {path.name: CAPTURE.file_fingerprint(path) for path in paths},
                        "Owned height inputs changed during the run")
        report["passed"] = True
        print(lines[2].decode("ascii"))
        return 0
    except (CAPTURE.Failure, OSError, ValueError) as error:
        report["failure"] = str(error)
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print(f"Capture: {work}")


if __name__ == "__main__":
    sys.exit(main())
