"""Changed-copy packet rejection; pure controls do not qualify native execution."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('chat_packet_controls_subject', HERE / 'packet.py')
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


class PacketControls(unittest.TestCase):
    def test_original_packet(self):
        self.assertEqual(len(P.load()), 81)

    def copy_packet(self, root):
        directory = root / P.FOLDER
        directory.mkdir(parents=True)
        for name in (*P.SOURCES, 'finite.json', 'provenance.json'):
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(HERE / name, target)
        return directory

    def test_changed_source_and_resource_bytes(self):
        with tempfile.TemporaryDirectory(prefix='chat-packet-input-controls-') as temporary:
            root = Path(temporary)
            directory = self.copy_packet(root)
            self.assertEqual(len(P.load(root)), 81)
            for name in P.SOURCES:
                path = directory / name
                raw = path.read_bytes()
                with self.subTest(source=name), self.assertRaises(ValueError):
                    path.write_bytes(raw + b'\n')
                    P.load(root)
                path.write_bytes(raw)

    def test_changed_proof(self):
        original = P.read(HERE / 'provenance.json')
        changes = (
            lambda value: value.update(rows=True),
            lambda value: value.update(rows=80),
            lambda value: value.update(scope='original full draw'),
            lambda value: value.update(extra=1),
            lambda value: value.update(client_jar_sha256='0' * 64),
            lambda value: value.update(finite_sha256='0' * 64),
            lambda value: value.update(input_unchanged=False),
            lambda value: value.update(helper_unchanged=1),
            lambda value: value['source_sha256'].pop('inputs/font.txt'),
            lambda value: value['source_sha256'].__setitem__('extra.py', '0' * 64),
            lambda value: value['helper_sha256'].pop('ChatObservation$1.class'),
            lambda value: value['tools'].__setitem__('java', True),
            lambda value: value['vms'].pop(),
            lambda value: value['vms'][0].update(exit=True),
            lambda value: value['vms'][0].update(exit=1),
            lambda value: value['vms'][0].update(timed_out=True),
            lambda value: value['vms'][0].update(output_overflow=True),
            lambda value: value['vms'][0].update(pipe_capture_complete=False),
            lambda value: value['vms'][0].update(stdout_bytes=True),
            lambda value: value['vms'][0].update(stdout_sha256='0' * 64),
            lambda value: value['vms'][0].update(stderr_bytes=-1),
            lambda value: value['vms'][0].update(extra=1),
        )
        with tempfile.TemporaryDirectory(prefix='chat-packet-proof-controls-') as temporary:
            root = Path(temporary)
            directory = self.copy_packet(root)
            path = directory / 'provenance.json'
            for index, change in enumerate(changes):
                with self.subTest(proof=index), self.assertRaises(ValueError):
                    value = copy.deepcopy(original)
                    change(value)
                    path.write_text(json.dumps(value), encoding='utf-8')
                    P.load(root)
            raw = (HERE / 'provenance.json').read_bytes()
            for changed in (raw.replace(b'"rows": 81', b'"rows": 81, "rows": 81'),
                            raw.replace(b'"rows": 81', b'"rows": NaN')):
                with self.subTest(raw=changed[:24]), self.assertRaises(ValueError):
                    path.write_bytes(changed)
                    P.load(root)

    def test_changed_finite_and_rows(self):
        original = P.read(HERE / 'finite.json')
        changes = (
            lambda value: value.update(format=True),
            lambda value: value.update(scope='complete original Font construction'),
            lambda value: value.update(extra=1),
            lambda value: value['rows'].pop(),
            lambda value: value['rows'].__setitem__(0, 'CHAT_CASE 0 1 10 1 0 0 extra'),
            lambda value: value['rows'].__setitem__(1, 'CHAT_LINE 0 0 0 null 0041'),
            lambda value: value['rows'].__setitem__(1, True),
            lambda value: value['rows'].__setitem__(3, 'CHAT_LINE 1 0 0 text GGGG'),
        )
        with tempfile.TemporaryDirectory(prefix='chat-packet-finite-controls-') as temporary:
            root = Path(temporary)
            directory = self.copy_packet(root)
            for index, change in enumerate(changes):
                with self.subTest(finite=index), self.assertRaises(ValueError):
                    value = copy.deepcopy(original)
                    change(value)
                    (directory / 'finite.json').write_text(json.dumps(value), encoding='utf-8')
                    P.load(root)


if __name__ == '__main__':
    unittest.main()
