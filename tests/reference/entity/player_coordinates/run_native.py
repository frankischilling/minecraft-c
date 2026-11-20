"""Replay source-bound coordinate rows through the genuine native NBT/Player fixture."""

import argparse
import importlib.util
import json
from pathlib import Path
import sys
import tempfile

sys.dont_write_bytecode = True
from packet import digest, need, read_reference, rows

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    expected = read_reference()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="player-coordinate-", dir=args.work_dir.resolve()))
    controller = ROOT / "tests/tools/entity/test_player_nbt.py"
    spec = importlib.util.spec_from_file_location("coordinate_native_controller", controller)
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    executable = args.executable.resolve(strict=True)
    paths = [executable, Path(__file__), controller, HERE / "packet.py", HERE / "selected.txt",
             HERE / "provenance.json"]
    before = {str(path): digest(path) for path in paths}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        result = native.main(["--executable", str(executable), "--kind", "coordinates",
                              "--work-dir", str(output / "run"), "--source-root", str(ROOT)])
        need(result == 0, "actual native coordinate controller completed")
        captures = list((output / "run").glob("player-nbt-*/native.stdout"))
        need(len(captures) == 1, "exact one actual native process capture")
        text = captures[0].read_text(encoding="ascii")
        need(rows(text) == expected, "five exact original/native coordinate rows")
        need(read_reference() == expected, "coordinate source packet unchanged through replay")
        report.update(status="PASS", compared_rows=5, native_stdout_sha256=digest(captures[0]),
                      native_report=str(captures[0].parent / "report.json"))
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in paths}
        if report["input_after_sha256"] != before:
            report["status"] = "FAIL"
            report["input_changed"] = True
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    need(report["status"] == "PASS", "native coordinate input integrity")
    print("Player coordinates:5 exact original/native rows; " + str(output / "report.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
