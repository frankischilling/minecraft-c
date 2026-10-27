"""Create authored resources and exercise genuine sj, image and GL/list owners."""
import argparse
from pathlib import Path
import struct
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tests/reference/client/render_resources"))
from capture import native, write_json
from protocol import fingerprint, require


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)


def resources(root):
    (root / "font").mkdir(parents=True)
    # A genuine independent white128x128RGBA PNG. These bytes are authored
    # input, not original pixels or captured source widths/list answers.
    pixels = (b"\x00" + b"\xff\xff\xff\xff" * 128) * 128
    image = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 128, 128, 8, 6, 0, 0, 0))
    image += chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b"")
    (root / "font/white.png").write_bytes(image)
    (root / "font.txt").write_bytes(bytes(range(32, 127)) + b"\n")
    (root / "not-image.bin").write_bytes(b"authored input with no recognized ImageIO image\n")
    return {p.relative_to(root).as_posix(): fingerprint(p) for p in sorted(root.rglob("*")) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="source-font-", dir=args.work_dir.resolve()))
    resource_root = work / "resources"
    before = resources(resource_root)
    _, report = native(args.executable, [resource_root], work, ROOT, "font")
    after = {p.relative_to(resource_root).as_posix(): fingerprint(p)
             for p in sorted(resource_root.rglob("*")) if p.is_file()}
    require(after == before, "actual resource files unchanged after complete retirement")
    write_json(work / "report.json", {"status": "PASS", "native": report,
                                      "resources_before": before, "resources_after": after})
    print(work)


if __name__ == "__main__":
    main()
