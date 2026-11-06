"""The inventory replay must reject incomplete or rebound method-family evidence."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import FILES, HERE, read_packet


class PacketTests(unittest.TestCase):
    def copy(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        for name in (*FILES, "provenance.json"):
            shutil.copyfile(HERE / name, directory / name)
        return directory

    def rebind(self, directory, name):
        path = directory / "provenance.json"
        data = json.loads(path.read_text())
        data["files_sha256"][name] = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        path.write_text(json.dumps(data), encoding="utf-8")

    def test_complete_matrix(self):
        packet = read_packet()
        self.assertEqual(len(packet["declarations"]), 96)
        self.assertEqual(len(packet["native_rows"]), 13098)
        self.assertEqual(len(packet["cube_ids"]), 57)

    def test_changed_color_bits(self):
        directory = self.copy()
        path = directory / "observations.tsv"
        data = path.read_text().replace("COLOR\t1\t0\t00ffffff", "COLOR\t1\t0\t00000000", 1)
        path.write_text(data, encoding="ascii", newline="\n")
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_forged_declaring_class_even_after_rebind(self):
        directory = self.copy()
        path = directory / "observations.tsv"
        path.write_text(path.read_text().replace("DECL\t1\teo\tuu\tuu\tuu", "DECL\t1\teo\tbk\tuu\tuu", 1),
                        encoding="ascii", newline="\n")
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_incomplete_intermediate_inventory_even_after_rebind(self):
        directory = self.copy()
        path = directory / "source-pins.json"
        data = json.loads(path.read_text())
        data["classes"]["jp"]["declared_methods_complete"] = False
        path.write_text(json.dumps(data), encoding="utf-8")
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_wrong_geometry_partition_even_after_rebind(self):
        directory = self.copy()
        path = directory / "observations.tsv"
        path.write_text(path.read_text().replace("BLOCK\t1\t0\n", "BLOCK\t1\t13\n", 1),
                        encoding="ascii", newline="\n")
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_raw_lf_transport_even_after_rebind(self):
        directory = self.copy()
        path = directory / "observations.tsv"
        path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)


if __name__ == "__main__":
    unittest.main()
