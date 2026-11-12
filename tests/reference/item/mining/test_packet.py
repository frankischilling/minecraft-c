"""Reject incomplete or internally inconsistent original mining packets."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import read_packet

HERE = Path(__file__).resolve().parent


class PacketTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        for name in ("provenance.json", "observations.tsv", "classes.tsv", "source-pins.json"):
            shutil.copyfile(HERE / name, self.directory / name)

    def tearDown(self):
        self.temporary.cleanup()

    def replace(self, mutate, refresh=True):
        path = self.directory / "observations.tsv"
        lines = path.read_text().splitlines()
        mutate(lines)
        path.write_text("\n".join(lines) + "\n")
        if refresh:
            proof = json.loads((self.directory / "provenance.json").read_text())
            proof["observations_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            (self.directory / "provenance.json").write_text(json.dumps(proof))
        with self.assertRaises(ValueError):
            read_packet(self.directory)

    def test_complete(self):
        self.assertEqual(len(read_packet(self.directory)), 19595)

    def test_hash(self):
        self.replace(lambda lines: lines.__setitem__(0, lines[1]), refresh=False)

    def test_missing(self):
        self.replace(lambda lines: lines.pop(0))

    def test_duplicate(self):
        self.replace(lambda lines: lines.__setitem__(0, lines[1]))

    def test_speed(self):
        self.replace(lambda lines: lines.__setitem__(0, lines[0].replace("3f800000", "nan")))

    def test_correspondence(self):
        def change(lines):
            row = lines[0].split("\t")
            row[5] = "40000000"
            lines[0] = "\t".join(row)
        self.replace(change)

    def test_null_block_inventory(self):
        def change(lines):
            row = lines[0].split("\t")
            row[8] = "0"
            lines[0] = "\t".join(row)
        self.replace(change)

    def test_class_hash(self):
        (self.directory / "classes.tsv").write_text("CLASS\t1\tgm\tgm\tgm\n")
        with self.assertRaises(ValueError):
            read_packet(self.directory)

    def test_method_hash(self):
        (self.directory / "source-pins.json").write_text("{}\n")
        with self.assertRaises(ValueError):
            read_packet(self.directory)


if __name__ == "__main__":
    unittest.main()
