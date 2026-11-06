"""Compare all submitted direct/list GPU color vectors with actual driver observations."""

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
spec = importlib.util.spec_from_file_location("lighting_color_input_capture", SUPPORT)
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)
capture, digest, ensure, require_success = (
    support.capture, support.digest, support.ensure, support.require_success)
from reference_profiles import load_references, select_reference


def words(value):
    return re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){11}", value)


def rows(text):
    found = [line for line in text.splitlines() if line.startswith("NATIVE_COLOR_INPUT ")]
    ensure(len(found) == 512, "all256 direct and compiled twelve-component GPU input vectors")
    for value in range(256):
        for path, line in zip(("direct", "list"), found[value*2:(value+1)*2]):
            cells = line.split()
            ensure(len(cells) == 4 and cells[:3] == ["NATIVE_COLOR_INPUT", str(value), path] and words(cells[3]),
                   "ordered actual three-vertex RGBA color-input row")
    return found


def projection(reference):
    observed = reference["observations"]
    ensure(len(observed["rows"]) == 2560, "complete raw color-feedback/current-state observation inventory")
    driver = reference["profile"]["driver"]
    ensure(observed["headers"]["COLOR_DRIVER"] == "COLOR_DRIVER " + " | ".join(
        driver[key] for key in ("vendor", "renderer", "version")), "original observations match their complete driver profile")
    ensure(observed["headers"]["COLOR_LAYOUT"] == "COLOR_LAYOUT color_type=5121 color_size=4 color_stride=32 color_buffer=0 "
           "vertex_type=5126 vertex_size=3 vertex_stride=32 vertex_buffer=0", "actual source unsigned-byte array layout")
    decoded = [line for line in observed["rows"] if line.startswith("COLOR_DECODE ")]
    ensure(len(decoded) == 256, "all256 actual original color conversion observations")
    projected = []
    for value, line in enumerate(decoded):
        cells = line.split()
        ensure(len(cells) == 5 and cells[:2] == ["COLOR_DECODE", str(value)] and
               all(words(vector) for vector in cells[2:]), "all twelve direct/list/repeated-list component words")
        ensure(cells[3] == cells[4], "original compiled color vectors survive independent poisoned-storage replay")
        projected.extend(["NATIVE_COLOR_INPUT " + str(value) + " direct " + cells[2],
                          "NATIVE_COLOR_INPUT " + str(value) + " list " + cells[3]])
    return projected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    references, reference_paths, reference_hashes = load_references("color_inputs", None, None, rows)
    for reference in references:
        original = reference["fixture"]
        ensure(original["original_fixture"] == reference["observations_path"].name and
               original["original_fixture_sha256"] == reference["observations_sha256"] and
               re.fullmatch(r"[0-9a-f]{64}", original["original_capture_sha256"]),
               "complete raw observed color fixture and successful-capture provenance")
        ensure(projection(reference) == original["rows"], "exact observed twelve-component color-input projection")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="lighting-color-inputs-", dir=args.work_dir)).resolve(strict=True)
    inputs = [executable, *reference_paths, HERE / "run_color_inputs.py", SUPPORT]
    before = {str(path.resolve()): digest(path) for path in inputs}
    ensure(all(before.get(path) == value for path, value in reference_hashes.items()),
           "parsed original reference bytes match the initial input snapshot")
    report = {"status": "FAIL", "input_before_sha256": before, "complete_inventory": False}
    try:
        receipt = capture([str(executable), "--color-inputs"], output, "native", 60,
                          os.environ.copy(), output)
        report["capture"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
        reference, context = select_reference(references, text)
        original = reference["fixture"]
        report.update(context, reference_profile=reference["profile"]["id"], scope=original["scope"],
                      original_fixture_sha256=original["original_fixture_sha256"],
                      original_capture_sha256=original["original_capture_sha256"])
        lines = text.splitlines()
        ensure(lines and re.fullmatch(r"Source lighting color inputs: 512 rows; [1-9][0-9]* checks", lines[-1]),
               "all actual native draws and checked cleanup completed")
        actual, expected = rows(text), original["rows"]
        report["mismatches"] = [{"index": index, "native": native, "original": legacy}
                                for index, (native, legacy) in enumerate(zip(actual, expected)) if native != legacy]
        ensure(actual == expected, "submitted GPU direct/list color vectors differ from the captured driver inputs")
        report.update(status="PASS", complete_inventory=True, rows=512, direct_rows=256, list_rows=256,
                      vertices=1536, components=6144)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
            report["complete_inventory"] = False
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable and complete original color references unchanged")
    print("Source lighting color inputs: 512 exact original GPU color-vector rows; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
