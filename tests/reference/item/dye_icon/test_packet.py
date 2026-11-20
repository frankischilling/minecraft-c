"""Public packet integrity and finite corruption controls."""
import shutil
import tempfile
import unittest
from pathlib import Path
import packet
class PacketTests(unittest.TestCase):
    def test_original(self): self.assertEqual(len(packet.read_packet()),33)
    def test_each_artifact_corruption(self):
        for name in packet.PINS:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temp:
                dst=Path(temp)
                for p in Path(__file__).resolve().parent.iterdir():
                    if p.is_file(): shutil.copy2(p,dst/p.name)
                target=dst/name
                target.write_bytes(target.read_bytes()+b'X')
                with self.assertRaises((ValueError,UnicodeError)): packet.read_packet(dst)
    def test_missing_raw_repeat(self):
        with tempfile.TemporaryDirectory() as temp:
            dst=Path(temp)
            for p in Path(__file__).resolve().parent.iterdir():
                if p.is_file(): shutil.copy2(p,dst/p.name)
            (dst/'ordinary-1.stdout').unlink()
            with self.assertRaises(OSError): packet.read_packet(dst)
if __name__=='__main__': unittest.main()
