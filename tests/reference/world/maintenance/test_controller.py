"""Authored subprocess controls for evidence validation, not World fixtures."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

import capture
from run_native import validate_report


class NativeEvidence(unittest.TestCase):
    def control(self, code, checks=1, timeout=5, tamper=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report = capture.capture([sys.executable, "-c", code], root, "native", timeout)
            report.update(kind="native_maintenance", checks=checks)
            path = root / "run.json"
            path.write_text(json.dumps(report))
            if tamper:
                (root / report["stdout_file"]).write_bytes(b"changed")
            return validate_report(path)[0]

    def test_genuine_positive_control(self):
        self.assertEqual(self.control("print('Provider maintenance: 1 checks')")["checks"], 1)

    def test_exit_is_required(self):
        with self.assertRaises(AssertionError):
            self.control("import sys; print('Provider maintenance: 1 checks'); sys.exit(7)")

    def test_stderr_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("import sys; print('Provider maintenance: 1 checks'); print('failure', file=sys.stderr)")

    def test_missing_final_marker_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("print('all case rows, but unfinished')")

    def test_zero_final_count_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("print('Provider maintenance: 0 checks')", checks=0)

    def test_modified_output_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("print('Provider maintenance: 1 checks')", tamper=True)

    def test_timeout_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("import time; time.sleep(2)", timeout=0.05)

    def test_output_is_bounded(self):
        previous = capture.STDOUT_LIMIT
        capture.STDOUT_LIMIT = 8192
        try:
            with self.assertRaises(AssertionError):
                self.control("import sys; sys.stdout.write('x'*1000000)")
        finally:
            capture.STDOUT_LIMIT = previous


if __name__ == "__main__":
    unittest.main()
