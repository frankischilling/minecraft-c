"""Exercise the read-only region CLI with small, generated public fixtures."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import stat
import struct
import subprocess
import tempfile
import threading
import unittest
import zlib


SECTOR_BYTES = 4096
HEADER_BYTES = 8192
ROOT = b"\x0a\x00\x00\x03\x00\x01n\x00\x00\x00\x11\x00"
MAX_STDOUT = 1024 * 1024
MAX_STDERR = 64 * 1024


def compressed(payload: bytes, selector: int) -> bytes:
    return gzip.compress(payload, mtime=0) if selector == 1 else zlib.compress(payload)


def set_location(data: bytearray, x: int, z: int, location: int, timestamp: int = 0) -> None:
    index = x + 32 * z
    struct.pack_into(">I", data, index * 4, location)
    struct.pack_into(">I", data, SECTOR_BYTES + index * 4, timestamp)


def region(records: list[tuple[int, int, int, bytes, int]]) -> bytes:
    """Each small test record gets one sector following the two header sectors."""
    data = bytearray(HEADER_BYTES + len(records) * SECTOR_BYTES)
    for index, (x, z, selector, payload, timestamp) in enumerate(records, start=2):
        if len(payload) + 5 > SECTOR_BYTES:
            raise ValueError("test record must fit in one sector")
        set_location(data, x, z, (index << 8) | 1, timestamp)
        offset = index * SECTOR_BYTES
        struct.pack_into(">I", data, offset, len(payload) + 1)
        data[offset + 4] = selector
        data[offset + 5:offset + 5 + len(payload)] = payload
    return bytes(data)


class RegionInspectTests(unittest.TestCase):
    executable: Path

    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="mc-region-cli-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def fixture(self, data: bytes, name: str = "fixture.mcr") -> Path:
        path = self.root / name
        path.write_bytes(data)
        return path

    def snapshot(self) -> dict[str, tuple[int, str] | None]:
        result: dict[str, tuple[int, str] | None] = {}
        for path in self.root.rglob("*"):
            relative = path.relative_to(self.root).as_posix()
            if path.is_dir():
                result[relative] = None
            else:
                data = path.read_bytes()
                result[relative] = (len(data), hashlib.sha256(data).hexdigest())
        return result

    def invoke(self, *arguments: object, code: int = 0) -> tuple[str, str]:
        """Bound both captured streams and verify all fixture files stay unchanged."""
        before = self.snapshot()
        buffers = [bytearray(), bytearray()]
        exceeded = threading.Event()
        timed_out = False
        try:
            process = subprocess.Popen(
                [str(self.executable), *(str(argument) for argument in arguments)],
                cwd=self.root, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            )

            def capture(stream, index: int, limit: int) -> None:
                try:
                    while data := stream.read(4096):
                        remaining = limit - len(buffers[index])
                        buffers[index].extend(data[:remaining])
                        if len(data) > remaining:
                            exceeded.set()
                            try:
                                process.kill()
                            except OSError:
                                pass
                            break
                finally:
                    stream.close()

            readers = [
                threading.Thread(target=capture, args=(process.stdout, 0, MAX_STDOUT), daemon=True),
                threading.Thread(target=capture, args=(process.stderr, 1, MAX_STDERR), daemon=True),
            ]
            for reader in readers:
                reader.start()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                timed_out = True
                process.kill()
                process.wait(timeout=5)
            finally:
                for reader in readers:
                    reader.join(timeout=5)
            self.assertFalse(any(reader.is_alive() for reader in readers), "output pipe did not close")
            self.assertFalse(timed_out, "CLI exceeded its 10-second test timeout")
            self.assertFalse(exceeded.is_set(), "CLI exceeded bounded output capture")
            stdout, stderr = (bytes(value).decode("utf-8", errors="strict") for value in buffers)
            self.assertEqual(process.returncode, code, f"stdout={stdout[:2000]!r}; stderr={stderr[:2000]!r}")
            return stdout, stderr
        finally:
            self.assertEqual(self.snapshot(), before, "CLI changed fixture contents or created paths")

    def inspect(self, *arguments: object, code: int = 0) -> dict:
        stdout, _ = self.invoke(*arguments, code=code)
        value = json.loads(stdout)
        self.assertIsInstance(value, dict)
        return value

    def assert_selected_ok(self, document: dict, selector: int, payload: bytes) -> dict:
        self.assertEqual(document["status"], "ok")
        self.assertEqual(document["file_bytes"], 12288)
        self.assertEqual(document["trailing_bytes"], 0)
        self.assertEqual(document["present_entries"], 1)
        self.assertEqual(document["invalid_entries"], 0)
        self.assertEqual(len(document["entries"]), 1)
        entry = document["entries"][0]
        self.assertEqual((entry["x"], entry["z"]), (0, 0))
        self.assertEqual(entry["location"], 513)
        self.assertEqual(entry["offset"], 8192)
        self.assertEqual(entry["extent_bytes"], 4096)
        self.assertEqual(entry["issues"], 0)
        self.assertEqual(entry["record_length"], len(payload) + 1)
        self.assertEqual(entry["selector"], selector)
        self.assertEqual(entry["record_status"], "ok")
        self.assertEqual(entry["compression"], {
            "status": "ok", "consumed": len(payload), "trailing_bytes": 0,
            "output_bytes": len(ROOT),
        })
        nbt = entry["nbt"]
        self.assertEqual(nbt["status"], "ok")
        self.assertEqual(nbt["consumed"], len(ROOT))
        self.assertEqual(nbt["trailing_bytes"], 0)
        self.assertIsNone(nbt["error_offset"])
        self.assertEqual(nbt["root"]["type"], 10)
        self.assertEqual(nbt["root"]["name_utf16"], [])
        self.assertEqual(nbt["root"]["name_length"], 0)
        self.assertFalse(nbt["root"]["name_truncated"])
        self.assertEqual(nbt["root"]["child_count"], 1)
        self.assertEqual(nbt["root"]["children"][0]["type"], 3)
        self.assertEqual(nbt["root"]["children"][0]["name_utf16"], [110])
        return entry

    def test_selected_gzip_record_composes_metadata_decompression_and_nbt(self) -> None:
        payload = compressed(ROOT, 1)
        path = self.fixture(region([(0, 0, 1, payload, 0xFFFFFFFF)]))
        entry = self.assert_selected_ok(self.inspect(path, 0, 0), 1, payload)
        self.assertEqual(entry["timestamp"], 4294967295)

    def test_selected_zlib_record_uses_the_same_nbt_boundary(self) -> None:
        payload = compressed(ROOT, 2)
        path = self.fixture(region([(0, 0, 2, payload, 17)]))
        entry = self.assert_selected_ok(self.inspect(path, 0, 0), 2, payload)
        self.assertEqual(entry["timestamp"], 17)

    def test_metadata_listing_does_not_decode_record_contents(self) -> None:
        path = self.fixture(region([(2, 5, 1, b"not compressed data", 7)]))
        document = self.inspect(path)
        self.assertEqual(document["status"], "ok")
        self.assertEqual(document["present_entries"], 1)
        entry = document["entries"][0]
        self.assertEqual((entry["x"], entry["z"]), (2, 5))
        self.assertEqual(entry["record_status"], "ok")
        self.assertNotIn("compression", entry)
        self.assertNotIn("nbt", entry)

    def test_empty_default_listing_and_all_local_coordinate_entries(self) -> None:
        path = self.fixture(bytes(HEADER_BYTES))
        self.assertEqual(self.inspect(path)["entries"], [])
        document = self.inspect("--all", path)
        self.assertEqual(len(document["entries"]), 1024)
        self.assertEqual(document["present_entries"], 0)
        self.assertEqual(document["invalid_entries"], 0)
        for index, entry in enumerate(document["entries"]):
            self.assertEqual((entry["x"], entry["z"]), (index % 32, index // 32))
            self.assertEqual(entry["location"], 0)
            self.assertEqual(entry["issues"], 0)
            self.assertEqual(entry["record_status"], "missing region entry")

    def test_missing_selection_is_distinct_and_selected_coordinates_override_all(self) -> None:
        path = self.fixture(bytes(HEADER_BYTES))
        document = self.inspect("--all", path, 31, 31, code=3)
        self.assertEqual(document["status"], "diagnostic")
        self.assertEqual(len(document["entries"]), 1)
        entry = document["entries"][0]
        self.assertEqual((entry["x"], entry["z"]), (31, 31))
        self.assertEqual(entry["record_status"], "missing region entry")
        self.assertNotIn("compression", entry)

    def test_edge_coordinates_and_unsigned_metadata_are_not_reinterpreted(self) -> None:
        payload = compressed(ROOT, 2)
        data = bytearray(region([
            (31, 0, 2, payload, 0xFFFFFFFF),
            (0, 31, 2, payload, 0x80000000),
            (31, 31, 2, payload, 0x7FFFFFFF),
        ]))
        set_location(data, 17, 3, 0xFFFFFF01, 0x89ABCDEF)
        path = self.fixture(bytes(data))
        document = self.inspect(path, code=1)
        self.assertEqual(document["present_entries"], 4)
        self.assertEqual(document["invalid_entries"], 1)
        entries = {(entry["x"], entry["z"]): entry for entry in document["entries"]}
        self.assertEqual(entries[(31, 0)]["timestamp"], 4294967295)
        self.assertEqual(entries[(0, 31)]["timestamp"], 2147483648)
        self.assertEqual(entries[(31, 31)]["timestamp"], 2147483647)
        invalid = entries[(17, 3)]
        self.assertEqual(invalid["location"], 4294967041)
        self.assertEqual(invalid["timestamp"], 2309737967)
        self.assertEqual(invalid["offset"], 68719472640)
        self.assertEqual(invalid["extent_bytes"], 4096)
        self.assertTrue(invalid["issues"] & 4)

    def test_three_trailing_boundaries_are_reported_separately(self) -> None:
        for selector in (1, 2):
            with self.subTest(selector=selector):
                raw = ROOT + b"nbt-tail"
                member = compressed(raw, selector)
                payload = member + b"compressed-tail"
                path = self.fixture(region([(0, 0, selector, payload, 0)]) + b"disk", f"tail-{selector}.mcr")
                document = self.inspect(path, 0, 0, code=1)
                self.assertEqual(document["status"], "diagnostic")
                self.assertEqual(document["trailing_bytes"], 4)
                entry = document["entries"][0]
                self.assertEqual(entry["issues"], 0)
                self.assertEqual(entry["compression"], {
                    "status": "ok", "consumed": len(member),
                    "trailing_bytes": 15, "output_bytes": len(raw),
                })
                self.assertEqual(entry["nbt"]["status"], "ok")
                self.assertEqual(entry["nbt"]["consumed"], 12)
                self.assertEqual(entry["nbt"]["trailing_bytes"], 8)

    def test_unicode_and_space_path_with_read_only_permissions(self) -> None:
        payload = compressed(ROOT, 1)
        path = self.fixture(region([(0, 0, 1, payload, 0)]), "region space \u03bb \u4e16\u754c.mcr")
        path.chmod(stat.S_IREAD)
        self.addCleanup(path.chmod, stat.S_IREAD | stat.S_IWRITE)
        self.assert_selected_ok(self.inspect(path, 0, 0), 1, payload)

    def test_short_header_is_a_file_diagnostic(self) -> None:
        path = self.fixture(bytes(63))
        document = self.inspect(path, code=1)
        self.assertEqual(document["file_bytes"], 63)
        self.assertEqual(document["status"], "truncated region header")

    def test_partial_final_sector_is_not_padded(self) -> None:
        path = self.fixture(bytes(HEADER_BYTES) + b"partial")
        document = self.inspect(path, code=1)
        self.assertEqual(document["status"], "diagnostic")
        self.assertEqual(document["file_bytes"], 8199)
        self.assertEqual(document["trailing_bytes"], 7)
        self.assertEqual(document["entries"], [])

    def test_overlapping_entries_are_both_diagnosed(self) -> None:
        payload = compressed(ROOT, 1)
        data = bytearray(region([(0, 0, 1, payload, 1)]))
        set_location(data, 1, 0, 513, 2)
        path = self.fixture(bytes(data))
        document = self.inspect(path, code=1)
        self.assertEqual(document["invalid_entries"], 2)
        self.assertEqual(len(document["entries"]), 2)
        for entry in document["entries"]:
            self.assertTrue(entry["issues"] & 8)
            self.assertEqual(entry["record_status"], "invalid region record")
        selected = self.inspect(path, 0, 0, code=1)["entries"][0]
        self.assertNotIn("compression", selected)

    def test_signed_record_length_bits_are_retained_with_diagnostic(self) -> None:
        payload = compressed(ROOT, 2)
        data = bytearray(region([(0, 0, 2, payload, 0)]))
        struct.pack_into(">I", data, HEADER_BYTES, 0xFFFFFFFF)
        path = self.fixture(bytes(data))
        entry = self.inspect(path, 0, 0, code=1)["entries"][0]
        self.assertEqual(entry["record_length"], 4294967295)
        self.assertTrue(entry["issues"] & 16)
        self.assertEqual(entry["record_status"], "invalid region record")
        self.assertNotIn("compression", entry)

    def test_unknown_compression_selector_is_a_record_diagnostic(self) -> None:
        path = self.fixture(region([(0, 0, 3, b"own fixture", 0)]))
        entry = self.inspect(path, 0, 0, code=1)["entries"][0]
        self.assertEqual(entry["selector"], 3)
        self.assertTrue(entry["issues"] & 64)
        self.assertEqual(entry["record_status"], "unsupported compression selector")
        self.assertNotIn("compression", entry)

    def test_checksum_failure_does_not_attempt_nbt(self) -> None:
        for selector in (1, 2):
            with self.subTest(selector=selector):
                payload = bytearray(compressed(ROOT, selector))
                payload[-1] ^= 1
                path = self.fixture(region([(0, 0, selector, bytes(payload), 0)]), f"checksum-{selector}.mcr")
                entry = self.inspect(path, 0, 0, code=1)["entries"][0]
                self.assertEqual(entry["record_status"], "ok")
                self.assertNotEqual(entry["compression"]["status"], "ok")
                for name in ("consumed", "trailing_bytes", "output_bytes"):
                    self.assertIsNone(entry["compression"][name])
                self.assertEqual(entry["nbt"]["status"], "not attempted")
                self.assertNotIn("root", entry["nbt"])

    def test_nbt_failure_keeps_successful_compression_evidence(self) -> None:
        raw = b"\x0a\x00\x00\x03"
        payload = compressed(raw, 2)
        path = self.fixture(region([(0, 0, 2, payload, 0)]))
        entry = self.inspect(path, 0, 0, code=1)["entries"][0]
        self.assertEqual(entry["compression"]["status"], "ok")
        self.assertEqual(entry["compression"]["output_bytes"], 4)
        self.assertNotEqual(entry["nbt"]["status"], "ok")
        self.assertIsInstance(entry["nbt"]["error_offset"], int)
        self.assertLessEqual(entry["nbt"]["error_offset"], 4)
        self.assertIsNone(entry["nbt"]["consumed"])
        self.assertIsNone(entry["nbt"]["trailing_bytes"])
        self.assertNotIn("root", entry["nbt"])

    def test_summary_limits_keep_full_lengths_and_explicit_truncation(self) -> None:
        name = b"r" * 130
        children = b"".join(b"\x01" + struct.pack(">H", len(key)) + key + b"\x01"
                            for key in (f"key-{index}".encode("ascii") for index in range(33)))
        raw = b"\x0a" + struct.pack(">H", len(name)) + name + children + b"\x00"
        payload = compressed(raw, 1)
        path = self.fixture(region([(0, 0, 1, payload, 0)]))
        entry = self.inspect(path, 0, 0)["entries"][0]
        self.assertEqual(entry["nbt"]["consumed"], len(raw))
        summary = entry["nbt"]["root"]
        self.assertEqual(summary["name_length"], 130)
        self.assertEqual(summary["name_utf16"], [114] * 128)
        self.assertTrue(summary["name_truncated"])
        self.assertEqual(summary["child_count"], 33)
        self.assertEqual(len(summary["children"]), 32)
        self.assertTrue(summary["child_summary_truncated"])

    def test_input_output_and_allocation_budgets_have_resource_exit(self) -> None:
        payload = compressed(ROOT, 2)
        path = self.fixture(region([(0, 0, 2, payload, 0)]))
        for arguments in (
            ("--max-input-bytes", "8191"),
            ("--max-output-bytes", "3"),
            ("--max-allocation-bytes", "0"),
        ):
            with self.subTest(arguments=arguments):
                self.invoke(*arguments, path, 0, 0, code=2)

    def test_invalid_arguments_do_not_modify_the_input(self) -> None:
        path = self.fixture(bytes(HEADER_BYTES))
        cases = [
            (), ("--unknown",), ("--max-input-bytes",),
            ("--max-input-bytes", "no"), ("--max-output-bytes", "-1"),
            ("--max-allocation-bytes", "18446744073709551616"),
            (str(path), "0"), (str(path), "32", "0"),
            (str(path), "0", "-1"), (str(path), "1.5", "0"),
            (str(path), "0", "0", "extra"),
        ]
        for arguments in cases:
            with self.subTest(arguments=arguments):
                self.invoke(*arguments, code=2)

    def test_help_and_missing_file_never_create_files_or_directories(self) -> None:
        stdout, stderr = self.invoke("--help")
        self.assertIn("usage", (stdout + stderr).lower())
        self.assertIn("--all", stdout + stderr)
        missing = self.root / "missing directory" / "absent.mcr"
        self.invoke(missing, code=2)
        self.assertFalse(missing.parent.exists())
        self.invoke(self.root, code=2)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    arguments = parser.parse_args()
    RegionInspectTests.executable = arguments.executable.resolve()
    if not RegionInspectTests.executable.is_file():
        parser.error("--executable must name the built mc_region_inspect program")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(RegionInspectTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
