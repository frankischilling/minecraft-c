"""Portable corruption controls; synthetic transport is not execution evidence."""
import base64
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, HERE / filename)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


A = module('furnace_controls_audit', 'audit.py')
P = module('furnace_controls_publish', 'publish.py')
C = P.C


def payload(kind, value):
    if kind in (1, 2, 3, 4):
        return struct.pack({1: '>b', 2: '>h', 3: '>i', 4: '>q'}[kind], value)
    if kind == 8:
        raw = value.encode('ascii')
        return struct.pack('>H', len(raw)) + raw
    if kind == 9:
        child, values = value
        return bytes([child]) + struct.pack('>i', len(values)) + b''.join(payload(child, v) for v in values)
    if kind == 10:
        return b''.join(bytes([v[0]]) + payload(8, name) + payload(*v) for name, v in value.items()) + b'\0'
    raise ValueError('undeclared controlled NBT tag')


def stdout(cases, observed, pins):
    lines = ['16 achievements', '151 recipes', 'FURNACE_ORIGINS ' + json.dumps({
        'jar_sha256': A.JAR, 'classes': {name: value['class_sha256'] for name, value in pins['classes'].items()}}),
        'FURNACE_RECIPES_BEFORE ' + json.dumps(observed['recipes_before'])]
    for row in copy.deepcopy(observed['rows']):
        for key, raw_key in (('nbt', 'nbt_base64'), ('input_nbt', 'input_nbt_base64')):
            wire = row.pop(key)
            row[raw_key] = None if wire is None else base64.b64encode(b'\x0a\0\0' + payload(10, wire['tags'])).decode()
        lines.append('FURNACE_ROW ' + json.dumps(row))
    lines += ['FURNACE_RECIPES_AFTER ' + json.dumps(observed['recipes_after']),
              'FURNACE_COMPLETE ' + json.dumps(dict(rows=len(cases), network=0, native=0, execs=0, writes=0))]
    return ('\n'.join(lines) + '\n').encode()


def process(directory, stem, out=b'', err=b''):
    result = dict(exit=0, timed_out=False, pipe_capture_complete=True, reader_errors={}, output_overflow=False,
                  timeout_seconds=300)
    for stream, raw, limit in (('stdout', out, C.C.STDOUT_LIMIT), ('stderr', err, C.C.STDERR_LIMIT)):
        filename = stem + '.' + stream
        (directory / filename).write_bytes(raw)
        result.update({stream + '_file': filename, stream + '_limit': limit,
                       stream + '_bytes': len(raw), stream + '_sha256': hashlib.sha256(raw).hexdigest()})
    return result


def controlled_transport(directory):
    cases = A.load(HERE / 'cases.json')['cases']
    observed = A.load(HERE / 'finite.json')['observations']
    pins = A.load(HERE / 'source-pins.json')
    traces = A.load(HERE / 'traces.json')
    raw = stdout(cases, observed, pins)
    report = dict(observations=observed, pins=pins, traces={}, live_Code={}, trace_artifacts={})
    warnings = ('\n'.join(C.expected_warnings(directory / 'classes')) + '\n').encode()
    for key, stem in (('compile', 'compile'), ('java_version', 'java-version'), ('javac_version', 'javac-version')):
        report[key] = process(directory, stem)
    for stem in ('original', 'original-repeat'):
        report[stem] = process(directory, stem, raw, warnings)
    for stem in ('trace-interpreter', 'trace-default'):
        target = directory / (stem + '-jdi')
        target.mkdir()
        files = {'target.stdout': raw, 'target.stderr': warnings,
                 'calls.ndjson': ('\n'.join(json.dumps(r) for r in traces['rows']) + '\n').encode(),
                 'live-Code.json': json.dumps(traces['live_Code']).encode()}
        for name, value in files.items():
            (target / name).write_bytes(value)
        report['trace_artifacts'][stem] = {name: dict(bytes=len(value), sha256=hashlib.sha256(value).hexdigest())
                                            for name, value in files.items()}
        report['traces'][stem], report['live_Code'][stem] = copy.deepcopy(traces['rows']), copy.deepcopy(traces['live_Code'])
        summary = f'FURNACE_TRACE_COMPLETE rows={len(cases)} entries={sum(len(r["entries"]) for r in traces["rows"])} live-code={len(traces["live_Code"])}\n'
        report[stem] = process(directory, stem, summary.encode())
    return report, cases


