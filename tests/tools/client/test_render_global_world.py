"""Run genuine n/DK device owners inside private native game/World paths."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This native APPDATA isolation profile requires Windows")
    with tempfile.TemporaryDirectory(prefix="mc-render-world-") as directory:
        root = Path(directory)
        environment = os.environ.copy()
        environment["APPDATA"] = str(root / "appdata")
        (root / "appdata").mkdir()
        return subprocess.run(
            [str(args.executable.resolve()), str(args.resources.resolve()),
             str(args.assets.resolve()), str(root / "world")],
            env=environment, timeout=230, check=False,
        ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
