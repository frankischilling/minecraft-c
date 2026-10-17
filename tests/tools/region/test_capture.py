"""Regression tests for reporting and input preservation in offline capture checks."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "scripts/reference/region/compare_capture.py"
HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}


class CaptureTests(unittest.TestCase):
    executable: Path

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="mc-region-capture-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.capture = self.root / "inputs/capture.json"
        self.capture.parent.mkdir()
        self.work = self.root / "reports"

    def run_helper(self, code, *, capture=None, work=None, executable=None):
        result = subprocess.run(
            [sys.executable, str(HELPER), "--executable", str(executable or self.executable),
             "--capture", str(capture or self.capture), "--work-dir", str(work or self.work)],
            capture_output=True, text=True, timeout=30, check=False,
        )
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result

    def generated_capture(self):
        """Simulated observation data tests the helper; it is not original evidence."""
        raw = bytes.fromhex("0a000000")
        payload = zlib.compress(raw)
        data = bytearray(12288)
        struct.pack_into(">I", data, 0, 0x201)
        struct.pack_into(">I", data, 4096, 0xFFFFFFFF)
        struct.pack_into(">I", data, 8192, 1 + len(payload))
        data[8196] = 2
        data[8197:8197 + len(payload)] = payload
        digest = hashlib.sha256(data).hexdigest()
        capture = {"jars": {side: {"sha256": value} for side, value in HASHES.items()}, "results": {}}
        for runtime in ("temurin17", "oracle21"):
            for side in HASHES:
                path = self.capture.parent / "fixtures" / runtime / side / "generated-0-0.fixture"
                path.parent.mkdir(parents=True)
                path.write_bytes(data)
                capture["results"][runtime + "-" + side] = [{
                    "fixture": "generated", "x": 0, "z": 0, "constructor": "OK", "mutated": False,
                    "root": "ROOT", "full_drain": "DRAIN", "present": "true", "packed": "00000201",
                    "timestamp": "ffffffff", "before_sha256": digest, "after_sha256": digest,
                    "decompressed_hex": raw.hex(), "canonical_root_hex": raw.hex(),
                }]
        return capture

    def snapshot(self):
        return {str(path.relative_to(self.root)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in self.capture.parent.rglob("*") if path.is_file()}

    def test_generated_capture_is_explicitly_recorded_mode_and_read_only(self):
        self.capture.write_text(json.dumps(self.generated_capture()), encoding="utf-8")
        before = self.snapshot()
        self.run_helper(0)
        report = json.loads((self.work / "manifest.json").read_text())
        self.assertEqual(report["mode"], "recorded_observations")
        self.assertEqual(report["live_original_region_check"], "pending")
        self.assertEqual(report["selected_records"], 4)
        self.assertEqual(report["errors"], [])
        self.assertEqual(self.snapshot(), before)

    def test_malformed_shapes_replace_stale_pass_with_failure(self):
        self.work.mkdir()
        valid = self.generated_capture()
        cases = [[], {"jars": [], "results": {}},
                 {"jars": valid["jars"], "results": []},
                 {"jars": valid["jars"], "results": {"temurin17-client": [None]}}]
        for case in cases:
            with self.subTest(case=case):
                self.capture.write_text(json.dumps(case), encoding="utf-8")
                (self.work / "manifest.json").write_text('{"status":"pass"}')
                before = self.snapshot()
                self.run_helper(1)
                report = json.loads((self.work / "manifest.json").read_text())
                self.assertEqual(report["status"], "fail")
                self.assertTrue(report["errors"])
                self.assertEqual(self.snapshot(), before)

    def test_missing_capture_and_probe_replace_stale_pass_with_unavailable(self):
        self.work.mkdir()
        self.capture.write_text("{}")
        for capture, executable in [(self.root / "absent.json", self.executable),
                                    (self.capture, self.root / "absent-probe")]:
            with self.subTest(capture=capture, executable=executable):
                (self.work / "manifest.json").write_text('{"status":"pass"}')
                self.run_helper(77, capture=capture, executable=executable)
                self.assertEqual(json.loads((self.work / "manifest.json").read_text())["status"], "unavailable")

    def test_input_ancestor_is_rejected_before_writing(self):
        self.capture.write_text("{}")
        before = self.snapshot()
        self.run_helper(1, work=self.capture.parent)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse((self.capture.parent / "manifest.json").exists())

    def test_preexisting_hard_link_artifact_does_not_modify_input(self):
        self.capture.write_text(json.dumps(self.generated_capture()), encoding="utf-8")
        self.work.mkdir()
        victim = self.capture.parent / "fixtures/temurin17/client/generated-0-0.fixture"
        try:
            (self.work / "manifest.json").hardlink_to(victim)
        except (OSError, NotImplementedError) as error:
            self.skipTest("filesystem hard links unavailable: " + str(error))
        before = self.snapshot()
        self.run_helper(0)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(json.loads((self.work / "manifest.json").read_text())["status"], "pass")

    def test_symlinked_fixture_root_cannot_be_overwritten_by_output_symlink(self):
        actual = self.root / "actual-fixtures"
        actual.mkdir()
        victim = actual / "fixture.bin"
        victim.write_bytes(b"preserve this generated test input")
        self.work.mkdir()
        try:
            (self.capture.parent / "fixtures").symlink_to(actual, target_is_directory=True)
            (self.work / "manifest.json").symlink_to(victim)
        except (OSError, NotImplementedError) as error:
            self.skipTest("filesystem symlinks unavailable: " + str(error))
        self.capture.write_text("{}")
        before = victim.read_bytes()
        result = self.run_helper(1)
        self.assertIn("overlaps inputs", result.stderr)
        self.assertEqual(victim.read_bytes(), before)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    CaptureTests.executable = args.executable.resolve()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(CaptureTests))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
