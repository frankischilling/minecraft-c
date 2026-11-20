"""Reject damaged scene rows and changed original or authored inputs."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import HERE, digest, read_packet


class ScenePacketTests(unittest.TestCase):
    def test_packet(self):
        self.assertEqual(len(read_packet()), 160)

    def test_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for source in HERE.iterdir():
                if source.is_file():
                    shutil.copyfile(source, root / source.name)
            receipt = json.loads((root / 'provenance.json').read_text())
            for name in (*receipt['captured_helpers_sha256'], 'selected.txt'):
                raw = (root / name).read_bytes()
                broken = bytearray(raw)
                broken[len(raw) // 2] ^= 1
                (root / name).write_bytes(broken)
                with self.subTest(name=name), self.assertRaises((AssertionError, ValueError)):
                    read_packet(root)
                (root / name).write_bytes(raw)
            rows = (root / 'selected.txt').read_text().splitlines()
            for selected in (rows[:-1], [rows[1], rows[0], *rows[2:]],
                             [rows[0].rsplit(',', 1)[0], *rows[1:]]):
                (root / 'selected.txt').write_text('\n'.join(selected) + '\n', encoding='ascii')
                receipt['selected_sha256'] = digest(root / 'selected.txt')
                (root / 'provenance.json').write_text(json.dumps(receipt), encoding='ascii')
                with self.assertRaises(AssertionError):
                    read_packet(root)

    def test_reject_untranslated_predecessor(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for source in HERE.iterdir():
                if source.is_file():
                    shutil.copyfile(source, root / source.name)
            shutil.copyfile(HERE / 'history/capture2/selected.txt', root / 'selected.txt')
            receipt = json.loads((root / 'provenance.json').read_text())
            receipt['selected_sha256'] = digest(root / 'selected.txt')
            (root / 'provenance.json').write_text(json.dumps(receipt), encoding='ascii')
            with self.assertRaises(AssertionError):
                read_packet(root)


if __name__ == '__main__':
    unittest.main()
