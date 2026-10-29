"""Exercise the genuine Windows source loop with isolated empty/populated saves."""

import argparse
import gzip
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def string(value):
    data = value.encode("ascii")
    return struct.pack(">H", len(data)) + data


def tag(kind, key, value):
    return bytes([kind]) + string(key) + value


def metadata(display, played, version):
    data = tag(8, "LevelName", string(display))
    data += tag(4, "LastPlayed", struct.pack(">q", played))
    data += tag(4, "SizeOnDisk", struct.pack(">q", 3145728))
    data += tag(3, "version", struct.pack(">i", version))
    return b"\x0a\x00\x00" + tag(10, "Data", data + b"\x00") + b"\x00"


def snapshot(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*") if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This native APPDATA isolation profile requires Windows")
    executable = args.executable.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    artifacts = args.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    resources_before = snapshot(assets)
    for mode in ("empty", "populated", "large", "double"):
        with tempfile.TemporaryDirectory(prefix="mc-rq-loop-") as work:
            appdata = Path(work)
            game = appdata / ".minecraft"
            saves = game / "saves"
            saves.mkdir(parents=True)
            (saves / "preserve-me.bin").write_bytes(b"unrelated save-directory file\x00\xff")
            if mode == "large":
                worlds = [(f"world{i:02d}", f"Scroll world {i:02d}", 1304000000000, 19132)
                          for i in range(16)]
            elif mode in ("populated", "double"):
                worlds = [
                    ("older", "Old world", 1303000000000, 19132),
                    ("bravo", "", 1304000000000, 19132),
                    ("alpha", "Alpha world", 1304000000000, 0),
                ]
            else:
                worlds = []
            for folder, display, played, version in worlds:
                directory = saves / folder
                directory.mkdir()
                filename = "level.dat_old" if folder == "bravo" else "level.dat"
                (directory / filename).write_bytes(
                    gzip.compress(metadata(display, played, version), mtime=0))
                (directory / "preserve-chunk.dat").write_bytes(b"authored untouched chunk")
            before = snapshot(saves)
            environment = os.environ.copy()
            environment["APPDATA"] = str(appdata)
            capture = artifacts / (mode + ".png")
            result = subprocess.run([str(executable), str(assets), mode, str(capture)],
                                    env=environment, timeout=110, check=False)
            if result.returncode:
                return result.returncode
            if snapshot(saves) != before:
                raise RuntimeError("World-selection loop modified save files or created a lock")
            data = capture.read_bytes()
            if data[:8] != b"\x89PNG\r\n\x1a\n" or len(data) < 4096:
                raise RuntimeError("World-selection capture is missing or incomplete")
            if struct.unpack(">II", data[16:24]) != (854, 480):
                raise RuntimeError("World-selection capture has unexpected native dimensions")
            if not (game / "stats").is_dir() or not (game / "options.txt").is_file():
                raise RuntimeError("The genuine native default game directory was not used")
    if snapshot(assets) != resources_before:
        raise RuntimeError("World-selection loop modified imported source resources")
    print("World-selection loop: original-receiver input, held entry, rows, Cancel, "
          "large-list scrollbar drag, real-clock double-click failure order, "
          "World-load frontier, captures, save preservation and cleanup passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
