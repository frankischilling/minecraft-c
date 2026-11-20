"""Reject corrupt V3 quiet-setter receipts and retain failed one-process runs.

All clock rows and process receipts authored here are synthetic reader controls.
No benchmark executable is started and no timing observation is qualified.
"""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    'quiet_setter_public_runner', ROOT / 'scripts/benchmarks/quiet_setter_noop.py')
RUN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUN)


def wire(count=8, depth=0, setter='block'):
    # Literal output fold bound by V3's independently retained raw receipts.
    # These positive ticks are transport fixtures, not performance evidence.
    metadata = int(setter == 'metadata')
    value = 15416205869579715459
    lines = ['16 achievements', '151 recipes',
             f'QUIET_NOOP_CONTEXT count={count} depth={depth} metadata={metadata} calls=8192 '
             'samples=7 mutation_error_bytes=24296 acquisition_error_bytes=4648']
    for index in range(7):
        lines.append(f'QUIET_NOOP_SAMPLE sample={index} calls=8192 ticks=1000 '
                     f'frequency=10000000 checksum={value} changes=0')
    lines.extend([
        f'QUIET_NOOP_INVARIANTS count={count} depth={depth} metadata={metadata} calls=57344 '
        f'planes=123 checksum={value} diagnostic=456 diagnostic_compared_bytes=22646 '
        'diagnostic_excluded_padding=1650 diagnostic_named_compared=6298 '
        'diagnostic_unknown_compared=16348 diagnostic_layout=8267465906294600077 '
        'source_state=1 scopes=1 ownership=1',
        'QUIET_NOOP_FINISH checks=89 cleanup=1 genuine_source_world=1'])
    return ('\n'.join(lines) + '\n').encode('utf-8')


class ProtocolTests(unittest.TestCase):
    def test_complete_declared_scenes(self):
        for count in (8, 256):
            for depth in (0, 1):
                for setter in ('block', 'metadata'):
                    with self.subTest(count=count, depth=depth, setter=setter):
                        result = RUN.parse(wire(count, depth, setter), count, depth, setter)
                        self.assertEqual(len(result['samples']), 7)
                        self.assertEqual(result['invariants']['calls'], 57344)
                        self.assertEqual(result['invariants']['diagnostic_compared_bytes'], 22646)
                        self.assertGreater(result['median_ns_per_call'], 0)

    def test_startup_pair_optional_but_not_partial(self):
        raw = wire()
        self.assertEqual(RUN.parse(raw, 8, 0, 'block'),
                         RUN.parse(raw.split(b'\n', 2)[2], 8, 0, 'block'))
        for value in (raw.split(b'\n', 1)[1], raw.replace(b'151 recipes\n', b'', 1),
                      raw.replace(b'16 achievements\n', b'16 achievements\n16 achievements\n', 1)):
            with self.subTest(value=value[:80]), self.assertRaises(ValueError):
                RUN.parse(value, 8, 0, 'block')

    def test_windows_crlf_transport(self):
        raw = wire().replace(b'\n', b'\r\n')
        before = hashlib.sha256(raw).hexdigest()
        self.assertEqual(RUN.parse(raw, 8, 0, 'block'), RUN.parse(wire(), 8, 0, 'block'))
        self.assertEqual(hashlib.sha256(raw).hexdigest(), before)
        self.assertNotEqual(before, hashlib.sha256(wire()).hexdigest())

    def test_order_count_and_completion(self):
        lines = wire().splitlines()
        reordered = list(lines)
        reordered[3], reordered[4] = reordered[4], reordered[3]
        corrupt = [wire().rstrip(b'\n'), b'\n'.join(lines[:-1]) + b'\n',
                   b'\n'.join(reordered) + b'\n', wire() + lines[-1] + b'\n',
                   wire().replace(b'count=8', b'count=256'),
                   wire().replace(b'depth=0', b'depth=1'),
                   wire().replace(b'metadata=0', b'metadata=1'),
                   wire().replace(b'sample=6', b'sample=5'),
                   wire().replace(b'calls=8192', b'calls=8191', 1),
                   wire().replace(b'calls=57344', b'calls=57343')]
        for value in corrupt:
            with self.subTest(value=value[-150:]), self.assertRaises(ValueError):
                RUN.parse(value, 8, 0, 'block')

    def test_finite_layout_and_semantic_guards(self):
        for key, value in (
                ('mutation_error_bytes', 24295), ('acquisition_error_bytes', 0),
                ('diagnostic_compared_bytes', 22645), ('diagnostic_excluded_padding', 1651),
                ('diagnostic_named_compared', 6297), ('diagnostic_unknown_compared', 16347),
                ('diagnostic_layout', 8267465906294600076), ('diagnostic', 0),
                ('planes', 0), ('changes', 1), ('ticks', 0), ('frequency', 0),
                ('checks', 0), ('checksum', 15416205869579715460),
                ('source_state', 0), ('scopes', 0), ('ownership', 0),
                ('cleanup', 0), ('genuine_source_world', 0)):
            import re
            damaged = re.sub(rb'\b' + key.encode('ascii') + rb'=[0-9]+',
                             key.encode('ascii') + b'=' + str(value).encode('ascii'), wire(), count=1)
            with self.subTest(key=key), self.assertRaises(ValueError):
                RUN.parse(damaged, 8, 0, 'block')

    def test_integer_grammar_bounds_and_frequency(self):
        corrupt = [wire().replace(b'ticks=1000', replacement, 1) for replacement in
                   (b'ticks=01000', b'ticks=-1', b'ticks=+1', b'ticks=1.0',
                    b'ticks=18446744073709551616', b'ticks=1000 ticks=1000',
                    b'ticks=1000 extra=1', b'ticks=1000  frequency=10000000')]
        corrupt.append(wire().replace(b'frequency=10000000', b'frequency=9999999', 1))
        for value in corrupt:
            with self.subTest(value=value[:160]), self.assertRaises(ValueError):
                RUN.parse(value, 8, 0, 'block')

    def test_encoding_and_output_bound(self):
        for value in (wire() + b'\0\n', wire().replace(b'\n', b'\r'),
                      wire().replace(b'\n', b'\r\r\n'),
                      wire().replace(b'QUIET_NOOP_CONTEXT', b'\xff', 1),
                      b'x' * RUN.BASE.BOUND + b'\n'):
            with self.subTest(size=len(value)), self.assertRaises(ValueError):
                RUN.parse(value, 8, 0, 'block')

    def test_undeclared_scene(self):
        for count, depth, setter in ((64, 0, 'block'), (8, 2, 'block'), (8, 0, 'other')):
            with self.subTest(count=count, depth=depth, setter=setter), self.assertRaises(ValueError):
                RUN.parse(wire(), count, depth, setter)


class RetainedRunTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.saving = RUN.BASE.load_module('quiet_test_source_seed',
                                          ROOT / 'tests/tools/world/test_source_saving.py')
        cls.capture = RUN.BASE.load_module('quiet_test_capture_contract',
                                           ROOT / 'tests/reference/client/controller/capture.py')

    def exercise(self, change=None, failure=False, stdout=None, inventory_failure=False):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            executable = root / 'synthetic-not-executed'
            executable.write_bytes(b'reader control, never executed')
            calls = []

            def capture(command, output, stem, timeout, working_directory=None):
                calls.append(command)
                self.assertEqual(len(command), 5)
                self.assertEqual(command[2:], ['8', '0', 'block'])
                self.assertEqual(working_directory, output)
                work = Path(command[1])
                (work / 'world/session.lock').write_bytes(bytes(8))
                stdout_path = output / (stem + '.stdout')
                stderr = output / (stem + '.stderr')
                stdout_path.write_bytes(wire() if stdout is None else stdout)
                stderr.write_bytes(b'')
                report = dict(command=command, exit=0, timeout_seconds=timeout, timed_out=False,
                              output_overflow=False, pipe_capture_complete=True, reader_errors={},
                              stdout_file=stdout_path.name, stderr_file=stderr.name, stderr_bytes=0,
                              stdout_bytes=stdout_path.stat().st_size,
                              stdout_sha256=RUN.BASE.pin(stdout_path)['sha256'],
                              stderr_sha256=RUN.BASE.pin(stderr)['sha256'])
                if change is not None:
                    change(work, executable, stdout_path, report)
                (output / (stem + '.capture.json')).write_text(json.dumps(report), encoding='utf-8')
                return report

            loader = lambda name, path: self.saving if path.name == 'test_source_saving.py' else (
                SimpleNamespace(capture=capture, require_success=self.capture.require_success))
            real_inventory = RUN.BASE.inventory

            def inventory(directory):
                if inventory_failure and (directory / 'world/session.lock').exists():
                    raise OSError('synthetic post-native inventory failure')
                return real_inventory(directory)

            args = ['--executable', str(executable), '--artifacts', str(root / 'artifacts'),
                    '--chunks', '8', '--depth', '0', '--setter', 'block']
            with mock.patch.object(RUN.BASE, 'load_module', side_effect=loader), \
                    mock.patch.object(RUN.BASE, 'inventory', side_effect=inventory), \
                    contextlib.redirect_stdout(io.StringIO()):
                if failure:
                    with self.assertRaises(Exception):
                        RUN.main(args)
                else:
                    self.assertEqual(RUN.main(args), 0)
            outputs = list((root / 'artifacts').iterdir())
            self.assertEqual(len(outputs), 1)
            self.assertEqual(len(calls), 1, 'one attempt, with no automatic rerun')
            self.assertTrue((outputs[0] / 'native.stdout').is_file())
            self.assertTrue((outputs[0] / 'native.capture.json').is_file())
            report = json.loads((outputs[0] / 'report.json').read_text(encoding='utf-8'))
            self.assertEqual(report['status'], 'FAIL' if failure else 'PASS_FIXED_WORK_SEMANTICS')
            return report

    def test_one_process_complete_authored_files(self):
        report = self.exercise()
        self.assertTrue(report['inputs_unchanged'])
        self.assertTrue(report['authored_files_unchanged'])
        self.assertEqual(set(report['files_after']) - set(report['authored_inputs']),
                         {'world/session.lock'})
        self.assertEqual(report['files_after']['world/session.lock']['bytes'], 8)

    def test_windows_crlf_capture_keeps_raw_hash(self):
        raw = wire().replace(b'\n', b'\r\n')
        report = self.exercise(stdout=raw)
        self.assertEqual(report['native']['stdout_bytes'], len(raw))
        self.assertEqual(report['native']['stdout_sha256'], hashlib.sha256(raw).hexdigest())
        self.assertNotEqual(report['native']['stdout_sha256'], hashlib.sha256(wire()).hexdigest())
        self.assertEqual(report['observation']['context']['calls'], 8192)

    def test_transport_clean_file_corruption_retained(self):
        changes = {
            'rewrite': lambda w, e, s, r: (w / 'world/level.dat').write_bytes(b'changed'),
            'extra': lambda w, e, s, r: (w / 'world/extra').write_bytes(b''),
            'short_session': lambda w, e, s, r: (w / 'world/session.lock').write_bytes(bytes(7)),
            'missing_session': lambda w, e, s, r: (w / 'world/session.lock').unlink(),
        }
        for name, change in changes.items():
            with self.subTest(name=name):
                report = self.exercise(change, failure=True)
                self.assertTrue(report['inputs_unchanged'])
                self.assertFalse(report['authored_files_unchanged'])
                self.assertIn('file_continuity_error', report)
                self.assertIn('error', report)

    def test_changed_stdout_and_executable_retained(self):
        for change in (lambda w, e, s, r: s.write_bytes(wire() + b'changed\n'),
                       lambda w, e, s, r: s.with_suffix('.stderr').write_bytes(b'changed\n')):
            with self.subTest(change=change):
                report = self.exercise(change, failure=True)
                self.assertIn('error', report)
        report = self.exercise(lambda w, e, s, r: e.write_bytes(b'changed executable'), failure=True)
        self.assertFalse(report['inputs_unchanged'])
        self.assertIn('continuity_error', report)

    def test_capture_failures_retained(self):
        for key, value in (('exit', 1), ('timed_out', True), ('output_overflow', True),
                           ('pipe_capture_complete', False), ('stderr_bytes', 1),
                           ('reader_errors', {'stdout': [{'phase': 'read'}]})):
            with self.subTest(key=key):
                report = self.exercise(lambda w, e, s, r: r.update({key: value}), failure=True)
                self.assertIn('error', report)
                self.assertTrue(report['inputs_unchanged'])
                self.assertIn('files_after', report)
                self.assertTrue(report['authored_files_unchanged'])

    def test_native_and_file_failures_both_retained(self):
        def damage(work, executable, stdout, receipt):
            receipt['timed_out'] = True
            (work / 'world/level.dat').write_bytes(b'changed')

        report = self.exercise(damage, failure=True)
        self.assertTrue(report['native']['timed_out'])
        self.assertIn('files_after', report)
        self.assertIn('file_continuity_error', report)
        self.assertFalse(report['authored_files_unchanged'])
        self.assertIn('error', report)

    def test_inventory_failure_retained(self):
        report = self.exercise(failure=True, inventory_failure=True)
        self.assertFalse(report['authored_files_unchanged'])
        self.assertIn('files_after_error', report)
        self.assertIn('error', report)
        report = self.exercise(lambda w, e, s, r: r.update(timed_out=True),
                               failure=True, inventory_failure=True)
        self.assertTrue(report['native']['timed_out'])
        self.assertIn('files_after_error', report)
        self.assertIn('error', report)


if __name__ == '__main__':
    unittest.main()
