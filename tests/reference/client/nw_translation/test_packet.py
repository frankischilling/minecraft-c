"""Corruption controls for the captured NW translations and ordered source stores."""
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
        for name in (*FILES, 'provenance.json'):
            shutil.copyfile(HERE / name, directory / name)
        return directory

    def rebind(self, directory, name):
        path = directory / 'provenance.json'
        data = json.loads(path.read_text())
        data['files_sha256'][name] = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        path.write_text(json.dumps(data), encoding='utf-8')

    def test_complete_matrix(self):
        packet = read_packet()
        self.assertEqual((len(packet['rows']), len(packet['profiles']), len(packet['stores'])),
                         (106, 36, 318))

    def test_changed_output_bit(self):
        directory = self.copy()
        path = directory / 'observations.tsv'
        data = path.read_text().replace('c03ff00000000000', 'c03ff00000000001', 1)
        path.write_text(data, encoding='ascii', newline='\n')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_missing_profile_after_rebind(self):
        directory = self.copy()
        path = directory / 'profiles.tsv'
        path.write_text('\n'.join(path.read_text().splitlines()[:-1]) + '\n', encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_changed_call_argument_after_rebind(self):
        directory = self.copy()
        path = directory / 'profiles.tsv'
        path.write_text(path.read_text().replace('00000000,00000000,3d800000',
                                                '00000000,00000000,3d800001', 1),
                        encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_reordered_stores_after_rebind(self):
        directory = self.copy()
        path = directory / 'stores.tsv'
        rows = path.read_text().splitlines()
        rows[0], rows[1] = rows[1], rows[0]
        path.write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_changed_store_pc_after_rebind(self):
        directory = self.copy()
        path = directory / 'stores.tsv'
        path.write_text(path.read_text().replace('\t8\ts\t', '\t7\ts\t', 1), encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_changed_partial_state_after_rebind(self):
        directory = self.copy()
        path = directory / 'stores.tsv'
        path.write_text(path.read_text().replace('c03ff00000000000', 'c03ff00000000001', 1),
                        encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_changed_method_code_after_rebind(self):
        directory = self.copy()
        path = directory / 'source-pins.json'
        data = json.loads(path.read_text())
        method = next(m for m in data['classes']['nw']['methods']
                      if m['name'] == 'c' and m['descriptor'] == '(FFF)V')
        method['code_sha256'] = '0' * 64
        path.write_text(json.dumps(data), encoding='utf-8')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_live_code_pin(self):
        directory = self.copy()
        path = directory / 'provenance.json'
        data = json.loads(path.read_text())
        data['live_code'] = data['live_code'].replace('(FFF)V', '(DDD)V')
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_missing_source_guard(self):
        directory = self.copy()
        path = directory / 'provenance.json'
        data = json.loads(path.read_text())
        data['source_guards'] = data['source_guards'][:1]
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_boolean_count_is_not_integer(self):
        directory = self.copy()
        path = directory / 'provenance.json'
        data = json.loads(path.read_text())
        data['read_only_jdi_fresh_vms'] = True
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_duplicate_json_field(self):
        directory = self.copy()
        path = directory / 'provenance.json'
        path.write_text(path.read_text().replace('"format": 1', '"format": 1, "format": 1', 1),
                        encoding='utf-8')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_selected_lf_transport_after_rebind(self):
        directory = self.copy()
        path = directory / 'stores.tsv'
        path.write_bytes(path.read_bytes().replace(b'\n', b'\r\n'))
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)


if __name__ == '__main__':
    unittest.main()
