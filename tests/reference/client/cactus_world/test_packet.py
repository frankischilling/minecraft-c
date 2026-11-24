"""Integrity and semantic corruption controls; no original JAR, Java or GL required."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import FILES, HERE, read_packet, validate_events


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_packet()

    def copy_packet(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        for name in (*FILES, 'provenance.json'):
            shutil.copyfile(HERE / name, root / name)
        return root

    def reject_event(self, mutate, message):
        events = copy.deepcopy(self.packet['events'])
        mutate(events)
        with self.assertRaisesRegex(ValueError, message):
            validate_events(events, self.packet['observed'], self.packet['pins'], self.packet['live'])

    def test_complete_source_packet(self):
        self.assertEqual((len(self.packet['rows']), len(self.packet['events'])), (25, 2133))

    def test_changed_raw_vertex_bit(self):
        root = self.copy_packet()
        path = root / 'selected.txt'
        raw = path.read_text()
        row = next(line for line in raw.splitlines() if line.startswith('CACTUS\t'))
        fields = row.split('\t')
        words = fields[15].split(',')
        words[0] = f'{int(words[0], 16) ^ 1:08x}'
        fields[15] = ','.join(words)
        path.write_text(raw.replace(row, '\t'.join(fields), 1), encoding='ascii', newline='\n')
        with self.assertRaisesRegex(ValueError, 'changed captured'):
            read_packet(root)

    def test_rebound_frozen_profile(self):
        root = self.copy_packet()
        path = root / 'profiles.txt'
        path.write_text(path.read_text().replace('direct-air 0 0 1', 'direct-air 0 0 0', 1), encoding='ascii', newline='\n')
        prov = json.loads((root / 'provenance.json').read_text())
        prov['files_sha256']['profiles.txt'] = hashlib.sha256(path.read_bytes()).hexdigest()
        prov['frozen_authored_files_sha256']['profiles.txt'] = prov['files_sha256']['profiles.txt']
        (root / 'provenance.json').write_text(json.dumps(prov), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'exact frozen'):
            read_packet(root)

    def test_incomplete_row_set(self):
        root = self.copy_packet()
        path = root / 'selected.txt'
        path.write_text('\n'.join(path.read_text().splitlines()[:-1]) + '\n', encoding='ascii', newline='\n')
        with self.assertRaises(ValueError):
            read_packet(root)

    def test_duplicate_json_field(self):
        root = self.copy_packet()
        path = root / 'provenance.json'
        path.write_text(path.read_text().replace('"format": 1,', '"format": 1, "format": 1,', 1), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'duplicate JSON'):
            read_packet(root)

    def test_boolean_profile_count(self):
        root = self.copy_packet()
        path = root / 'provenance.json'
        data = json.loads(path.read_text())
        data['profiles'] = True
        path.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'exact profiles'):
            read_packet(root)

    def test_missing_jdi_event(self):
        self.reject_event(lambda events: events.pop(6), 'contiguous typed')

    def test_changed_nested_query_argument(self):
        self.reject_event(lambda events: events[2]['args'].__setitem__(0, 7), 'nested EW query')

    def test_changed_query_return_bit(self):
        self.reject_event(lambda events: events[5].__setitem__('result', 80), 'authored block query result')

    def test_boolean_integer_query_result(self):
        self.reject_event(lambda events: events[5].__setitem__('result', True), 'typed EW result')

    def test_changed_query_receiver(self):
        self.reject_event(lambda events: events[1].__setitem__('receiver', 'ref:replacement'), 'genuine EW receiver')

    def test_changed_query_caller_pc(self):
        self.reject_event(lambda events: events[1].__setitem__('caller', 'uu.d(Lxp;III)F@12'), 'actual UU brightness')

    def test_reordered_return(self):
        self.reject_event(lambda events: events[5].__setitem__('call', 3), 'nested actual return')

    def test_changed_offset_store_pc(self):
        def mutate(events):
            event = next(e for e in events if e['event'] == 'store')
            event['at'] = 'nw.c(FFF)V@7'
        self.reject_event(mutate, 's8/t19/u30 store order')

    def test_changed_offset_value(self):
        def mutate(events):
            event = next(e for e in events if e['event'] == 'store')
            event['new'] = f'{int(event["new"], 16) ^ 1:016x}'
        self.reject_event(mutate, 'float widening')

    def test_changed_store_prefix(self):
        def mutate(events):
            event = next(e for e in events if e['event'] == 'store' and e['field'] == 't')
            event['nw']['o'] += 48
        self.reject_event(mutate, 'current pre-store prefix')

    def test_changed_nullable_failure_pc(self):
        def mutate(events):
            event = next(e for e in events if e['event'] == 'exception')
            event['at'] = 'uu.d(Lxp;III)F@12'
        self.reject_event(mutate, 'source throw/catch PCs')

    def test_changed_nullable_failure_prefix(self):
        def mutate(events):
            event = next(e for e in events if e['event'] == 'exception' and e['profile'] == 9)
            event['nw']['o'] -= 48
        self.reject_event(mutate, 'source throw/catch PCs')

    def test_changed_live_code(self):
        live = list(self.packet['live'])
        live[0] = live[0][:-1] + ('0' if live[0][-1] != '0' else '1')
        with self.assertRaisesRegex(ValueError, 'live method Code'):
            validate_events(self.packet['events'], self.packet['observed'], self.packet['pins'], live)


if __name__ == '__main__':
    unittest.main()
