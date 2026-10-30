"""Compare compiled-list current attributes with captured legacy GL observations."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / "controller/capture.py"
spec = importlib.util.spec_from_file_location("lighting_list_state_capture", SUPPORT)
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)
capture, digest, ensure, require_success = (
    support.capture, support.digest, support.ensure, support.require_success)
from reference_profiles import load_references, select_reference

LABELS = ("before_compile", "after_compile", "before_call", "after_call",
          "before_repeat", "after_repeat")


def words(value, count):
    return re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){" + str(count - 1) + r"}", value)


def rows(text):
    found = [line for line in text.splitlines()
             if line.startswith(("NATIVE_LIST_NORMAL ", "NORMAL_LIST_STATE "))]
    ensure(len(found) == 262, "all 256 compiled-list normal values and six state rows")
    for value, line in zip(range(-128, 128), found[:256]):
        cells = line.split()
        ensure(len(cells) == 3 and cells[:2] == ["NATIVE_LIST_NORMAL", str(value)] and
               words(cells[2], 3), "ordered compiled-list current-normal row")
    for label, line in zip(LABELS, found[256:]):
        cells = line.split()
        ensure(len(cells) == 11 and cells[:2] == ["NORMAL_LIST_STATE", label],
               "ordered current-normal/color/material state row")
        ensure(words(cells[2], 3) and all(words(value, 4) for value in cells[3:8]) and
               cells[8] in ("0", "1") and all(value.isdecimal() for value in cells[9:]),
               "all current attributes, tracked materials and selectors")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    references, reference_paths, reference_hashes = load_references("list_state", None, None, rows)
    for reference in references:
        original = reference["fixture"]
        ensure(all(re.fullmatch(r"[0-9a-f]{64}", original[key]) for key in
                   ("original_fixture_sha256", "original_capture_sha256")),
               "original fixture and successful capture provenance")
        ensure(reference["observations_sha256"] == original["original_fixture_sha256"],
               "unchanged complete original driver observations")
        projected = []
        for line in reference["observations"]["rows"]:
            if line.startswith("NORMAL_LIST_DECODE "):
                cells = line.split()
                ensure(len(cells) == 7 and words(cells[5], 9),
                       "actual compiled feedback includes three whole normal vectors")
                components = cells[5].split(",")
                ensure(components[:3] == components[3:6] == components[6:9],
                       "all three original submitted normals match the constant authored bytes")
                projected.append("NATIVE_LIST_NORMAL " + cells[1] + " " +
                                 ",".join(cells[5].split(",")[-3:]))
            elif line.startswith("NORMAL_LIST_STATE "):
                projected.append(line)
        ensure(rows("\n".join(projected)) == original["rows"],
               "exact projection from observed feedback and current-state rows")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="lighting-list-state-", dir=args.work_dir)).resolve(strict=True)
    inputs = [executable, *reference_paths, HERE / "run_list_state.py", SUPPORT]
    before = {str(path.resolve()): digest(path) for path in inputs}
    ensure(all(before.get(path) == value for path, value in reference_hashes.items()),
           "parsed original reference bytes match the initial input snapshot")
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable), "--list-state"], output, "native", 60,
                          os.environ.copy(), output)
        report["capture"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
        reference, context = select_reference(references, text)
        original = reference["fixture"]
        expected = original["rows"]
        report.update(context, reference_profile=reference["profile"]["id"], scope=original["scope"],
                      original_fixture_sha256=original["original_fixture_sha256"],
                      original_capture_sha256=original["original_capture_sha256"])
        lines = text.splitlines()
        ensure(lines and re.fullmatch(r"Source lighting list state: 262 rows; [1-9][0-9]* checks",
                                     lines[-1]), "completed native controls and checked cleanup")
        actual = rows(text)
        report["mismatches"] = [
            {"index": index, "native": native, "original": legacy}
            for index, (native, legacy) in enumerate(zip(actual, expected)) if native != legacy]
        ensure(actual == expected, "native/legacy compiled-list current attributes differ")
        report.update(status="PASS", normal_rows=256, state_rows=6, rows=262)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                         encoding="utf-8", newline="\n")
        ensure(same, "native executable and captured list-state inputs unchanged")
    print("Source lighting list attributes: 256 exact feedback projections and six exact state rows; " +
          str(output / "run.json"))


if __name__ == "__main__":
    main()
