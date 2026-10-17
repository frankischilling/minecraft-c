#!/usr/bin/env python3
"""Public reporting/input-preservation checks; no original classes are run."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "scripts/reference/noise/compare_noise.py"


class ReferenceGuards(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="noise reference ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.inputs = self.root / "inputs"
        self.inputs.mkdir()
        self.client = self.inputs / "client.jar"
        self.server = self.inputs / "server.jar"
        self.client.write_bytes(b"generated prerequisite placeholder, not a game binary")
        self.server.write_bytes(b"another generated prerequisite placeholder")
        self.work = self.root / "reports"

    def run_helper(self, *, work=None, executable=None, client=None):
        return subprocess.run(
            [sys.executable, str(HELPER), "--executable", str(executable or sys.executable),
             "--client", str(client or self.client), "--server", str(self.server),
             "--java", str(self.root / "absent-java"),
             "--javac", str(self.root / "absent-javac"),
             "--work-dir", str(work or self.work), "--tested-commit", "a" * 40],
            capture_output=True, text=True, encoding="utf-8", timeout=30, check=False)

    def snapshot(self):
        return {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                for path in self.inputs.iterdir() if path.is_file()}

    def test_missing_prerequisites_replace_stale_pass_with_unavailable(self):
        self.work.mkdir()
        cases = [{}, {"executable": self.root / "absent-probe"},
                 {"client": self.root / "absent-client.jar"}]
        before = self.snapshot()
        for case in cases:
            with self.subTest(case=case):
                (self.work / "manifest.json").write_text('{"status":"pass"}')
                result = self.run_helper(**case)
                self.assertEqual(result.returncode, 77, result.stderr)
                report = json.loads((self.work / "manifest.json").read_text())
                self.assertEqual(report["status"], "unavailable")
                self.assertEqual(report["tested_commit"], "a" * 40)
                self.assertEqual(self.snapshot(), before)

    def test_input_ancestor_is_rejected_before_writing(self):
        before = self.snapshot()
        result = self.run_helper(work=self.inputs)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse((self.inputs / "manifest.json").exists())

    def test_symlinked_artifact_cannot_overwrite_input(self):
        self.work.mkdir()
        try:
            (self.work / "manifest.json").symlink_to(self.client)
        except (OSError, NotImplementedError) as error:
            self.skipTest("filesystem symlinks unavailable: " + str(error))
        before = self.snapshot()
        result = self.run_helper()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(self.snapshot(), before)

    def test_hard_link_artifact_preserves_input(self):
        self.work.mkdir()
        try:
            (self.work / "manifest.json").hardlink_to(self.client)
        except (OSError, NotImplementedError) as error:
            self.skipTest("filesystem hard links unavailable: " + str(error))
        before = self.snapshot()
        result = self.run_helper()
        self.assertIn(result.returncode, (1, 77), result.stderr)
        self.assertEqual(self.snapshot(), before)
        if result.returncode == 77:
            self.assertEqual(json.loads((self.work / "manifest.json").read_text())["status"],
                             "unavailable")
        else:
            self.assertIn("output", result.stderr.lower())


if __name__ == "__main__":
    unittest.main(verbosity=2)
