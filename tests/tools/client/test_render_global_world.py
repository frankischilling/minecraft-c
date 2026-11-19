"""Run genuine n/DK device owners inside private native game/World paths."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resources", type=Path, required=True)
    assets = parser.add_mutually_exclusive_group(required=True)
    assets.add_argument("--assets", type=Path)
    assets.add_argument("--authored-assets", action="store_true")
    parser.add_argument("--rebuild-budget", action="store_true")
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This native APPDATA isolation profile requires Windows")
    with tempfile.TemporaryDirectory(prefix="mc-render-world-") as directory:
        root = Path(directory)
        environment = os.environ.copy()
        environment["APPDATA"] = str(root / "appdata")
        (root / "appdata").mkdir()
        resource_assets = args.assets
        if args.authored_assets:
            benchmark = Path(__file__).resolve().parents[3] / "scripts/benchmarks"
            sys.path.insert(0, str(benchmark))
            from client_frames import author_frame_assets

            resource_assets = root / "assets"
            author_frame_assets(resource_assets)
            environment["ALSOFT_DRIVERS"] = "null"
        command = [str(args.executable.resolve()), str(args.resources.resolve()),
                   str(resource_assets.resolve()), str(root / "world")]
        if args.rebuild_budget:
            command.append("--rebuild-budget")
        return subprocess.run(
            command,
            env=environment, timeout=230, check=False,
        ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