class FurnacePacket(unittest.TestCase):
    def test_public_packet(self):
        result = A.audit()
        self.assertEqual(result['original_cases'], 207)
        self.assertEqual(result['BM_cases'], 27)
        self.assertEqual(result['source_entries'], 2754)
        self.assertEqual(result['live_Code_pins'], 74)

    def test_each_public_input_and_output_is_pinned(self):
        with tempfile.TemporaryDirectory(prefix='furnace-public-') as tmp:
            packet = Path(tmp) / 'packet'
            shutil.copytree(HERE, packet, ignore=shutil.ignore_patterns('__pycache__'))
            for name in (*A.EXECUTION, *A.PUBLICATION, *A.OUTPUTS):
                path = packet / name
                old = path.read_bytes()
                path.write_bytes(old + b' ')
                with self.subTest(name=name), self.assertRaises(ValueError):
                    A.audit(packet)
                path.write_bytes(old)

    def test_duplicate_and_nonfinite_json(self):
        with tempfile.TemporaryDirectory(prefix='furnace-json-') as tmp:
            path = Path(tmp) / 'bad.json'
            for value in ('{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}'):
                path.write_text(value)
                with self.assertRaises(ValueError):
                    A.load(path)

    def test_declared_state_identity_and_width_corruptions(self):
        cases = A.load(HERE / 'cases.json')['cases']
        observed = A.load(HERE / 'finite.json')['observations']
        mutations = (lambda row: row['before']['fields'].__setitem__(0, 1),
                     lambda row: row['before']['xyz'].__setitem__(0, 1),
                     lambda row: row['before']['player'].__setitem__(0, 'missing'),
                     lambda row: row['after']['stacks'].__setitem__('bad-label', dict(id=1, count=1, damage=0, animation=0)),
                     lambda row: row['after']['fields'].__setitem__(0, 2147483648),
                     lambda row: row.__setitem__('returned', 1),
                     lambda row: row.__setitem__('exception', 'invented.failure'))
        for index, mutate in enumerate(mutations):
            damaged = copy.deepcopy(observed)
            mutate(damaged['rows'][0])
            with self.subTest(index=index), self.assertRaises(ValueError):
                A.validate(cases, damaged)
        wire = next(r['nbt'] for r in observed['rows'] if r['nbt'] is not None)
        for child in ([2, 32768], [1, 128], [True, 1], [9, [0, [1]]]):
            with self.subTest(tag=child), self.assertRaises(ValueError):
                A.tag(*child)
        self.assertIn('BurnTime', wire['tags'])

    def test_trace_types_call_sites_and_live_code(self):
        cases = A.load(HERE / 'cases.json')['cases']
        trace = A.load(HERE / 'traces.json')
        pins = A.load(HERE / 'source-pins.json')
        mutations = (lambda r: r[0].__setitem__('index', 1),
                     lambda r: r[0]['entries'][0].__setitem__('method', 'sk.fake()V'),
                     lambda r: r[0]['entries'][0].__setitem__('receiver', {'type': 'sk'}),
                     lambda r: r[0]['entries'][0].__setitem__('source_parent', [{'method': 'sk.<init>()V', 'pc': 9999}]))
        for index, mutate in enumerate(mutations):
            damaged = copy.deepcopy(trace['rows'])
            mutate(damaged)
            with self.subTest(index=index), self.assertRaises(ValueError):
                A.validate_traces(cases, damaged, trace['live_Code'], pins)
        code = copy.deepcopy(trace['live_Code'])
        next(iter(code.values()))['code_sha256'] = '0' * 64
        with self.assertRaises(ValueError):
            A.validate_traces(cases, trace['rows'], code, pins)

    def test_complete_controlled_transport_reparses_all_four_targets(self):
        with tempfile.TemporaryDirectory(prefix='furnace-transport-') as tmp:
            directory = Path(tmp)
            report, cases = controlled_transport(directory)
            P.validate_transport(directory, report, cases)

    def test_report_result_or_trace_cannot_replace_actual_stream(self):
        with tempfile.TemporaryDirectory(prefix='furnace-report-') as tmp:
            directory = Path(tmp)
            report, cases = controlled_transport(directory)
            for mutate in (lambda r: r['observations']['rows'][1].__setitem__('result', 4),
                           lambda r: r['traces']['trace-interpreter'][0]['entries'].reverse(),
                           lambda r: r['live_Code']['trace-default'].clear()):
                changed = copy.deepcopy(report)
                mutate(changed)
                with self.assertRaises((ValueError, AssertionError)):
                    P.validate_transport(directory, changed, cases)

    def test_status_extent_hash_and_pipe_completion_corruptions(self):
        with tempfile.TemporaryDirectory(prefix='furnace-status-') as tmp:
            directory = Path(tmp)
            report, cases = controlled_transport(directory)
            for key, bad in (('exit', 1), ('exit', False), ('timed_out', True),
                             ('pipe_capture_complete', False), ('output_overflow', True),
                             ('reader_errors', {'stdout': ['error']}), ('timeout_seconds', 301),
                             ('stdout_bytes', 0), ('stdout_sha256', '0' * 64), ('stdout_limit', 1),
                             ('stdout_file', '../original.stdout')):
                changed = copy.deepcopy(report)
                changed['original'][key] = bad
                with self.subTest(key=key), self.assertRaises(ValueError):
                    P.validate_transport(directory, changed, cases)

    def test_every_actual_raw_stream_and_journal_is_hash_bound(self):
        with tempfile.TemporaryDirectory(prefix='furnace-raw-') as tmp:
            directory = Path(tmp)
            report, cases = controlled_transport(directory)
            paths = [directory / (stem + '.stdout') for stem in ('original', 'original-repeat', 'trace-interpreter', 'trace-default')]
            paths += [directory / (stem + '-jdi') / name for stem in ('trace-interpreter', 'trace-default')
                      for name in ('target.stdout', 'target.stderr', 'calls.ndjson', 'live-Code.json')]
            for path in paths:
                old = path.read_bytes()
                path.write_bytes(old + b' ')
                with self.subTest(path=path.name), self.assertRaises(ValueError):
                    P.validate_transport(directory, report, cases)
                path.write_bytes(old)

    def test_changed_well_formed_raw_result_is_reparsed_not_just_hashed(self):
        with tempfile.TemporaryDirectory(prefix='furnace-raw-result-') as tmp:
            directory = Path(tmp)
            report, cases = controlled_transport(directory)
            path = directory / 'original-repeat.stdout'
            lines = path.read_text().splitlines()
            index = next(i for i, line in enumerate(lines) if line.startswith('FURNACE_ROW ') and '"profile": "size"' in line)
            row = json.loads(lines[index][len('FURNACE_ROW '):])
            row['result'] = 4
            lines[index] = 'FURNACE_ROW ' + json.dumps(row)
            raw = ('\n'.join(lines) + '\n').encode()
            path.write_bytes(raw)
            report['original-repeat'].update(stdout_bytes=len(raw), stdout_sha256=hashlib.sha256(raw).hexdigest())
            with self.assertRaises(ValueError):
                P.validate_transport(directory, report, cases)


if __name__ == '__main__':
    unittest.main()
