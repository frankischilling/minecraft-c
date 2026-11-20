"""Run the real Windows RenderGlobal control with a private game directory."""
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
        raise RuntimeError("This APPDATA isolation profile requires Windows")
    with tempfile.TemporaryDirectory(prefix="mc-render-global-") as directory:
        environment = os.environ.copy()
        environment["APPDATA"] = directory
        result = subprocess.run([str(args.executable.resolve()), str(args.resources.resolve()),
                                 str(args.assets.resolve())], env=environment, timeout=230,
                                check=False)
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
