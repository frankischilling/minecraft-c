"""Compare real Registry inventory families and retained Leaf controls to originals."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from packet import digest, read_packet

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    expected = read_packet()["native_rows"]
    leaf = HERE.parent / "leaf_inventory"
    spec = importlib.util.spec_from_file_location("retained_leaf_inventory_packet", leaf / "packet.py")
    leaf_packet = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = leaf_packet
    spec.loader.exec_module(leaf_packet)
    leaf_expected = leaf_packet.read_packet()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="block-inventory-", dir=args.work_dir.resolve()))
    inputs = [executable, Path(__file__).resolve(), HERE / "packet.py", HERE / "provenance.json",
              HERE / "observations.tsv", HERE / "source-pins.json", HERE / "inputs.json", HERE / "guards.tsv",
              leaf / "packet.py", leaf / "provenance.json", leaf / "observations.txt", leaf / "source-pins.json",
              ROOT / "tests/client/render/test_inventory_block.c", ROOT / "tests/client/render/inventory_family_controls.inc",
              ROOT / "tests/client/render/leaf_inventory_controls.inc", ROOT / "src/block/registry/rendering.c",
              ROOT / "include/mc/block/registry/rendering.h", ROOT / "src/client/render/blocks/inventory.c",
              ROOT / "src/client/render/blocks/inventory_color.h", ROOT / "src/client/render_engine/colorizer.c",
              ROOT / "src/client/render_engine/colorizer.h"]
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "inputs_before": before, "scope": "CPU actual Registry inventory virtuals; no original GL"}
    try:
        child = subprocess.run([str(executable)], capture_output=True, timeout=120)
        (output / "native.stdout").write_bytes(child.stdout)
        (output / "native.stderr").write_bytes(child.stderr)
        if child.returncode:
            raise AssertionError(child.stderr.decode("utf-8", errors="replace"))
        all_lines = child.stdout.decode("utf-8").splitlines()
        actual = [line for line in all_lines if line.startswith(("BLOCK\t", "COLOR\t", "SHAPE\t", "TEXTURE\t", "TEXTURE_LATE\t"))]
        leaf_actual = [line for line in all_lines if line.startswith(("LEAF_INVENTORY_COLOR ", "LEAF_INVENTORY_SHAPE "))]
        if actual != expected:
            first = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]), min(len(actual), len(expected)))
            raise AssertionError(f"Block inventory differs at row {first}: native={actual[first:first+1]}, original={expected[first:first+1]}; lengths={len(actual)},{len(expected)}")
        if leaf_actual != leaf_expected:
            raise AssertionError("retained Leaf inventory original rows differ")
        report.update(status="PASS", rows=len(actual), retained_leaf_rows=len(leaf_actual))
    finally:
        report["inputs_after"] = {str(path): digest(path) for path in inputs}
        if report["inputs_after"] != before:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        if report["inputs_after"] != before:
            raise AssertionError("Block inventory native inputs changed")
    print(f"PASS: {len(expected)} exact original Block inventory rows and {len(leaf_expected)} retained Leaf rows; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
