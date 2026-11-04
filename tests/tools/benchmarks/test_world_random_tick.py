"""Reject changed source work/RNG, process order and callback observations."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "benchmark_random_tick", ROOT / "tests/tools/world/benchmark_random_tick.py")
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class WorldRandomTickReceipt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.receipt = json.loads((ROOT / "tests/benchmarks/world_random_tick_windows.json").read_text())

    def test_full_fixed_work_receipt(self):
        result = RUNNER.audit_receipt(self.receipt)
        self.assertEqual(result["initial"]["air_qhigh"]["baseline"]["count"], 42)
        self.assertEqual(result["confirmation"]["stone_q0"]["candidate"]["count"], 84)

    def test_skipped_samples_rejected(self):
        changed = copy.deepcopy(self.receipt)
        process = changed["cohorts"][0]["processes"][0]
        cells = process["rows"][0].split()
        cells[11] = "28879"
        process["rows"][0] = " ".join(cells)
        with self.assertRaisesRegex(AssertionError, "work required"):
            RUNNER.audit_receipt(changed)

    def test_changed_rng_rejected(self):
        changed = copy.deepcopy(self.receipt)
        process = changed["cohorts"][0]["processes"][0]
        cells = process["rows"][0].split()
        cells[8] = "000000000000"
        process["rows"][0] = " ".join(cells)
        with self.assertRaisesRegex(AssertionError, "RNG/LCG"):
            RUNNER.audit_receipt(changed)

    def test_dropped_process_rejected(self):
        changed = copy.deepcopy(self.receipt)
        changed["cohorts"][1]["processes"].pop()
        with self.assertRaisesRegex(AssertionError, "process/order"):
            RUNNER.audit_receipt(changed)

    def test_callback_prefix_change_rejected(self):
        changed = copy.deepcopy(self.receipt)
        rows = changed["controls"]["candidate_world_rows"]
        position = next(i for i, row in enumerate(rows) if row.startswith("RANDOM_SOUND "))
        rows[position] += " changed"
        with self.assertRaisesRegex(AssertionError, "callback/failure/state"):
            RUNNER.audit_receipt(changed)

    def test_full_error_record_change_rejected(self):
        changed = copy.deepcopy(self.receipt)
        rows = changed["controls"]["candidate_raw_transcript"]
        position = next(i for i, row in enumerate(rows) if "Captured Chunk null block byte array" in row)
        rows[position] = rows[position].replace("java/lang/NullPointerException", "changed")
        with self.assertRaisesRegex(AssertionError, "control transcript"):
            RUNNER.audit_receipt(changed)


if __name__ == "__main__":
    unittest.main()
