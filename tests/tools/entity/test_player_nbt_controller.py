"""Text/capture controls only; no simulated Player, World or gameplay answers."""

import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("nbt_contract_driver", HERE / "test_player_nbt.py")
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)
# During private qualification this points at the exact immutable copied cohort.
source = HERE.parents[2]
if not (source / "tests/reference/client/statistics/support.py").is_file():
    source = source / "validation/source"
capture, _ = driver.load_capture(source)


class NbtControllerControls(unittest.TestCase):
    def test_positive_final_nbt_marker(self):
        self.assertEqual(driver.completion("151 recipes\nPlayer NBT: 12 checks\n", "nbt"), 12)

    def test_marker_missing_zero_negative_and_trailing_output(self):
        for text in ("", "Player NBT: 0 checks", "Player NBT: -1 checks",
                     "Player NBT: 1 checks\npartial", "Player NBT: 1 check"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                driver.completion(text, "nbt")

    def test_actual_coordinate_fixture_completion(self):
        self.assertEqual(driver.completion("Player active tick: 22 checks\n"
                                           "Player NBT coordinates: 22 checks\n", "coordinates"), 22)

    def test_coordinate_fixture_wrong_or_missing_count(self):
        for prefix in ("", "Player active tick: 21 checks\n", "Player active tick: 0 checks\n"):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                driver.completion(prefix + "Player NBT coordinates: 22 checks\n", "coordinates")

    def child(self, body):
        with tempfile.TemporaryDirectory(prefix="nbt-capture-control-") as root:
            return capture.run_bounded([sys.executable, "-B", "-c", body], cwd=root,
                                       prefix="child", timeout=5, limit=65536)

    def test_real_clean_child(self):
        result = self.child("print('Player NBT: 1 checks')")
        driver.require_process(result)

    def test_real_failed_exit_overrides_marker(self):
        result = self.child("print('Player NBT: 1 checks');raise SystemExit(7)")
        with self.assertRaises(ValueError):
            driver.require_process(result)

    def test_real_stderr_overrides_marker(self):
        result = self.child("import sys;print('Player NBT: 1 checks');sys.stderr.write('diagnostic')")
        with self.assertRaises(ValueError):
            driver.require_process(result)

    def test_reader_receipt_required_even_with_clean_exit(self):
        result = self.child("print('Player NBT: 1 checks')")
        receipt = copy.deepcopy(result.capture_report)
        for changed in (None, {"stderr": [{"phase": "write", "type": "OSError"}]}):
            result.capture_report = dict(receipt)
            if changed is None:
                result.capture_report.pop("reader_errors")
            else:
                result.capture_report["reader_errors"] = changed
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                driver.require_process(result)


if __name__ == "__main__":
    unittest.main()
