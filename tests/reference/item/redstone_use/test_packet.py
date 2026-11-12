"""Original-only admission and in-memory semantic corruption controls."""
import copy
import importlib.util
from pathlib import Path
import sys
import json
import tempfile
import unittest

sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location('redstone_use_admission',
    Path(__file__).with_name('packet.py'))
a = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(a)


CAPTURE = Path(__file__).resolve().parent
PREPARED = CAPTURE


class AuthoredTests(unittest.TestCase):
    def test_original_authored_operands_and_code(self):
        self.assertEqual({k: len(v) for k, v in a.authored(PREPARED).items()},
                         {'use': 42, 'support': 12})


class AdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.payload, cls.records, cls.rosters = {}, {}, {}
        cls.capture_root = a.load(CAPTURE/'provenance.json')['capture_root']
        for cohort in ('use', 'support'):
            directory = CAPTURE/(cohort+'-ordinary-0')
            cls.payload[cohort] = {key: (directory/name).read_bytes() for key, name in
                (('stdout', 'original.stdout'), ('stderr', 'original.stderr'), ('selected', 'selected.txt'),
                 ('entropy', 'own-entropy.jsonl'), ('exceptions', 'source-exceptions.jsonl'))}
            cls.rosters[cohort] = a.profiles(PREPARED/(cohort+'-profiles.tsv'), cohort)
            prefix = 'USE\t' if cohort == 'use' else 'SUPPORT\t'
            lines = cls.payload[cohort]['stdout'].decode('ascii').splitlines()
            cls.records[cohort] = tuple([a.loads(x.split('\t', 1)[1]) for x in lines if x.startswith(tag)]
                                       for tag in (prefix, 'OWN_ENTROPY\t', 'SOURCE_EXCEPTION\t'))

    def reject_record(self, mutate, message, cohort='use'):
        rows, own, exceptions = copy.deepcopy(self.records[cohort])
        mutate(rows, own, exceptions)
        with self.assertRaisesRegex(ValueError, message):
            a.validate_records(cohort, rows, own, exceptions, self.rosters[cohort])

    @staticmethod
    def row(rows, name):
        return next(r for r in rows if r['name'] == name)

    def test_actual_sealed_four_vm_capture(self):
        result = a.validate(CAPTURE)
        self.assertEqual(result['status'], 'PASS')
        self.assertEqual([r['rows'] for r in result['runs'].values()], [42, 42, 12, 12])
        self.assertEqual(a.read_packet(CAPTURE), result['rows'])

    def test_actual_raw_streams_admitted(self):
        for cohort in ('use', 'support'):
            summary, rows, errors = a.validate_streams(cohort, self.payload[cohort],
                                                      self.rosters[cohort], self.capture_root)
            self.assertEqual(summary['rows'], 42 if cohort == 'use' else 12)
            self.assertEqual(len(errors), 6 if cohort == 'use' else 1)

    def reject_capture(self, mutate, message):
        with tempfile.TemporaryDirectory(prefix='redstone-use-corruption-') as name:
            folder = Path(name)
            for path in a.frozen_files():
                target = folder / path
                target.parent.mkdir(exist_ok=True)
                target.write_bytes((CAPTURE / path).read_bytes())
            report = a.load(folder / 'capture.json')
            mutate(report)
            (folder / 'capture.json').write_text(json.dumps(report) + '\n', encoding='ascii', newline='\n')
            provenance = a.load(folder / 'provenance.json')
            provenance['capture_sha256'] = a.sha(folder / 'capture.json')
            (folder / 'provenance.json').write_text(json.dumps(provenance) + '\n', encoding='ascii', newline='\n')
            with self.assertRaisesRegex(ValueError, message):
                a.validate_evidence(folder)

    def test_compiled_helper_closure_semantically_closed(self):
        self.reject_capture(lambda r: [r[k].update({'Injected.class': '0' * 64})
                                       for k in ('helpers_before', 'helpers_after')],
                            'three compiled authored helper')

    def test_all_four_vm_outcomes_required(self):
        self.reject_capture(lambda r: r['runs'].pop(), 'all four independent outcomes')

    def test_saved_world_bytes_not_waived(self):
        self.reject_capture(lambda r: r['runs'][0]['world_after'].update({'level.dat': '0' * 64}),
                            'saved input continuity')

    def test_arbitrary_throwables_and_setup_npe_refused(self):
        for name in ('java.lang.AssertionError', 'java.lang.LinkageError',
                     'java.lang.NoClassDefFoundError', 'java.lang.ExceptionInInitializerError',
                     'java.lang.SecurityException', 'java.lang.NullPointerException'):
            with self.subTest(exception=name):
                self.reject_record(lambda rows, own, errors: rows[0].update(result='UNRETURNED', exception=name),
                                   'source-reachable outcomes')

    def test_whitelisted_class_at_wrong_profile_still_refused(self):
        self.reject_record(lambda rows, own, errors: self.row(rows, 'null-world').update(
            exception='java.lang.IllegalStateException'), 'source-reachable outcomes')
        self.reject_record(lambda rows, own, errors: self.row(rows, 'null-player-occupied').update(
            result='UNRETURNED', exception='java.lang.NullPointerException'), 'source-reachable outcomes')

    def test_true_source_and_helper_exception_boundaries(self):
        for profile, frames in (
                ('null-stack', [['iz', 'a', 78], ['RedstoneUseObservation', 'main', 251]]),
                ('null-player-supported', [['ie', 'a', 13], ['RedstoneUseObservation', 'main', 251]]),
                ('callback-refuse-first-cell', [['RedstoneUseObservation', 'main', 251]])):
            def mutate(rows, own, errors):
                next(e for e in errors if e['name'] == profile)['frames'] = frames
            with self.subTest(profile=profile):
                self.reject_record(mutate, 'source/helper boundary')
        self.reject_record(lambda rows, own, errors: errors.clear(), 'source/helper boundary', 'support')

    def test_consumption_and_late_callback_stat_suffix(self):
        for profile, change in (
                ('count-0', {'stack_after': [0, 0, 331, 0]}),
                ('count--2147483648', {'stack_after': [-2147483648, 0, 331, 0]}),
                ('callback-count-99', {'stack_after': [98, 0, 331, 0]}),
                ('callback-id-332', {'used331_delta': 1, 'used332_delta': 0}),
                ('null-player-supported', {'used331_delta': 1}),
                ('face-0', {'used331_delta': True})):
            with self.subTest(profile=profile, change=change):
                self.reject_record(lambda rows, own, errors: self.row(rows, profile).update(change),
                                   'consumption/live-ID/stat')

    def test_callback_prefix_and_no_premature_player_gate(self):
        self.reject_record(lambda rows, own, errors: self.row(rows, 'callback-refuse-first-cell').update(events=[]),
                           'callback/effect prefix')
        self.reject_record(lambda rows, own, errors: self.row(rows, 'null-player-supported').update(
            changes=[], stack_after=[2, 0, 331, 0]), 'consumption/live-ID/stat')

    def test_world_entropy_conditioning_without_cross_vm_seed_equality(self):
        def changed(rows, own, errors):
            own[1]['world_before']['seed'] = own[1]['world_after']['seed'] = '000000000000'
        self.reject_record(changed, 'stream continuation')
        self.reject_record(lambda rows, own, errors: own[0].update(world_random_same=False), 'own entropy')
        rows, own, errors = copy.deepcopy(self.records['use'])
        for state in own:
            state['world_before']['seed'] = state['world_after']['seed'] = '000000000000'
        # A synthetic alternate constructor seed remains legal within its own
        # stream; this does not claim another original process was executed.
        a.validate_records('use', rows, own, errors, self.rosters['use'])

    def test_queue_and_set_history_not_reset(self):
        self.reject_record(lambda rows, own, errors: rows[1].update(queues_before=[]), 'queue conditioning')
        self.reject_record(lambda rows, own, errors: rows[1]['set_before'].update(mod_count=0),
                           'retained modification history')
        self.reject_record(lambda rows, own, errors: rows[0]['set_after'].update(capacity=16, table_id=3),
                           'empty Set boundary topology')
        self.reject_record(lambda rows, own, errors: rows[0]['set_after'].update(set_id=9), 'same Set owners')

    def test_no_ignored_daemon_or_setup_stderr(self):
        for change in (lambda b: b + b'Exception in thread "Thread-0" java.lang.NullPointerException\n',
                       lambda b: b + b'WARNING: extra\n', lambda b: b[:-1],
                       lambda b: b.replace(b'RedstoneUseObservation', b'OtherHelper', 1),
                       lambda b: b.replace(b'\n', b'\r\n')):
            payload = copy.deepcopy(self.payload['use'])
            payload['stderr'] = change(payload['stderr'])
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, 'SecurityManager-only stderr'):
                a.validate_streams('use', payload, self.rosters['use'], self.capture_root)

    def test_closed_stdout_guards_and_independent_projections(self):
        for mutate, message in (
                (lambda p: p.update(stdout=p['stdout'].replace(b'151 recipes\r\n',
                    b'151 recipes\r\nignored daemon output\r\n', 1)), 'ordered original row'),
                (lambda p: p.update(stdout=p['stdout'].replace(b'network=2', b'network=0')), 'guards/completion'),
                (lambda p: p.update(selected=p['selected'].replace(b'"result":"true"', b'"result":"false"', 1)),
                 'raw/projection join'),
                (lambda p: p.update(exceptions=b'\n'), 'raw/projection join')):
            payload = copy.deepcopy(self.payload['use']); mutate(payload)
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                a.validate_streams('use', payload, self.rosters['use'], self.capture_root)

    def test_capture_metadata_cannot_mask_failed_or_incomplete_process(self):
        receipt = a.load(CAPTURE/'use-ordinary-0/original.capture.json')
        for change in ({'exit': 1}, {'exit': False}, {'timed_out': True}, {'pipe_capture_complete': False},
                       {'reader_errors': {'stderr': 'read failed'}}, {'output_overflow': True},
                       {'stderr_bytes': 0}, {'stdout_sha256': '0' * 64}):
            altered = copy.deepcopy(receipt); altered.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                a.validate_receipt(altered, self.payload['use'])


if __name__ == '__main__':
    unittest.main()
