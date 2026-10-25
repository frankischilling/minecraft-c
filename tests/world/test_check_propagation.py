"""Authored subprocess controls for the driver result contract, not World fixtures."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("checked_lighting", Path(__file__).with_name("check_propagation.py"))
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class DriverContract(unittest.TestCase):
    def run_control(self, code):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with contextlib.redirect_stdout(io.StringIO()):
                status = driver.run_checked([sys.executable, "-c", code], root, timeout=5)
            return status, json.loads((root / "run.json").read_text()), (root / "stdout.log").read_bytes()

    def test_positive_count_and_intentional_source_diagnostic(self):
        code = "print('Light too large, skipping!'); print('Lighting propagation: 1476 checks')"
        status, report, stdout = self.run_control(code)
        self.assertEqual(status, 0)
        self.assertEqual(report["checks"], 1476)
        self.assertIn(b"Light too large, skipping!\n", stdout.replace(b"\r\n", b"\n"))

    def test_empty_success_is_rejected(self):
        self.assertEqual(self.run_control("pass")[0], 1)

    def test_stderr_is_rejected(self):
        self.assertEqual(self.run_control("import sys; print('Lighting propagation: 1 checks'); print('failure',file=sys.stderr)")[0], 1)

    def test_zero_count_is_rejected(self):
        self.assertEqual(self.run_control("print('Lighting propagation: 0 checks')")[0], 1)

    def test_marker_must_be_final(self):
        self.assertEqual(self.run_control("print('Lighting propagation: 1 checks'); print('unfinished')")[0], 1)

    def test_native_failure_is_rejected(self):
        self.assertEqual(self.run_control("import sys; print('Lighting propagation: 1 checks'); sys.exit(7)")[0], 1)

    def test_output_is_bounded(self):
        previous = driver.STDOUT_LIMIT
        driver.STDOUT_LIMIT = 8192
        try:
            status, report, stdout = self.run_control("import sys; sys.stdout.write('x'*1000000)")
            self.assertEqual(status, 1)
            self.assertTrue(report["output_overflow"])
            self.assertEqual(len(stdout), 8192)
        finally:
            driver.STDOUT_LIMIT = previous


if __name__ == "__main__":
    unittest.main()
