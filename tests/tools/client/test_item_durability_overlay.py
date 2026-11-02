"""Audit native entered durability rectangles against unchanged original draws."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def snapshot(paths):
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="durability-overlay-", dir=args.work_dir.resolve()))
    reference = ROOT / "tests/reference/client/item_durability_overlay"
    spec = importlib.util.spec_from_file_location("durability_original", reference / "compare.py")
    comparison = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(comparison)
    inputs = [executable, Path(__file__).resolve(),
              ROOT / "tests/client/render/test_inventory_block.c",
              ROOT / "tests/client/render/test_render_blocks_faces.c",
              ROOT / "src/client/render/item_overlay.c",
              ROOT / "src/client/render/item_overlay.h"]
    inputs += [path for path in reference.rglob("*") if path.is_file()]
    before = snapshot(inputs)
    result = subprocess.run([str(executable)], capture_output=True, timeout=120)
    (output / "stdout.txt").write_bytes(result.stdout)
    (output / "stderr.txt").write_bytes(result.stderr)
    report = {"status": "FAIL", "exit_code": result.returncode,
              "inputs_before": before, "inputs_after": snapshot(inputs)}
    try:
        if result.returncode or b"Inventory Block controls:" not in result.stdout:
            print("\n".join(line for line in result.stdout.decode("utf-8").splitlines()
                            if not line.startswith("ITEM_DURABILITY ")))
            print(result.stderr.decode("utf-8"), end="", file=sys.stderr)
            raise RuntimeError(f"Native durability overlay executable failed: {output}")
        if report["inputs_after"] != before:
            raise RuntimeError("Selected durability execution inputs changed during run")
        expected = json.loads((reference / "finite.json").read_text(encoding="utf-8"),
                              object_pairs_hook=comparison.unique)["rows"]
        comparison.compare(comparison.rows(result.stdout.decode("utf-8")), expected)
        report.update(status="PASS", profiles=9, entered_rectangles=24,
                      active_words=576, completed_native_draws=0,
                      independent_inactive_backing_audits=True)
    finally:
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                         encoding="utf-8", newline="\n")
    print("Durability overlay:9 original profiles;24 entered rectangles;576 exact active words")
    print("Each process's inactive backing audited; native CPU component executes no GL draw")


if __name__ == "__main__":
    main()
