"""Run real YC/World dirty custody with authored self-removing producers."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location(
        "sign_dirty_capture", ROOT / "tests/reference/client/controller/capture.py")
    if spec is None or spec.loader is None:
        raise RuntimeError("YC dirty capture support unavailable")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="dirty-", dir=args.work_dir.resolve()))
    game = out / "worlds"
    game.mkdir()
    executable = args.executable.resolve(strict=True)
    paths = [Path(__file__).resolve(), executable,
             ROOT / "tests/client/gui/test_source_sign_screen_dirty.c",
             ROOT / "tests/world/runtime/test_entity_publication.c",
             ROOT / "src/client/gui/source_sign_screen.c",
             ROOT / "src/client/gui/source_sign_screen_methods.c",
             ROOT / "src/client/gui/source_sign_screen_internal.h",
             ROOT / "include/mc/world/runtime/world_access_error.h"]
    def pins():
        return {str(path): capture.digest(path) for path in paths}
    report = {"status": "FAIL", "scope": __doc__, "inputs_before": pins()}
    try:
        command = [str(executable), "world-sign-screen-dirty", str(game).encode("utf-8").hex()]
        receipt = capture.capture(command, out, "native", 90)
        report["capture"] = receipt
        capture.require_success(receipt, empty_stderr=True)
        lines = (out / receipt["stdout_file"]).read_bytes().decode("ascii", errors="strict").splitlines()
        if not lines or lines[:-1] not in ([], ["16 achievements"], ["16 achievements", "151 recipes"]):
            raise ValueError("Unexpected YC dirty bootstrap output")
        match = re.fullmatch(r"YC dirty ownership passed: ([1-9][0-9]*) checks", lines[-1])
        if match is None:
            raise ValueError("Missing complete YC dirty ownership footer")
        report["checks"] = int(match[1])
        report["inputs_after"] = pins()
        if report["inputs_before"] != report["inputs_after"]:
            raise ValueError("YC dirty source/product inputs changed")
        report["status"] = "PASS"
    finally:
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "checks": report["checks"],
                      "report": str(out / "report.json")}))


if __name__ == "__main__":
    main()
