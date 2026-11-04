"""Reject incomplete, reordered and altered scalar replay transcripts."""
import copy
import json
from pathlib import Path
import unittest

from run_native import compare_rows, load_packet


class NativeReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.expected = load_packet(Path(__file__).resolve().parent)

    def lines(self):
        rows = ['DYE_ICON_NATIVE_INIT\t{"id":351,"initial_bh":78}']
        rows.extend("DYE_ICON_NATIVE\t" + json.dumps({k: v for k, v in row.items()
                    if k != "item_stack_icon"}) for row in self.expected)
        return rows + ["DYE_ICON_NATIVE_COMPLETE rows=33 restored_bh=78"]

    def test_exact_scalar_projection(self):
        self.assertEqual(len(compare_rows(self.expected, "\n".join(self.lines()))), 33)

    def test_dropped_duplicate_or_reordered_row(self):
        for mode in ("drop", "duplicate", "reorder"):
            with self.subTest(mode=mode):
                lines = self.lines()
                if mode == "drop":
                    lines.pop(2)
                elif mode == "duplicate":
                    lines[2] = lines[1]
                else:
                    lines[1], lines[2] = lines[2], lines[1]
                with self.assertRaises(AssertionError):
                    compare_rows(self.expected, "\n".join(lines))

    def test_changed_signed_icon_operand_or_pass(self):
        for field in ("bh", "damage", "direct_icon", "stack_icon", "color", "full3d", "rotate"):
            with self.subTest(field=field):
                lines = self.lines()
                row = json.loads(lines[1].split("\t", 1)[1])
                row[field] = not row[field] if type(row[field]) is bool else row[field] + 1
                lines[1] = "DYE_ICON_NATIVE\t" + json.dumps(row)
                with self.assertRaises(AssertionError):
                    compare_rows(self.expected, "\n".join(lines))

    def test_changed_types_extra_fields_and_completion(self):
        for mode in ("float", "bool_as_int", "extra", "completion", "duplicate_key"):
            with self.subTest(mode=mode):
                lines = self.lines()
                row = json.loads(lines[1].split("\t", 1)[1])
                if mode == "float":
                    row["damage"] = 0.0
                elif mode == "bool_as_int":
                    row["rotate"] = 0
                elif mode == "extra":
                    row["unknown"] = 1
                elif mode == "completion":
                    lines[-1] = "DYE_ICON_NATIVE_COMPLETE rows=32 restored_bh=78"
                if mode != "completion":
                    lines[1] = "DYE_ICON_NATIVE\t" + json.dumps(row)
                    if mode == "duplicate_key":
                        lines[1] = lines[1][:-1] + ',"damage":0}'
                with self.assertRaises(AssertionError):
                    compare_rows(self.expected, "\n".join(lines))

    def test_supporting_original_item_wrapper_remains_checked(self):
        expected = copy.deepcopy(self.expected)
        expected[0]["item_stack_icon"] += 1
        with self.assertRaises(AssertionError):
            compare_rows(expected, "\n".join(self.lines()))


if __name__ == "__main__":
    unittest.main()
