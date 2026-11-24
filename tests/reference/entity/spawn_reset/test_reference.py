"""Check record admission without embedding original numeric answers."""
import json
from pathlib import Path
import unittest

from capture_reference import rows


class Records(unittest.TestCase):
    def setUp(self):
        cases = json.loads((Path(__file__).parent / "scenarios.json").read_text())["scenarios"]
        self.records = []
        for body in range(2):
            for case in cases:
                failure = case.get("failure")
                prefix = ["SPAWN", str(body), str(case["id"]), "0" if failure else "1",
                          failure["class"] if failure else "-", failure["method"] if failure else "-"]
                self.records.append(" ".join(prefix + ["0" * 16] * 18 + ["0" * 8] * 8 +
                                             ["0"] * 4 + ["0" * 12] * 2))

    def valid(self, records):
        return rows("\n".join(records))

    def test_accepts_exact_inventory(self):
        self.assertEqual(self.valid(self.records), self.records)

    def test_missing(self):
        with self.assertRaises(AssertionError):
            self.valid(self.records[:-1])

    def test_extra(self):
        with self.assertRaises(AssertionError):
            self.valid(self.records + self.records[:1])

    def test_order(self):
        self.records[0], self.records[1] = self.records[1], self.records[0]
        with self.assertRaises(AssertionError):
            self.valid(self.records)

    def test_body(self):
        self.records[0] = self.records[0].replace("SPAWN 0 0", "SPAWN 1 0")
        with self.assertRaises(AssertionError):
            self.valid(self.records)

    def test_completion(self):
        self.records[8] = self.records[8].replace(" 0 sn e ", " 1 sn e ")
        with self.assertRaises(AssertionError):
            self.valid(self.records)

    def test_failure_frame(self):
        self.records[8] = self.records[8].replace(" sn e ", " gs t_ ")
        with self.assertRaises(AssertionError):
            self.valid(self.records)

    def test_short(self):
        self.records[0] = " ".join(self.records[0].split()[:-1])
        with self.assertRaises(AssertionError):
            self.valid(self.records)

    def test_widths(self):
        for offset, value in [(6, "0" * 15), (24, "0" * 9), (36, "0" * 13), (32, "+0")]:
            with self.subTest(offset=offset):
                records = list(self.records)
                cells = records[0].split()
                cells[offset] = value
                records[0] = " ".join(cells)
                with self.assertRaises(AssertionError):
                    self.valid(records)


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
