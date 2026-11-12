"""Integrity and semantic corruption controls for the original Ladder packet."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import FILES, HERE, lines, read_packet, validate_events, validate_rows


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_packet()

    def copy_packet(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        for name in (*FILES, 'provenance.json'):
            shutil.copyfile(HERE / name, root / name)
        return root

    def reject_event(self, select, change, message):
        events = copy.deepcopy(self.packet['events'])
        change(next(e for e in events if select(e)))
        with self.assertRaisesRegex(ValueError, message):
            validate_events(events, self.packet['observed'], self.packet['pins'], self.packet['live'])

    def test_complete_packet(self):
        self.assertEqual((len(self.packet['rows']), len(self.packet['inputs'])), (35, 35))
        self.assertEqual(self.packet['metrics']['events'], 1133)
        self.assertEqual(self.packet['metrics']['queries'], 160)

    def test_changed_product(self):
        root = self.copy_packet()
        path = root / 'selected.txt'
        path.write_bytes(path.read_bytes().replace(b'metadata-0', b'metadata-1', 1))
        with self.assertRaisesRegex(ValueError, 'changed captured'):
            read_packet(root)

    def test_rebound_profile_pin(self):
        root = self.copy_packet()
        path = root / 'profiles.txt'
        path.write_bytes(path.read_bytes().replace(b'metadata-0 0 ', b'metadata-0 1 ', 1))
        data = json.loads((root / 'provenance.json').read_text())
        changed = hashlib.sha256(path.read_bytes()).hexdigest()
        data['files_sha256'][path.name] = changed
        data['frozen_authored_files_sha256'][path.name] = changed
        (root / 'provenance.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'exact frozen'):
            read_packet(root)

    def test_boolean_profile_count(self):
        root = self.copy_packet()
        data = json.loads((root / 'provenance.json').read_text())
        data['profiles'] = True
        (root / 'provenance.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'unchanged executed'):
            read_packet(root)

    def test_source_write(self):
        root = self.copy_packet()
        data = json.loads((root / 'provenance.json').read_text())
        data['source_writes_from_jdi'] = 1
        (root / 'provenance.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'unchanged executed'):
            read_packet(root)

    def test_duplicate_json(self):
        root = self.copy_packet()
        path = root / 'provenance.json'
        path.write_text(path.read_text().replace('"format": 1,', '"format": 1, "format": 1,', 1))
        with self.assertRaisesRegex(ValueError, 'duplicate JSON'):
            read_packet(root)

    def test_missing_invalid_metadata_call(self):
        selected = [line for line in lines(HERE / 'selected.txt')
                    if not line.startswith('LADDER\t0\t')]
        with self.assertRaisesRegex(ValueError, 'complete source roles'):
            validate_rows(selected, self.packet['profiles'])

    def test_live_code_fingerprint(self):
        live = list(self.packet['live'])
        fields = live[0].split('\t')
        fields[-1] = '0' * 64
        live[0] = '\t'.join(fields)
        with self.assertRaisesRegex(ValueError, 'live method Code'):
            validate_events(self.packet['events'], self.packet['observed'], self.packet['pins'], live)

    def test_actual_texture_getter(self):
        self.reject_event(lambda e: e['event'] == 'return' and e['at'].startswith('uu.a(I)I@'),
                          lambda e: e.update(result=0), 'mutable Block texture getter result')

    def test_query_coordinates(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'].startswith('ew.e('),
                          lambda e: e['args'].__setitem__(0, e['args'][0]+1), 'actual EW coordinates')

    def test_metadata_source_order(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'].startswith('ew.e('),
                          lambda e: e.update(caller=e['caller'].replace('@117', '@116')),
                          'metadata query after actual color')

    def test_emission_static_read(self):
        self.reject_event(lambda e: e['event'] == 'field-read' and e['field'] == 'uu.s',
                          lambda e: e.update(element=1), 'emission-array element read')

    def test_brightness_result(self):
        self.reject_event(lambda e: e['event'] == 'return' and e['at'].startswith('ew.a(IIII)F@'),
                          lambda e: e.update(result='00000000'), 'Dimension brightness')

    def test_vertex_caller_pc(self):
        self.reject_event(lambda e: e['event'] == 'entry' and e['at'].startswith('nw.a(DDDDD)V@'),
                          lambda e: e.update(caller=e['caller'].replace('@578', '@579')),
                          'orientation-specific source vertex PC')

    def test_packed_word_from_actual_arguments(self):
        observed = copy.deepcopy(self.packet['observed'])
        observed[2]['words'][0] = f'{int(observed[2]["words"][0], 16)^1:08x}'
        with self.assertRaisesRegex(ValueError, 'packed active words'):
            validate_events(self.packet['events'], observed, self.packet['pins'], self.packet['live'])

    def test_exact_null_prefix(self):
        self.reject_event(lambda e: e['event'] == 'exception',
                          lambda e: e.update(at=e['at'].replace('@7', '@8')),
                          'original NULL PC')


if __name__ == '__main__':
    unittest.main()
