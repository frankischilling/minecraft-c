"""Compare real singleplayer Block harvests with the unchanged original corpus."""
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
    root = Path(__file__).resolve().parents[3]
    packet = root / "tests/reference/block/harvest"
    # The packet reader binds both selected inputs and original observations
    # before either is used to drive or judge the actual controller calls.
    spec = importlib.util.spec_from_file_location("block_harvest_packet", packet / "packet.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    corpus = module.read_packet(packet)
    controller = [row for row in corpus["profiles"] if row["mode"] == 1]
    expected = [line for row in controller for line in corpus["by_profile"][row["index"]]]
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="block-harvest-", dir=args.work_dir.resolve()))
    support_path = root / "tests/reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("block_harvest_resources", support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / "resources"
    support.resources_at(resources)
    observations = []

    columns = ("index", "mode", "block", "metadata", "remote", "held_id", "held_count",
               "held_damage", "below_id", "world_seed", "math_seed", "fault")

    def run(row):
        fields = [str(row[key]) for key in columns]
        game = output / ("game-" + fields[0])
        game.mkdir()
        child = subprocess.run([str(args.executable.resolve(strict=True)), str(resources), str(game),
                                *fields], capture_output=True, timeout=120)
        (output / (fields[0] + ".stdout")).write_bytes(child.stdout)
        (output / (fields[0] + ".stderr")).write_bytes(child.stderr)
        lines = child.stdout.decode("utf-8").splitlines()
        if child.returncode or not any(line.endswith("concrete Block harvest checks") for line in lines):
            raise AssertionError("Block harvest profile " + fields[0] + " failed: " +
                                 child.stderr.decode("utf-8"))
        return [line for line in lines if line.split("\t", 1)[0] in ("RESULT", "ITEM", "EVENT")]

    for row in controller:
        actual = run(row)
        profile_expected = corpus["by_profile"][row["index"]]
        if actual != profile_expected:
            raise AssertionError(f"Block harvest differs for {row}:\nnative={actual}\noriginal={profile_expected}")
        observations.extend(actual)
    if observations != expected:
        raise AssertionError("Block harvest original profile ordering differs")
    controls = [
        "PROFILE\t1000\t1\t79\t0\t0\t-1\t0\t0\t1\t0\t911\t4",
        "PROFILE\t1001\t1\t79\t0\t0\t-1\t0\t0\t1\t0\t911\t5",
        "PROFILE\t1002\t1\t78\t0\t0\t269\t1\t0\t1\t0\t911\t6",
        "PROFILE\t1003\t1\t78\t0\t0\t269\t1\t0\t1\t0\t911\t7",
    ]
    for row in controls:
        run(dict(zip(columns, map(int, row.split("\t")[1:]))))
    print(f"PASS: {len(controller)} exact original controller harvest profiles, "
          f"{len(observations)} rows, and four native failure-prefix controls")


if __name__ == "__main__":
    main()
