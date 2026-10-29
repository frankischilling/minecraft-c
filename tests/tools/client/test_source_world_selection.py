"""Exercise canonical RQ with real empty and populated SaveFormat directories."""

import argparse
import gzip
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def string(value):
    raw = value.encode("ascii")
    return struct.pack(">H", len(raw)) + raw


def tag(kind, key, value):
    return bytes([kind]) + string(key) + value


def metadata(display, played, version):
    data = tag(8, "LevelName", string(display))
    data += tag(4, "LastPlayed", struct.pack(">q", played))
    data += tag(4, "SizeOnDisk", struct.pack(">q", 1048576))
    data += tag(3, "version", struct.pack(">i", version))
    return b"\x0a\x00\x00" + tag(10, "Data", data + b"\x00") + b"\x00"


def snapshot(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*") if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    env = dict(os.environ)
    if os.name != "nt":
        env["LC_ALL"] = "en_US.UTF-8"
        env["TZ"] = "UTC"
    with tempfile.TemporaryDirectory(prefix="minecraft-rq-") as work:
        for mode in ("empty", "populated"):
            root = Path(work) / mode
            (root / "saves").mkdir(parents=True)
            (root / "lang").mkdir()
            (root / "lang/en_US.lang").write_text(
                "selectWorld.title=Choose save\nselectWorld.world=Realm\n"
                "selectWorld.conversion=Convert\nselectWorld.select=Load\n"
                "selectWorld.rename=Rename\nselectWorld.delete=Delete\n"
                "selectWorld.create=New\ngui.cancel=Back\n", encoding="ascii")
            (root / "lang/stats_US.lang").write_text("# empty labels\n", encoding="ascii")
            if mode == "populated":
                for folder, display, played, version in (
                    ("older", "Old", 1000, 19132),
                    ("bravo", "", 2000, 19132),
                    ("alpha", "Alpha", 2000, 0),
                ):
                    directory = root / "saves" / folder
                    directory.mkdir()
                    filename = "level.dat_old" if folder == "bravo" else "level.dat"
                    (directory / filename).write_bytes(
                        gzip.compress(metadata(display, played, version), mtime=0))
            before = snapshot(root)
            result = subprocess.run([str(executable), str(root), mode],
                                    env=env, timeout=150, check=False)
            if result.returncode:
                return result.returncode
            if snapshot(root) != before:
                raise RuntimeError("RQ inspection modified the save/resource files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
