"""Run actual GY construction, tick and retained DN ownership controls."""
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
    evidence = Path(tempfile.mkdtemp(prefix="explosion-", dir=args.work_dir.resolve()))
    path = Path(__file__).resolve().parents[2] / "reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("explosion_authored_resources", path)
    resources = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(resources)
    resources.resources_at(evidence / "resources")
    (evidence / "game").mkdir()
    result = subprocess.run([str(executable), str(evidence / "resources"), str(evidence / "game")],
                            capture_output=True, timeout=180)
    (evidence / "stdout.txt").write_bytes(result.stdout)
    (evidence / "stderr.txt").write_bytes(result.stderr)
    print(result.stdout.decode("utf-8"), end="")
    print(result.stderr.decode("utf-8"), end="", file=sys.stderr)
    if result.returncode or b"Explosion particle source-order/ownership checks" not in result.stdout:
        raise RuntimeError(f"Explosion particle native exit {result.returncode}: {evidence}")
    reference = Path(__file__).resolve().parents[2] / "reference/entity/explosion_particle"
    compare_spec = importlib.util.spec_from_file_location("explosion_original_comparison", reference / "compare.py")
    comparison = importlib.util.module_from_spec(compare_spec)
    compare_spec.loader.exec_module(comparison)
    expected = json.loads((reference / "finite.json").read_text(encoding="utf-8"))["ticks"]
    comparison.compare(comparison.tick_rows(result.stdout.decode("utf-8")), expected)
    print("Explosion particle: seven exact unchanged-original tick rows; independent constructor entropy")


if __name__ == "__main__":
    main()
