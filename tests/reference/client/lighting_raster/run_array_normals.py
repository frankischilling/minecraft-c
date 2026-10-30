"""Compare actual submitted GPU normal vectors with captured legacy array inputs."""

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
spec = importlib.util.spec_from_file_location("lighting_array_normals_capture", SUPPORT)
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)
capture, digest, ensure, require_success = (
    support.capture, support.digest, support.ensure, support.require_success)
from reference_profiles import load_references, select_reference


def words(value):
    return re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){2}", value)


def rows(text):
    found = [line for line in text.splitlines() if line.startswith("NATIVE_ARRAY_NORMAL ")]
    ensure(len(found) == 256, "all 256 direct-array GPU normal observations")
    for value, line in zip(range(-128, 128), found):
        cells = line.split()
        ensure(len(cells) == 6 and cells[:2] == ["NATIVE_ARRAY_NORMAL", str(value)] and
               all(words(vector) for vector in cells[2:5]) and cells[5] == "0",
               "ordered three complete GPU normal vectors and unknown current normal")
    return found


def projection(reference):
    decoded = [line for line in reference["rows"] if line.startswith("NORMAL_DECODE ")]
    ensure(len(decoded) == 256, "complete original array transform-feedback inventory")
    projected = []
    for value, line in zip(range(-128, 128), decoded):
        cells = line.split()
        ensure(len(cells) == 5 and cells[:2] == ["NORMAL_DECODE", str(value)] and
               all(words(vector) for vector in cells[2:]),
               "actual current/array/formula observations for each original byte value")
        projected.append("NATIVE_ARRAY_NORMAL " + str(value) + " " +
                         " ".join([cells[3]] * 3) + " 0")
    return projected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    references, reference_paths, reference_hashes = load_references("array_normals", None, None, rows)
    for reference in references:
        original = reference["fixture"]
        ensure(original["original_fixture"] == reference["observations_path"].name and
               original["original_fixture_sha256"] == reference["observations_sha256"],
               "unchanged complete original array observations")
        ensure(projection(reference["observations"]) == original["rows"],
               "exact projection from actual original array TF")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="lighting-array-normals-", dir=args.work_dir)).resolve(strict=True)
    inputs = [executable, *reference_paths, HERE / "run_array_normals.py", SUPPORT]
    before = {str(path.resolve()): digest(path) for path in inputs}
    ensure(all(before.get(path) == value for path, value in reference_hashes.items()),
           "parsed original reference bytes match the initial input snapshot")
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        receipt = capture([str(executable), "--array-normals"], output, "native", 60,
                          os.environ.copy(), output)
        report["capture"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
        reference, context = select_reference(references, text)
        original = reference["fixture"]
        expected = original["rows"]
        report.update(context, reference_profile=reference["profile"]["id"], scope=original["scope"],
                      original_fixture_sha256=original["original_fixture_sha256"])
        lines = text.splitlines()
        ensure(lines and re.fullmatch(r"Source lighting array normals: 256 rows; [1-9][0-9]* checks",
                                     lines[-1]), "completed actual native draws and checked cleanup")
        layouts = [line for line in lines if line.startswith("ARRAY_NORMAL_LAYOUT ")]
        ensure(len(layouts) == 1, "one actual submitted normal-attribute layout receipt")
        layout = re.fullmatch(r"ARRAY_NORMAL_LAYOUT attribute=3 size=3 type=5126 stride=([0-9]+) "
                              r"offset=([0-9]+) normalized=0 buffer_is_bound=1 vertices=3", layouts[0])
        ensure(layout and 12 <= int(layout[1]) <= 4096 and int(layout[2]) <= 4096,
               "actual enabled float3 normal attribute in the queried bound GPU buffer")
        report["gpu_layout"] = layouts[0]
        actual = rows(text)
        report["mismatches"] = [
            {"index": index, "native": native, "original": legacy}
            for index, (native, legacy) in enumerate(zip(actual, expected)) if native != legacy]
        ensure(actual == expected, "submitted GPU normal vectors differ from legacy array inputs")
        report.update(status="PASS", rows=256, vertices=768, components=2304)
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
        ensure(same, "native executable and original array-normal inputs unchanged")
    print("Source lighting array normals: 256 exact observed GPU input cases; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
