"""Compare real source fog state and six authored shader pixel profiles."""
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
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def pixel_rows(text):
    rows = [line for line in text.splitlines()
            if line.startswith(("FOG_PIXEL ", "FOG_PIXEL_EXP ", "FOG_PIXEL_EXP2 "))]
    ensure(len(rows) == 30, "six GPU pixel profiles missing")
    points = [(16,16),(32,16),(32,32),(24,24),(40,24)]
    identities = [(tag, selector, x, y)
                  for tag in ("FOG_PIXEL","FOG_PIXEL_EXP","FOG_PIXEL_EXP2")
                  for selector in (34139,34140) for x,y in points]
    for row, identity in zip(rows, identities):
        cells = row.split()
        ensure(len(cells) == 8 and (cells[0], *map(int,cells[1:4])) == identity,
               "GPU pixel row identity differs")
        ensure(all(0 <= int(c) <= 255 for c in cells[4:]), "GPU pixel byte encoding differs")
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="fog-state-", dir=args.work_dir.resolve()))
    reference = module("original_fog_state_rows", HERE / "capture_reference.py")
    picker_path = HERE.parents[2] / "tools/client/test_picking.py"
    picker = module("fog_state_authored_world", picker_path)
    for name, data in picker.RESOURCES.items():
        target = out / "resources" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    picker.authored_world(out / "world")
    expected = json.loads((HERE / "finite.json").read_text())
    original_rows = reference.rows("\n".join(expected["rows"]))
    original_pixels = pixel_rows("\n".join(expected["pixel_rows"]))
    inputs = [executable, HERE / "finite.json", HERE / "run_native.py",
              HERE / "capture_reference.py", HERE / "FogStateObservation.java", HERE / "source-pins.json",
              picker_path, HERE.parent / "controller/capture.py",
              HERE.parent / "controller/classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status":"FAIL", "input_before_sha256":before,
              "original_driver":expected["driver"], "original_initial_nv":expected["initial_nv"],
              "original_pixel_profile":expected["pixel_profile"],
              "quantization_policy":"exact RGBA bytes for the finite authored probes; no tolerance"}
    try:
        native = capture([str(executable), str(out / "resources"), str(out / "world")], out,
                         "native", 90, os.environ.copy(), out.resolve())
        require_success(native, empty_stderr=True)
        text = (out / native["stdout_file"]).read_text()
        observed = reference.rows(text)
        initials = [line for line in text.splitlines() if line.startswith("FOG_INITIAL ")]
        drivers = [line for line in text.splitlines() if line.startswith("FOG_DRIVER ")]
        ensure(len(initials)==1 and len(drivers)==1, "native actual NV/driver receipt missing")
        report.update(native=native, native_driver=drivers[0], native_initial_nv=initials[0],
                      native_state_rows=observed)
        advertised = initials[0].split()[1] == "1"
        common = [i for i in range(28) if i not in (23,24)]
        ensure(all([n.split()[i] for i in common] == [o.split()[i] for i in common]
                   for n,o in zip(observed,original_rows)),
               "native/original common fog state/cursor values differ")
        report["original_common_field_rows"] = 48
        if advertised:
            ensure(observed == original_rows and initials[0] == expected["initial_nv"],
                   "NV-present native/full original fog state rows differ")
            report.update(original_exact_state_rows=48,
                          capability_profile="actual NV-present native context; complete original state rows")
        else:
            ensure(all(row.split()[23:25] == ["0","-1"] for row in observed),
                   "bytecode-derived skipped-selector branch differs")
            report.update(original_exact_state_rows=0, source_derived_skipped_selector_rows=48,
                          capability_profile="actual NV-absent native context; common original fields compared separately from derived skipped selector; no original false-capability observation")
        observed_pixels = pixel_rows(text)
        comparisons = []
        for n,o in zip(observed_pixels,original_pixels):
            nc,oc=n.split(),o.split()
            nv,ov=list(map(int,nc[4:])),list(map(int,oc[4:]))
            comparisons.append({"identity":nc[:4],"native_rgba":nv,"original_rgba":ov,
                                "delta":[a-b for a,b in zip(nv,ov)]})
        report["gpu_pixels"] = comparisons
        report["gpu_max_channel_error"] = max(abs(d) for row in comparisons for d in row["delta"])
        ensure(report["gpu_max_channel_error"] == 0, "authored GPU RGBA bytes differ; see per-pixel receipt")
        ensure(re.fullmatch(r"Source PX fog state: [1-9][0-9]* checks; 48 scalar rows and 6 authored GPU profiles",
                            text.splitlines()[-1]), "native fog state completion missing")
        report.update(status="PASS", gpu_profiles=6, gpu_pixels_compared=30)
    finally:
        report["input_after_sha256"] = {str(p.resolve()):digest(p) for p in inputs}
        same=before==report["input_after_sha256"]
        if not same: report["status"]="FAIL"
        (out / "run.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
        ensure(same,"fog state replay inputs changed")
    print("Source PX fog state: 48 state rows and 30 exact pixel rows; "+str(out/"run.json"))


if __name__ == "__main__":
    main()
