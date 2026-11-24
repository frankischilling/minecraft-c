"""Exercise subprocess failure gates without a JDK, assets or a game fixture."""

import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "propagation_processes", ROOT / "tests/reference/world/propagation/processes.py")
PROCESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROCESS)


class CaptureContract(unittest.TestCase):
    def run_child(self, source, limit=4096, timeout=5, directory=None):
        return PROCESS.bounded([sys.executable, "-c", source], timeout,
                               os.environ.copy(), limit, directory)

    def test_both_streams_are_drained(self):
        result, output = self.run_child(
            "import sys; sys.stdout.write('out'); sys.stderr.write('err')")
        self.assertTrue(PROCESS.successful(result))
        self.assertEqual(output, {"stdout": b"out", "stderr": b"err"})

    def test_nonzero_exit_fails(self):
        result, _ = self.run_child("raise SystemExit(7)")
        self.assertEqual(result["exit"], 7)
        self.assertFalse(PROCESS.successful(result))

    def test_deadline_kills_and_reaps(self):
        result, _ = self.run_child("import time; time.sleep(10)", timeout=.1)
        self.assertTrue(result["timed_out"])
        self.assertIsInstance(result["exit"], int)
        self.assertFalse(PROCESS.successful(result))

    def test_stdout_limit_rejects_excess(self):
        result, output = self.run_child("import sys; sys.stdout.buffer.write(bytes(1000000))")
        self.assertEqual(len(output["stdout"]), 4096)
        self.assertTrue(result["streams"]["stdout"]["limit_exceeded"])
        self.assertFalse(PROCESS.successful(result))

    def test_stderr_limit_rejects_excess(self):
        result, output = self.run_child("import sys; sys.stderr.buffer.write(bytes(1000000))")
        self.assertEqual(len(output["stderr"]), 4096)
        self.assertTrue(result["streams"]["stderr"]["limit_exceeded"])
        self.assertFalse(PROCESS.successful(result))

    def test_exact_limit_is_retained(self):
        result, output = self.run_child("import sys; sys.stdout.buffer.write(bytes(4096))")
        self.assertTrue(PROCESS.successful(result))
        self.assertEqual(output["stdout"], bytes(4096))

    def test_file_capture_and_closed_stdin(self):
        with tempfile.TemporaryDirectory() as name:
            directory = Path(name)
            result, output = self.run_child(
                "import sys; print('eof' if sys.stdin.read() == '' else 'unexpected')",
                directory=directory)
            self.assertTrue(PROCESS.successful(result))
            self.assertEqual(output, {})
            self.assertEqual((directory / "stdout.log").read_bytes().splitlines(), [b"eof"])
            self.assertEqual((directory / "stderr.log").read_bytes(), b"")


if __name__ == "__main__":
    unittest.main()
