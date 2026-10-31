#!/usr/bin/env python3
"""Authored parser-only controls. No native child, resource owner or GL claim.

Run this source controller normally and under -O in a future qualified cohort.
These bytes have not been executed in the source-only correction stage.
"""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("close_driver", Path(__file__).with_name("test_close_callbacks.py"))
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)

# Authored fixed protocol, independent of the driver's expected_rows generator.
POSITIVE = b"""CLOSE stream-success 1 0
NESTED_STREAM stream-success 7 1
CASE_OK stream-success 1 1 0 0
CLOSE stream-retry 1 0
NESTED_STREAM stream-retry 7 1
CLOSE stream-retry 2 0
NESTED_STREAM stream-retry 7 1
CASE_OK stream-retry 2 2 0 0
CLOSE font-success 1 0
NESTED_STREAM font-success 7 1
NESTED_FONT font-success 3 1
NESTED_RETAIN font-success 3
CASE_OK font-success 1 1 1 1
CLOSE font-retry 1 0
NESTED_STREAM font-retry 7 1
NESTED_FONT font-retry 3 1
NESTED_RETAIN font-retry 3
CLOSE font-retry 2 0
NESTED_STREAM font-retry 7 1
NESTED_FONT font-retry 3 1
NESTED_RETAIN font-retry 3
CASE_OK font-retry 2 2 2 2
Source close callbacks: 99 checks;4 genuine-filesystem cases
"""


def outcome(stdout=POSITIVE, stderr=b"", code=0, **capture):
    state = {"reader_errors": {}, "pipe_capture_complete": True, "reaped": True,
             "timed_out": False, "output_overflow": False}
    state.update(capture)
    return SimpleNamespace(stdout=stdout, stderr=stderr, returncode=code, capture_report=state)


class ParserControls(unittest.TestCase):
    def test_complete_controls(self):
        self.assertEqual(driver.validate(outcome()), 99)
        self.assertEqual(driver.validate(outcome(stdout=POSITIVE.replace(b"\n", b"\r\n"))), 99)

    def test_wrong_process_or_reader_outcomes(self):
        for label, value in (
            ("ordinary failure", outcome(code=1)),
            ("invalid input", outcome(code=2)),
            ("native crash", outcome(code=-11)),
            ("sanitizer stderr", outcome(stderr=b"ERROR: AddressSanitizer: heap-use-after-free\n")),
            ("exit-zero stderr read fault", outcome(reader_errors={"stderr": [{"phase": "read", "type": "OSError"}]})),
            ("incomplete reader", outcome(pipe_capture_complete=False)),
            ("unreaped", outcome(reaped=False)),
            ("timeout", outcome(timed_out=True)),
            ("overflow", outcome(output_overflow=True)),
        ):
            with self.subTest(label=label), self.assertRaises(ValueError):
                driver.validate(value)

    def test_non_lf_crlf_row_separators(self):
        # Keep the final LF; these five ASCII controls must remain row data,
        # never alternative separators that recreate the accepted row list.
        for label, separator in (
            ("vertical tab", b"\x0b"),
            ("form feed", b"\x0c"),
            ("file separator", b"\x1c"),
            ("group separator", b"\x1d"),
            ("record separator", b"\x1e"),
        ):
            with self.subTest(label=label), self.assertRaisesRegex(ValueError, "protocol mismatch"):
                driver.validate(outcome(stdout=POSITIVE.replace(b"\n", separator, 1)))

    def test_wrong_ownership_protocol(self):
        for label, text in (
            ("nested retire admitted", POSITIVE.replace(b"NESTED_STREAM stream-success 7 1", b"NESTED_STREAM stream-success 0 0")),
            ("nested font admitted", POSITIVE.replace(b"NESTED_FONT font-success 3 1", b"NESTED_FONT font-success 0 0")),
            ("retention admitted", POSITIVE.replace(b"NESTED_RETAIN font-success 3", b"NESTED_RETAIN font-success 0")),
            ("extra physical close", POSITIVE.replace(b"CASE_OK stream-success", b"CLOSE stream-success 2 0\nCASE_OK stream-success")),
            ("retry lost", POSITIVE.replace(b"CASE_OK stream-retry 2 2 0 0", b"CASE_OK stream-retry 1 1 0 0")),
            ("zero checks", POSITIVE.replace(b"99 checks", b"0 checks")),
            ("missing newline", POSITIVE[:-1]),
            ("unreported diagnostic", b"native failure\n" + POSITIVE),
        ):
            with self.subTest(label=label), self.assertRaises(ValueError):
                driver.validate(outcome(stdout=text))


if __name__ == "__main__":
    unittest.main()
