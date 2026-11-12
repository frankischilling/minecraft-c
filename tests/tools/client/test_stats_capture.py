"""Authored real-child fault controls for both stats capture call sites; no JAR."""

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


HERE = Path(__file__).resolve().parent
SUPPORT = load("stats_capture_control_support", HERE.parents[1] / "reference/client/statistics/support.py")
CONTRACT = load("stats_capture_control_contract", HERE / "test_stats_file.py")
CALLS = (("reference", SUPPORT.run_bounded), ("contract", CONTRACT.run))
CHILD = "import sys;sys.stdout.buffer.write(b'complete\\n');sys.stdout.flush();sys.stderr.buffer.write(b'actual diagnostic\\n');sys.stderr.flush()"


class CaptureControls(unittest.TestCase):
    def test_real_clean_controls(self):
        for label, call in CALLS:
            with self.subTest(call=label), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                result = call([sys.executable, "-c", CHILD], root, "child")
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"complete\n")
                self.assertEqual(result.stderr, b"actual diagnostic\n")
                self.assertEqual(result.capture_report["reader_errors"], {})
                self.assertTrue(result.capture_report["reaped"])
                self.assertTrue(result.capture_report["pipe_capture_complete"])

    def test_nonzero_child_is_preserved(self):
        for label, call in CALLS:
            with self.subTest(call=label), tempfile.TemporaryDirectory() as directory:
                result = call([sys.executable, "-c", "raise SystemExit(7)"], Path(directory), "child")
                self.assertEqual(result.returncode, 7)
                self.assertEqual(result.capture_report["reader_errors"], {})

    def fault(self, call, channel, fault):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            real_open, real_read, real_popen = Path.open, Path.read_bytes, subprocess.Popen
            processes, unhandled = [], []
            target = root / ("child.capture.json" if fault.startswith("report_") else "child." + channel)
            expected_phase = fault.removeprefix("report_") if not fault.startswith("report_") else fault
            if fault == "short": expected_phase = "write"
            if fault in ("capture_read", "capture_short_read"): expected_phase = "capture_read"

            class Pipe:
                def __init__(self, stream): self.stream = stream
                def read1(self, count):
                    if fault == "read": raise OSError("injected pipe read failure")
                    return self.stream.read1(count)
                def close(self):
                    self.stream.close()
                    if fault == "pipe_close": raise OSError("injected pipe close failure")

            class Writer:
                def __init__(self, stream): self.stream = stream
                def __enter__(self): return self
                def __exit__(self, *args):
                    self.stream.close()
                    if fault in ("close", "report_close"): raise OSError("injected capture close failure")
                def write(self, data):
                    if fault in ("write", "report_write"): raise OSError("injected capture write failure")
                    if fault in ("short", "report_short"): return self.stream.write(data[:-1])
                    return self.stream.write(data)

            def launch(*args, **kwargs):
                if fault == "process_open": raise OSError("injected Popen failure")
                process = real_popen(*args, **kwargs)
                processes.append(process)
                if fault in ("read", "pipe_close"):
                    setattr(process, channel, Pipe(getattr(process, channel)))
                return process

            def opened(path, *args, **kwargs):
                selected = path == target and args and args[0] in ("xb", "x")
                if selected and fault in ("open", "report_open"): raise OSError("injected capture open failure")
                stream = real_open(path, *args, **kwargs)
                return Writer(stream) if selected else stream

            def read(path):
                if path == target:
                    if fault == "capture_read": raise OSError("injected capture reread failure")
                    if fault == "capture_short_read": return b""
                return real_read(path)

            with mock.patch.object(subprocess, "Popen", launch), mock.patch.object(Path, "open", opened), \
                 mock.patch.object(Path, "read_bytes", read), mock.patch.object(threading, "excepthook", unhandled.append):
                with self.assertRaises(ValueError) as raised:
                    call([sys.executable, "-c", CHILD], root, "child", timeout=5)
            report = raised.exception.capture_report
            error_channel = "process" if fault.startswith("report_") or fault == "process_open" else channel
            if fault == "report_short": expected_phase = "report_write"
            if fault == "process_open": expected_phase = "open"
            self.assertIn(expected_phase, [entry["phase"] for entry in report["reader_errors"][error_channel]])
            self.assertEqual(unhandled, [])
            self.assertTrue(all(process.poll() is not None for process in processes))
            if processes: self.assertTrue(report["reaped"])

    def test_all_io_faults_reject(self):
        for label, call in CALLS:
            for channel in ("stdout", "stderr"):
                for fault in ("open", "read", "write", "short", "close", "pipe_close", "capture_read", "capture_short_read"):
                    with self.subTest(call=label, channel=channel, fault=fault):
                        self.fault(call, channel, fault)
            for fault in ("process_open", "report_open", "report_write", "report_short", "report_close"):
                with self.subTest(call=label, fault=fault):
                    self.fault(call, "stdout", fault)

    def test_overflow_before_sleep(self):
        for label, call in CALLS:
            for channel in ("stdout", "stderr"):
                with self.subTest(call=label, channel=channel), tempfile.TemporaryDirectory() as directory:
                    code = "import sys,time;sys." + channel + ".write('x'*65);sys." + channel + ".flush();time.sleep(10)"
                    start = time.monotonic()
                    with self.assertRaises(ValueError) as raised:
                        call([sys.executable, "-c", code], Path(directory), "child", timeout=3, limit=64)
                    report = raised.exception.capture_report
                    self.assertTrue(report["output_overflow"])
                    self.assertFalse(report["timed_out"])
                    self.assertTrue(report["reaped"])
                    self.assertEqual(report["reader_errors"], {})
                    self.assertLess(time.monotonic() - start, 6)

    def test_timeout_reaps(self):
        for label, call in CALLS:
            with self.subTest(call=label), tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(ValueError) as raised:
                    call([sys.executable, "-c", "import time;time.sleep(10)"], Path(directory), "child", timeout=.05)
                self.assertTrue(raised.exception.capture_report["timed_out"])
                self.assertTrue(raised.exception.capture_report["reaped"])


if __name__ == "__main__":
    unittest.main()
