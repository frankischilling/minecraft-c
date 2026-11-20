"""Finite packet/parser controls; no JVM, game, or native process is launched."""

import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("appearance_packet", HERE / "packet.py")
packet = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packet)


def corruptions(original):
    cases = {"missing": original[:-1], "extra": original + [original[-1]],
             "duplicate-class": [original[0]] + original[:-1]}

    def changed(name, index, value):
        rows = original.copy()
        rows[index] = value
        cases[name] = rows

    for name, first, second in (("class-order", 0, 1), ("damage-order", 202, 203),
                                ("bm-order", 4242, 4243)):
        rows = original.copy()
        rows[first], rows[second] = rows[second], rows[first]
        cases[name] = rows
    fields = original[0].split("\t")
    for index, value in ((1, "0"), (1, "01"), (2, "CK"), (3, "zz"), (4, "zz"),
                         (5, "zz"), (6, "zz"), (7, "-0")):
        row = fields.copy()
        row[index] = value
        changed("class-" + str(index) + "-" + value, 0, "\t".join(row))
    changed("wrong-row-kind", 202, original[202].replace("APPEARANCE", "CLASS"))
    changed("wrong-item", 202, original[202].replace("\t1\t", "\t2\t", 1))
    changed("extra-column", 202, original[202] + "\t0")
    changed("missing-column", 202, "\t".join(original[202].split("\t")[:-1]))
    for index, value in ((2, "-2147483647"), (3, "2"), (6, "2"), (7, "true")):
        row = original[202].split("\t")
        row[index] = value
        changed("appearance-" + str(index), 202, "\t".join(row))
    for value in ("+0", "-0", "00", "2147483648", "-2147483649", " 0", "0.0", ""):
        row = original[202].split("\t")
        row[5] = value
        changed("color-" + repr(value), 202, "\t".join(row))
    for index, value in ((1, "18"), (2, "-81"), (3, "4"), (5, "0")):
        row = original[4242].split("\t")
        row[index] = value
        changed("bm-" + str(index), 4242, "\t".join(row))
    changed("bad-total", -1, "COMPLETE\t202\t38\t4040\t63")
    return cases


class PacketControls(unittest.TestCase):
    def test_complete_actual_capture(self):
        summary = packet.audit()
        self.assertEqual((summary["appearance_rows"], summary["bm_rows"]), (4040, 64))
        self.assertEqual(len(packet.reference_lines()), 4307)

    def test_each_frozen_artifact_rejects_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "packet"
            shutil.copytree(HERE, copied)
            for name in packet.PINS:
                with self.subTest(artifact=name):
                    path = copied / name
                    original = path.read_bytes()
                    path.write_bytes(original + b"x")
                    with self.assertRaises(ValueError):
                        packet.audit(copied)
                    path.write_bytes(original)

    def test_missing_artifact(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(OSError):
                packet.audit(temporary)

    def test_complete_row_corruptions(self):
        for name, rows in corruptions(packet.reference_lines()).items():
            with self.subTest(case=name):
                with self.assertRaises(ValueError):
                    packet.parse_lines(rows)

    def test_literal_observed_color_is_pinned(self):
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "packet"
            shutil.copytree(HERE, copied)
            path = copied / "original.stdout"
            rows = path.read_text(encoding="ascii").splitlines()
            row = rows[202].split("\t")
            row[5] = str(int(row[5]) - 1)
            rows[202] = "\t".join(row)
            path.write_bytes(("\n".join(rows) + "\n").encode("ascii"))
            with self.assertRaises(ValueError):
                packet.audit(copied)


if __name__ == "__main__":
    unittest.main()
