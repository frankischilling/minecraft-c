"""Portable corruption controls; controlled transport is not new execution evidence."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, HERE / filename)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


A = module('furnace_world_controls_audit', 'audit.py')
P = module('furnace_world_controls_publish', 'publish.py')
C = P.C


def process(directory, stem, stdout=b'', stderr=b''):
    result = dict(exit=0, timed_out=False, pipe_capture_complete=True, reader_errors={}, output_overflow=False, timeout_seconds=300)
    for stream, raw, limit in (('stdout', stdout, C.C.STDOUT_LIMIT), ('stderr', stderr, C.C.STDERR_LIMIT)):
        name = stem + '.' + stream
        (directory / name).write_bytes(raw)
        result.update({stream + '_file': name, stream + '_limit': limit, stream + '_bytes': len(raw),
                       stream + '_sha256': hashlib.sha256(raw).hexdigest()})
    return result


def controlled_transport(directory):
    declaration = A.load(HERE / 'cases.json')
    report = dict(rows=A.load(HERE / 'finite.json')['rows'], entropy=A.load(HERE / 'random.json')['streams'],
                  pins=A.load(HERE / 'source-pins.json'), setup={}, completion={}, traces={}, live_Code={}, trace_artifacts={})
    setup = A.load(HERE / 'setup.json')['world']
    traces = A.load(HERE / 'traces.json')
    for key, stem in (('compile', 'compile'), ('java_version', 'java-version'), ('javac_version', 'javac-version')):
        report[key] = process(directory, stem)
    warnings = ('\n'.join(C.expected_warnings(directory / 'classes')) + '\n').encode()
    for stem in A.MODES:
        report['setup'][stem] = copy.deepcopy(setup)
        report['completion'][stem] = dict(rows=7, network=0, native=0, execs=0, denied_writes=0, world_writes=3, canonical_chunks=4)
        lines = ['16 achievements', '151 recipes', 'FURNACE_WORLD_ORIGINS ' + json.dumps(dict(
            jar_sha256=A.JAR, classes={name: value['class_sha256'] for name, value in report['pins']['classes'].items()})),
            'FURNACE_WORLD_SETUP ' + json.dumps(setup)]
        for row, random in zip(report['rows'], report['entropy'][stem]):
            lines += ['FURNACE_WORLD_ROW ' + json.dumps(row), 'FURNACE_WORLD_RANDOM ' + json.dumps(random)]
        lines.append('FURNACE_WORLD_COMPLETE ' + json.dumps(report['completion'][stem]))
        raw = ('\n'.join(lines) + '\n').encode()
        if stem.startswith('trace-'):
            target = directory / (stem + '-jdi')
            target.mkdir()
            files = {'target.stdout': raw, 'target.stderr': warnings,
                     'phases.ndjson': ('\n'.join(json.dumps(v) for v in traces['rows']) + '\n').encode(),
                     'live-Code.json': json.dumps(traces['live_Code']).encode()}
            for name, value in files.items():
                (target / name).write_bytes(value)
            report['trace_artifacts'][stem] = {name: dict(bytes=len(v), sha256=hashlib.sha256(v).hexdigest()) for name, v in files.items()}
            report['traces'][stem], report['live_Code'][stem] = copy.deepcopy(traces['rows']), copy.deepcopy(traces['live_Code'])
            summary = f'FURNACE_WORLD_TRACE_COMPLETE rows=7 phases={sum(len(v["phases"]) for v in traces["rows"])} live-code={len(traces["live_Code"])}\n'
            report[stem] = process(directory, stem, summary.encode())
        else:
            report[stem] = process(directory, stem, raw, warnings)
    return declaration, report


class FurnaceWorldPacket(unittest.TestCase):
    def test_public_packet(self):
        result = A.audit()
        self.assertEqual((result['steps'], result['source_phases'], result['live_Code_pins']), (7, 196, 23))

    def test_each_public_file_is_pinned(self):
        with tempfile.TemporaryDirectory(prefix='furnace-world-public-') as tmp:
            directory = Path(tmp) / 'packet'
            shutil.copytree(HERE, directory, ignore=shutil.ignore_patterns('__pycache__'))
            for name in (*A.EXECUTION, *A.PUBLICATION, *A.OUTPUTS):
                path = directory / name
                old = path.read_bytes()
                path.write_bytes(old + b' ')
                with self.subTest(name=name), self.assertRaises(ValueError):
                    A.audit(directory)
                path.write_bytes(old)

    def test_state_continuity_identity_width_and_validity_corruptions(self):
        declaration, rows = A.load(HERE / 'cases.json'), A.load(HERE / 'finite.json')['rows']
        mutations = (lambda r: r[2]['before']['active'].pop(),
                     lambda r: r[2]['before']['tiles']['T1'].__setitem__('invalid', True),
                     lambda r: r[3]['before']['tiles']['T0']['fields'].__setitem__(2, 198),
                     lambda r: r[4]['before']['tiles']['T0']['slots'].__setitem__(1, 'S1'),
                     lambda r: r[0]['after']['tiles']['T0']['fields'].__setitem__(0, 2147483648),
                     lambda r: r[0]['after']['maps'][0]['values'].__setitem__(0, 'missing'),
                     lambda r: r[0]['after']['stacks']['S0'].__setitem__(3, True),
                     lambda r: r[0].__setitem__('returned', 1))
        for index, mutate in enumerate(mutations):
            bad = copy.deepcopy(rows)
            mutate(bad)
            with self.subTest(index=index), self.assertRaises(ValueError):
                A.validate(declaration, bad)

    def test_actual_phase_and_code_corruptions(self):
        declaration, traces = A.load(HERE / 'cases.json'), A.load(HERE / 'traces.json')
        pins = A.load(HERE / 'source-pins.json')
        for mutate in (lambda r: r[1]['phases'][0].__setitem__('method', 'fd.fake()V'),
                       lambda r: r[1]['phases'][0]['state'].__setitem__('iterating', 1),
                       lambda r: r[1]['phases'][0]['state']['tiles']['T0'].__setitem__('invalid', 0),
                       lambda r: r[1]['phases'][0].__setitem__('source_parent', [{'method': 'fd.g()V', 'pc': 9999}])):
            rows = copy.deepcopy(traces['rows'])
            mutate(rows)
            with self.assertRaises(ValueError):
                A.validate_traces(declaration, rows, traces['live_Code'], pins)
        codes = copy.deepcopy(traces['live_Code'])
        next(iter(codes.values()))['code_sha256'] = '0' * 64
        with self.assertRaises(ValueError):
            A.validate_traces(declaration, traces['rows'], codes, pins)

    def test_unreset_random_stream_continuity(self):
        declaration, streams = A.load(HERE / 'cases.json'), A.load(HERE / 'random.json')['streams']
        streams['default'][1]['before'][0] = '000000000000'
        with self.assertRaises(ValueError):
            A.validate_random(declaration, streams)

    def test_complete_controlled_four_target_transport(self):
        with tempfile.TemporaryDirectory(prefix='furnace-world-transport-') as tmp:
            declaration, report = controlled_transport(Path(tmp))
            P.validate_transport(Path(tmp), report, declaration)

    def test_report_does_not_replace_any_original_stream_or_phase_journal(self):
        with tempfile.TemporaryDirectory(prefix='furnace-world-report-') as tmp:
            directory = Path(tmp)
            declaration, report = controlled_transport(directory)
            for mutate in (lambda r: r['rows'][1]['after']['tiles']['T1'].__setitem__('invalid', True),
                           lambda r: r['traces']['trace-default'][1]['phases'].reverse(),
                           lambda r: r['entropy']['default'][0]['before'].__setitem__(0, '000000000000'),
                           lambda r: r['setup']['interpreter'].__setitem__('remote', True)):
                bad = copy.deepcopy(report)
                mutate(bad)
                with self.assertRaises((ValueError, AssertionError)):
                    P.validate_transport(directory, bad, declaration)

    def test_each_retained_raw_stream_and_phase_file_is_hash_bound(self):
        with tempfile.TemporaryDirectory(prefix='furnace-world-raw-') as tmp:
            directory = Path(tmp)
            declaration, report = controlled_transport(directory)
            paths = [directory / (stem + '.stdout') for stem in A.MODES]
            paths += [directory / (stem + '-jdi') / name for stem in A.MODES[2:]
                      for name in ('target.stdout', 'target.stderr', 'phases.ndjson', 'live-Code.json')]
            for path in paths:
                old = path.read_bytes()
                path.write_bytes(old + b' ')
                with self.subTest(path=path.name), self.assertRaises(ValueError):
                    P.validate_transport(directory, report, declaration)
                path.write_bytes(old)

    def test_process_status_count_hash_and_completion_corruptions(self):
        with tempfile.TemporaryDirectory(prefix='furnace-world-status-') as tmp:
            directory = Path(tmp)
            declaration, report = controlled_transport(directory)
            for key, value in (('exit', 1), ('exit', False), ('timed_out', True), ('pipe_capture_complete', False),
                               ('output_overflow', True), ('reader_errors', {'stdout': ['error']}),
                               ('stdout_bytes', 0), ('stdout_sha256', '0' * 64), ('stdout_limit', 1)):
                bad = copy.deepcopy(report)
                bad['interpreter'][key] = value
                with self.subTest(key=key), self.assertRaises(ValueError):
                    P.validate_transport(directory, bad, declaration)

    def test_duplicate_and_nonfinite_json(self):
        with tempfile.TemporaryDirectory(prefix='furnace-world-json-') as tmp:
            path = Path(tmp) / 'bad.json'
            for text in ('{"x":1,"x":2}', '{"x":NaN}', '{"x":Infinity}'):
                path.write_text(text)
                with self.assertRaises(ValueError):
                    A.load(path)


if __name__ == '__main__':
    unittest.main()
