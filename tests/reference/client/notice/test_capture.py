"""Authored controls for failed/partial subprocess evidence."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
sys.dont_write_bytecode = True
import capture


class CaptureControls(unittest.TestCase):
    def child(self, code, timeout=3):
        with tempfile.TemporaryDirectory() as directory:
            return capture.capture([sys.executable, "-c", code], Path(directory), "child", timeout)

    def test_success(self):
        capture.require_success(self.child("print('complete')"), empty_stderr=True)

    def test_nonzero(self):
        with self.assertRaises(AssertionError):
            capture.require_success(self.child("raise SystemExit(7)"))

    def test_stderr(self):
        with self.assertRaises(AssertionError):
            capture.require_success(self.child("import sys;sys.stderr.write('failure')"), empty_stderr=True)

    def test_timeout(self):
        result = self.child("import time;time.sleep(10)", .05)
        self.assertTrue(result["timed_out"])
        with self.assertRaises(AssertionError):
            capture.require_success(result)

    def overflow(self, channel):
        with mock.patch.object(capture, "STDOUT_LIMIT", 4096), mock.patch.object(capture, "STDERR_LIMIT", 4096):
            result = self.child(f"import os,time;os.write({channel},b'x'*4097);time.sleep(10)")
        self.assertTrue(result["output_overflow"])
        self.assertFalse(result["timed_out"])
        self.assertLessEqual(result["stdout_bytes"], 4096)
        self.assertLessEqual(result["stderr_bytes"], 4096)
        with self.assertRaises(AssertionError):
            capture.require_success(result)

    def test_stdout_limit(self):
        self.overflow(1)

    def test_stderr_limit(self):
        self.overflow(2)

    def io_fault(self, fault):
        original = Path.open

        class Broken:
            def __init__(self, stream):
                self.stream = stream
            def __enter__(self):
                return self
            def __exit__(self, *args):
                self.stream.close()
            def write(self, payload):
                if fault == "exception":
                    raise OSError("authored capture write failure")
                return self.stream.write(payload[:-1])

        def opening(path, *args, **kwargs):
            stream = original(path, *args, **kwargs)
            return Broken(stream) if path.name == "child.stderr" and args and args[0] == "xb" else stream

        with mock.patch.object(Path, "open", opening):
            result = self.child("import sys;sys.stderr.write('failure')")
        self.assertIn("stderr", result["reader_errors"])
        with self.assertRaises(AssertionError):
            capture.require_success(result)

    def test_write_exception(self):
        self.io_fault("exception")

    def test_short_write(self):
        self.io_fault("short")

    def test_missing_reader_evidence(self):
        result = self.child("print('complete')")
        del result["reader_errors"]
        with self.assertRaises(AssertionError):
            capture.require_success(result)


if __name__ == "__main__":
    unittest.main()
