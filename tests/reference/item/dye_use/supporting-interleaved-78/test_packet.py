"""Original packet integrity controls; no JAR, JVM, native executable or assets."""
import copy
import json
from pathlib import Path
import unittest

import packet

HERE = Path(__file__).resolve().parent


class DyePacket(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = [json.loads(line.split('\t', 1)[1])
                    for line in (HERE / 'selected.jsonl').read_text().splitlines()
                    if line.startswith('DYE\t')]
        cls.declared = packet.load(HERE / 'declared-inputs.json')['rows']
        cls.manifest = packet.load(HERE / 'manifest.json')

    def corrupt(self, name, mutation):
        index = next(i for i, row in enumerate(self.rows) if row['name'] == name)
        row = copy.deepcopy(self.rows[index])
        mutation(row)
        with self.assertRaises((ValueError, TypeError, KeyError, IndexError)):
            packet.validate_row(row, self.declared[index], index, self.manifest['row_sha256'][index])

    def test_complete_frozen_cohort_and_two_raw_repeats(self):
        self.assertEqual(packet.audit()['profiles'], 78)

    def test_explicit_no_native_equivalence(self):
        report = packet.audit()
        self.assertEqual(report['native_comparisons'], 0)
        self.assertEqual(report['read_only_jdi_runs'], 0)
        self.assertEqual(report['expected_exceptions'], 14)

    def test_all_six_declared_callback_failures_retained(self):
        self.assertEqual(sum(r['exception'] == 'java.lang.IllegalStateException' for r in self.rows), 6)

    def test_all_eight_null_reference_outcomes_retained(self):
        self.assertEqual(sum(r['exception'] == 'java.lang.NullPointerException' for r in self.rows), 8)

    def test_unknown_observation_field(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r.update(invented_native_equivalence=True))

    def test_profile_identity(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r.update(index=999))

    def test_channel_substitution(self):
        self.corrupt('crop_stage0_direct', lambda r: r.update(channel='wrapper'))

    def test_false_result_on_success(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r.update(result='false'))

    def test_invented_exception_class(self):
        self.corrupt('crop_callback_fail_first_wrapper', lambda r: r.update(exception='NONE'))

    def test_truncated_stack(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r['stack_after'].pop())

    def test_signed_count_normalization(self):
        self.corrupt('crop_wrap_count_wrapper', lambda r: r['stack_after'].__setitem__(0, 0))

    def test_unsigned_count_transport(self):
        self.corrupt('crop_wrap_count_wrapper', lambda r: r['stack_after'].__setitem__(0, 1 << 31))

    def test_initial_stack_mutation(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r['stack_before'].__setitem__(1, 0))

    def test_rng_word_width(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['item_random_after'].update(seed_bits='0'))

    def test_rng_boolean_type(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['item_random_after'].update(have_gaussian=0))

    def test_world_item_stream_substitution(self):
        self.corrupt('grass_plane_wrapper', lambda r: r.update(item_random_after=r['world_random_after']))

    def test_lost_live_alias(self):
        self.corrupt('grass_callback_live_gm_b_to_world_wrapper', lambda r: r.update(gm_b_is_world_after=False))

    def test_invented_direct_statistic(self):
        self.corrupt('crop_stage0_direct', lambda r: r.update(used_delta=1))

    def test_missing_remote_wrapper_statistic(self):
        self.corrupt('remote_crop_wrapper', lambda r: r.update(used_delta=0))

    def test_invented_remote_field_change(self):
        self.corrupt('remote_crop_wrapper', lambda r: r.update(remote_after=False))

    def test_omitted_callback(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r.update(notifications=[]))

    def test_false_callback_count(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r.update(observer_cells=0))

    def test_alias_callback_order(self):
        def mutate(r):
            next(e for e in r['notifications'] if e['event'] == 'gm_b_alias_world')['after_cell'] = 2
        self.corrupt('grass_callback_live_gm_b_to_world_wrapper', mutate)

    def test_callback_count_store_value(self):
        def mutate(r):
            next(e for e in r['notifications'] if e['event'] == 'authored_count_store')['value'] = 98
        self.corrupt('crop_callback_count99_wrapper', mutate)

    def test_grass_early_decrement_not_repeated_after_callback(self):
        self.corrupt('grass_callback_count99_wrapper', lambda r: r['stack_after'].__setitem__(0, 98))

    def test_crop_late_decrement_uses_callback_store(self):
        self.corrupt('crop_callback_count99_wrapper', lambda r: r['stack_after'].__setitem__(0, 1))

    def test_original_byte_before_change(self):
        self.corrupt('crop_stage0_wrapper', lambda r: r['changes'][0].__setitem__(3, 255))

    def test_changed_array_role(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['changes'][0].__setitem__(1, 5))

    def test_changed_array_value(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['changes'][0].__setitem__(4, 255))

    def test_duplicate_byte_change(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['changes'].append(r['changes'][0]))

    def test_byte_change_order(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['changes'].reverse())

    def test_missing_full_map_cell(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['blocks_metadata_after'].pop())

    def test_wrong_block_map_value(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['blocks_metadata_after'][0].__setitem__(3, 255))

    def test_wrong_metadata_map_value(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['blocks_metadata_after'][0].__setitem__(4, 15))

    def test_map_order(self):
        self.corrupt('grass_plane_wrapper', lambda r: r['blocks_metadata_after'].reverse())


if __name__ == '__main__':
    unittest.main()
