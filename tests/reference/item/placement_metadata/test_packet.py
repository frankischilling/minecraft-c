"""Finite packet integrity and malformed-row controls; no JVM or native launch."""

import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("placement_metadata_packet", HERE / "packet.py")
packet = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packet)


class PacketControls(unittest.TestCase):
    def test_complete_actual_capture(self):
        self.assertEqual(packet.audit()["metadata_rows"], 4040)
        self.assertEqual(len(packet.reference_lines()), 4243)

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

    def test_complete_row_corruption_controls(self):
        original = packet.reference_lines()
        cases = {}

        def changed(name, index, value):
            rows = original.copy()
            rows[index] = value
            cases[name] = rows

        cases["missing"] = original[:-1]
        cases["extra"] = original + [original[-1]]
        cases["duplicate-class"] = [original[0]] + original[:-1]
        rows = original.copy()
        rows[0], rows[1] = rows[1], rows[0]
        cases["class-order"] = rows
        rows = original.copy()
        rows[202], rows[203] = rows[203], rows[202]
        cases["operand-order"] = rows
        changed("wrong-row-kind", 202, original[202].replace("METADATA", "CLASS"))
        changed("wrong-owner", 0, "CLASS\t1\tck\tqr")
        changed("unknown-owner", 0, "CLASS\t1\tck\tzz")
        changed("invalid-receiver", 0, "CLASS\t1\tCK\tgm")
        changed("zero-id", 0, "CLASS\t0\tck\tgm")
        changed("noncanonical-id", 0, "CLASS\t01\tck\tgm")
        changed("wrong-metadata-id", 202, "METADATA\t2\t-2147483648\t0")
        changed("wrong-operand", 202, "METADATA\t1\t-2147483647\t0")
        changed("extra-column", 202, original[202] + "\t0")
        changed("missing-column", 202, "METADATA\t1\t-2147483648")
        changed("bad-total", -1, "COMPLETE\t202\t38\t4039")
        for value in ("+0", "-0", "00", "2147483648", "-2147483649", " 0", "0.0", ""):
            changed("result-" + repr(value), 202, "METADATA\t1\t-2147483648\t" + value)
        for name, rows in cases.items():
            with self.subTest(case=name):
                with self.assertRaises(ValueError):
                    packet.parse_lines(rows)

    def test_literal_observed_value_is_pinned(self):
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "packet"
            shutil.copytree(HERE, copied)
            path = copied / "original.stdout"
            data = path.read_bytes()
            path.write_bytes(data.replace(b"METADATA\t1\t-2147483648\t0\n",
                                          b"METADATA\t1\t-2147483648\t1\n", 1))
            with self.assertRaises(ValueError):
                packet.audit(copied)


if __name__ == "__main__":
    unittest.main()
