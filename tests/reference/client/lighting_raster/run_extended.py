"""Compare successor specular, two-sided, color-control and unlit raster scenes exactly."""

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
sys.path.insert(0, str(HERE))
SPEC = importlib.util.spec_from_file_location("lighting_raster_extended_parser", HERE / "capture_extended.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("extended raster parser unavailable")
PARSER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PARSER)
rows = PARSER.rows
from capture import capture, digest, ensure, require_success
from reference_profiles import load_references, select_reference


def pixel_difference(actual, expected):
    a, b = bytes.fromhex(actual.split()[3]), bytes.fromhex(expected.split()[3])
    different = [index for index in range(len(a)) if a[index] != b[index]]
    pixels = sorted({index // 4 for index in different})
    return {"different_channels": len(different), "different_pixels": len(pixels),
            "maximum_channel_difference": max((abs(a[index] - b[index]) for index in different), default=0),
            "native_sha256": hashlib.sha256(a).hexdigest(), "original_sha256": hashlib.sha256(b).hexdigest(),
            "examples": [{"x": pixel % 64, "y": pixel // 64,
                          "native": list(a[pixel * 4:pixel * 4 + 4]),
                          "original": list(b[pixel * 4:pixel * 4 + 4])} for pixel in pixels[:12]]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--profiles", type=int, nargs="+",
                        help="diagnose selected indices; default requires the entire captured inventory")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    case_path = HERE / "extended-inputs.json"
    case_bytes = case_path.read_bytes()
    cases = json.loads(case_bytes)["cases"]
    references, reference_paths, reference_hashes = load_references("extended", cases, 3, rows)
    reference_hashes[str(case_path.resolve())] = hashlib.sha256(case_bytes).hexdigest()
    indices = list(range(len(cases))) if args.profiles is None else args.profiles
    ensure(indices and len(indices) == len(set(indices)) and all(0 <= index < len(cases) for index in indices),
           "unique declared profile indices")
    offsets = [0]
    for case in cases:
        offsets.append(offsets[-1] + case["frames"] * 3)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="lighting-raster-extended-", dir=args.work_dir)).resolve(strict=True)
    inputs = [executable, *reference_paths, HERE / "extended-inputs.json", HERE / "source-pins.json",
              HERE / "run_extended.py", HERE / "capture_extended.py", HERE / "capture_reference.py", HERE.parent / "controller/capture.py",
              HERE.parent / "controller/classfile.py", HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    ensure(all(before.get(path) == value for path, value in reference_hashes.items()),
           "parsed original reference bytes match the initial input snapshot")
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [], "selected_indices": indices,
              "complete_inventory": indices == list(range(len(cases))),
              "scope": "Successor exact RGBA8 frames and separate source/model state for authored legacy-GL controls. "
                       "No unchanged Minecraft/NW body or complete game frame is qualified."}
    try:
        mismatches = 0
        for index in indices:
            case = cases[index]
            receipt = capture([str(executable), str(case["native_index"])], output, "native" + str(index), 30,
                              os.environ.copy(), output)
            profile = {"name": case["name"], "index": index, "capture": receipt}
            report["profiles"].append(profile)
            require_success(receipt, empty_stderr=True)
            text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
            lines = text.splitlines()
            marker = r"Source lighting raster: " + re.escape(case["name"]) + r"; [1-9][0-9]* checks"
            ensure(lines and re.fullmatch(marker, lines[-1]), "normal fixture completion after checked cleanup")
            reference, context = select_reference(references, text)
            identity = reference["profile"]["id"]
            ensure(report.setdefault("reference_profile", identity) == identity,
                   "the captured driver remains the same for every profile")
            profile.update(context, reference_profile=identity,
                           reference_fixture_sha256=reference["sha256"])
            original = reference["fixture"]["rows"][offsets[index]:offsets[index + 1]]
            actual = rows(text, case)
            difference = []
            for row_index, (native, legacy) in enumerate(zip(actual, original)):
                if native != legacy:
                    detail = {"index": row_index, "kind": native.split()[0]}
                    if native.startswith("RASTER "):
                        detail.update(pixel_difference(native, legacy))
                    else:
                        detail.update(native=native, original=legacy)
                    difference.append(detail)
            profile.update(status="FAIL" if difference else "PASS", mismatches=difference,
                           rows=len(actual), frames=case["frames"])
            mismatches += len(difference)
        report.update(rows=sum(cases[index]["frames"] * 3 for index in indices),
                      frames=sum(cases[index]["frames"] for index in indices), mismatches=mismatches)
        ensure(mismatches == 0, "native/legacy exact lighting pixels or state differ; see per-profile diagnostics")
        report["status"] = "PASS"
    except Exception as error:
        report["failure"] = str(error)
        raise
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable and original fixture/replay inputs unchanged")
    print("Source extended lighting raster: " + str(len(indices)) + " profiles, " + str(report["frames"]) +
          " exact RGBA8 frames; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
