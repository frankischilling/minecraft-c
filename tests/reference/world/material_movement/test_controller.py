"""Authored subprocess controls for evidence validation, not World fixtures."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import capture
from run_native import validate_report


class NativeEvidence(unittest.TestCase):
    def control(self, code, checks=1, timeout=5, tamper=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report = capture.capture([sys.executable, "-c", code], root, "native", timeout)
            report.update(kind="native_material_movement", checks=checks)
            path = root / "run.json"
            path.write_text(json.dumps(report))
            if tamper:
                (root / report["stdout_file"]).write_bytes(b"changed")
            return validate_report(path)[0]

    def test_genuine_positive_control(self):
        self.assertEqual(self.control("print('Material movement: 1 checks')")["checks"], 1)

    def test_exit_is_required(self):
        with self.assertRaises(AssertionError):
            self.control("import sys; print('Material movement: 1 checks'); sys.exit(7)")

    def test_stderr_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("import sys; print('Material movement: 1 checks'); print('failure', file=sys.stderr)")

    def test_missing_final_marker_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("print('all case rows, but unfinished')")

    def test_zero_final_count_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("print('Material movement: 0 checks')", checks=0)

    def test_modified_output_is_rejected(self):
        with self.assertRaises(AssertionError):
            self.control("print('Material movement: 1 checks')", tamper=True)

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

    def io_fault(self, fault):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            real_open = Path.open
            channel = "stdout" if fault == "short" else "stderr"

            class FaultingWriter:
                def __init__(self, stream):
                    self.stream = stream

                def __enter__(self):
                    return self

                def __exit__(self, *args):
                    self.stream.close()
                    if fault == "close":
                        raise OSError("injected capture close failure")

                def write(self, data):
                    if fault == "write":
                        raise OSError("injected stderr capture write failure")
                    if fault == "short":
                        return self.stream.write(data[:-1])
                    return self.stream.write(data)

            def controlled_open(path, *args, **kwargs):
                stream = real_open(path, *args, **kwargs)
                if path == root / ("native." + channel) and args and args[0] == "xb":
                    return FaultingWriter(stream)
                return stream

            code = "print('Material movement: 1 checks')"
            if fault == "write":
                code += "; import sys; print('actual native diagnostic', file=sys.stderr)"
            with mock.patch.object(Path, "open", controlled_open):
                report = capture.capture([sys.executable, "-c", code], root, "native", 5)
            report.update(kind="native_material_movement", checks=1)
            path = root / "run.json"
            path.write_text(json.dumps(report))
            with self.assertRaises(AssertionError):
                validate_report(path)
            with self.assertRaises(AssertionError):
                capture.require_success(report)
            self.assertIn(channel, report["reader_errors"])
            self.assertTrue(report["reader_errors"][channel])

    def test_writer_fault_is_rejected(self):
        self.io_fault("write")

    def test_partial_write_is_rejected(self):
        self.io_fault("short")

    def test_close_fault_is_rejected(self):
        self.io_fault("close")

    def test_missing_reader_metadata_is_rejected(self):
        report = {"exit": 0, "timed_out": False, "output_overflow": False,
                  "pipe_capture_complete": True, "stderr_bytes": 0}
        with self.assertRaises(AssertionError):
            capture.require_success(report)


if __name__ == "__main__":
    unittest.main()
