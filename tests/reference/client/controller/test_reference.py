"""Reject malformed controller records and incomplete process evidence."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

from capture import capture, digest, validate_native_result
from capture_reference import rows


def fixture():
    result = ["CTRLCTOR 0 1 0 -1 -1 -1 00000000 00000000 00000000 0",
              "CTRLCTOR 1 1 0 -1 -1 -1 00000000 00000000 00000000 0"]
    for scenario in range(9):
        completed = scenario < 3
        failure = "dc <init>" if scenario == 6 else "ob b"
        result.append("CTRLNEW " + str(scenario) + " " + ("1 - -" if completed else "0 " + failure) +
                      (" 1" if completed or scenario == 6 else " 0") +
                      (" 1 1 1 0" if completed else " 0 0 0 0") + " 0000000000000000" * 3 +
                      " 00000000 00000000")
        if completed:
            result.extend("CTRLYAW " + str(scenario) + " " + str(k) + " c3340000 c1300000"
                          for k in range(3))
    return result


class RecordTests(unittest.TestCase):
    def test_complete_inventory(self):
        self.assertEqual(rows("\n".join(fixture())), fixture())

    def test_missing(self):
        with self.assertRaises(Exception):
            rows("\n".join(fixture()[:-1]))

    def test_duplicate(self):
        data = fixture()
        data[-1] = data[-2]
        with self.assertRaises(Exception):
            rows("\n".join(data))

    def test_wrong_failure(self):
        data = "\n".join(fixture()).replace("CTRLNEW 6 0 dc <init>", "CTRLNEW 6 0 ob b")
        with self.assertRaises(Exception):
            rows(data)

    def test_wrong_yaw(self):
        with self.assertRaises(Exception):
            rows("\n".join(fixture()).replace("c3340000", "00000000", 1))

    def test_ctor_negative_zero(self):
        with self.assertRaises(Exception):
            rows("\n".join(fixture()).replace("00000000", "80000000", 1))

    def test_wrong_id_prefix(self):
        with self.assertRaises(Exception):
            rows("\n".join(fixture()).replace("CTRLNEW 3 0 ob b 0", "CTRLNEW 3 0 ob b 1"))


class ProcessTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        command = [sys.executable, "-c", "print('PASS: 1 single-player controller checks')"]
        self.report = capture(command, self.directory, "native", 10)
        value = digest(Path(sys.executable))
        self.report.update(executable_sha256=value, executable_before_sha256=value,
                           executable_after_sha256=value, executable_unchanged=True,
                           mode="controller", checks=1)
        self.receipt = self.directory / "run.json"

    def tearDown(self):
        self.temporary.cleanup()

    def validate(self, change=None):
        report = copy.deepcopy(self.report)
        if change:
            change(report)
        self.receipt.write_text(json.dumps(report), encoding="utf-8")
        return validate_native_result(self.directory / "native.stdout", self.receipt, "controller")

    def test_complete_process(self):
        self.assertEqual(self.validate()["checks"], 1)

    def test_failed_exit(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(exit=1))

    def test_boolean_exit(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(exit=False))

    def test_timeout(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(timed_out=True))

    def test_reader_error(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(reader_errors={"stdout": ["failure"]}))

    def test_incomplete_pipe(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(pipe_capture_complete=False))

    def test_overflow(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(output_overflow=True))

    def test_wrong_executable(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(executable_before_sha256="0" * 64))

    def test_wrong_mode(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(mode="spawn-reference"))

    def test_changed_stdout(self):
        with (self.directory / "native.stdout").open("ab") as output:
            output.write(b"extra\n")
        with self.assertRaises(Exception):
            self.validate()

    def test_wrong_check_count(self):
        with self.assertRaises(Exception):
            self.validate(lambda r: r.update(checks=2))

    def actual_process(self, program):
        command = [sys.executable, "-c", program]
        report = capture(command, self.directory, "actual-control", 10)
        value = digest(Path(sys.executable))
        report.update(executable_sha256=value, executable_before_sha256=value,
                      executable_after_sha256=value, executable_unchanged=True,
                      mode="controller", checks=1)
        receipt = self.directory / "actual-control.json"
        receipt.write_text(json.dumps(report), encoding="utf-8")
        return receipt

    def test_actual_nonzero_exit_with_terminal_marker(self):
        receipt = self.actual_process("print('PASS: 1 single-player controller checks'); raise SystemExit(7)")
        self.assertEqual(json.loads(receipt.read_text())["exit"], 7)
        with self.assertRaises(Exception):
            validate_native_result(self.directory / "actual-control.stdout", receipt, "controller")

    def test_actual_stderr_with_terminal_marker(self):
        receipt = self.actual_process("import sys; print('PASS: 1 single-player controller checks'); sys.stderr.write('control\\n')")
        self.assertEqual(json.loads(receipt.read_text())["exit"], 0)
        with self.assertRaises(Exception):
            validate_native_result(self.directory / "actual-control.stdout", receipt, "controller")

    def test_actual_success_without_terminal_marker(self):
        receipt = self.actual_process("print('completed without a terminal marker')")
        self.assertEqual(json.loads(receipt.read_text())["exit"], 0)
        with self.assertRaises(Exception):
            validate_native_result(self.directory / "actual-control.stdout", receipt, "controller")


if __name__ == "__main__":
    unittest.main()
