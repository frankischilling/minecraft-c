"""Admit only a complete raw-bit position record inventory."""
import json
from pathlib import Path
import unittest

from capture_position import rows


class Records(unittest.TestCase):
    def setUp(self):
        self.lines = ["POSITION " + str(index) + " " + " ".join(["0" * 16] * 9)
                      for index in range(36)]

    def parse(self, lines):
        return rows("\n".join(lines))

    def test_complete(self):
        self.assertEqual(self.parse(self.lines), self.lines)

    def test_missing(self):
        with self.assertRaises(AssertionError):
            self.parse(self.lines[:-1])

    def test_extra(self):
        with self.assertRaises(AssertionError):
            self.parse(self.lines + self.lines[:1])

    def test_reordered(self):
        self.lines[0], self.lines[1] = self.lines[1], self.lines[0]
        with self.assertRaises(AssertionError):
            self.parse(self.lines)

    def test_short(self):
        self.lines[0] = " ".join(self.lines[0].split()[:-1])
        with self.assertRaises(AssertionError):
            self.parse(self.lines)

    def test_malformed_bits(self):
        for value in ["0" * 15, "0" * 17, "7FF8000000000000", "+000000000000000", "nan"]:
            with self.subTest(value=value):
                lines = list(self.lines)
                cells = lines[0].split()
                cells[2] = value
                lines[0] = " ".join(cells)
                with self.assertRaises(AssertionError):
                    self.parse(lines)


class NativeCompletion(unittest.TestCase):
    def setUp(self):
        import tempfile
        self.directory = tempfile.TemporaryDirectory()
        self.output = Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)

    def process(self, code="print('PASS: 1 Player spawn reset checks')"):
        import sys
        from capture import capture, digest
        result = capture([sys.executable, "-c", code], self.output, "native", 10)
        executable_hash = digest(Path(sys.executable))
        result.update(executable_before_sha256=executable_hash,
                      executable_after_sha256=executable_hash,
                      executable_sha256=executable_hash, executable_unchanged=True,
                      checks=1, mode="spawn-reference")
        self.save(result)
        return result

    def save(self, result):
        (self.output / "run.json").write_text(json.dumps(result))

    def admit(self):
        from capture import validate_native_result
        return validate_native_result(self.output / "native.stdout", self.output / "run.json",
                                      "spawn-reference")

    def test_completed_process(self):
        self.process()
        self.assertEqual(self.admit()["exit"], 0)

    def test_nonzero_process(self):
        self.process("print('PASS: 1 Player spawn reset checks'); raise SystemExit(1)")
        with self.assertRaises(AssertionError):
            self.admit()

    def test_incomplete_capture(self):
        result = self.process()
        result["pipe_capture_complete"] = False
        self.save(result)
        with self.assertRaises(AssertionError):
            self.admit()

    def test_missing_marker(self):
        self.process("print('numeric rows alone')")
        with self.assertRaises(AssertionError):
            self.admit()

    def test_stderr_failure(self):
        self.process("import sys; print('PASS: 1 Player spawn reset checks'); print('failure', file=sys.stderr)")
        with self.assertRaises(AssertionError):
            self.admit()

    def test_changed_stdout(self):
        self.process()
        with (self.output / "native.stdout").open("ab") as stream:
            stream.write(b"changed")
        with self.assertRaises(AssertionError):
            self.admit()

    def test_changed_executable_fingerprint(self):
        result = self.process()
        result["executable_after_sha256"] = "0" * 64
        self.save(result)
        with self.assertRaises(AssertionError):
            self.admit()

    def test_wrong_mode(self):
        result = self.process()
        result["mode"] = "position-probe"
        self.save(result)
        with self.assertRaises(AssertionError):
            self.admit()

    def test_missing_receipt(self):
        from capture import validate_native_result
        self.process()
        with self.assertRaises(AssertionError):
            validate_native_result(self.output / "native.stdout", None, "spawn-reference")


if __name__ == "__main__":
    unittest.main()
