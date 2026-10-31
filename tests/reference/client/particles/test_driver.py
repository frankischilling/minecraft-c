"""Real subprocess rejection controls; executable under normal Python and -O."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

import run as driver


class DriverControls(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def invoke(self, body, timeout=10):
        return driver.run([sys.executable, "-B", "-c", body], self.root, "control", timeout)

    def test_exit_failure_with_positive_marker(self):
        with self.assertRaises(RuntimeError):
            self.invoke("print('Pickup particles: 1 checks');raise SystemExit(7)")

    def test_stderr_failure_with_positive_marker(self):
        with self.assertRaises(RuntimeError):
            self.invoke("import sys;print('Pickup particles: 1 checks');sys.stderr.write('diagnostic')")

    def test_marker_validation(self):
        for text in ("", "partial\n", "Pickup particles: 0 checks\n", "Pickup particles: 2 checks\npartial\n"):
            with self.subTest(text=text), self.assertRaises(RuntimeError):
                driver.native_count(text)
        self.assertEqual(driver.native_count("row\nPickup particles: 2 checks\n"), 2)

    def test_clean_real_process_report(self):
        self.assertEqual(driver.native_count(self.invoke("print('Pickup particles: 3 checks')")), 3)
        report = json.loads((self.root / "control.capture.json").read_text())
        self.assertEqual(report["reader_errors"], {})
        self.assertTrue(report["reaped"])
        self.assertTrue(report["pipe_capture_complete"])

    def test_output_bound(self):
        with self.assertRaises(ValueError):
            self.invoke("import sys;sys.stdout.write('x' * (3*1024*1024))")

    def test_timeout(self):
        with self.assertRaises(ValueError):
            self.invoke("import time;time.sleep(5)", timeout=1)

    def test_complete_profile_inventory(self):
        labels = driver.expected_labels()
        self.assertEqual(len(labels), 71)
        rows = ["\t".join(label) + "\tfixture" for label in labels]
        original = "\n".join(rows + ["Original particles: 71 records"])
        self.assertEqual(driver.profile_records(original, True), rows)
        for changed in (rows[:-1], rows + [rows[-1]], [rows[1], rows[0], *rows[2:]]):
            with self.assertRaises(RuntimeError):
                driver.profile_records("\n".join(changed + ["Original particles: 71 records"]), True)

    def test_clear_ambient_java_options(self):
        from unittest import mock
        with mock.patch.dict(driver.os.environ, {"JAVA_TOOL_OPTIONS": "bad", "_JAVA_OPTIONS": "bad",
                                               "JDK_JAVA_OPTIONS": "bad", "PARTICLE_SENTINEL": "kept"}):
            env = driver.controlled_environment()
        self.assertTrue(all(name not in env for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS")))
        self.assertEqual(env["PARTICLE_SENTINEL"], "kept")

    def test_origins_are_utf8_on_all_platform_codecs(self):
        jar = self.root / "path-\u00e9" / "fixture.jar"
        jar.parent.mkdir()
        jar.write_bytes(b"authored path-identity fixture, not a JAR input")
        origin = self.root / "origins.tsv"
        origin.write_text("em\t" + str(jar.resolve()) + "\n", encoding="utf-8")
        rows = driver.read_origins(origin)
        self.assertEqual(rows[0][0], "em")
        self.assertTrue(Path(rows[0][1]).samefile(jar))


if __name__ == "__main__":
    unittest.main()
