"""Fail-closed controls for unsupported terrain-submission capability receipts."""

import importlib.util
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
DRIVER = (Path(__file__).resolve().parents[2] /
          "reference/client/render_global_submit/run_native.py")
SPEC = importlib.util.spec_from_file_location("submit_capability_driver", DRIVER)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("terrain-submission driver unavailable")
SUBMIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUBMIT)


class SubmitCapabilityTests(unittest.TestCase):
    def setUp(self):
        self.case = {"name": "occlusion16", "actual_capability": True}
        self.capture = {
            "exit": 77, "reader_errors": {}, "timed_out": False,
            "output_overflow": False, "pipe_capture_complete": True,
            "stderr_bytes": 0,
        }
        self.headers = [
            "SUBMIT_FORMAT 8 8 8 8 24 0",
            "SUBMIT_CONTEXT occlusion16 4 5 1",
            "SUBMIT_VENDOR occlusion16 Authored vendor",
            "SUBMIT_RENDERER occlusion16 Authored renderer",
            "SUBMIT_VERSION occlusion16 4.5 Authored version",
            "SUBMIT_CX occlusion16 status=0 static_a=1 source_returned=1 "
            "capability_read=1 capability=0 source_pc=20",
            "SUBMIT_CAPABILITY occlusion16 0",
        ]
        self.marker = ("Source N submit unsupported: occlusion16; "
                       "N-not-entered; cleanup-complete")
        self.text = "\n".join([*self.headers, self.marker]) + "\n"

    def reject_text(self, text):
        with self.assertRaises(AssertionError):
            SUBMIT.unsupported_receipt(self.capture, text, self.case)

    def test_completed_false_capability(self):
        context = SUBMIT.unsupported_receipt(self.capture, self.text, self.case)
        self.assertEqual(context["capability"], 0)
        self.assertEqual(context["gl_major"], 4)
        self.assertEqual(context["gl_minor"], 5)
        self.assertEqual(context["profile"], 1)
        self.assertEqual(context["renderer"], "Authored renderer")
        context, headers = SUBMIT.capability_receipt(self.text, "occlusion16", 0)
        self.assertEqual(headers, self.headers)
        self.assertEqual(context["vendor"], "Authored vendor")

    def test_supported_receipt_cannot_be_unsupported(self):
        text = self.text.replace("capability=0", "capability=1").replace(
            "SUBMIT_CAPABILITY occlusion16 0", "SUBMIT_CAPABILITY occlusion16 1")
        context, _ = SUBMIT.capability_receipt(text, "occlusion16", 1)
        self.assertEqual(context["capability"], 1)
        self.reject_text(text)

    def test_wrong_case_or_exit(self):
        for case in ({"name": "groups_pass0", "actual_capability": True},
                     {"name": "occlusion16"},
                     {"name": "occlusion16", "actual_capability": False},
                     {"name": "occlusion16", "actual_capability": 1}):
            with self.subTest(case=case), self.assertRaises(AssertionError):
                SUBMIT.unsupported_receipt(self.capture, self.text, case)
        for exit_code in (0, 1, None, True, 77.0, "77"):
            with self.subTest(exit=exit_code), self.assertRaises(AssertionError):
                SUBMIT.unsupported_receipt(
                    self.capture | {"exit": exit_code}, self.text, self.case)

    def test_incomplete_or_failed_capture(self):
        changes = (
            {"reader_errors": {"stdout": [{"phase": "read"}]}},
            {"reader_errors": None}, {"timed_out": True},
            {"output_overflow": True}, {"pipe_capture_complete": False},
            {"stderr_bytes": 1},
        )
        for change in changes:
            with self.subTest(change=change), self.assertRaises(AssertionError):
                SUBMIT.unsupported_receipt(self.capture | change, self.text, self.case)
        capture = dict(self.capture)
        del capture["reader_errors"]
        with self.assertRaises(AssertionError):
            SUBMIT.unsupported_receipt(capture, self.text, self.case)

    def test_missing_or_duplicate_header(self):
        for index, header in enumerate(self.headers):
            for operation, headers in (
                ("missing", self.headers[:index] + self.headers[index + 1:]),
                ("duplicate", [*self.headers, header]),
            ):
                with self.subTest(header=header, operation=operation):
                    self.reject_text("\n".join([*headers, self.marker]) + "\n")

    def test_wrong_capability_or_source_evidence(self):
        changes = (
            ("status=0", "status=1"), ("static_a=1", "static_a=0"),
            ("source_returned=1", "source_returned=0"),
            ("capability_read=1", "capability_read=0"),
            ("source_pc=20", "source_pc=19"),
            ("capability=0", "capability=1"),
            ("SUBMIT_CAPABILITY occlusion16 0", "SUBMIT_CAPABILITY occlusion16 1"),
            ("SUBMIT_CONTEXT occlusion16 4 5 1", "SUBMIT_CONTEXT occlusion16 2 1 1"),
            ("SUBMIT_CONTEXT occlusion16 4 5 1", "SUBMIT_CONTEXT occlusion16 4 5 2"),
            ("SUBMIT_FORMAT 8 8 8 8 24 0", "SUBMIT_FORMAT 8 8 8 0 24 0"),
            ("SUBMIT_FORMAT 8 8 8 8 24 0", "SUBMIT_FORMAT 8 8 8 8 0 0"),
            ("SUBMIT_VENDOR occlusion16 Authored vendor", "SUBMIT_VENDOR occlusion16 "),
        )
        for before, after in changes:
            with self.subTest(before=before, after=after):
                self.reject_text(self.text.replace(before, after))

    def test_missing_cleanup_or_extra_execution_output(self):
        for marker in ("", self.marker.replace("cleanup-complete", "cleanup-failed"),
                       self.marker.replace("N-not-entered", "N-entered")):
            with self.subTest(marker=marker):
                self.reject_text("\n".join([*self.headers, marker]) + "\n")
        for extra in ("SUBMIT occlusion16 OK", "SUBMIT_TG occlusion16 0",
                      "Source N submit: occlusion16; 100 checks", "unexpected output"):
            for text in (extra + "\n" + self.text, self.text + extra + "\n",
                         "\n".join([*self.headers, extra, self.marker]) + "\n"):
                with self.subTest(extra=extra, text=text):
                    self.reject_text(text)


if __name__ == "__main__":
    unittest.main()
