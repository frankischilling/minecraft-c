"""Run native-only render Entity-list controls in a fresh authored World directory."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SUPPORT = ROOT / "tests/reference/client/controller/capture.py"
SPEC = importlib.util.spec_from_file_location("world_render_entities_capture", SUPPORT)
CAPTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPTURE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run-", dir=args.work_dir.resolve()))
    world = work / "worlds"
    world.mkdir()
    inputs = [executable, Path(__file__).resolve(), HERE / "test_render_entities.c",
              HERE.parent / "test_entity_publication.c", SUPPORT,
              ROOT / "include/mc/world/runtime/render_entities.h",
              ROOT / "src/world/runtime/render_entities.c"]
    before = {str(path): CAPTURE.digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Native-only genuine GAME World/Store/Chunk/Entity contracts; "
                       "authored metadata and Chunk bytes; no JVM, original parity or assets."}
    try:
        receipt = CAPTURE.capture([str(executable), "world-render-entities",
                                   str(world).encode("utf-8").hex()], work, "native", 90,
                                  working_directory=work)
        report["capture"] = receipt
        CAPTURE.require_success(receipt, empty_stderr=True)
        output = (work / receipt["stdout_file"]).read_text(encoding="utf-8")
        match = re.fullmatch(r"16 achievements\r?\n151 recipes\r?\n"
                             r"PASS: ([1-9][0-9]*) World render Entity List checks "
                             r"\(world-render-entities\)\r?\n", output)
        CAPTURE.ensure(match is not None, "one complete native contract/checked-cleanup marker")
        CAPTURE.ensure(int(match[1]) == 95, "all95 genuine List and lease checks")
        report.update(status="PASS", checks=int(match[1]),
                      bootstrap_console=["16 achievements", "151 recipes"],
                      original_equality_claim=False)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path): CAPTURE.digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (work / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        CAPTURE.ensure(same, "native executable and all declared contract inputs unchanged")
    print("World render Entity Lists: " + str(report["checks"]) + " native checks; " + str(work / "run.json"))


if __name__ == "__main__":
    main()
