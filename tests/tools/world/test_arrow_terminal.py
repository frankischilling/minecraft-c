"""Run native Arrow publication and checked World shooter-graph retirement.

Actual constructor/World/Chunk producers supply the graph. Explicit native
fault controls claim ownership/admission behavior, not Arrow flight, rendering,
NBT, a completed Skeleton parent or unchanged-original equality.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "arrow_terminal_capture", ROOT / "scripts/reference/java/compare_int_set.py")
capture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(capture)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bindings(executable):
    paths = {ROOT / "CMakeLists.txt", Path(__file__).resolve(), Path(SPEC.origin),
             ROOT / "cmake/tests/arrow_terminal.cmake",
             ROOT / "tests/world/runtime/test_arrow_terminal.c",
             ROOT / "tests/world/runtime/test_entity_publication.c",
             ROOT / "tests/support/java_random.h"}
    for directory in ("src", "include"):
        paths.update(path for path in (ROOT / directory).rglob("*")
                     if path.is_file() and path.suffix in (".c", ".h", ".inc"))
    result = {path.relative_to(ROOT).as_posix(): digest(path) for path in sorted(paths)}
    result["native_executable"] = digest(executable)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    work = None
    report = {"scope": __doc__, "original_equality_claim": False, "passed": False,
              "processes": []}
    try:
        executable = args.executable.resolve(strict=True)
        work = capture.capture_root(args.work_dir, [sys.executable])
        world = work / "world-\u00e9-\u4e16\u754c"
        capture.require(not world.exists() and not world.is_symlink(), "Fresh owned Arrow World")
        before = bindings(executable)
        report["input_sha256_before"] = before
        stdout, stderr = capture.run(
            [str(executable), "arrow-terminal", str(world).encode("utf-8").hex()],
            work=work, name="native", records=report["processes"],
            deadline=time.monotonic() + 90)
        capture.require(not stderr, "Arrow ownership control stderr must be empty")
        lines = stdout.decode("ascii").replace("\r\n", "\n").splitlines()
        capture.require(len(lines) == 3 and lines[:2] == ["16 achievements", "151 recipes"],
                        "Complete actual one-namespace bootstrap")
        result = re.fullmatch(r"PASS: ([1-9][0-9]*) Arrow World ownership checks \(arrow-terminal\)",
                              lines[2])
        capture.require(result is not None, "Complete native Arrow ownership footer")
        report["checks"] = int(result.group(1))
        report["bootstrap_console"] = lines[:2]
        after = bindings(executable)
        report["input_sha256_after"] = after
        capture.require(before == after, "Native source/helper/executable inputs unchanged")
        report["inputs_unchanged"] = True
        report["world_files"] = {}
        for path in sorted(world.rglob("*")):
            capture.require(not path.is_symlink(), "Owned World has no linked external artifact")
            if path.is_file():
                report["world_files"][path.relative_to(world).as_posix()] = {
                    "bytes": path.stat().st_size, "sha256": digest(path)}
        capture.require(bool(report["world_files"]), "Retained actual World artifacts")
        report["passed"] = True
        print("\n".join(lines))
        return 0
    except (capture.Failure, OSError, UnicodeError, ValueError) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("Capture: " + str(work))


if __name__ == "__main__":
    raise SystemExit(main())
