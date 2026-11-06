"""Pure decoder controls; synthetic objects are never native observations."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('db_current_controller', HERE / 'redstone_torch_current.py')
c = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(c)


def error():
    value = {k: 0 for k in c.ERROR_KEYS}
    for k in ('operation', 'hook_class', 'hook_method', 'cause_hook_class', 'cause_hook_method'): value[k] = None
    for k in ('db_source_null', 'vm_source_null', 'mod_count_stored'): value[k] = False
    value.update(db_sites=[0] * 6, cause={'status': 0, 'operation': None, 'system_error': 0},
                 registry={'status': 0, 'message': None, 'initializer_pc': -1, 'block_id': -1,
                           'constructor_class': None, 'constructor_pc': -1, 'profile_status': 0,
                           'active_initializer_class': None, 'active_initializer_pc': -1, 'item_argument': -1,
                           'format': {'status': 0, 'offset': 0, 'length': 0, 'value': 0, 'flags': 0, 'conversion': 0, 'date_time': False}},
                 schedule={'status': 0, 'operation': None, 'pc': 0, 'presence_pc': 0, 'provider_pc': 0,
                           'record': 0, 'membership': 0, 'ordered': 0})
    return value


def snapshot():
    return {'arrays': [[0, i // 5, i % 5, 'p:' + format(100 + i, 'x'), c.LENGTHS[i % 5], '0' * 64] for i in range(20)],
            'worlds': [{'role': 0, 'identity': 'p:1', 'time': 0, 'a': False, 'B': False, 'o': False, 's': False, 'M': 0,
                        'light': {'identity': 'p:2', 'capacity': 0, 'records': []},
                        'ordered': {'identity': 'p:3', 'records': []}, 'membership': {'identity': 'p:4', 'records': []},
                        'entities': {'identity': 'p:5', 'capacity': 0, 'records': []}}],
            'history': {'owner': 'p:6', 'identity': 1, 'backing': 'hb:1', 'backing_kind': 1,
                        'allocated_records': 0, 'size': 0, 'capacity': 0, 'modCount': 0, 'unused_slots_null': True, 'records': []},
            'position_set': {'owner': 'p:7', 'map': 'p:8', 'table': None, 'size': 0, 'capacity': 0,
                             'threshold': 0, 'modCount': 0, 'constructed_keys': 0, 'load_factor': '3f400000',
                             'output': True, 'nodes': []}, 'sequence': {'initialized': False, 'next': 0}}


def source_snapshot(current):
    """Hand-authored decoder counterpart, not reconstructed original output."""
    world = current['worlds'][0]
    return {'arrays_before': [[*v[:3], i + 1000, *v[4:]] for i, v in enumerate(current['arrays'])],
            'worlds_before': [{'role': 0, 'alias': 1, 'time': 0, 'a': False, 'B': False, 'o': False, 's': False, 'M': 0,
                               'light': {'alias': 2, 'size': 0, 'slots': []},
                               'scheduled_tree': {'alias': 3, 'size': 0, 'nodes': []},
                               'scheduled_hash': {'alias': 4, 'size': 0, 'nodes': []},
                               'entities': {'alias': 5, 'size': 0, 'slots': []}}],
            'history_before': {'alias': 6, 'backing': 7, 'size': 0, 'capacity': 0, 'modCount': 0, 'slots': []},
            'position_set_before': {'alias': 8, 'size': 0, 'modCount': 0, 'nodes': []},
            'qy_before': {'initialized': False, 'counter': None}}


def records():
    profiles = [{'name': 'decoder-' + str(i), 'values': [0, 0, 76, 8, 65, 8, 5, 76, 1, 0, 0, 1, 0, 1, 0]}
                for i in range(70)]
    rng = {'seed': '0005deece672', 'gaussian': '0' * 16, 'have': False}
    owners = {'worlds': [{'role': 0, 'alias': 'p:9', 'state': rng}], 'independent': {'alias': 'p:a', 'state': rng}}
    rows, own = [], []
    for i, p in enumerate(profiles):
        rows.append({'cohort': 'lifecycle', 'index': i, 'name': p['name'], 'values': p['values'],
                     'native': {'invoke_ok': True, 'entered': True, 'returned': True, 'value': False,
                                'observer_refused': False, 'source_null': False, 'native_failure': False, 'error': error()},
                     'center_before': [76, 5], 'center_after': [76, 5], 'before': snapshot(), 'after': snapshot(), 'changes': [], 'events': []})
        own.append({'cohort': 'lifecycle', 'index': i, 'name': p['name'], 'mode': 1, 'owners_before': owners,
                    'owners_after': owners, 'supplied_alias': 'p:a', 'math_before': None, 'math_after': None,
                    'seed_uniquifier_before': '0' * 16, 'seed_uniquifier_after': '0' * 16})
    constructor = {'role': 0, 'time': 0, 'loaded': True, 'identity': 'p:1', 'uniquifier_before': '0' * 16,
                   'uniquifier_after': '0' * 16, 'math_before': None, 'math_after': None}
    finish = {'cohort': 'lifecycle', 'rows': 70, 'retired': True, 'terminal_chunk_counts': [4]}
    return profiles, constructor, rows, own, finish


def encode(constructor, rows, own, finish):
    lines = ['DB_CURRENT_WORLD\t' + json.dumps(constructor)]
    for row, state in zip(rows, own):
        lines += ['DB_CURRENT\t' + json.dumps(row), 'DB_CURRENT_ENTROPY\t' + json.dumps(state)]
    lines += ['DB_CURRENT_FINISH\t' + json.dumps(finish)]
    return c.PRELUDE + ('\n'.join(lines) + '\n').encode('ascii')


class DecoderTests(unittest.TestCase):
    def test_fixed_inputs_and_six_copied_nbt_times(self):
        value = c.inputs()
        self.assertEqual({k: len(v) for k, v in value['cohorts'].items()}, c.COHORTS)
        for role in c.read(c.REFERENCE / 'world-inputs.json').values():
            self.assertEqual(c.nbt_time(c.REFERENCE / 'world-inputs' / role['name'] / 'level.dat'), role['authored_nbt_time'])

    def test_complete_synthetic_decoder_only_roster(self):
        p, constructor, rows, own, finish = records()
        value = c.parse_current(encode(constructor, rows, own, finish), 'lifecycle', p)
        self.assertEqual(len(value[1]), 70)

    def test_binary_stdout_transport_and_constructor_types(self):
        p, constructor, rows, own, finish = records()
        raw = encode(constructor, rows, own, finish)
        other_transport = b'16 achievements\n151 recipes\n' if b'\r' in c.PRELUDE else b'16 achievements\r\n151 recipes\r\n'
        self.assertTrue(raw.startswith(c.PRELUDE))
        self.assertNotIn(b'\r', raw[len(c.PRELUDE):])
        for changed in (raw.replace(c.PRELUDE, other_transport, 1),
                        raw.replace(c.PRELUDE, b'151 recipes\n16 achievements\n', 1)):
            with self.assertRaises(ValueError): c.parse_current(changed, 'lifecycle', p)
        for mutate in (lambda v: v.update(uniquifier_after=True),
                       lambda v: v.update(math_after={'seed': '0' * 12, 'gaussian': '0' * 16, 'have': 0})):
            item = copy.deepcopy(constructor); mutate(item)
            with self.assertRaises(ValueError): c.parse_current(encode(item, rows, own, finish), 'lifecycle', p)

    def test_unknown_duplicate_nonfinite_and_incomplete_records(self):
        p, constructor, rows, own, finish = records(); raw = encode(constructor, rows, own, finish)
        changes = [raw + b'extra\n', raw[len(c.PRELUDE):], raw.replace(b'"index": 0', b'"index": 0, "index": 0', 1),
                   raw.replace(b'"M": 0', b'"M": NaN', 1), raw.replace(b'DB_CURRENT_ENTROPY', b'OTHER', 1),
                   encode(constructor, rows[:-1], own[:-1], finish)]
        for changed in changes:
            with self.subTest(changed=changed[:60]), self.assertRaises((ValueError, KeyError)):
                c.parse_current(changed, 'lifecycle', p)

    def test_nested_bool_integer_and_raw_rng_corruptions(self):
        for mutate in (lambda rows, own: rows[0]['before']['worlds'][0].update(M=True),
                       lambda rows, own: rows[0]['native'].update(returned=1),
                       lambda rows, own: rows[0]['native']['error']['registry']['format'].update(date_time=0),
                       lambda rows, own: own[0]['owners_before']['independent']['state'].update(have=1),
                       lambda rows, own: rows[0]['before']['arrays'][0].__setitem__(4, True)):
            p, constructor, rows, own, finish = records(); mutate(rows, own)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                c.parse_current(encode(constructor, rows, own, finish), 'lifecycle', p)

    def test_literal_M_hash_and_history_values_not_masked(self):
        for mutate in (lambda snap: snap['worlds'][0].update(M=1),
                       lambda snap: snap['arrays'][0].__setitem__(5, '1' * 64),
                       lambda snap: snap['history'].update(modCount=1)):
            native = snapshot(); source = source_snapshot(native); mutate(native)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                c.compare_snapshot(native, source, 'before', c.Relations())

    def test_owner_alias_history_is_bijective_and_typed(self):
        relations = c.Relations(); relations.bind('XU', 1, 'xu:1'); relations.bind('XU', 1, 'xu:1')
        for source, native in ((1, 'xu:2'), (2, 'xu:1'), (False, None)):
            with self.subTest(source=source, native=native), self.assertRaises(ValueError):
                relations.bind('XU', source, native)

    def test_scheduler_deadline_sequence_and_E_F_same_record_join(self):
        native = snapshot(); source = source_snapshot(native)
        record = {'identity': 'p:b', 'xyz': [8, 65, 8], 'block': 76, 'time': 2, 'order': 0}
        original = {'alias': 500, 'class': 'qy', 'xyz': [8, 65, 8], 'block': 76, 'time': 2, 'order': 0}
        for key in ('ordered', 'membership'): native['worlds'][0][key]['records'] = [copy.deepcopy(record)]
        for key in ('scheduled_tree', 'scheduled_hash'):
            source['worlds_before'][0][key].update(size=1, nodes=[{'key': copy.deepcopy(original)}])
        native['sequence'] = {'initialized': True, 'next': 1}
        source['qy_before'] = {'initialized': True, 'counter': 1}
        c.compare_snapshot(native, source, 'before', c.Relations())
        for mutate in (lambda n: n['worlds'][0]['membership']['records'][0].update(identity='p:c'),
                       lambda n: n['worlds'][0]['ordered']['records'][0].update(time=3),
                       lambda n: n['worlds'][0]['ordered']['records'][0].update(order=1)):
            changed = copy.deepcopy(native); mutate(changed)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                c.compare_snapshot(changed, source, 'before', c.Relations())

    def test_C_order_and_independent31_positions_are_literal(self):
        native = snapshot(); source = source_snapshot(native)
        native['worlds'][0]['light']['records'] = [{'identity': 'p:b', 'region': [0, 8, 65, 8, 8, 65, 8]}]
        native['worlds'][0]['light']['capacity'] = 10
        source['worlds_before'][0]['light'].update(size=1, slots=[{'alias': 500, 'class': 'st', 'region': [0, 8, 65, 8, 8, 65, 8]}])
        c.compare_snapshot(native, source, 'before', c.Relations())
        native['worlds'][0]['light']['records'][0]['region'][0] = 1
        with self.assertRaises(ValueError): c.compare_snapshot(native, source, 'before', c.Relations())
        row = {'values': [0] * 15, 'events': [{'kind': 'particle', 'world': 0, 'name': 'smoke', 'position': ['1' * 16] * 3,
               'motion': ['0' * 16] * 3, 'states': {}}]}; row['values'][11] = 1
        logical, raw, drops = c.project_events(row)
        self.assertEqual(logical[0]['position'], row['events'][0]['position'])
        row['values'][11] = 0
        self.assertNotIn('position', c.project_events(row)[0][0])
        self.assertEqual(c.project_events(row)[1][0]['position'], row['events'][0]['position'])

    def test_native_refusal_is_distinct_from_source_ISE_and_unknown_policy_stays_red(self):
        native = {'invoke_ok': True, 'entered': True, 'returned': False, 'value': False,
                  'observer_refused': True, 'source_null': False, 'native_failure': False, 'error': error()}
        native['error']['status'] = 7
        original = {'exception': 'java.lang.IllegalStateException', 'result': 'UNRETURNED', 'values': [0] * 15}
        c.compare_outcome(native, original)
        for mutate in (lambda v: v.update(observer_refused=False), lambda v: v.update(native_failure=True),
                       lambda v: v['error'].update(status=5), lambda v: v['error'].update(db_source_null=True)):
            changed = copy.deepcopy(native); mutate(changed)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError): c.compare_outcome(changed, original)
        native.update(observer_refused=False, source_null=True)
        native['error'].update(status=5, db_source_null=True)
        original['exception'] = 'java.lang.NullPointerException'
        c.compare_outcome(native, original)
        original['exception'] = 'java.lang.LinkageError'
        with self.assertRaises(ValueError): c.compare_outcome(native, original)


class AccountingControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('db_current_accounting_controls', HERE / 'redstone_torch_accounting.py')
        cls.a = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.a)

    def counter_fixture(self):
        """Synthetic counter-only unit operands; never a native observation."""
        a = self.a
        before = 0x123456789abcdef
        first, second = a.advance_counter(before, 2), a.advance_counter(before, 3)
        state = {'seed': '000000000001', 'gaussian': '0' * 16, 'have': False}
        rows = [{'index': 0, 'events': [{'kind': 'item', 'math_post': state, 'uniquifier': f'{first:016x}'}]},
                {'index': 1, 'events': []}]
        own = [{'seed_uniquifier_before': f'{before:016x}', 'seed_uniquifier_after': f'{first:016x}',
                'math_before': None, 'math_after': state},
               {'seed_uniquifier_before': f'{first:016x}', 'seed_uniquifier_after': f'{second:016x}',
                'math_before': state, 'math_after': state}]
        return rows, own, {1: {}}

    def test_exact_published_inferred_and_cold_holder_counts(self):
        result = self.a.exact_counts(*self.counter_fixture())
        self.assertEqual({k: result[k] for k in ('published_HL', 'inferred_unpublished_HL',
                                               'first_Math_holder', 'default_constructor_steps')},
                         {'published_HL': 1, 'inferred_unpublished_HL': 1,
                          'first_Math_holder': 1, 'default_constructor_steps': 3})

    def test_missing_extra_and_repaired_local_counts_rejected(self):
        changes = [lambda r, o: o[1].update(seed_uniquifier_after=o[1]['seed_uniquifier_before']),
                   lambda r, o: o[1].update(seed_uniquifier_after=f"{self.a.advance_counter(int(o[1]['seed_uniquifier_before'], 16), 2):016x}"),
                   lambda r, o: o[0].update(math_before=o[0]['math_after'])]
        for mutate in changes:
            rows, own, inferred = self.counter_fixture()
            mutate(rows, own)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                self.a.exact_counts(rows, own, inferred)

    def test_cross_row_and_actual_callback_counter_joins_rejected(self):
        def shifted(r, o):
            o[1]['seed_uniquifier_before'] = f"{self.a.advance_counter(int(o[1]['seed_uniquifier_before'], 16), 1):016x}"
            o[1]['seed_uniquifier_after'] = f"{self.a.advance_counter(int(o[1]['seed_uniquifier_after'], 16), 1):016x}"
        for mutate in (shifted, lambda r, o: r[0]['events'][0].update(uniquifier='0' * 16),
                       lambda r, o: o[0].update(seed_uniquifier_before=True),
                       lambda r, o: o[0]['math_after'].update(have=True)):
            rows, own, inferred = self.counter_fixture()
            mutate(rows, own)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                self.a.exact_counts(rows, own, inferred)

    def test_no_other_cohort_or_row_gets_unpublished_rule(self):
        for row in ({'cohort': 'powered', 'index': 58}, {'cohort': 'burnout', 'index': 59},
                    {'cohort': 'aging', 'index': 62}, {'cohort': 'lifecycle', 'index': 57}):
            self.assertIsNone(self.a.native_bounded(row, {}, {}))


if __name__ == '__main__':
    unittest.main()
