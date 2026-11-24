"""Keep native launch evidence when process or later save snapshots fail."""

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("native_run_world", HERE / "test_world_selection_loop.py")
WORLD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WORLD)


class NativeRunCaptureTests(unittest.TestCase):
    def fixture(self, root):
        executable = root / "program.exe"
        executable.write_bytes(b"original native product\0\xff")
        evidence, saves = root / "evidence", root / "saves"
        evidence.mkdir()
        saves.mkdir()
        return executable, evidence, saves

    def invoke(self, executable, evidence, saves):
        return WORLD.retained_native_run([str(executable)], {}, evidence, saves, {},
                                         "native-control", 123, 1)

    def receipt(self, evidence):
        execution = json.loads((evidence / "execution.json").read_text(encoding="utf-8"))
        products = execution["native_products"]
        self.assertEqual(products["status"], "PASS")
        product = next(p for p in products["products"] if p["role"] == "executable")
        self.assertEqual((evidence / product["artifact"]).read_bytes(), b"original native product\0\xff")
        return execution

    def test_native_failure_keeps_binary_even_when_final_snapshot_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable, evidence, saves = self.fixture(Path(temporary))
            failed = subprocess.CompletedProcess([str(executable)], 7, "native prefix\n", "native failure\n")

            def launched(*args, **kwargs):
                self.assertTrue((evidence / "native-products/manifest.json").is_file())
                executable.write_bytes(b"later build replaced the product")
                return failed

            with patch.object(WORLD.subprocess, "run", side_effect=launched), \
                    patch.object(WORLD, "save_title_files", side_effect=OSError("late snapshot")):
                self.assertIs(self.invoke(executable, evidence, saves), failed)
            execution = self.receipt(evidence)
            self.assertEqual((execution["event"], execution["exit_code"]), ("result", 7))
            self.assertEqual(execution["after_snapshot_error"]["error"], "late snapshot")
            self.assertEqual((evidence / "stdout.txt").read_bytes(), b"native prefix\n")
            self.assertEqual((evidence / "stderr.txt").read_bytes(), b"native failure\n")

    def test_launch_error_keeps_product_and_original_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable, evidence, saves = self.fixture(Path(temporary))
            failure = OSError("native launch refused")
            with patch.object(WORLD.subprocess, "run", side_effect=failure), \
                    patch.object(WORLD, "save_title_files", side_effect=OSError("late snapshot")):
                with self.assertRaises(OSError) as raised:
                    self.invoke(executable, evidence, saves)
            self.assertIs(raised.exception, failure)
            self.assertEqual(self.receipt(evidence)["event"], "launch-error")

    def test_successful_process_still_reports_snapshot_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable, evidence, saves = self.fixture(Path(temporary))
            failure = OSError("late snapshot")
            with patch.object(WORLD.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")), \
                    patch.object(WORLD, "save_title_files", side_effect=failure):
                with self.assertRaises(OSError) as raised:
                    self.invoke(executable, evidence, saves)
            self.assertIs(raised.exception, failure)
            self.assertEqual(self.receipt(evidence)["exit_code"], 0)

    def test_capture_refusal_prevents_launch_and_keeps_old_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable, evidence, saves = self.fixture(Path(temporary))
            existing = evidence / "native-products"
            existing.mkdir()
            marker = existing / "marker"
            marker.write_bytes(b"preserve existing capture")
            with patch.object(WORLD.subprocess, "run") as launch, \
                    patch.object(WORLD, "save_title_files", return_value={}):
                with self.assertRaises(RuntimeError):
                    self.invoke(executable, evidence, saves)
                launch.assert_not_called()
            execution = json.loads((evidence / "execution.json").read_text(encoding="utf-8"))
            self.assertEqual(execution["event"], "product-capture-error")
            self.assertEqual(execution["native_products"]["status"], "FAIL")
            self.assertEqual(marker.read_bytes(), b"preserve existing capture")
            self.assertEqual((evidence / "stdout.txt").read_bytes(), b"")


if __name__ == "__main__":
    unittest.main()
