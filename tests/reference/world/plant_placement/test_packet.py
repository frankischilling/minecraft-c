"""Finite packet corruption controls; no original or native process launch."""

import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("plant_packet", HERE / "packet.py")
packet = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packet)


def corruptions(original):
    cases = {"missing": original[:-1], "extra": original + [original[-1]],
             "duplicate": [original[0]] + original[:-1]}

    def changed(name, index, value):
        rows = original.copy()
        rows[index] = value
        cases[name] = rows

    for name, first, second in (("class-order", 0, 1), ("channel-order", 8, 9), ("profile-order", 8, 11)):
        rows = original.copy()
        rows[first], rows[second] = rows[second], rows[first]
        cases[name] = rows
    for column in range(1, 6):
        row = original[0].split("\t")
        row[column] = "999" if column == 1 else "zz"
        changed("class-column-" + str(column), 0, "\t".join(row))
    for column in range(1, 11):
        row = original[8].split("\t")
        row[column] = "wrong" if column == 2 else str(int(row[column]) + 1)
        changed("plant-column-" + str(column), 8, "\t".join(row))
    for column, value in ((11, "TRUE"), (11, "UNRETURNED"), (12, "java.lang.IllegalStateException"),
                           (13, "1"), (13, "-1"), (13, "328705")):
        row = original[8].split("\t")
        row[column] = value
        changed(f"outcome-{column}-{value}", 8, "\t".join(row))
    for value in ("+0", "-0", "00", "2147483648", "-2147483649", " 0", "0.0", ""):
        row = original[8].split("\t")
        row[13] = value
        changed("noncanonical-" + repr(value), 8, "\t".join(row))
    changed("extra-column", 8, original[8] + "\t0")
    changed("missing-column", 8, "\t".join(original[8].split("\t")[:-1]))
    changed("wrong-row-kind", 8, original[8].replace("PLANT", "CLASS"))
    changed("bad-total", -1, "COMPLETE\t8\t288\t863")
    row = original[-2].split("\t")
    row[11:13] = ["false", "NONE"]
    changed("lost-null-prefix", -2, "\t".join(row))
    return cases


class PacketControls(unittest.TestCase):
    def test_actual_complete_packet(self):
        result = packet.audit()
        self.assertEqual((result["profiles"], result["calls"], result["source_exceptions"]), (288, 864, 24))
        self.assertEqual(len(packet.reference_lines()), 873)

    def test_all_artifact_pins(self):
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

    def test_row_corruptions(self):
        profiles = packet.parse_profiles((HERE / "profiles.tsv").read_bytes())
        for name, rows in corruptions(packet.reference_lines()).items():
            with self.subTest(case=name):
                with self.assertRaises(ValueError):
                    packet.parse_lines(rows, profiles)

    def test_profile_corruptions(self):
        data = (HERE / "profiles.tsv").read_bytes()
        lines = data.splitlines(keepends=True)
        changed = [b"".join(lines[1:]), data + lines[-1],
                   lines[1] + lines[0] + b"".join(lines[2:]), data.replace(b"\n", b"\r\n"),
                   data.replace(b"below_6_0", b"duplicate", 1),
                   data.replace(b"\t64\t", b"\t064\t", 1),
                   data.replace(b"\t64\t", b"\t2147483648\t", 1),
                   data.replace(b"\t64\t", b"\t65\t", 1)]
        for index, mutation in enumerate(changed):
            with self.subTest(profile=index):
                with self.assertRaises(ValueError):
                    packet.parse_profiles(mutation)

    def test_literal_result_and_missing_packet(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(OSError):
                packet.audit(temporary)
            copied = Path(temporary) / "packet"
            shutil.copytree(HERE, copied)
            path = copied / "original.stdout"
            data = path.read_bytes()
            self.assertIn(b"\tfalse\tNONE\t0\n", data)
            path.write_bytes(data.replace(b"\tfalse\tNONE\t0\n", b"\ttrue\tNONE\t0\n", 1))
            with self.assertRaises(ValueError):
                packet.audit(copied)


if __name__ == "__main__":
    unittest.main()
