"""Run authored screen resource controls in a fresh temporary directory."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="minecraft-screen-") as work:
        result = subprocess.run([str(executable), "resource", work], timeout=150, check=False)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
