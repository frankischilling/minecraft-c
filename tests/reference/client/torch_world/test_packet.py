"""Independent integrity and semantic corruption controls; no Java/assets/GL."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import FILES, HERE, read_packet, validate_events, validate_rows, lines


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_packet()

    def copy_packet(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        for name in (*FILES, 'provenance.json'):
            shutil.copyfile(HERE / name, root / name)
        return root

    def reject_event(self, select, mutate, message):
        events = copy.deepcopy(self.packet['events'])
        mutate(next(event for event in events if select(event)))
        with self.assertRaisesRegex(ValueError, message):
            validate_events(events, self.packet['observed'], self.packet['pins'], self.packet['live'])

    def test_complete_packet(self):
        self.assertEqual((len(self.packet['rows']), len(self.packet['events'])), (66, 8723))

    def test_changed_raw_word(self):
        root = self.copy_packet()
        path = root / 'selected.txt'
        text = path.read_text()
        line = next(line for line in text.splitlines() if line.startswith('TORCH\t'))
        fields = line.split('\t')
        words = fields[15].split(',')
        words[0] = f'{int(words[0], 16) ^ 1:08x}'
        fields[15] = ','.join(words)
        path.write_text(text.replace(line, '\t'.join(fields), 1), encoding='ascii', newline='\n')
        with self.assertRaisesRegex(ValueError, 'changed captured'):
            read_packet(root)

    def test_rebound_source_profile(self):
        root = self.copy_packet()
        path = root / 'profiles.txt'
        path.write_text(path.read_text().replace('world-50-meta-0 0 50 0', 'world-50-meta-0 0 50 1', 1), encoding='ascii', newline='\n')
        provenance = json.loads((root / 'provenance.json').read_text())
        pin = hashlib.sha256(path.read_bytes()).hexdigest()
        provenance['files_sha256'][path.name] = pin
        provenance['frozen_authored_files_sha256'][path.name] = pin
        (root / 'provenance.json').write_text(json.dumps(provenance), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'exact frozen'):
            read_packet(root)

    def test_duplicate_json_key(self):
        root = self.copy_packet()
        path = root / 'provenance.json'
        path.write_text(path.read_text().replace('"format": 1,', '"format": 1, "format": 1,', 1), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'duplicate JSON'):
            read_packet(root)

    def test_boolean_count(self):
        root = self.copy_packet()
        path = root / 'provenance.json'
        data = json.loads(path.read_text())
        data['profiles'] = True
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'exact profiles'):
            read_packet(root)

    def test_false_backing_audit_count(self):
        selected = lines(HERE / 'selected.txt')
        fields = selected[3].split('\t')
        fields[2] = str(int(fields[2])+1)
        selected[3] = '\t'.join(fields)
        with self.assertRaisesRegex(ValueError, 'own-prior backing'):
            validate_rows(selected, self.packet['profiles'])

    def test_changed_orientation(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['profile'] == 1 and e['at'] == 'cv.a(Luu;DDDDD)V@0',
                          lambda e: e['args'].__setitem__(4, '3fd99999a0000000'), 'orientation/direct geometry')

    def test_changed_query_receiver(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'] == 'ew.e(III)I@0',
                          lambda e: e.__setitem__('receiver', 'ref:fakeCache'), 'genuine EW receiver')

    def test_changed_query_coordinate(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'] == 'ew.e(III)I@0',
                          lambda e: e['args'].__setitem__(1, 63), 'genuine EW receiver')

    def test_changed_metadata_result(self):
        self.reject_event(lambda e: e['event'] == 'return' and e['at'].startswith('ew.e('),
                          lambda e: e.__setitem__('result', 1), 'nibble metadata result')

    def test_changed_live_minimum_emission(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'] == 'ew.a(IIII)F@0',
                          lambda e: e['args'].__setitem__(3, 0), 'live minimum-light')

    def test_changed_live_bm_before_override(self):
        self.reject_event(lambda e: e['event'] == 'return' and e['profile'] == 53 and e['at'].startswith('uu.a('),
                          lambda e: e.__setitem__('result', 17), 'live bm before override')

    def test_changed_fresh_array_identity(self):
        self.reject_event(lambda e: e['event'] == 'field-read' and e['at'] == 'cv.c(Luu;III)Z@34',
                          lambda e: e.__setitem__('value', 'ref:replacementArray'), 'same genuine static-final array')

    def test_changed_fresh_array_element(self):
        self.reject_event(lambda e: e['event'] == 'field-read' and e['at'] == 'cv.c(Luu;III)Z@34',
                          lambda e: e.__setitem__('element', 0), 'same genuine static-final array')

    def test_changed_gray_color_after_emission(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['profile'] == 50 and e['at'] == 'nw.a(FFF)V@0',
                          lambda e: e['args'].__setitem__(0, '3d4ccccd'), 'fresh emission capture and gray color')

    def test_changed_geometry_vertex_order(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'] == 'nw.a(DDDDD)V@0',
                          lambda e: e.__setitem__('caller', 'cv.a(Luu;DDDDD)V@245'), 'five-quad source vertex PCs')

    def test_changed_uv_operand(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'] == 'nw.a(DD)V@0',
                          lambda e: e['args'].__setitem__(0, '0000000000000000'), 'nested UV/vertex operands')

    def test_changed_uv_store_prefix(self):
        self.reject_event(lambda e: e['event'] == 'return' and e['at'].startswith('nw.a(DD)V@'),
                          lambda e: e['nw'].__setitem__('o', e['nw']['o']+8), 'UV stores and retained fields')

    def test_changed_quad_expansion(self):
        self.reject_event(lambda e: e['event'] == 'return' and e['at'].startswith('nw.a(DDD)V@'),
                          lambda e: e['nw'].__setitem__('h', e['nw']['h']+1), 'quad expansion/index stores')

    def test_changed_source_null_pc(self):
        self.reject_event(lambda e: e['event'] == 'exception' and e['profile'] == 60,
                          lambda e: e.__setitem__('at', 'cv.c(Luu;III)Z@7'), 'NPE throw/catch PCs')

    def test_changed_source_null_prefix(self):
        self.reject_event(lambda e: e['event'] == 'exception' and e['profile'] == 64,
                          lambda e: e['nw'].__setitem__('o', e['nw']['o']+8), 'NPE throw/catch PCs')

    def test_boolean_integer_result(self):
        self.reject_event(lambda e: e['event'] == 'return' and e['at'].startswith('ew.e('),
                          lambda e: e.__setitem__('result', False), 'typed actual query result')

    def test_changed_live_code(self):
        live = list(self.packet['live'])
        live[0] = live[0][:-1]+('1' if live[0][-1] != '1' else '0')
        with self.assertRaisesRegex(ValueError, 'live Code matches'):
            validate_events(self.packet['events'], self.packet['observed'], self.packet['pins'], live)


if __name__ == '__main__':
    unittest.main()
