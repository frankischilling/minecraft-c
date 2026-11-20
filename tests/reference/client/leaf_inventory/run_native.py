"""Compare real CPU Leaf inventory controls with two fresh original captures."""
import argparse
import json
from pathlib import Path
import subprocess
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
    expected = read_packet()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="leaf-inventory-", dir=args.work_dir.resolve()))
    inputs = [executable, Path(__file__).resolve(), HERE / "packet.py", HERE / "provenance.json",
              HERE / "observations.txt", HERE / "source-pins.json",
              ROOT / "tests/client/render/test_inventory_block.c",
              ROOT / "tests/client/render/leaf_inventory_controls.inc",
              ROOT / "src/client/render/blocks/inventory.c",
              ROOT / "src/client/render/blocks/inventory_color.h",
              ROOT / "src/block/registry/rendering.c",
              ROOT / "src/client/render_engine/colorizer.c",
              ROOT / "src/client/render_engine/colorizer.h"]
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "inputs_before": before, "scope": "CPU BK/JH colors and UU.g bounds; no original GL"}
    try:
        child = subprocess.run([str(executable)], capture_output=True, timeout=120)
        (output / "native.stdout").write_bytes(child.stdout)
        (output / "native.stderr").write_bytes(child.stderr)
        if child.returncode:
            raise AssertionError(child.stderr.decode("utf-8", errors="replace"))
        rows = [line for line in child.stdout.decode("utf-8").splitlines()
                if line.startswith(("LEAF_INVENTORY_COLOR ", "LEAF_INVENTORY_SHAPE "))]
        if rows != expected:
            raise AssertionError(f"Leaf inventory differs: native={rows}, original={expected}")
        report.update(status="PASS", rows=len(rows))
    finally:
        report["inputs_after"] = {str(path): digest(path) for path in inputs}
        if report["inputs_after"] != before:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        if report["inputs_after"] != before:
            raise AssertionError("Leaf inventory native inputs changed")
    print("PASS: 17 exact original Leaf inventory CPU rows; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
