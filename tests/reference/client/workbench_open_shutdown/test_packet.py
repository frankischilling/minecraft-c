"""Public Java-free receipt and corruption controls; no native run or assets needed."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('workbench_open_shutdown_packet', HERE / 'packet.py')
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = P.load(HERE / 'native-validation.json')

    def corrupt(self, mutation, check_files=False):
        document = copy.deepcopy(self.original)
        mutation(document)
        with self.assertRaises((ValueError, KeyError, TypeError)):
            P.validate(document, P.ROOT if check_files else None)

    def test_sealed_historical_public_receipt(self):
        result = P.audit_public()
        self.assertEqual(result['native_checks'], 7446)
        self.assertEqual(result['source_shutdown_or_pixel_comparisons'], 0)
        self.assertFalse(result['current_source_qualification'])

    def test_repaired_manifest_cannot_replace_sealed_capture(self):
        document = copy.deepcopy(self.original)
        name = next(iter(document['runtime_source_files']))
        document['runtime_source_files'][name]['sha256'] = '0' * 64
        document['runtime_source_digest'] = P.canonical(document['runtime_source_files'])
        document['selected_execution_inputs']['digest'] = P.canonical({
            **document['runtime_source_files'], **document['harness_source_files'], **{
                'build-windows/' + n: v for n, v in document['selected_execution_inputs']['binaries'].items()}})
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'receipt.json'
            path.write_text(json.dumps(document), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'sealed historical capture'):
                P.audit_public(path)

    def test_current_audit_rejects_none_root(self):
        with self.assertRaisesRegex(ValueError, 'requires a directory'):
            P.audit_current_sources(None)

    def test_current_audit_requires_complete_runtime_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'all current runtime sources bound'):
                P.audit_current_sources(directory)

    def test_receipt_without_private_inputs(self):
        self.assertEqual(P.validate(self.original)['profiles'], 4)

    def test_duplicate_json_key(self):
        with self.assertRaises(ValueError):
            json.loads('{"status":"PASS","status":"FAIL"}', object_pairs_hook=P.unique)

    def test_failed_status(self):
        self.corrupt(lambda d: d.update(status='FAIL'))

    def test_boolean_schema(self):
        self.corrupt(lambda d: d.update(schema=True))

    def test_extra_document_field(self):
        self.corrupt(lambda d: d.update(process_address='000001ED52842C48'))

    def test_absolute_private_path(self):
        self.corrupt(lambda d: d.update(scope='C:\\private\\capture'))

    def test_runtime_hash(self):
        self.corrupt(lambda d: d['runtime_source_files'][next(iter(d['runtime_source_files']))].update(sha256='0' * 64))

    def test_missing_runtime_source(self):
        self.corrupt(lambda d: d['runtime_source_files'].pop(next(iter(d['runtime_source_files']))))

    def test_runtime_parent_traversal(self):
        def mutation(d):
            value = d['runtime_source_files'].pop(next(iter(d['runtime_source_files'])))
            d['runtime_source_files']['src/../outside.c'] = value
        self.corrupt(mutation)

    def test_runtime_current_bytes_even_with_repaired_manifest(self):
        def mutation(d):
            name = next(iter(d['runtime_source_files']))
            d['runtime_source_files'][name]['sha256'] = '0' * 64
            d['runtime_source_digest'] = P.canonical(d['runtime_source_files'])
            d['selected_execution_inputs']['digest'] = P.canonical({
                **d['runtime_source_files'], **d['harness_source_files'], **{
                    'build-windows/' + n: v for n, v in d['selected_execution_inputs']['binaries'].items()}})
        self.corrupt(mutation, check_files=True)

    def test_harness_hash(self):
        self.corrupt(lambda d: d['harness_source_files'][next(iter(d['harness_source_files']))].update(sha256='0' * 64))

    def test_binary_substitution(self):
        self.corrupt(lambda d: d['selected_execution_inputs']['binaries']['OpenAL32.dll'].update(sha256='0' * 64))

    def test_incomplete_selected_inputs(self):
        self.corrupt(lambda d: d['selected_execution_inputs'].update(
            count=d['selected_execution_inputs']['count'] - 1))

    def test_resource_count(self):
        self.corrupt(lambda d: d['imported_resources'].update(count=412))

    def test_resource_digest(self):
        self.corrupt(lambda d: d['imported_resources'].update(digest='not-a-hash'))

    def test_unmeasured_cohort_time(self):
        self.corrupt(lambda d: d['cohort'].update(total_seconds=65))

    def test_authored_inventory(self):
        self.corrupt(lambda d: d['authored_scene']['inventory'][1].__setitem__(2, 3))

    def test_source_field_repair(self):
        self.corrupt(lambda d: d['authored_scene'].update(source_inventory_repairs=True))

    def test_missing_profile(self):
        self.corrupt(lambda d: d['profiles'].pop())

    def test_duplicate_profile(self):
        self.corrupt(lambda d: d['profiles'].__setitem__(3, copy.deepcopy(d['profiles'][2])))

    def test_profile_order(self):
        self.corrupt(lambda d: d['profiles'].reverse())

    def test_profile_check_count(self):
        self.corrupt(lambda d: d['profiles'][0].update(checks=d['profiles'][0]['checks'] + 1))

    def test_profile_measured_time(self):
        self.corrupt(lambda d: d['profiles'][0].update(
            ctest_seconds=d['profiles'][0]['ctest_seconds'] + 1))

    def test_leaked_prepare_address(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['prepare'][0].update(r='000001ED52842C48'))

    def test_new_world_tick(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['prepare'][0].update(**{'world-ticks': '1'}))

    def test_changed_entity_snapshot(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['prepare'][0].update(
            entities=str(int(d['profiles'][0]['receipts']['prepare'][0]['entities']) + 1)))

    def test_missing_idempotence(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['transfer'][0].update(idempotent='0'))

    def test_missing_source_denial(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['denials'][0].update(router='returned'))

    def test_wrong_busy_phase(self):
        self.corrupt(lambda d: d['profiles'][1]['receipts']['busy'][0].update(phase='world-audit'))

    def test_unrelated_hold_release(self):
        self.corrupt(lambda d: d['profiles'][2]['receipts']['release'][0].update(released='world'))

    def test_source_close_claim(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['terminal'][0].update(**{'source-close': '1'}))

    def test_source_save_claim(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['finish'][0].update(**{'source-save': '1'}))

    def test_missing_window_cleanup(self):
        self.corrupt(lambda d: d['profiles'][0]['receipts']['finish'][0].update(**{'windows-absent': '0'}))

    def test_changed_receipt_rehashed(self):
        def mutation(d):
            d['profiles'][0]['receipts']['transfer'][0]['same-it'] = '0'
            d['profiles'][0]['receipts_digest'] = P.canonical(d['profiles'][0]['receipts'])
        self.corrupt(mutation)

    def test_changed_input_flag(self):
        self.corrupt(lambda d: d['profiles'][0].update(selected_inputs_unchanged=False))

    def test_asset_change_flag(self):
        self.corrupt(lambda d: d['profiles'][0].update(resources_unchanged=False))

    def test_missing_actual_image(self):
        self.corrupt(lambda d: d['profiles'][0].update(live_source_window_capture=False))

    def test_invalid_private_artifact_hash(self):
        self.corrupt(lambda d: d['profiles'][0]['private_artifacts']['run.json'].update(sha256='invalid'))

    def test_missing_raw_report_hash(self):
        self.corrupt(lambda d: d['profiles'][0]['private_artifacts'].pop('run.json'))

    def test_changed_save_manifest(self):
        self.corrupt(lambda d: d['profiles'][0]['stopped_save_files'].update({'alpha/level.dat': '0' * 64}))

    def test_removed_unrelated_region(self):
        self.corrupt(lambda d: d['profiles'][0]['stopped_save_files'].pop('alpha/region/r.9.9.mcr'))

    def test_missing_busy_disk_snapshot(self):
        self.corrupt(lambda d: d['profiles'][1]['unchanged_save_phases'].remove('open-container-busy'))

    def test_harness_failure_called_production_red(self):
        self.corrupt(lambda d: d['failed_cohorts_preserved'][0].update(production_red=True))

    def test_erased_genuine_red(self):
        self.corrupt(lambda d: d['failed_cohorts_preserved'].pop())
        self.corrupt(lambda d: d['intervening_focused_cohort'].update(passed=7))

    def test_claimed_setup_source_snapshot(self):
        self.corrupt(lambda d: d['failed_cohorts_preserved'][0].update(source_unchanged=True))

    def test_missing_failure_artifacts(self):
        self.corrupt(lambda d: d['failed_cohorts_preserved'][2]['private_artifacts'].pop('stderr.txt'))

    def test_erased_preceding_sdk_failure(self):
        self.corrupt(lambda d: d.pop('preceding_sdk_cohort'))

    def test_preceding_sdk_called_all_pass(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort'].update(status='PASS', passed=4))

    def test_preceding_sdk_measured_total(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort'].update(total_seconds=100.70))

    def test_preceding_sdk_case_status(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort']['cases'][0].update(status='PASS'))

    def test_preceding_setup_claimed_terminal(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort']['failed_sdk_profiles'][0].update(
            terminal_prepare_reached=True))

    def test_preceding_failed_input_continuity(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort']['failed_sdk_profiles'][0].update(
            selected_inputs_unchanged=False))

    def test_preceding_failed_resource_continuity(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort']['failed_sdk_profiles'][0].update(
            resources_unchanged=False))

    def test_preceding_failed_execution_artifact(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort']['failed_sdk_profiles'][0][
            'private_artifacts'].pop('execution.json'))

    def test_preceding_unverified_cause(self):
        self.corrupt(lambda d: d['preceding_sdk_cohort']['failed_sdk_profiles'][0].update(
            observation='Every input and focus path is verified.'))

    def test_missing_limit(self):
        self.corrupt(lambda d: d['limits'].pop())


    def corrupt_join(self, mutation):
        def apply(document):
            record = document['profiles'][0]
            mutation(record['receipts'])
            record['receipts_digest'] = P.canonical(record['receipts'])
        self.corrupt(apply)

    def test_join_missing_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r.pop('join'))

    def test_join_duplicate_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'].append(copy.deepcopy(r['join'][0])))

    def test_join_extra_field_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(unproved='1'))

    def test_join_zero_attempt_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(attempts='0', busy='0'))

    def test_join_over_limit_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(attempts='35001', busy='35000'))

    def test_join_busy_mismatch_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(busy=r['join'][0]['attempts']))

    def test_join_non_ok_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(status='1'))

    def test_join_unconsumed_owner_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(**{'slot-null': '0'}))

    def test_join_wrong_profile_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(profile='container'))

    def test_join_changed_limit_even_with_repaired_digest(self):
        self.corrupt_join(lambda r: r['join'][0].update(limit='35001'))

    def test_join_noncanonical_integer_or_type_even_with_repaired_digest(self):
        for key, value in [('attempts', True), ('attempts', 1), ('attempts', None),
                           ('attempts', '01'), ('attempts', '+1'), ('attempts', ' 1'),
                           ('attempts', '1.0'), ('attempts', '1e0'), ('busy', '-0'),
                           ('busy', False), ('limit', 35000), ('status', 0), ('slot-null', True)]:
            with self.subTest(key=key, value=value):
                self.corrupt_join(lambda r: r['join'][0].update({key: value}))

    def test_join_consistent_recount_cannot_rebind_capture(self):
        def mutation(receipts):
            row = receipts['join'][0]
            attempts = 2 if row['attempts'] == '1' else 1
            row.update(attempts=str(attempts), busy=str(attempts - 1))
        self.corrupt_join(mutation)

    def test_join_bounds_are_contract_controls_only(self):
        for attempts in (1, 35000):
            row = {'profile': 'clean', 'attempts': str(attempts), 'busy': str(attempts - 1),
                   'limit': '35000', 'status': '0', 'slot-null': '1'}
            self.assertEqual(P.validate_join([row], 'clean'), [row])

    def test_join_requires_schema_two(self):
        self.corrupt(lambda d: d.update(schema=1))

if __name__ == '__main__':
    unittest.main()
