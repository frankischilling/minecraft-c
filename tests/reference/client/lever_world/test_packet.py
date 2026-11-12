"""Reject damaged Lever evidence and altered captured helpers."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import HERE, digest, read_packet


class LeverPacketTests(unittest.TestCase):
    def test_original_rows(self):
        self.assertEqual(len(read_packet()), 32)

    def test_corruptions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for source in HERE.iterdir():
                if source.is_file():
                    shutil.copyfile(source, root / source.name)
            original = (root / 'selected.txt').read_bytes()
            for marker in (b'LEVER', b'\t0\t-1\t', b'\ttrue\t', b'\tfalse\t', b'\t8\t', b'41040000'):
                with self.subTest(marker=marker):
                    (root / 'selected.txt').write_bytes(original.replace(marker, b'BROKEN', 1))
                    with self.assertRaises(AssertionError):
                        read_packet(root)
            (root / 'selected.txt').write_bytes(original)
            provenance = json.loads((root / 'provenance.json').read_text())
            for name in provenance['captured_helpers_sha256']:
                data = (root / name).read_bytes()
                (root / name).write_bytes(data + b'\n')
                with self.assertRaises(AssertionError):
                    read_packet(root)
                (root / name).write_bytes(data)
            # Even a rehashed row with missing native words violates the
            # finite schema rather than becoming a new oracle silently.
            row = original.decode('ascii').splitlines()[0].rsplit(',', 1)[0]
            rows = original.decode('ascii').splitlines()
            rows[0] = row
            (root / 'selected.txt').write_text('\n'.join(rows) + '\n', encoding='ascii')
            provenance['selected_sha256'] = digest(root / 'selected.txt')
            (root / 'provenance.json').write_text(json.dumps(provenance), encoding='ascii')
            with self.assertRaises(AssertionError):
                read_packet(root)


if __name__ == '__main__':
    unittest.main()
