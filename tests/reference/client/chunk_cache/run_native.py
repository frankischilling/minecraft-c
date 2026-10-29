"""Replay canonical EW and compare every unchanged-original finite row."""
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
    out = Path(tempfile.mkdtemp(prefix="chunk-cache-", dir=args.work_dir.resolve()))
    picking_path = HERE.parents[2] / "tools/client/test_picking.py"
    picking = module("ew_native_authored_inputs", picking_path)
    for name, data in picking.RESOURCES.items():
        path = out / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    picking.authored_world(out / "world")
    resource_before = inventory(out / "resources")
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "original EW finite inventory")
    inputs = [executable,picking_path,HERE / "run_native.py",HERE / "capture_reference.py",
              HERE / "finite.json",HERE / "source-pins.json",HERE / "ChunkCacheObservation.java",
              HERE.parent / "controller/capture.py",HERE.parent / "controller/classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()):digest(p) for p in inputs}
    report = {"status":"FAIL","input_before_sha256":before,"resource_before_sha256":resource_before}
    try:
        receipt = capture([str(executable),str(out / "resources"),str(out / "world")],out,"native",
                          100,os.environ.copy(),out.resolve())
        report["native"] = receipt
        require_success(receipt,empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"Source EW: [1-9][0-9]* checks; 950 original rows and canonical leases",
                            text.splitlines()[-1]), "native EW normal completion")
        actual = rows(text)
        report["actual_rows"] = actual
        report["mismatches"] = [{"index":i,"native":a,"original":b}
                                for i,(a,b) in enumerate(zip(actual,expected)) if a != b]
        ensure(actual == expected,"native/original EW finite rows differ")
        report.update(status="PASS",rows=950,
                      scope="actual cached Chunk identities/leases, whole ew constructor/accessors, no World reacquisition in cache reads, real lazy Chest; no CV/DK geometry claim")
    finally:
        report["input_after_sha256"] = {str(p.resolve()):digest(p) for p in inputs}
        report["resource_after_sha256"] = inventory(out / "resources")
        same = before == report["input_after_sha256"] and resource_before == report["resource_after_sha256"]
        if not same: report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
        ensure(same,"native EW inputs/resources changed")
    print("Source EW: 950 exact original rows and canonical lease controls; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
