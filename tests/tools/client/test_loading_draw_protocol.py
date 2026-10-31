"""Authored/unrun pure-data admission controls; no native child or fixture preparation."""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
SCRIPT = Path(__file__).with_name("test_loading_draw.py")
spec = importlib.util.spec_from_file_location("loading_prefix_driver", SCRIPT)
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


def receipt(data):
    return {"command": [sys.executable], "exit": 0, "reaped": True,
            "pipe_capture_complete": True, "reader_errors": {}, "timed_out": False,
            "output_overflow": False, "stdout_bytes": len(data), "stderr_bytes": 0}


class Protocol(unittest.TestCase):
    def test_complete_LF_and_CRLF(self):
        for ending in (b"\n", b"\r\n"):
            data = b"loading prefix normal: 91 checks passed" + ending
            self.assertEqual(driver.admit(receipt(data), data, b"", Path(sys.executable), "normal"), 91)

    def test_actual_process_fields(self):
        data = b"loading prefix normal: 91 checks passed\n"
        changes = [("exit", 1), ("exit", False), ("reaped", False),
                   ("pipe_capture_complete", False), ("reader_errors", {"stdout": "writer failed"}),
                   ("reader_errors", None), ("timed_out", True), ("output_overflow", True),
                   ("stdout_bytes", len(data) - 1), ("stdout_bytes", True),
                   ("stderr_bytes", 1), ("stderr_bytes", False), ("command", []),
                   ("command", str(sys.executable)), ("command", [str(SCRIPT)]), ("command", [17])]
        for key, value in changes:
            with self.subTest(key=key, value=value):
                r = receipt(data)
                r[key] = value
                with self.assertRaises((ValueError, OSError)):
                    driver.admit(r, data, b"", Path(sys.executable), "normal")
        r = receipt(data)
        del r["reader_errors"]
        with self.assertRaises(ValueError):
            driver.admit(r, data, b"", Path(sys.executable), "normal")

    def test_exact_footer(self):
        clean = b"loading prefix normal: 91 checks passed\n"
        streams = [clean[:-1], clean.replace(b"normal", b"negative"),
                   clean.replace(b"91", b"0"), clean.replace(b"91", b"-1"),
                   b"prefix\n" + clean, clean + b"extra\n", clean + b"\n",
                   clean.replace(b"\n", b"\v"), clean.replace(b"\n", b"\f"),
                   clean.replace(b"\n", b"\r"), clean.replace(b"\n", b"\x85"),
                   clean.replace(b"\n", b"\xe2\x80\xa8"), clean.replace(b"\n", b"\r\r\n")]
        for stream in streams:
            with self.subTest(stream=stream):
                with self.assertRaises(ValueError):
                    driver.admit(receipt(stream), stream, b"", Path(sys.executable), "normal")

    def test_stderr_and_unknown_mode(self):
        data = b"loading prefix normal: 91 checks passed\n"
        r = receipt(data)
        r["stderr_bytes"] = 1
        with self.assertRaises(ValueError):
            driver.admit(r, data, b"x", Path(sys.executable), "normal")
        with self.assertRaises(ValueError):
            driver.admit(receipt(data), data, b"", Path(sys.executable), "unknown")


if __name__ == "__main__":
    unittest.main()
