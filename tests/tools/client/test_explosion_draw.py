"""Run genuine GY/XW CPU sprite calls over actual World and NW owners."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="explosion-draw-", dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / "reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("explosion_draw_resources", support_path)
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
    print(child.stdout.decode("utf-8"), end="")
    print(child.stderr.decode("utf-8"), end="", file=sys.stderr)
    if child.returncode or b"six actual XW/NW CPU profiles" not in child.stdout:
        raise RuntimeError(f"Explosion draw exited {child.returncode}: {output}")
    reference = Path(__file__).resolve().parents[2] / "reference/entity/explosion_particle"
    spec = importlib.util.spec_from_file_location("explosion_draw_comparison", reference / "compare.py")
    comparison = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(comparison)
    expected = json.loads((reference / "finite.json").read_text(encoding="utf-8"))["draws"]
    comparison.compare(comparison.draw_rows(child.stdout.decode("utf-8")), expected)
    print("Explosion sprite: six exact unchanged-original NW rows, including all 48 words")


if __name__ == "__main__":
    main()
