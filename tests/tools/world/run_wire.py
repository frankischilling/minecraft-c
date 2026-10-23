"""Run public native wire contracts in newly owned temporary worlds."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="minecraft-c-wire-") as directory:
        result = subprocess.run([str(executable), "world-wire", directory.encode("utf-8").hex()],
                                stdin=subprocess.DEVNULL, timeout=80)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
