"""Reject malformed, rebound or internally inconsistent original harvest packets."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import EVENT_COLUMNS, ITEM_COLUMNS, RESULT_COLUMNS, read_packet

HERE = Path(__file__).resolve().parent
FILES = ('profiles.tsv', 'observations.tsv', 'guards.tsv', 'source-pins.json', 'schema.json', 'provenance.json')


class PacketTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        for name in FILES:
            shutil.copyfile(HERE / name, self.directory / name)

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, name, value, refresh=True):
        raw = value if isinstance(value, bytes) else value.encode('utf-8')
        (self.directory / name).write_bytes(raw)
        if refresh:
            path = self.directory / 'provenance.json'
            proof = json.loads(path.read_text())
            proof['files'][name] = {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}
            path.write_text(json.dumps(proof), encoding='utf-8')

    def reject(self):
        with self.assertRaises(ValueError):
            read_packet(self.directory)

    def row(self, tag, profile, column, value, occurrence=0, refresh=True):
        lines = (self.directory / 'observations.tsv').read_text().splitlines()
        columns = {'RESULT': RESULT_COLUMNS, 'ITEM': ITEM_COLUMNS, 'EVENT': EVENT_COLUMNS}[tag]
        matches = [i for i, line in enumerate(lines) if line.startswith(f'{tag}\t{profile}\t')]
        cells = lines[matches[occurrence]].split('\t')
        cells[columns.index(column) + 1] = value
        lines[matches[occurrence]] = '\t'.join(cells)
        self.write('observations.tsv', '\n'.join(lines) + '\n', refresh)
        self.reject()

    def test_complete(self):
        packet = read_packet(self.directory)
        self.assertEqual(len(packet['profiles']), 92)
        self.assertEqual(sum(p['mode'] == 1 for p in packet['profiles']), 63)
        self.assertEqual(len(packet['by_profile']), 92)

    def test_transport_hash(self):
        self.row('RESULT', 0, 'target_id', '8', refresh=False)

    def test_crlf_transport(self):
        raw = (self.directory / 'observations.tsv').read_bytes().replace(b'\n', b'\r\n')
        self.write('observations.tsv', raw)
        self.reject()

    def test_missing_profile(self):
        rows = (self.directory / 'profiles.tsv').read_text().splitlines()[1:]
        self.write('profiles.tsv', '\n'.join(rows) + '\n')
        self.reject()

    def test_profile_rebound(self):
        raw = (self.directory / 'profiles.tsv').read_text().replace('PROFILE\t0\t1\t18', 'PROFILE\t0\t1\t79', 1)
        self.write('profiles.tsv', raw)
        self.reject()

    def test_missing_result(self):
        rows = (self.directory / 'observations.tsv').read_text().splitlines()
        del rows[1]
        self.write('observations.tsv', '\n'.join(rows) + '\n')
        self.reject()

    def test_duplicate_result(self):
        raw = (self.directory / 'observations.tsv').read_text()
        self.write('observations.tsv', raw.replace('RESULT\t0\t', 'RESULT\t1\t', 1))
        self.reject()

    def test_trailing_record(self):
        raw = (self.directory / 'observations.tsv').read_text()
        self.write('observations.tsv', raw + 'RESULT\t0\tOK\n')
        self.reject()

    def test_integer_noncanonical(self):
        self.row('RESULT', 0, 'mined_delta', '-0')

    def test_integer_range(self):
        self.row('RESULT', 0, 'current_count', str(1 << 31))

    def test_random_state_range(self):
        self.row('RESULT', 0, 'world48_after', str(1 << 48))

    def test_declared_random_seed(self):
        self.row('RESULT', 0, 'world48_before', '0')

    def test_source_exception_outcome(self):
        self.row('RESULT', 80, 'status', 'OK')

    def test_unreturned_controller_boolean(self):
        self.row('RESULT', 62, 'removed', '1')

    def test_empty_stack_sentinel(self):
        self.row('RESULT', 0, 'current_id', '0')

    def test_item_payload(self):
        self.row('ITEM', 0, 'id', '4')

    def test_item_delay(self):
        self.row('ITEM', 0, 'delay', '9')

    def test_item_alias_lifetime(self):
        self.row('ITEM', 0, 'in_chunk', '0')

    def test_item_nan_position(self):
        self.row('ITEM', 0, 'x64', '7ff8000000000000')

    def test_item_float_width(self):
        self.row('ITEM', 0, 'yaw32', '0')

    def test_observer_sequence(self):
        self.row('EVENT', 0, 'sequence', '1')

    def test_observer_nonentity_sentinel(self):
        self.row('EVENT', 0, 'item_id', '0')

    def test_observer_publication_identity(self):
        self.row('EVENT', 0, 'item_id', '18', occurrence=1)

    def test_controller_effect_order(self):
        self.row('EVENT', 42, 'used_delta', '1')

    def test_direct_snow_statistic_order(self):
        self.row('EVENT', 76, 'mined_delta', '1')

    def test_ice_statistic_order(self):
        self.row('EVENT', 53, 'mined_delta', '0', occurrence=1)

    def test_source_guard(self):
        raw = (self.directory / 'guards.tsv').read_text().replace('source_network_attempts\t0', 'source_network_attempts\t1')
        self.write('guards.tsv', raw)
        self.reject()

    def test_jar_identity(self):
        path = self.directory / 'provenance.json'
        proof = json.loads(path.read_text())
        proof['client_jar_sha256'] = '0' * 64
        path.write_text(json.dumps(proof))
        self.reject()

    def test_class_identity(self):
        pins = json.loads((self.directory / 'source-pins.json').read_text())
        pins['classes']['bk']['class_sha256'] = '0' * 64
        self.write('source-pins.json', json.dumps(pins))
        self.reject()

    def test_inherited_method_resolution(self):
        pins = json.loads((self.directory / 'source-pins.json').read_text())
        body = next(m for m in pins['classes']['uu']['methods'] if m['name'] == 'a' and m['descriptor'] == '(Lfd;Lgs;IIII)V')
        pins['classes']['nr']['methods'].append(body)
        self.write('source-pins.json', json.dumps(pins))
        self.reject()

    def test_method_code(self):
        pins = json.loads((self.directory / 'source-pins.json').read_text())
        body = next(m for m in pins['classes']['jr']['methods'] if m['name'] == 'a' and m['descriptor'] == '(Lfd;Lgs;IIII)V')
        body['code_bytes'] = 0
        self.write('source-pins.json', json.dumps(pins))
        self.reject()

    def test_schema_identity(self):
        schema = json.loads((self.directory / 'schema.json').read_text())
        schema['RESULT'][0] = 'guess'
        self.write('schema.json', json.dumps(schema))
        self.reject()


if __name__ == '__main__':
    unittest.main()
