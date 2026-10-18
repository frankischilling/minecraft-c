"""Exercise UTF-8 stdin paths in the C region probe without Java or private inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import unittest
import zlib


class RegionProbeTests(unittest.TestCase):
    executable: Path

    def test_utf8_directory_and_filename_preserve_ordered_full_records(self):
        with tempfile.TemporaryDirectory(prefix="mc-region-probe-") as temporary:
            root = Path(temporary).resolve()
            directory = root / "region directory \u03bb \u4e16\u754c"
            directory.mkdir()
            path = directory / "region file \U0001f9a7 \u03bb.mcr"
            data = bytearray(4 * 4096)
            records = {}
            for x, z, sector, timestamp, value in ((31, 0, 2, 0xFFFFFFFF, 17), (0, 1, 3, 0x80000000, 34)):
                raw = b"\x0a\x00\x00\x03\x00\x01n" + struct.pack(">i", value) + b"\0"
                payload = zlib.compress(raw)
                struct.pack_into(">I", data, (x + z * 32) * 4, sector * 256 + 1)
                struct.pack_into(">I", data, 4096 + (x + z * 32) * 4, timestamp)
                struct.pack_into(">I", data, sector * 4096, len(payload) + 1)
                data[sector * 4096 + 4] = 2
                data[sector * 4096 + 5:sector * 4096 + 5 + len(payload)] = payload
                records[(x, z)] = (sector, timestamp, raw, payload)
            path.write_bytes(data)
            before = hashlib.sha256(path.read_bytes()).hexdigest()
            coordinates = ((31, 0), (0, 1), (0, 0))
            requests = "".join(f"{path}\t{x}\t{z}\n" for x, z in coordinates).encode("utf-8")
            self.assertIn(b"\xce\xbb", requests)
            self.assertIn(b"\xf0\x9f\xa6\xa7", requests)
            buffers = [bytearray(), bytearray()]
            exceeded = threading.Event()

            with tempfile.TemporaryFile() as source:
                source.write(requests)
                source.seek(0)
                with subprocess.Popen([str(self.executable)], stdin=source, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, cwd=root) as process:
                    def capture(stream, index, limit):
                        while chunk := stream.read(4096):
                            remaining = limit - len(buffers[index])
                            buffers[index].extend(chunk[:remaining])
                            if len(chunk) > remaining:
                                exceeded.set()
                                process.kill()
                                break

                    threads = [threading.Thread(target=capture, args=(process.stdout, 0, 128 * 1024)),
                               threading.Thread(target=capture, args=(process.stderr, 1, 64 * 1024))]
                    for thread in threads:
                        thread.start()
                    try:
                        process.wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                        self.fail("region probe exceeded its test deadline")
                    finally:
                        for thread in threads:
                            thread.join(timeout=5)
                    self.assertFalse(any(thread.is_alive() for thread in threads), "probe streams did not close")
                    self.assertFalse(exceeded.is_set(), "probe output exceeded the protocol bound")
                    self.assertEqual(process.returncode, 0, bytes(buffers[1]).decode("utf-8", errors="replace"))

            self.assertEqual(bytes(buffers[1]), b"")
            lines = bytes(buffers[0]).decode("ascii").splitlines()
            self.assertEqual(len(lines), len(coordinates))
            for line, coordinate in zip(lines, coordinates):
                expected = {"open_status": "ok", "view": {"length": len(data), "trailing_bytes": 0,
                            "present_entries": 2, "invalid_entries": 0},
                            "locations": data[:4096].hex(), "timestamps": data[4096:8192].hex(),
                            "entry": {"location": 0, "timestamp": 0, "issues": 0, "record_length": 0,
                                      "byte_offset": 0, "extent_bytes": 0, "selector": 0},
                            "record_status": "missing region entry", "payload_hex": None,
                            "compression_status": None, "compressed_consumed": None, "compressed_trailing_bytes": None,
                            "raw_hex": None, "nbt_status": None, "nbt_consumed": None,
                            "raw_trailing_bytes": None, "canonical_hex": None}
                if coordinate in records:
                    sector, timestamp, raw, payload = records[coordinate]
                    expected.update(entry={"location": sector * 256 + 1, "timestamp": timestamp, "issues": 0,
                                           "record_length": len(payload) + 1, "byte_offset": sector * 4096,
                                           "extent_bytes": 4096, "selector": 2},
                                    record_status="ok", payload_hex=payload.hex(), compression_status="ok",
                                    compressed_consumed=len(payload), compressed_trailing_bytes=0,
                                    raw_hex=raw.hex(), nbt_status="ok", nbt_consumed=len(raw),
                                    raw_trailing_bytes=0, canonical_hex=raw.hex())
                self.assertEqual(json.loads(line), expected)
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)
            self.assertEqual(set(root.rglob("*")), {directory, path})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    arguments = parser.parse_args()
    RegionProbeTests.executable = arguments.executable.resolve()
    if not RegionProbeTests.executable.is_file():
        parser.error("--executable must name the built mc_region_probe program")
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(RegionProbeTests))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
