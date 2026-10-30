"""Run precipitation against authored assets and disposable original-format chunks."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib


def png():
    def part(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    return (b"\x89PNG\r\n\x1a\n" + part(b"IHDR", struct.pack(">IIBBBBB", 16, 16, 8, 6, 0, 0, 0))
            + part(b"IDAT", zlib.compress((b"\0" + b"\xff\xff\xff\xff" * 16) * 16))
            + part(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="precipitation-", dir=args.work_dir.resolve()))
    spec = importlib.util.spec_from_file_location("precipitation_worlds", Path(__file__).with_name("test_picking.py"))
    fixture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fixture)
    resources = dict(fixture.RESOURCES)
    for name in ("pack.png", "gui/unknown_pack.png", "environment/snow.png", "environment/rain.png"):
        resources[name] = png()
    for name, data in resources.items():
        path = root / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    fixture.authored_world(root / "world")
    result = subprocess.run([str(args.executable.resolve(strict=True)), str(root / "resources"),
                             str(root / "world")], env=os.environ.copy(), capture_output=True,
                            text=True, timeout=100)
    (root / "stdout.txt").write_text(result.stdout)
    (root / "stderr.txt").write_text(result.stderr)
    print(result.stdout, end="")
    if result.returncode or result.stderr or "checks passed" not in result.stdout:
        raise AssertionError(f"precipitation failed ({result.returncode}): {result.stderr}; {root}")
    reference = Path(__file__).resolve().parents[2] / "reference/client/precipitation/finite.json"
    expected = json.loads(reference.read_text())
    rows = [row for row in result.stdout.splitlines() if row.startswith("PRECIP_PREFIX ")]
    if expected["jar_sha256"] != fixture.JAR or rows != expected["rows"] or len(rows) != 100:
        raise AssertionError("precipitation original/native prefix rows differ")
    print(f"Precipitation fixture: {root}")


if __name__ == "__main__":
    main()
