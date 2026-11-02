"""Run actual Player attack, animal death and Wolf alert ownership controls."""
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
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="attack-", dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / "reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("attack_statistics_resources", support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / "resources"
    support.resources_at(resources)
    game = output / "game"
    game.mkdir()
    result = subprocess.run([str(executable), str(resources), str(game)], capture_output=True,
                            timeout=180)
    (output / "stdout.txt").write_bytes(result.stdout)
    (output / "stderr.txt").write_bytes(result.stderr)
    print(result.stdout.decode("utf-8"), end="")
    print(result.stderr.decode("utf-8"), end="", file=sys.stderr)
    if result.returncode or b"Player attack source-order/ownership checks" not in result.stdout:
        raise RuntimeError(f"Native Player attack exited {result.returncode}: {output}")
    reference = Path(__file__).resolve().parents[2] / "reference/entity/player_attack"
    compare_spec = importlib.util.spec_from_file_location("player_attack_comparison", reference / "compare.py")
    comparison = importlib.util.module_from_spec(compare_spec)
    compare_spec.loader.exec_module(comparison)
    expected = json.loads((reference / "finite.json").read_text(encoding="utf-8"))["rows"]
    comparison.compare(comparison.rows(result.stdout.decode("utf-8")), expected)
    print("Player attack: five exact selected unchanged-original CPU rows; independent entropy")


if __name__ == "__main__":
    main()
