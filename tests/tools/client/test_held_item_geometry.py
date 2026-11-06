"""Run actual RA entered geometry groups and canonical Player icon selectors."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="held-item-geometry-", dir=args.work_dir.resolve()))
    spec = importlib.util.spec_from_file_location("held_geometry_resources",
        ROOT / "tests/reference/client/player_statistics/support.py")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / "resources"
    support.resources_at(resources)
    game = output / "game"
    game.mkdir()
    child = subprocess.run([str(args.executable.resolve(strict=True)), str(resources), str(game)],
                           capture_output=True, timeout=180)
    (output / "stdout.txt").write_bytes(child.stdout)
    (output / "stderr.txt").write_bytes(child.stderr)
    stdout = child.stdout.decode("utf-8")
    print(stdout, end="")
    print(child.stderr.decode("utf-8"), end="", file=sys.stderr)
    if child.returncode or "36 exact source groups; no Display" not in stdout:
        raise RuntimeError(f"Held Item geometry executable failed: {output}")
    reference = ROOT / "tests/reference/client/held_item_geometry"
    spec = importlib.util.spec_from_file_location("held_geometry_original", reference / "compare.py")
    comparison = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(comparison)
    comparison.compare(comparison.rows(stdout), json.loads((reference / "finite.json").read_text(
        encoding="utf-8"), object_pairs_hook=comparison.unique)["rows"])
    print("Held Item geometry:19008 actual native NW words match direct genuine-owner original Sword/Bow authority")


if __name__ == "__main__":
    main()
