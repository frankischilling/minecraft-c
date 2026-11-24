"""Changed-copy guards for the Chest original-reference packet."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
S = importlib.util.spec_from_file_location('chest_packet_audit', HERE / 'audit.py')
A = importlib.util.module_from_spec(S)
S.loader.exec_module(A)
S = importlib.util.spec_from_file_location('chest_packet_publisher', HERE / 'publish.py')
P = importlib.util.module_from_spec(S)
S.loader.exec_module(P)


def synthetic_transport(directory):
    """A reader-control fixture, not a newly executed original observation."""
    declaration = next(case for case in A.load(HERE / 'cases.json')['cases']
                       if case['profile'] == 'cd-shift-storage-first')
    observed = next(row for row in A.load(HERE / 'finite.json')['rows']
                    if row['profile'] == declaration['profile'])
    pins = A.load(HERE / 'source-pins.json')
    origins = {'jar_sha256': A.JAR, 'classes': {
        name: value['class_sha256'] for name, value in pins['classes'].items()}}
    rows = [observed]
    raw = ('16 achievements\n151 recipes\nCHEST_ORIGINS ' + json.dumps(origins) +
           '\nCHEST_CONTAINER ' + json.dumps(observed) +
           '\nCHEST_GUARD {"network":0,"native_links":0}\n').encode('utf-8')
    (directory / 'classes').mkdir()
    warnings = ('\n'.join(P.C.expected_warnings(directory / 'classes')) + '\n').encode('utf-8')
    report = {'pins': pins, 'rows': rows}
    for key, stem, stdout, stderr in (
            ('compile', 'compile', b'', b''),
            ('java_version', 'java-version', b'', b'synthetic tool version\n'),
            ('javac_version', 'javac-version', b'synthetic compiler version\n', b''),
            ('original', 'original', raw, warnings),
            ('original-repeat', 'original-repeat', raw, warnings)):
        record = {'exit': 0, 'timeout_seconds': 60, 'timed_out': False,
                  'pipe_capture_complete': True, 'reader_errors': {}, 'output_overflow': False}
        for stream, data, limit in (('stdout', stdout, P.C.C.STDOUT_LIMIT),
                                    ('stderr', stderr, P.C.C.STDERR_LIMIT)):
            name = stem + '.' + stream
            (directory / name).write_bytes(data)
            record.update({stream + '_file': name, stream + '_bytes': len(data),
                           stream + '_sha256': hashlib.sha256(data).hexdigest(), stream + '_limit': limit})
        report[key] = record
    return report, [declaration]


def reseal_stream(directory, report, stem, stream, data):
    """Expose semantic checks independently of the outer recorded stream hash."""
    (directory / (stem + '.' + stream)).write_bytes(data)
    report[stem][stream + '_bytes'] = len(data)
    report[stem][stream + '_sha256'] = hashlib.sha256(data).hexdigest()


class Packet(unittest.TestCase):
    def test_original_packet(self):
        result = A.audit()
        self.assertEqual((result['original_rows'], result['classes'], result['processes']), (111, 19, 2))
        self.assertEqual(result['native_qualification'], 'PENDING')

    def test_every_file_pin(self):
        with tempfile.TemporaryDirectory(prefix='chest-packet-pins-') as tmp:
            root = Path(tmp)
            for path in HERE.iterdir():
                if path.is_file():
                    shutil.copyfile(path, root / path.name)
            for name in (*A.EXECUTION, *A.PUBLICATION, *A.OUTPUTS):
                with self.subTest(name=name):
                    path = root / name
                    original = path.read_bytes()
                    path.write_bytes(original + b'\n')
                    with self.assertRaises(ValueError):
                        A.audit(root)
                    path.write_bytes(original)

    def test_missing_reordered_extra_and_changed_rows(self):
        cases = A.load(HERE / 'cases.json')['cases']
        rows = A.load(HERE / 'finite.json')['rows']
        mutations = (
            lambda value: value.pop(),
            lambda value: value.reverse(),
            lambda value: value.append(copy.deepcopy(value[0])),
            lambda value: value[0].update(extra=1),
            lambda value: value[0]['before']['left'].pop(),
            lambda value: value[0]['before']['stacks'].update(X={'id': 1, 'count': 1, 'damage': 0, 'animation': 0}),
            lambda value: value[0]['layout'][0].__setitem__(0, 9),
            lambda value: value[0]['base'].update(window=7),
            lambda value: value[0]['calls'].append('forged callback'),
            lambda value: value[0]['after']['left'].__setitem__(0, 'unobserved'),
        )
        for index, change in enumerate(mutations):
            with self.subTest(index=index):
                value = copy.deepcopy(rows)
                change(value)
                with self.assertRaises(ValueError):
                    A.validate(cases, value)

    def test_same_child_alias(self):
        cases = A.load(HERE / 'cases.json')['cases']
        rows = A.load(HERE / 'finite.json')['rows']
        index = next(index for index, case in enumerate(cases) if case['profile'] == 'og-get-alias')
        rows[index]['after']['right'][0] = None
        with self.assertRaises(ValueError):
            A.validate(cases, rows)

    def test_duplicate_and_nonfinite_json(self):
        with tempfile.TemporaryDirectory(prefix='chest-json-') as tmp:
            path = Path(tmp) / 'packet.json'
            for raw in ('{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}'):
                path.write_text(raw)
                with self.assertRaises(ValueError):
                    A.load(path)

    def test_native_header_token_identity(self):
        S = importlib.util.spec_from_file_location('chest_projection_test', HERE / 'projection.py')
        projection = importlib.util.module_from_spec(S)
        S.loader.exec_module(projection)
        cases = A.load(HERE / 'cases.json')['cases']
        rows = A.load(HERE / 'finite.json')['rows']
        generated = projection.header(cases, rows)
        self.assertEqual(projection.tokens(generated),
                         projection.tokens((HERE / 'native-observations.h').read_text()))
        changed = generated.replace('"cd-layout-27"', '"cd-layout-28"', 1)
        self.assertFalse(projection.tokens(generated) == projection.tokens(changed))

    def test_publication_transport_positive(self):
        with tempfile.TemporaryDirectory(prefix='chest-publication-reader-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic_transport(directory)
            P.validate_transport(directory, report, cases)

    def test_publication_rejects_changed_report_row(self):
        with tempfile.TemporaryDirectory(prefix='chest-publication-row-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic_transport(directory)
            report['rows'][0]['after']['stacks']['A']['count'] += 7
            # This valid-shaped corruption passed the old publication guard.
            A.validate(cases, report['rows'])
            with self.assertRaises(ValueError):
                P.validate_transport(directory, report, cases)

    def test_publication_rejects_changed_stream_and_divergent_process(self):
        with tempfile.TemporaryDirectory(prefix='chest-publication-stream-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic_transport(directory)
            path = directory / 'original.stdout'
            raw = path.read_bytes()
            path.write_bytes(raw + b'extra transport\n')
            with self.assertRaises(ValueError):
                P.validate_transport(directory, report, cases)
            path.write_bytes(raw)
            changed = copy.deepcopy(report['rows'][0])
            changed['after']['stacks']['A']['count'] += 7
            altered = raw.replace(json.dumps(report['rows'][0]).encode('utf-8'),
                                  json.dumps(changed).encode('utf-8'))
            self.assertNotEqual(raw, altered)
            report['rows'] = [changed]
            reseal_stream(directory, report, 'original', 'stdout', altered)
            # The second unchanged process still rejects a resealed first one.
            with self.assertRaises(ValueError):
                P.validate_transport(directory, report, cases)

    def test_publication_rejects_incomplete_or_mislabelled_transport(self):
        with tempfile.TemporaryDirectory(prefix='chest-publication-status-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic_transport(directory)
            mutations = (
                ('exit', 1), ('exit', False), ('timed_out', True), ('pipe_capture_complete', False),
                ('reader_errors', {'stdout': 'controlled read failure'}), ('output_overflow', True),
                ('timeout_seconds', 0), ('stdout_file', '../original.stdout'), ('stdout_bytes', 0),
                ('stdout_sha256', '0' * 64), ('stdout_limit', 1),
            )
            for field, value in mutations:
                with self.subTest(field=field, value=value):
                    changed = copy.deepcopy(report)
                    changed['original'][field] = value
                    with self.assertRaises(ValueError):
                        P.validate_transport(directory, changed, cases)

    def test_publication_rejects_resealed_protocol_warning_and_origin_changes(self):
        with tempfile.TemporaryDirectory(prefix='chest-publication-semantics-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic_transport(directory)
            raw = (directory / 'original.stdout').read_bytes()
            variants = (raw + b'unknown preamble\n', raw.replace(b'"network":0', b'"network":1'),
                        raw.replace(A.JAR.encode(), b'0' * 64))
            for index, value in enumerate(variants):
                with self.subTest(index=index):
                    changed = copy.deepcopy(report)
                    reseal_stream(directory, changed, 'original', 'stdout', value)
                    with self.assertRaises(ValueError):
                        P.validate_transport(directory, changed, cases)
            (directory / 'original.stdout').write_bytes(raw)
            reseal_stream(directory, report, 'original', 'stderr', b'unreported warning\n')
            with self.assertRaises(ValueError):
                P.validate_transport(directory, report, cases)


if __name__ == '__main__':
    unittest.main()
