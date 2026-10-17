#!/usr/bin/env python3
"""Public prerequisite/report guards without original-class execution."""

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "scripts/reference/density/compare_density.py"


class ReferenceGuards(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="density reference ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.inputs = self.root / "inputs"
        self.inputs.mkdir()
        self.client = self.inputs / "client.jar"
        self.server = self.inputs / "server.jar"
        self.client.write_bytes(b"generated prerequisite placeholder, not a game binary")
        self.server.write_bytes(b"another generated prerequisite placeholder")
        self.work = self.root / "reports"

    def run_helper(self, *, work=None, executable=None, client=None, tools_present=False):
        java = sys.executable if tools_present else self.root / "absent-java"
        javac = sys.executable if tools_present else self.root / "absent-javac"
        return subprocess.run(
            [sys.executable, str(HELPER), "--executable", str(executable or sys.executable),
             "--client", str(client or self.client), "--server", str(self.server),
             "--java", str(java), "--javac", str(javac),
             "--work-dir", str(work or self.work), "--tested-commit", "a" * 40],
            capture_output=True, text=True, encoding="utf-8", timeout=30, check=False)

    def snapshot(self):
        return {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                for path in self.inputs.iterdir() if path.is_file()}

    def test_missing_prerequisites_replace_stale_pass_with_unavailable(self):
        self.work.mkdir()
        before = self.snapshot()
        for case in ({}, {"executable": self.root / "absent-probe"},
                     {"client": self.root / "absent-client.jar"}):
            with self.subTest(case=case):
                (self.work / "manifest.json").write_text('{"status":"pass"}')
                result = self.run_helper(**case)
                self.assertEqual(result.returncode, 77, result.stderr)
                report = json.loads((self.work / "manifest.json").read_text())
                self.assertEqual(report["status"], "unavailable")
                self.assertEqual(report["tested_commit"], "a" * 40)
                self.assertEqual(self.snapshot(), before)

    def test_wrong_hash_is_unavailable_before_version_or_probe_execution(self):
        before = self.snapshot()
        result = self.run_helper(tools_present=True)
        self.assertEqual(result.returncode, 77, result.stderr)
        report = json.loads((self.work / "manifest.json").read_text())
        self.assertEqual(report["status"], "unavailable")
        self.assertIn("mismatch", report["reason"])
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

    def test_completed_mismatch_survives_later_setup_unavailable(self):
        # Exercise the real report orchestration with generated process results.
        # No JVM, original classes or C executable run in this regression.
        spec = importlib.util.spec_from_file_location("density_guard_helper", HELPER)
        helper = importlib.util.module_from_spec(spec)
        with mock.patch.dict(sys.modules):
            sys.modules.pop("fixtures", None)
            sys.modules[spec.name] = helper
            sys.path.insert(0, str(HELPER.parent))
            try:
                spec.loader.exec_module(helper)
            finally:
                sys.path.pop(0)
            cases = [helper.Fixture("generated-init", "init\t0")]

            def generated_process(command, data=None):
                if "-d" in command:
                    classes = Path(command[command.index("-d") + 1])
                    (classes / "DensityProbe.class").write_bytes(b"generated class placeholder")
                    output = b""
                elif "-XshowSettings:properties" in command:
                    output = b"java.specification.version = 17\n"
                else:
                    output = b"javac 17.0.1\n"
                return helper.ProcessResult(0, output, b"")

            def generated_batch(comparison, label, command, requested, root=None):
                if label == "server-reference":
                    raise helper.SetupUnavailable("generated later setup failure", label, [])
                return [{"generated_value": 1 if label == "c-reference" else 2}]

            arguments = [str(HELPER), "--executable", sys.executable,
                         "--client", str(self.client), "--server", str(self.server),
                         "--java", sys.executable, "--javac", sys.executable,
                         "--work-dir", str(self.work), "--tested-commit", "b" * 40]
            hashes = {"client": hashlib.sha256(self.client.read_bytes()).hexdigest(),
                      "server": hashlib.sha256(self.server.read_bytes()).hexdigest()}
            with mock.patch.object(sys, "argv", arguments), \
                    mock.patch.object(helper, "JAR_HASHES", hashes), \
                    mock.patch.object(helper, "fixtures", return_value=cases), \
                    mock.patch.object(helper, "run", side_effect=generated_process), \
                    mock.patch.object(helper.Comparison, "batch", generated_batch), \
                    mock.patch.object(helper.Comparison, "controls", return_value={}), \
                    mock.patch.object(sys, "stdout", io.StringIO()), \
                    mock.patch.object(sys, "stderr", io.StringIO()):
                result = helper.main()
            report = json.loads((self.work / "manifest.json").read_text())
            self.assertEqual(result, 1)
            self.assertEqual(report["status"], "fail")
            self.assertEqual(report["matched_records"]["client_to_c"], 0)
            self.assertTrue(any("generated_value" in error for error in report["errors"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
