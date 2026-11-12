"""Replay entered whole AO and compare every unchanged-original field/vertex row."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import module, inventory, rows
from capture import capture, digest, ensure, require_success


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="render-blocks-ao-", dir=args.work_dir.resolve()))
    picking_path = HERE.parents[2] / "tools/client/test_picking.py"
    picking = module("ao_native_authored_inputs", picking_path)
    for name, data in picking.RESOURCES.items():
        path = out / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    picking.authored_world(out / "world")
    resource_before = inventory(out / "resources")
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "original AO finite inventory")
    inputs = [executable,picking_path,HERE / "run_native.py",HERE / "capture_reference.py",
              HERE / "finite.json",HERE / "source-pins.json",HERE / "profiles.txt",HERE / "AmbientOcclusionObservation.java",
              HERE.parent / "controller/capture.py",HERE.parent / "controller/classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()):digest(p) for p in inputs}
    report = {"status":"FAIL","input_before_sha256":before,"resource_before_sha256":resource_before}
    try:
        receipt = capture([str(executable),str(out / "resources"),str(out / "world"),str((HERE / "profiles.txt").resolve())],out,"native",
                          100,os.environ.copy(),out.resolve())
        report["native"] = receipt
        require_success(receipt,empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"Source AO: [1-9][0-9]* checks; 48 original whole-body rows, retained fields and NW vertices",
                            text.splitlines()[-1]), "native AO normal completion")
        actual = rows(text)
        report["actual_rows"] = actual
        report["mismatches"] = [{"index":i,"native":a,"original":b}
                                for i,(a,b) in enumerate(zip(actual,expected)) if a != b]
        if actual != expected:
            row = next(i for i,pair in enumerate(zip(actual,expected)) if pair[0] != pair[1])
            native_cells,original_cells=actual[row].split(),expected[row].split()
            cell=next((i for i,pair in enumerate(zip(native_cells,original_cells)) if pair[0]!=pair[1]),min(len(native_cells),len(original_cells)))
            ensure(False,f"native/original AO row{row} cell{cell} differs: native "
                   f"{native_cells[cell] if cell<len(native_cells) else '<missing>'}; original "
                   f"{original_cells[cell] if cell<len(original_cells) else '<missing>'}")
        report.update(status="PASS",rows=48,
                      scope="whole actual AO body, SAME canonical CV scratch/flags/colors and Block/EW/NW vertices; prepared finite operands, no draw/GL/full terrain frame claim")
    finally:
        report["input_after_sha256"] = {str(p.resolve()):digest(p) for p in inputs}
        report["resource_after_sha256"] = inventory(out / "resources")
        same = before == report["input_after_sha256"] and resource_before == report["resource_after_sha256"]
        if not same: report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
        ensure(same,"native AO inputs/resources changed")
    print("Source AO: 48 exact original field/vertex rows; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
