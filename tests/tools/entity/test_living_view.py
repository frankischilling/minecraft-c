"""Run actual LS view owners on authored resources in an owned temporary World."""
import argparse
from pathlib import Path
import subprocess
import tempfile
from test_player_construction import RESOURCES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--vectors", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mc-living-view-") as directory:
        root = Path(directory)
        for name, data in RESOURCES.items():
            path = root / "resources" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return subprocess.run([str(args.executable.resolve()), str(root / "resources"),
                               str(root / "world"), str(args.vectors.resolve())],
                              stdin=subprocess.DEVNULL, timeout=90, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
