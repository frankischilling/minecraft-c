"""Portable packet and publication corruption controls; no new game execution."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


A = module('chest_predicate_test_packet', HERE / 'packet.py')
P = module('chest_predicate_test_publisher', HERE / 'publish.py')


def record(directory, stem, stdout, stderr):
    value = {'exit': 0, 'timeout_seconds': 120, 'timed_out': False,
             'pipe_capture_complete': True, 'reader_errors': {}, 'output_overflow': False}
    for stream, raw, limit in (('stdout', stdout, P.C.C.STDOUT_LIMIT), ('stderr', stderr, P.C.C.STDERR_LIMIT)):
        name = stem + '.' + stream
        (directory / name).write_bytes(raw)
        value.update({stream + '_file': name, stream + '_bytes': len(raw),
                      stream + '_sha256': hashlib.sha256(raw).hexdigest(), stream + '_limit': limit})
    return value


def target_output(report, mode):
    origins = {'jar_sha256': A.JAR, 'classes': {name: value['class_sha256'] for name, value in report['pins']['classes'].items()}}
    lines = ['16 achievements', '151 recipes', 'CHEST_ORIGINS ' + json.dumps(origins)]
    for row, random in zip(report['rows'], report['entropy'][mode]):
        lines.extend(('CHEST_PLACEMENT ' + json.dumps(row), 'CHEST_RANDOM ' + json.dumps(random)))
    lines.append('CHEST_COMPLETE ' + json.dumps({'rows': 64, 'network': 0, 'native': 0, 'execs': 0,
        'denied_writes': 0, 'world_writes': 3, 'canonical_chunks': 4, 'registry_unchanged': True, 'remote': False}))
    return ('\n'.join(lines) + '\n').encode('utf-8')


def synthetic(directory):
    """Synthetic transport around already published records, not another observation."""
    cases = A.load(HERE / 'cases.json')['cases']
    traces = A.load(HERE / 'traces.json')
    report = {'rows': A.load(HERE / 'finite.json')['rows'], 'entropy': A.load(HERE / 'random.json')['streams'],
              'pins': A.load(HERE / 'source-pins.json'), 'traces': {}, 'live_Code': {}, 'trace_artifacts': {}}
    (directory / 'classes').mkdir()
    warnings = ('\n'.join(P.C.expected_warnings(directory / 'classes')) + '\n').encode('utf-8')
    for key, stem in (('compile', 'compile'), ('java_version', 'java-version'), ('javac_version', 'javac-version')):
        report[key] = record(directory, stem, b'', b'')
    for mode in ('interpreter', 'default'):
        report[mode] = record(directory, mode, target_output(report, mode), warnings)
    for mode in ('trace-interpreter', 'trace-default'):
        child = directory / (mode + '-jdi')
        child.mkdir()
        report['traces'][mode] = copy.deepcopy(traces['rows'])
        report['live_Code'][mode] = copy.deepcopy(traces['live_Code'])
        files = {'target.stdout': target_output(report, mode), 'target.stderr': warnings,
                 'queries.ndjson': ('\n'.join(json.dumps(row) for row in traces['rows']) + '\n').encode(),
                 'live-Code.json': (json.dumps(traces['live_Code']) + '\n').encode()}
        report['trace_artifacts'][mode] = {}
        for name, raw in files.items():
            (child / name).write_bytes(raw)
            report['trace_artifacts'][mode][name] = {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}
        summary = f'CHEST_TRACE_COMPLETE rows=64 queries={sum(len(row["queries"]) for row in traces["rows"])} live-code=3\n'
        report[mode] = record(directory, mode, summary.encode(), b'')
    return report, cases


class ChestPredicatePacket(unittest.TestCase):
    def test_public_packet(self):
        result = A.audit()
        self.assertEqual((result['original_cases'], result['public_predicates'], result['private_helpers']), (64, 55, 9))
        self.assertEqual((result['actual_World_queries'], result['live_Code_pins']), (482, 3))

    def test_each_file_pin(self):
        with tempfile.TemporaryDirectory(prefix='chest-predicate-pins-') as tmp:
            root = Path(tmp)
            for path in HERE.iterdir():
                if path.is_file():
                    shutil.copyfile(path, root / path.name)
            for name in (*A.EXECUTION, *A.PUBLICATION, *A.OUTPUTS):
                path = root / name
                original = path.read_bytes()
                path.write_bytes(original + b'\n')
                with self.subTest(name=name), self.assertRaises(ValueError):
                    A.audit(root)
                path.write_bytes(original)

    def test_rows_and_declared_inputs(self):
        cases = A.load(HERE / 'cases.json')['cases']
        rows = A.load(HERE / 'finite.json')['rows']
        changes = (lambda r: r.pop(), lambda r: r.reverse(), lambda r: r[0].update(extra=1),
                   lambda r: r[0].update(x=9), lambda r: r[0].update(array_changes=1),
                   lambda r: r[0].update(events=[['cell', 8, 64, 8]]),
                   lambda r: r[0].update(queues_after=[[0, 1, 2, 3, 4, 5, 6]]))
        for index, change in enumerate(changes):
            altered = copy.deepcopy(rows)
            change(altered)
            with self.subTest(index=index), self.assertRaises(ValueError):
                A.validate_rows(cases, altered)

    def test_actual_trace_shape_and_code(self):
        cases = A.load(HERE / 'cases.json')['cases']
        traces = A.load(HERE / 'traces.json')
        for change in (lambda r: r.pop(), lambda r: r[0].update(index=1),
                       lambda r: r[0]['queries'][0].update(result=256),
                       lambda r: r[0]['queries'][0]['source_frames'][0].update(pc=999),
                       lambda r: r[0]['entered_E'][0].update(world_null=True)):
            rows = copy.deepcopy(traces['rows'])
            change(rows)
            with self.assertRaises(ValueError):
                A.validate_traces(cases, rows, traces['live_Code'])
        code = copy.deepcopy(traces['live_Code'])
        code['fd.a(III)I']['code_sha256'] = '0' * 64
        with self.assertRaises(ValueError):
            A.validate_traces(cases, traces['rows'], code)

    def test_raw_rng_must_join_actual_process(self):
        cases = A.load(HERE / 'cases.json')['cases']
        streams = A.load(HERE / 'random.json')['streams']
        streams['interpreter'][0]['after'][0] = '0' * 12
        with self.assertRaises(ValueError):
            A.validate_random(cases, streams)

    def test_publication_reparse_positive(self):
        with tempfile.TemporaryDirectory(prefix='chest-predicate-transport-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic(directory)
            P.validate_transport(directory, report, cases)

    def test_publication_rejects_changed_report_return(self):
        with tempfile.TemporaryDirectory(prefix='chest-predicate-return-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic(directory)
            report['rows'][0]['result'] = 'false'
            A.validate_rows(cases, report['rows'])  # Valid shape is not evidence of actual execution.
            with self.assertRaises((ValueError, AssertionError)):
                P.validate_transport(directory, report, cases)

    def test_publication_rejects_changed_query_or_rng_report(self):
        with tempfile.TemporaryDirectory(prefix='chest-predicate-journal-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic(directory)
            report['traces']['trace-interpreter'][0]['queries'][0]['result'] = 1
            with self.assertRaises((ValueError, AssertionError)):
                P.validate_transport(directory, report, cases)
            report, cases = self.restore_report(directory, report)
            report['entropy']['interpreter'][0]['before'][0] = '0' * 12
            with self.assertRaises((ValueError, AssertionError)):
                P.validate_transport(directory, report, cases)

    def restore_report(self, directory, previous):
        previous['traces']['trace-interpreter'] = A.load(HERE / 'traces.json')['rows']
        return previous, A.load(HERE / 'cases.json')['cases']

    def test_transport_hash_completion_and_identity(self):
        with tempfile.TemporaryDirectory(prefix='chest-predicate-complete-') as tmp:
            directory = Path(tmp)
            report, cases = synthetic(directory)
            for field, value in (('exit', 1), ('exit', False), ('timed_out', True),
                                 ('pipe_capture_complete', False), ('output_overflow', True),
                                 ('reader_errors', {'stdout': 'read failure'}), ('stdout_bytes', 0),
                                 ('stdout_sha256', '0' * 64), ('stdout_file', '../interpreter.stdout')):
                altered = copy.deepcopy(report)
                altered['interpreter'][field] = value
                with self.subTest(field=field), self.assertRaises((ValueError, AssertionError)):
                    P.validate_transport(directory, altered, cases)
            path = directory / 'trace-default-jdi/queries.ndjson'
            path.write_bytes(path.read_bytes() + b'\n')
            with self.assertRaises((ValueError, AssertionError)):
                P.validate_transport(directory, report, cases)

    def test_duplicate_and_nonfinite_json(self):
        with tempfile.TemporaryDirectory(prefix='chest-predicate-json-') as tmp:
            path = Path(tmp) / 'data.json'
            for raw in ('{"x":1,"x":2}', '{"x":NaN}', '{"x":Infinity}'):
                path.write_text(raw)
                with self.assertRaises(ValueError):
                    A.load(path)


if __name__ == '__main__':
    unittest.main()
