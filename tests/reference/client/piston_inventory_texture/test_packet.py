"""Negative controls for the complete PistonBase method-family evidence."""
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
        path = Path(temporary.name)
        for name in (*FILES, 'provenance.json'):
            shutil.copyfile(HERE / name, path / name)
        return path

    def rebind(self, directory, name):
        path = directory / 'provenance.json'
        data = json.loads(path.read_text())
        data['files_sha256'][name] = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        path.write_text(json.dumps(data), encoding='utf-8')

    def change_row(self, directory, tag, column, value):
        path = directory / 'observations.tsv'
        rows = path.read_text().splitlines()
        index = next(i for i, line in enumerate(rows) if line.startswith(tag + '\t'))
        fields = rows[index].split('\t')
        fields[column] = value
        rows[index] = '\t'.join(fields)
        path.write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')
        self.rebind(directory, path.name)

    def test_complete_matrix(self):
        packet = read_packet()
        self.assertEqual(len(packet['rows']), 34612)
        self.assertEqual(sum(row.startswith('JQ_TEX\t') for row in packet['rows']), 34320)

    def test_changed_texture_bits(self):
        directory = self.copy()
        path = directory / 'observations.tsv'
        path.write_text(path.read_text().replace('0000006d', '0000006e', 1), encoding='ascii', newline='\n')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_truncated_matrix_after_rebind(self):
        directory = self.copy()
        path = directory / 'observations.tsv'
        path.write_text('\n'.join(path.read_text().splitlines()[:-1]) + '\n', encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_wrong_side_after_rebind(self):
        directory = self.copy()
        self.change_row(directory, 'JQ_TEX', 5, '2147483647')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_bounds_mutation_after_rebind(self):
        directory = self.copy()
        self.change_row(directory, 'JQ_TEX', 8, '0')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_late_bm_input_after_rebind(self):
        directory = self.copy()
        path = directory / 'observations.tsv'
        path.write_text(path.read_text().replace('JQ_INPUT\t29\t0\t1\tfffffffd',
                                                'JQ_INPUT\t29\t0\t1\tfffffffc', 1),
                        encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_inventory_hook_bounds_after_rebind(self):
        directory = self.copy()
        self.change_row(directory, 'JQ_INVENTORY_BOUNDS', 2, ','.join(['0000000000000000'] * 6))
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_changed_lazy_initialization_after_rebind(self):
        directory = self.copy()
        path = directory / 'initialization.json'
        data = json.loads(path.read_text())
        data[0]['prefix_call'] = 1
        path.write_text(json.dumps(data), encoding='utf-8')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_missing_guard_after_rebind(self):
        directory = self.copy()
        path = directory / 'guards.txt'
        path.write_text(path.read_text().splitlines()[0] + '\n', encoding='ascii', newline='\n')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_changed_reached_method_code_after_rebind(self):
        directory = self.copy()
        path = directory / 'source-pins.json'
        data = json.loads(path.read_text())
        method = next(m for m in data['classes']['jq']['methods']
                      if m['name'] == 'a' and m['descriptor'] == '(II)I')
        method['code_sha256'] = '0' * 64
        path.write_text(json.dumps(data), encoding='utf-8')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_missing_metadata_axis_after_rebind(self):
        directory = self.copy()
        path = directory / 'inputs.json'
        data = json.loads(path.read_text())
        data['metadata'].pop()
        path.write_text(json.dumps(data), encoding='utf-8')
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_boolean_count_is_not_integer(self):
        directory = self.copy()
        path = directory / 'provenance.json'
        data = json.loads(path.read_text())
        data['format'] = True
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_duplicate_json_key(self):
        directory = self.copy()
        path = directory / 'provenance.json'
        path.write_text(path.read_text().replace('"format": 1', '"format": 1,"format": 1', 1), encoding='utf-8')
        with self.assertRaises(ValueError):
            read_packet(directory)

    def test_changed_lf_transport_after_rebind(self):
        directory = self.copy()
        path = directory / 'observations.tsv'
        path.write_bytes(path.read_bytes().replace(b'\n', b'\r\n'))
        self.rebind(directory, path.name)
        with self.assertRaises(ValueError):
            read_packet(directory)


if __name__ == '__main__':
    unittest.main()
