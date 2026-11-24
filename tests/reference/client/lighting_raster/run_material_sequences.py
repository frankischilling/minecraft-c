"""Compare independent material prefixes and the unqueried draw sequence exactly."""

import argparse
import hashlib
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
spec = importlib.util.spec_from_file_location("material_sequences_capture", SUPPORT)
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)
capture, digest, ensure, require_success = (
    support.capture, support.digest, support.ensure, support.require_success)
from reference_profiles import load_references, select_reference


def material(line, name):
    cells = line.split()
    ensure(len(cells) == 16 and cells[:2] == ["MATERIAL", name], "complete ordered material row")
    ensure(all(re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){3}", value)
               for value in [*cells[2:10], cells[12]]) and
           all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[10:12]) and
           cells[13] in ("0", "1") and all(value.isdecimal() for value in cells[14:]),
           "all front/back material, shininess, current color and selector words")


def selected_rows(text, prefix):
    found = [line for line in text.splitlines()
             if line.startswith(("MATERIAL ", "RASTER ", "RASTER_STATE "))]
    if prefix is not None:
        ensure(len(found) == 1, "one end query for the complete declared prefix")
        material(found[0], "after_prefix_" + str(prefix))
    else:
        ensure(len(found) == 3, "complete unqueried draw, source state and following material query")
        pixel, state = (line.split() for line in found[:2])
        ensure(len(pixel) == 4 and pixel[:3] == ["RASTER", "material_switch_freeze", "0"] and
               re.fullmatch(r"[0-9a-f]{32768}", pixel[3]), "whole original64x64RGBA8 frame")
        ensure(len(state) == 8 and state[:3] == ["RASTER_STATE", "material_switch_freeze", "0"] and
               re.fullmatch(r"[01]{9}", state[3]) and all(value.isdecimal() for value in state[4:]),
               "complete separate source-state fields")
        material(found[2], "after_contiguous_draw")
    return found


def rows(text):
    found = [line for line in text.splitlines()
             if line.startswith(("MATERIAL ", "RASTER ", "RASTER_STATE "))]
    ensure(len(found) == 12, "all nine independent prefixes and the complete three-row draw sequence")
    for prefix in range(9):
        ensure(selected_rows(found[prefix], prefix) == [found[prefix]], "ordered fresh-context prefix")
    ensure(selected_rows("\n".join(found[9:]), None) == found[9:], "whole contiguous draw sequence")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    references, reference_paths, reference_hashes = load_references("material_sequences", None, None, rows)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="lighting-material-sequences-", dir=args.work_dir)).resolve(strict=True)
    inputs = [executable, *reference_paths, HERE / "run_material_sequences.py", SUPPORT]
    before = {str(path.resolve()): digest(path) for path in inputs}
    ensure(all(before.get(path) == value for path, value in reference_hashes.items()),
           "parsed original reference bytes match the initial input snapshot")
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [], "complete_inventory": False}
    try:
        mismatch_count = 0
        for prefix in [*range(9), None]:
            argument = "--material-sequence" if prefix is None else "--material-prefix=" + str(prefix)
            receipt = capture([str(executable), argument], output,
                              "native-sequence" if prefix is None else "native-prefix" + str(prefix),
                              30, os.environ.copy(), output)
            profile = {"name": argument, "capture": receipt, "status": "FAIL"}
            report["profiles"].append(profile)
            require_success(receipt, empty_stderr=True)
            text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
            reference, context = select_reference(references, text)
            identity = reference["profile"]["id"]
            ensure(report.setdefault("reference_profile", identity) == identity,
                   "the same captured driver is used for every fresh context")
            marker = (r"Source lighting material sequence: 3 rows; [1-9][0-9]* checks" if prefix is None else
                      r"Source lighting material prefix: " + str(prefix) + r"; [1-9][0-9]* checks")
            ensure(re.fullmatch(marker, text.splitlines()[-1]), "checked native cleanup after all observations")
            expected = (reference["fixture"]["rows"][9:] if prefix is None else
                        [reference["fixture"]["rows"][prefix]])
            actual = selected_rows(text, prefix)
            differences = []
            for index, (native, original) in enumerate(zip(actual, expected)):
                if native == original:
                    continue
                detail = {"index": index, "kind": native.split()[0]}
                if native.startswith("RASTER "):
                    a, b = bytes.fromhex(native.split()[3]), bytes.fromhex(original.split()[3])
                    changed = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
                    pixels = sorted({i // 4 for i in changed})
                    detail.update(different_channels=len(changed), different_pixels=len(pixels),
                                  maximum_channel_difference=max((abs(a[i] - b[i]) for i in changed), default=0),
                                  native_sha256=hashlib.sha256(a).hexdigest(),
                                  original_sha256=hashlib.sha256(b).hexdigest(),
                                  examples=[{"x": p % 64, "y": p // 64,
                                             "native": list(a[p*4:p*4+4]),
                                             "original": list(b[p*4:p*4+4])} for p in pixels[:2]])
                else:
                    detail.update(native=native, original=original)
                differences.append(detail)
            mismatch_count += len(differences)
            profile.update(context, status="FAIL" if differences else "PASS", mismatches=differences,
                           rows=len(actual), frames=1 if prefix is None else 0)
        report.update(rows=12, frames=1, contexts=10, mismatches=mismatch_count, complete_inventory=True)
        ensure(mismatch_count == 0, "native material prefix/draw sequences differ from complete original observations")
        report["status"] = "PASS"
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable and complete original material sequences unchanged")
    print("Source material sequences: nine fresh prefixes and one complete unqueried draw match exactly; " +
          str(output / "run.json"))


if __name__ == "__main__":
    main()
