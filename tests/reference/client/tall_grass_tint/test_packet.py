"""Public packet/corruption controls; no Java, assets, native executable or GL."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('tall_tint_packet', HERE / 'packet.py')
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)


class PacketTests(unittest.TestCase):
    def test_complete_original_packet(self):
        packet = P.read_packet()
        self.assertEqual((len(packet['rows']), packet['fresh_vms']), (28,2))

    def edited(self, name, change):
        with tempfile.TemporaryDirectory(prefix='tint-packet-') as temp:
            root = Path(temp)
            for source in (*P.FILES, 'provenance.json'):
                shutil.copyfile(HERE / source, root / source)
            path = root / name
            change(path)
            with self.assertRaises((ValueError, KeyError, TypeError, UnicodeError)):
                P.read_packet(root)

    def json_edit(self, change):
        def apply(path):
            value = json.loads(path.read_text())
            change(value)
            path.write_text(json.dumps(value) + '\n', encoding='utf-8', newline='\n')
        self.edited('provenance.json', apply)

    def test_capture_completion_controls(self):
        for key, value in (('status','FAIL'),('schema',True),('fresh_vms',1),('rows_per_vm',27),('inputs_unchanged',False)):
            with self.subTest(key=key):
                self.json_edit(lambda d: d.update({key:value}))

    def test_runtime_capture_controls(self):
        for key, value in (('exit',1),('exit',False),('timed_out',True),('pipe_capture_complete',False),
                           ('reader_errors',{'stdout':'failure'}),('output_overflow',True),('stdout_sha256','0'*64)):
            with self.subTest(key=key, value=value):
                self.json_edit(lambda d: d['runs'][0].update({key:value}))

    def test_original_archive_identity(self):
        self.json_edit(lambda d: d['runtime_inputs']['client_jar'].update(sha256='0'*64))

    def test_original_helper_identity(self):
        self.json_edit(lambda d: d['source_inputs'][next(iter(d['source_inputs']))].update(sha256='0'*64))

    def test_missing_original_vm(self):
        self.json_edit(lambda d: d['runs'].pop())

    def test_private_path_or_extra_field(self):
        self.json_edit(lambda d: d.update(process_address='0000012345678900'))
        self.json_edit(lambda d: d.update(scope='C:/private/capture'))

    def test_runtime_input_hash_shape(self):
        self.json_edit(lambda d: d['runtime_inputs']['modules'].update(sha256='bad'))

    def test_pinned_file_byte_controls(self):
        for name in P.FILES:
            with self.subTest(file=name):
                self.edited(name, lambda p: p.write_bytes(p.read_bytes().replace(b'0',b'1',1)))

    def test_lf_transport(self):
        self.edited('observations.tsv', lambda p: p.write_bytes(p.read_bytes().replace(b'\n',b'\r\n')))

    def test_duplicate_json_keys(self):
        self.edited('provenance.json', lambda p: p.write_text('{"status":"PASS","status":"PASS"}\n'))

    def test_input_binding_controls(self):
        original = P.text(HERE/'profiles.tsv').splitlines()
        for column, value in ((1,'1'),(2,'0'),(3,'16'),(4,'+8'),(5,'2147483648'),(7,'4'),(8,'4')):
            with self.subTest(column=column):
                lines = original.copy(); cells=lines[0].split('\t');cells[column]=value;lines[0]='\t'.join(cells)
                with self.assertRaises(ValueError):
                    P.profiles(lines)

    def test_row_semantic_controls(self):
        packet=P.read_packet()
        for index, column, value in ((0,9,'NULL'),(2,10,'00000000'),(2,11,'00000000'),
                                     (8,11,'7fc00000'),(8,14,'0'),(0,15,'1'),(8,16,'1'),
                                     (8,17,'0000000000000000'),(8,18,'7ff0000000000000,0,0'),
                                     (8,19,'0'),(8,22,'0')):
            with self.subTest(index=index,column=column):
                lines=packet['rows'].copy();cells=lines[index].split('\t');cells[column]=value;lines[index]='\t'.join(cells)
                with self.assertRaises(ValueError):
                    P.rows(lines, packet['profiles'])

    def test_missing_or_reordered_observations(self):
        packet=P.read_packet()
        for lines in (packet['rows'][:-1], list(reversed(packet['rows']))):
            with self.assertRaises(ValueError):
                P.rows(lines,packet['profiles'])


if __name__ == '__main__':
    unittest.main()
