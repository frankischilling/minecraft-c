"""Run public native redstone contracts in newly owned temporary worlds."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--scenario", default="world-redstone",
                        choices=("world-redstone", "world-redstone-multichunk"))
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="minecraft-c-redstone-") as directory:
        result = subprocess.run([str(executable), args.scenario, directory.encode("utf-8").hex()],
                                stdin=subprocess.DEVNULL, timeout=80)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
