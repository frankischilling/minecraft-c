"""Exercise genuine held-tool destruction through the singleplayer controller."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="mining-tools-", dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / "reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("mining_tools_resources", support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / "resources"
    support.resources_at(resources)
    observations = []
    for profile in range(6):
        game = output / ("game-" + str(profile))
        game.mkdir()
        child = subprocess.run([str(args.executable.resolve(strict=True)), str(resources),
                                str(game), str(profile)], capture_output=True, timeout=120)
        (output / (f"{profile}.stdout")).write_bytes(child.stdout)
        (output / (f"{profile}.stderr")).write_bytes(child.stderr)
        lines = child.stdout.decode("utf-8").splitlines()
        if child.returncode or not any(line.endswith("held-tool controller destruction checks")
                                       for line in lines):
            raise AssertionError("held-tool controller failed: " + child.stderr.decode("utf-8"))
        observations.extend(line for line in lines if line.startswith("CONTROLLER_DESTROY\t"))
        print(lines[-1])
    packet = Path(__file__).resolve().parents[2] / "reference/item/mining"
    sys.path.insert(0, str(packet))
    from packet import read_destroy_packet
    _, expected = read_destroy_packet(packet)
    if observations != expected:
        raise AssertionError(f"controller destruction differs: native={observations}, original={expected}")
    print("PASS: six exact original held-tool controller destruction rows")


if __name__ == "__main__":
    main()
