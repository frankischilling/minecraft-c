#!/usr/bin/env python3
"""Retain native snow support, synchronous removal and light-melt contracts.

The shared four-World transport fixture serializes sixteen input Chunks before
loading. Region files are input records, not final-state or original evidence.
Player harvesting and rendering are separate, unqualified snow methods.
"""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "tests/tools/world/test_falling.py"
SPEC = importlib.util.spec_from_file_location("snow_capture_support", HELPER)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("snow capture support is missing")
FALLING = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FALLING)
TILES = FALLING.TILES
CAPTURE = FALLING.CAPTURE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "constructed native snow contracts",
              "tested_commit": None, "passed": False, "original_comparison": False,
              "player_harvesting": False, "processes": []}
    try:
        CAPTURE.require(args.executable.is_file(), "native snow executable")
        work = CAPTURE.capture_root(args.work_dir, [sys.executable])
        paths = TILES.input_paths(args.executable)
        paths.update({"driver": Path(__file__).resolve(), "fixture_driver": FALLING.HELPER,
                      "shared_input_inspection": HELPER,
                      "test": ROOT / "tests/world/runtime/test_snow.c",
                      "fixture_source": ROOT / "tests/world/runtime/test_fluids.c",
                      "random_test_support": ROOT / "tests/support/java_random.h"})
        CAPTURE.require(all(path.is_file() and not path.is_symlink() for path in paths.values()),
                        "ordinary selected source inputs")
        report["inputs"] = {name: CAPTURE.file_fingerprint(path) for name, path in paths.items()}
        inputs = work / "inputs"
        inputs.mkdir(exist_ok=False)
        for index, (name, path) in enumerate(paths.items()):
            if name != "executable":
                (inputs / (str(index) + "-" + path.name)).write_bytes(path.read_bytes())
        root = work / "world-\u00e9-\u4e16\u754c"
        CAPTURE.require(not root.exists() and not root.is_symlink(), "fresh owned Unicode root")
        stdout, stderr = CAPTURE.run(
            [str(args.executable.resolve()), "world-snow", str(root).encode("utf-8").hex()],
            work=work, name="native", records=report["processes"], deadline=time.monotonic() + 80)
        CAPTURE.require(not stderr, "native snow stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines(keepends=True)
        CAPTURE.require(bool(lines) and lines[:-1] == ["16 achievements\n", "151 recipes\n"] * 2,
                        "complete source bootstrap console")
        matched = re.fullmatch(r"PASS: ([1-9][0-9]*) World snow checks \(world-snow\)\n", lines[-1])
        CAPTURE.require(matched is not None, "complete genuine native result")
        report["checks"] = int(matched.group(1))
        report["input_records"] = FALLING.inspect_inputs(root)
        for name, path in paths.items():
            CAPTURE.require(CAPTURE.file_fingerprint(path) == report["inputs"][name],
                            "source input changed during run: " + name)
        report["inputs_unchanged"] = True
        report["world_files"] = {path.relative_to(root).as_posix(): CAPTURE.file_fingerprint(path)
                                 for path in sorted(root.rglob("*")) if path.is_file()}
        report["passed"] = True
        print("".join(lines), end="")
        return 0
    except Exception as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("Capture: " + str(work), file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
