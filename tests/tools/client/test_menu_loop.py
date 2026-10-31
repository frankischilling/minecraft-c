"""Exercise the real Windows menu with an isolated native game directory."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This APPDATA isolation profile requires Windows")
    with tempfile.TemporaryDirectory(prefix="mc-source-menu-") as directory:
        environment = os.environ.copy()
        environment["APPDATA"] = directory
        assets = str(args.assets.resolve())
        subprocess.run([str(args.executable.resolve()), assets], env=environment,
                       timeout=100, check=True)
        capture = Path(directory) / "source-menu.png"
        subprocess.run([str(args.client.resolve()), "--assets", assets,
                        "--hidden", "--frames", "3", "--capture", str(capture)],
                       env=environment, timeout=100, check=True)
        data = capture.read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n" and len(data) > 4096
        assert struct.unpack(">II", data[16:24]) == (854, 480)
        assert (Path(directory) / ".minecraft" / "stats").is_dir()
    print("Source menu input, FPS, capture, native default directory and teardown passed")


if __name__ == "__main__":
    main()
