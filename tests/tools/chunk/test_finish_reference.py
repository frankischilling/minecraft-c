import importlib.util
import copy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("finish_reference", ROOT / "scripts/reference/chunk/compare_finish.py")
FINISH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FINISH)
INTEROP_SPEC = importlib.util.spec_from_file_location("finish_interop", ROOT / "tests/tools/chunk/test_finish_interop.py")
INTEROP = importlib.util.module_from_spec(INTEROP_SPEC)
INTEROP_SPEC.loader.exec_module(INTEROP)
with patch.dict(sys.modules, {"compare_finish": FINISH}):
    EXPORT_SPEC = importlib.util.spec_from_file_location("finish_export", ROOT / "scripts/reference/chunk/export_finish.py")
    EXPORT = importlib.util.module_from_spec(EXPORT_SPEC)
    EXPORT_SPEC.loader.exec_module(EXPORT)


class FinishReferenceTests(unittest.TestCase):
    def test_public_fixtures_require_every_fixed_case_and_both_sides(self):
        fixture = {"schema": 1, "cases": [
            {"index": number, "coordinates": case["coordinates"], "sides": {"client": {}, "server": {}}}
            for number, case in enumerate(FINISH.cases())]}
        INTEROP.validate_fixture(fixture)
        for mutation in ("missing_case", "duplicate_case", "empty_sides", "missing_side", "extra_side", "coordinates"):
            changed = copy.deepcopy(fixture)
            if mutation == "missing_case":
                changed["cases"].pop()
            elif mutation == "duplicate_case":
                changed["cases"][-1] = changed["cases"][0]
            elif mutation == "empty_sides":
                changed["cases"][0]["sides"] = {}
            elif mutation == "missing_side":
                del changed["cases"][0]["sides"]["server"]
            elif mutation == "extra_side":
                changed["cases"][0]["sides"]["extra"] = {}
            else:
                changed["cases"][0]["coordinates"] = [17, 17]
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                INTEROP.validate_fixture(changed)

    def test_export_requires_complete_original_case_selection(self):
        corpus = FINISH.cases()
        report = {"selected_cases": list(range(len(corpus))), "cases": [
            {"index": number, "coordinates": case["coordinates"], "sides": {"client": {}, "server": {}}}
            for number, case in enumerate(corpus)]}
        EXPORT.validate_capture_cases(report, corpus)
        for mutation in ("duplicate_case", "missing_side", "coordinates", "selected_cases"):
            changed = copy.deepcopy(report)
            if mutation == "duplicate_case":
                changed["cases"][-1] = changed["cases"][0]
            elif mutation == "missing_side":
                del changed["cases"][0]["sides"]["client"]
            elif mutation == "coordinates":
                changed["cases"][0]["coordinates"] = [17, 17]
            else:
                changed["selected_cases"] = list(reversed(changed["selected_cases"]))
            with self.subTest(mutation=mutation), self.assertRaises(FINISH.Failure):
                EXPORT.validate_capture_cases(changed, corpus)

    def test_packed_key_keeps_sign_and_aliases(self):
        self.assertEqual(FINISH.cache_key(0, 0), 0)
        self.assertEqual(FINISH.cache_key(32768, 0), 0)
        self.assertEqual(FINISH.cache_key(-1, -1), -1)
        self.assertEqual(FINISH.cache_key(-32769, -32769), -1)
        self.assertNotEqual(FINISH.cache_key(0, 0), FINISH.cache_key(-32768, 0))

    def test_signed_wrap(self):
        self.assertEqual(FINISH.i32(2147483647 * 16), -16)
        self.assertEqual(FINISH.i32(-2147483648 * 16), 0)
        self.assertEqual(FINISH.i32(2147483647 + 1), -2147483648)

    def test_corpus_has_complete_distinct_coordinate_cases(self):
        corpus = FINISH.cases()
        self.assertEqual(len(corpus), 6)
        self.assertEqual(len({tuple(case["coordinates"]) for case in corpus}), 6)
        for case in corpus:
            self.assertEqual(len(case["initial"]), 32768)
            self.assertEqual(len(case["repeat"]), 32768)
            self.assertNotEqual(case["initial"], case["repeat"])

    def test_world_height_uses_mapped_identity_and_unsigned_bytes(self):
        heightmap = bytearray(256)
        heightmap[255] = 254
        world = {"map": {"entries": [{"key": -1, "value": "mapped"}]},
                 "chunks": [{"object": {"ref": "mapped"}, "empty": False,
                             "heightmap": {"values": heightmap.hex()}}]}
        self.assertEqual(FINISH.world_height(world, -1, -1), 254)
        self.assertEqual(FINISH.world_height(world, -524289, -524289), 254)
        self.assertEqual(FINISH.world_height(world, 0, 0), 0)
        world["chunks"][0]["empty"] = True
        self.assertEqual(FINISH.world_height(world, -1, -1), 0)

    def test_height_border_asymmetry(self):
        world = {"map": {"entries": []}, "chunks": []}
        for x, z in ((32000000, 0), (0, 32000001), (-32000001, 0), (0, -32000001)):
            self.assertEqual(FINISH.world_height(world, x, z), 0)


if __name__ == "__main__":
    unittest.main()
