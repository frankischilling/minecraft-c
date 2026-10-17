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
HELPER = ROOT / "scripts/reference/math/compare_math.py"


class ReferenceGuards(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="math reference ")
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

    def test_completed_mismatch_survives_later_unavailable(self):
        self.exercise_completed_mismatch(self.root / "generated-workspace")

    def test_aliased_workspace_reaches_completed_mismatch(self):
        generated = self.root / "real-workspace"
        generated.mkdir()
        alias = self.root / "workspace-alias"
        try:
            alias.symlink_to(generated, target_is_directory=True)
        except (OSError, NotImplementedError) as error:
            self.skipTest("filesystem directory symlinks unavailable: " + str(error))
        self.exercise_completed_mismatch(alias)

    def exercise_completed_mismatch(self, generated):
        generated = generated.resolve()
        spec = importlib.util.spec_from_file_location("math_guard_helper", HELPER)
        helper = importlib.util.module_from_spec(spec)
        with mock.patch.dict(sys.modules):
            sys.modules.pop("fixtures", None)
            sys.modules[spec.name] = helper
            sys.path.insert(0, str(HELPER.parent))
            try:
                spec.loader.exec_module(helper)
            finally:
                sys.path.pop(0)
            # All processes and original inputs are generated. A separate
            # workspace keeps this test from writing any repository source.
            module_path = generated / "scripts/reference/math/compare_math.py"
            source = generated / "tests/reference/math/TrigMathObservation.java"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"generated observation placeholder; no executable code")
            cases = [helper.Fixture("sin", "00000000")]

            def generated_process(command, data=None):
                if "-d" in command:
                    classes = Path(command[command.index("-d") + 1])
                    (classes / "TrigMathObservation.class").write_bytes(b"generated class placeholder")
                    output = b""
                elif "-XshowSettings:properties" in command:
                    output = b"java.specification.version = 17\n"
                else:
                    output = b"javac 17.0.1\n"
                return helper.ProcessResult(0, output, b"")

            def generated_batch(comparison, label, command, requested, original=False):
                if label == "server-reference":
                    raise helper.SetupUnavailable("generated later unavailable prerequisite")
                table = ["00000000"] * helper.TABLE_VALUES
                if label == "client-reference":
                    table[-1] = "00000001"
                return {"table": table, "calls": [("sin", "(F)F", "00000000", "00000000", "-")]}

            arguments = [str(HELPER), "--executable", sys.executable,
                         "--client", str(self.client), "--server", str(self.server),
                         "--java", sys.executable, "--javac", sys.executable,
                         "--work-dir", str(self.work), "--tested-commit", "b" * 40]
            hashes = {"client": hashlib.sha256(self.client.read_bytes()).hexdigest(),
                      "server": hashlib.sha256(self.server.read_bytes()).hexdigest()}
            with mock.patch.object(sys, "argv", arguments), \
                    mock.patch.object(helper, "__file__", str(module_path)), \
                    mock.patch.object(helper, "source_paths", return_value=[source]), \
                    mock.patch.object(helper, "HARNESS_SHA256", hashlib.sha256(source.read_bytes()).hexdigest()), \
                    mock.patch.object(helper, "JAR_HASHES", hashes), \
                    mock.patch.object(helper, "fixtures", return_value=cases), \
                    mock.patch.object(helper, "run", side_effect=generated_process), \
                    mock.patch.object(helper.Comparison, "batch", generated_batch), \
                    mock.patch.object(sys, "stdout", io.StringIO()), \
                    mock.patch.object(sys, "stderr", io.StringIO()):
                result = helper.main()
            report = json.loads((self.work / "manifest.json").read_text())
            self.assertEqual(result, 1)
            self.assertEqual(report["status"], "fail")
            self.assertIn("matched_records", report, report["errors"])
            self.assertEqual(report["matched_records"]["client_to_c"], {"table_entries": 65535, "calls": 1})
            self.assertTrue(any("table[65535]" in error for error in report["errors"]))


class ProtocolGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("math_protocol_helper", HELPER)
        cls.helper = importlib.util.module_from_spec(spec)
        with mock.patch.dict(sys.modules):
            sys.modules.pop("fixtures", None)
            sys.modules[spec.name] = cls.helper
            sys.path.insert(0, str(HELPER.parent))
            try:
                spec.loader.exec_module(cls.helper)
            finally:
                sys.path.pop(0)
        cls.cases = [cls.helper.Fixture("sin", "7fa00001"),
                     cls.helper.Fixture("floor", "8000000000000000")]
        cls.table = [f"TABLE\t{index}\t00000000" for index in range(65536)]
        cls.digest = hashlib.sha256(bytes(65536 * 4)).hexdigest()

    def stream(self, original=False):
        lines = list(self.helper.HEADERS)
        if original:
            lines += ["TABLE_SHA256_BEFORE\t" + self.digest]
        lines += self.table
        lines += ["CALL\tsin\t(F)F\t7fa00001\t00000000\t-",
                  "CALL\tfloor\t(D)I\t8000000000000000\t00000000\t0",
                  "TABLE_SHA256_AFTER\t" + self.digest if original else "TABLE_UNCHANGED\t1",
                  "CALL_COUNT\t2"]
        return ("\n".join(lines) + "\n").encode("ascii")

    def test_only_validated_transport_controls_are_normalized(self):
        c_record = self.helper.parse_output(self.stream(), self.cases, False)
        original = self.helper.parse_output(self.stream(True), self.cases, True)
        self.assertEqual(c_record, original)
        self.assertEqual(c_record["calls"][0][2], "7fa00001")

    def test_control_hashes_immutability_order_and_raw_echo_are_required(self):
        c_data, original = self.stream(), self.stream(True)
        invalid = [(c_data.replace(b"TABLE_UNCHANGED\t1", b"TABLE_UNCHANGED\t0"), False),
                   (original.replace(self.digest.encode(), b"0" * 64), True),
                   (c_data.replace(b"TABLE\t65535\t", b"TABLE\t65534\t"), False),
                   (c_data.replace(b"CALL\tsin\t(F)F\t7fa00001", b"CALL\tsin\t(F)F\t7fe00001"), False),
                   (c_data.replace(b"00000000\t0\nTABLE_UNCHANGED", b"00000001\t0\nTABLE_UNCHANGED"), False),
                   (c_data.replace(b"CALL_COUNT\t2", b"CALL_COUNT\t3"), False),
                   (c_data + b"unexpected\n", False)]
        for data, is_original in invalid:
            with self.subTest(original=is_original, tail=data[-100:]):
                with self.assertRaises(ValueError):
                    self.helper.parse_output(data, self.cases, is_original)

    def test_request_bounds_are_checked_before_execution(self):
        self.helper.validate_cases(self.cases)
        for cases in ([], [self.cases[0]] * 769,
                      [self.helper.Fixture("sin", "0" * 16)],
                      [self.helper.Fixture("floor", "7FF0000000000000")]):
            with self.assertRaises(ValueError):
                self.helper.validate_cases(cases)


if __name__ == "__main__":
    unittest.main(verbosity=2)
