"""Pure failure-report controls; no native process or original method is run."""
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


SPEC = importlib.util.spec_from_file_location('sleep_nbt_parent_diagnostics_target',
    Path(__file__).with_name('test_player_sleep_nbt_parent.py'))
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)


def receipt(stdout, stderr, code=0):
    return {'command': ['declared native fixture'], 'exit': code,
            'timed_out': False, 'timeout_seconds': 180,
            'pipe_capture_complete': True, 'reader_errors': {},
            'output_overflow': False, 'stdout_bytes': len(stdout),
            'stderr_bytes': len(stderr), 'limit': P.MAX_BYTES, 'reaped': True}


class FailureDiagnostics(unittest.TestCase):
    def exercise(self, result, exception=None, successful=False):
        with tempfile.TemporaryDirectory(prefix='sleep-nbt-diagnostic-control-') as name:
            root = Path(name)
            executable = root / 'declared-native-file'
            executable.write_bytes(b'pure control; never executed')
            work = root / 'runs'
            reference = root / 'reference'
            original = {'schema': 'PLAYER_SLEEP_NBT_MATRIX_V1', 'rows': [None] * 40}
            witnesses = {'schema': 'PLAYER_SLEEP_NBT_PARENT_WITNESSES_V1',
                         'observations': [None] * 2}
            packet = SimpleNamespace(_read=mock.Mock(return_value=(original, witnesses, {})))

            def run(command, output, prefix, **kwargs):
                self.assertEqual(kwargs['timeout'], 180)
                self.assertEqual(kwargs['limit'], P.MAX_BYTES)
                (output / 'native.stdout').write_bytes(result.stdout)
                (output / 'native.stderr').write_bytes(result.stderr)
                (output / 'native.capture.json').write_text(json.dumps(result.capture_report))
                if exception is not None:
                    raise exception
                return result

            process = SimpleNamespace(run_bounded=mock.Mock(side_effect=run))
            resource = SimpleNamespace(resources_at=lambda output: output.mkdir())

            def module(unused_name, path):
                path = Path(path)
                if path == reference / 'packet.py':
                    return packet
                if path.parts[-3:] == ('client', 'statistics', 'support.py'):
                    return process
                self.assertEqual(path.parts[-3:], ('client', 'player_statistics', 'support.py'))
                return resource

            stderr, stdout = io.StringIO(), io.StringIO()
            argv = ['--executable', str(executable), '--work-dir', str(work),
                    '--source-root', str(root), '--reference-dir', str(reference)]
            caught = None
            with mock.patch.object(P, 'module', side_effect=module), \
                    mock.patch.object(P, 'source_paths', return_value=[executable]), \
                    mock.patch.object(P, 'native_rows', return_value=([None] * 40, 1, [])) as parse, \
                    mock.patch.object(P, 'compare_rows', return_value={'scalar_comparisons': 1}) as compare, \
                    contextlib.redirect_stderr(stderr), contextlib.redirect_stdout(stdout):
                try:
                    outcome = P.main(argv)
                except BaseException as error:
                    caught = error
                else:
                    self.assertTrue(successful)
                    self.assertEqual(outcome, 0)
                self.assertEqual(parse.call_count, int(successful))
                self.assertEqual(compare.call_count, int(successful))
            process.run_bounded.assert_called_once()
            directories = list(work.iterdir())
            self.assertEqual(len(directories), 1)
            output = directories[0]
            report = json.loads((output / 'report.json').read_text())
            return caught, stderr.getvalue(), report, str(output)

    def assert_retained_diagnostic(self, text, report, output, capture):
        self.assertIn('Player sleep NBT native failure', text)
        self.assertIn(str(Path(output) / 'report.json'), text)
        self.assertIn(str(Path(output) / 'native.capture.json'), text)
        self.assertIn(str(Path(output) / 'native.stdout'), text)
        self.assertIn(str(Path(output) / 'native.stderr'), text)
        self.assertIn('capture receipt:', text)
        self.assertEqual(report['capture'], capture)
        self.assertEqual(report['status'], 'FAIL')

    def test_nonzero_exit_exposes_actual_capture_and_stderr(self):
        stdout, stderr = b'declared partial stdout\n', b'declared native failure sentinel\n'
        capture = receipt(stdout, stderr, 17)
        result = SimpleNamespace(returncode=17, stdout=stdout, stderr=stderr,
                                 capture_report=capture)
        error, text, report, output = self.exercise(result)
        self.assertIsInstance(error, ValueError)
        self.assertEqual(str(error), 'complete successful native process/stream inventory')
        self.assert_retained_diagnostic(text, report, output, capture)
        self.assertIn('native exit: 17', text)
        self.assertIn('declared native failure sentinel', text)
        self.assertIn('"exit": 17', text)

    def test_every_existing_completion_guard_still_rejects(self):
        stdout = b'declared stdout\n'
        positive = SimpleNamespace(returncode=0, stdout=stdout, stderr=b'',
                                   capture_report=receipt(stdout, b''))
        mutations = [('exit', 2), ('reader_errors', {'stdout': [{'phase': 'read'}]}),
                     ('pipe_capture_complete', False), ('reaped', False),
                     ('timed_out', True), ('output_overflow', True),
                     ('stdout_bytes', len(stdout) + 1), ('stderr_bytes', 1)]
        for key, value in mutations:
            with self.subTest(field=key):
                result = copy.deepcopy(positive)
                result.capture_report[key] = value
                error, text, report, output = self.exercise(result)
                self.assertIsInstance(error, ValueError)
                self.assertEqual(str(error), 'complete successful native process/stream inventory')
                self.assert_retained_diagnostic(text, report, output, result.capture_report)
        result = copy.deepcopy(positive)
        result.stderr = b'nonempty stderr at successful exit\n'
        result.capture_report['stderr_bytes'] = len(result.stderr)
        error, text, report, output = self.exercise(result)
        self.assertIsInstance(error, ValueError)
        self.assert_retained_diagnostic(text, report, output, result.capture_report)
        self.assertIn('nonempty stderr at successful exit', text)

    def test_capture_helper_exception_keeps_attached_receipt(self):
        stdout, stderr = b'partial\n', b'declared capture failure sentinel\n'
        capture = receipt(stdout, stderr, -9)
        capture['timed_out'] = True
        error = ValueError('declared controller failure')
        error.capture_report = capture
        result = SimpleNamespace(returncode=-9, stdout=stdout, stderr=stderr,
                                 capture_report=capture)
        actual, text, report, output = self.exercise(result, exception=error)
        self.assertIs(actual, error)
        self.assert_retained_diagnostic(text, report, output, capture)
        self.assertIn('"timed_out": true', text)
        self.assertIn('declared capture failure sentinel', text)

    def test_large_stderr_diagnostic_is_bounded_and_points_to_complete_file(self):
        stderr = b'x' * (64 * 1024) + b'unprinted final sentinel'
        capture = receipt(b'', stderr, 1)
        result = SimpleNamespace(returncode=1, stdout=b'', stderr=stderr,
                                 capture_report=capture)
        error, text, report, output = self.exercise(result)
        self.assertIsInstance(error, ValueError)
        self.assert_retained_diagnostic(text, report, output, capture)
        self.assertLess(len(text), 40 * 1024)
        self.assertIn('stderr excerpt truncated', text)
        self.assertNotIn('unprinted final sentinel', text)

    def test_large_receipt_is_retained_completely_while_log_excerpt_is_bounded(self):
        capture = receipt(b'', b'', 3)
        capture['reader_errors'] = {'stdout': ['x' * (64 * 1024)]}
        result = SimpleNamespace(returncode=3, stdout=b'', stderr=b'',
                                 capture_report=capture)
        error, text, report, output = self.exercise(result)
        self.assertIsInstance(error, ValueError)
        self.assert_retained_diagnostic(text, report, output, capture)
        self.assertLess(len(text), 20 * 1024)
        self.assertIn('capture receipt excerpt truncated', text)

    def test_missing_stderr_file_reports_read_error_without_dropping_receipt(self):
        capture = receipt(b'', b'', -9)
        with mock.patch.object(Path, 'stat', side_effect=OSError('declared unavailable stat')):
            text = P.native_failure_details(Path('declared-output'), capture, None)
        self.assertIn('native exit: -9', text)
        self.assertIn('native stderr unavailable: declared unavailable stat', text)
        self.assertIn('"exit": -9', text)

    def test_diagnostic_sink_failure_does_not_replace_the_original_error(self):
        capture = receipt(b'', b'', 17)
        result = SimpleNamespace(returncode=17, stdout=b'', stderr=b'',
                                 capture_report=capture)
        with mock.patch.object(P, 'native_failure_details', side_effect=OSError('declared diagnostic failure')):
            error, text, report, output = self.exercise(result)
        self.assertIsInstance(error, ValueError)
        self.assertEqual(str(error), 'complete successful native process/stream inventory')
        self.assertEqual(text, '')
        self.assertEqual(report['failure_diagnostic_error'],
                         {'type': 'OSError', 'message': 'declared diagnostic failure'})
        self.assertEqual(report['capture'], capture)

    def test_success_keeps_existing_comparison_path_and_prints_no_failure(self):
        stdout = b'declared valid parser input\n'
        result = SimpleNamespace(returncode=0, stdout=stdout, stderr=b'',
                                 capture_report=receipt(stdout, b''))
        error, text, report, output = self.exercise(result, successful=True)
        self.assertIsNone(error)
        self.assertEqual(text, '')
        self.assertEqual(report['status'], 'PASS')
        self.assertEqual(report['capture'], result.capture_report)


if __name__ == '__main__':
    unittest.main()
