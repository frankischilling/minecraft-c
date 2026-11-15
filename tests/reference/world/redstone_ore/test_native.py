"""Reject corrupted Redstone Ore effects, source prefixes, traversal and cleanup transport."""
import copy
import hashlib
import json
import unittest

import run_native as N

W, P = N.W, N.P


def runs(data):
    result = []
    start = 0
    while start < len(data):
        end = start + 1
        while end < len(data) and data[end] == data[start]: end += 1
        if data[start]: result.append([start, end - start, data[start]])
        start = end
    return result


def raw_state(profile, source, phase, after):
    value = N.selected_state(source)
    value['phase'] = phase
    arrays = W.plane_arrays(profile, after)
    P.require(type(arrays) is list and len(arrays) == len(value['chunk_planes']), 'independent complete Redstone Ore plane model')
    for target, chunk in zip(value['chunk_planes'], arrays):
        P.require(P.exact(target['xyz'], chunk['xyz']) and set(chunk['planes']) == set(N.N.PLANES), 'independent Chunk coordinates')
        for name, data in chunk['planes'].items():
            P.require(len(data) == N.N.PLANES[name] and hashlib.sha256(data).hexdigest() == target['planes'][name],
                      'independently derived Redstone Ore plane matches actual original')
        target['planes'] = {name: runs(data) for name, data in chunk['planes'].items()}
    return value


def fixture(mode, originals):
    profile = N.profile_for(mode)
    original = originals[mode]
    exception = N.exception_class(original)
    normal = exception is None
    source_sites = dict(step=None, mutation=None)
    site = dict.fromkeys(('class', 'method')) | {'pc': 0}
    if not normal:
        wanted = 'os' if profile['operation'] == 'mining' else 'ob' if profile['operation'] == 'right-use' else \
                 'fd' if profile['operation'] == 'world-tick' else 'bs'
        frame = next(entry for entry in original['exception_stack'] if entry['class'] == wanted)
        site.update({'class': frame['class'], 'method': frame['method']})
    if exception == 'java/lang/NullPointerException':
        origin = W.model(profile, original['prepared_class'])['exception_origin']
        step = dict(source=0, registry_status=0, caller_pc=6, activation_pc=6, particle_pc=0,
                    opaque_pc=0, face=-1, query_null=False, query_pc=0, provider_pc=0)
        mutation = dict(ore_tick_pc=0, source_null=False, query_null=False, query_pc=0,
                        provider_pc=0, world_access_pc=0, access_fault=0)
        if origin['class'] == 'bs' and origin['method'] == 'i':
            step.update(source=1 if origin['pc'] == 1 else 2, particle_pc=origin['pc'])
            source_sites['step'] = step
        elif origin['class'] == 'bs' and origin['method'] == 'h':
            step.update(source=6, activation_pc=origin['pc'])
            source_sites['step'] = step
        elif origin['class'] == 'bs' and origin['method'] == 'a':
            mutation.update(ore_tick_pc=origin['pc'], source_null=True)
            source_sites['mutation'] = mutation
        elif origin['class'] == 'fd' and origin['method'] == 'c':
            step.update(source=4, query_null=True, provider_pc=6)
            source_sites['step'] = step
        else:
            mutation.update(world_access_pc=9, access_fault=1)
            if origin['method'] == 'a':
                step.update(source=5, particle_pc=291)
                source_sites.update(step=step, mutation=mutation)
            else:
                mutation.update(ore_tick_pc=24)
                source_sites['mutation'] = mutation
    work = None
    if profile['operation'] == 'world-tick':
        chunks = 361 if profile['random_player'] else 0
        work = dict(pc=198 if normal else 190, scheduled_returned=normal, random_returned=normal,
                    chunks=chunks if normal else 0, samples=chunks * 80 if normal else 0,
                    block_ticks=W.model(profile, original['prepared_class'])['block_ticks'] if chunks and normal else 0)
    continuation = None
    if profile['operation'] == 'mining':
        continuation = dict(kind='mining', pc=113 if normal else 74, block_id=original['before']['target'][0],
                            metadata=0, initial_hook_returned=normal, strength='00000000',
                            progress_stored=False, removal_returned=False)
    elif profile['operation'] == 'right-use':
        activated_returned = normal or bool(profile['sound_error_at'])
        continuation = dict(kind='right-use', pc=59 if normal else 56 if activated_returned else 31,
                            clicked_id=original['before']['target'][0], activation_entered=True,
                            activation_returned=activated_returned, activated=False, item_entered=activated_returned,
                            item_returned=normal, count_before=1 if activated_returned else 0, count_after=0)
    outcome = dict(mode=mode, input=copy.deepcopy(profile), prepared_class=copy.deepcopy(original['prepared_class']),
                   raw=0 if normal else 1, returned=normal, result=original['result'], exception=exception,
                   native_policy=False, site=site, source_sites=source_sites, world_tick=work, continuation=continuation)
    records = [('ORE_STATE ', raw_state(profile, original['before'], 'before', False))]
    for event in original['events']:
        records.append(('ORE_EVENT ', {key: copy.deepcopy(value) for key, value in event.items()
                                        if key not in ('stack', 'source_stack')}))
    records.append(('ORE_OUTCOME ', outcome))
    for phase in ('after', 'retired', 'cleanup'):
        records.append(('ORE_STATE ', raw_state(profile, original['after'], phase, True)))
        if phase == 'retired' and N.receipt_expected(profile):
            records.append(('ORE_RECEIPT ', dict(busy_before=9, before_holds=2, after_busy_holds=2, after_call_holds=1,
                retire=0, same_identity=True, selected_source=True, exception=exception, attempts=2)))
    return records


def text(records, mode):
    return '\n'.join(prefix + json.dumps(value, separators=(',', ':')) for prefix, value in records) + \
           '\nPASS: 101 World Redstone Ore checks;mode=' + mode + '\n'


def states(records, after=False):
    values = [value for prefix, value in records if prefix == 'ORE_STATE ']
    return values[1:] if after else values


def events(records):
    return [value for prefix, value in records if prefix == 'ORE_EVENT ']


def outcome(records):
    return next(value for prefix, value in records if prefix == 'ORE_OUTCOME ')


class NativeReader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.originals = {row['mode']: row for row in W.read_reference()['rows']}

    def check(self, records, mode):
        return N.validate(N.parse(text(records, mode), mode), mode, self.originals)

    def rejected(self, mode, mutation):
        records = fixture(mode, self.originals)
        mutation(records)
        with self.assertRaises(ValueError): self.check(records, mode)

    def test_all_declared_original_pairs(self):
        self.assertEqual(set(N.MODES), set(self.originals))
        for mode in N.MODES:
            with self.subTest(mode=mode):
                self.assertTrue(self.check(fixture(mode, self.originals), mode)['matched'])
                arguments = N.native_arguments('native.exe', N.profile_for(mode), N.HERE / 'prepared-world',
                                               self.originals[mode]['prepared_class'])
                self.assertEqual(len(arguments), 33)
                self.assertEqual(arguments[-1], 'none')

    def test_transport_completion_duplicate_json_and_exit(self):
        mode = N.MODES[0]
        value = text(fixture(mode, self.originals), mode)
        corruptions = (value.rstrip('\n'), value + 'noise\n', value.replace('PASS: 101', 'PASS: 0'),
                       value.replace('ORE_OUTCOME {', 'ORE_OUTCOME {"mode":"duplicate",', 1),
                       value.replace('ORE_STATE ', 'ORE_UNKNOWN ', 1), value + value)
        for index, bad in enumerate(corruptions):
            with self.subTest(index=index), self.assertRaises(ValueError): N.parse(bad, mode)
        capture = N.module('ore_reader_capture_control', N.ROOT / 'tests/reference/client/controller/capture.py')
        valid = dict(reader_errors={}, exit=0, timed_out=False, output_overflow=False,
                     pipe_capture_complete=True, stderr_bytes=0)
        capture.require_success(valid, empty_stderr=True)
        for key, value in (('exit', 7), ('stderr_bytes', 1), ('timed_out', True),
                           ('output_overflow', True), ('pipe_capture_complete', False),
                           ('reader_errors', {'stdout': 'decode failed'})):
            with self.subTest(key=key), self.assertRaises(AssertionError):
                capture.require_success(dict(valid, **{key: value}), empty_stderr=True)

    def test_literal_inputs_prepared_classes_counter_and_category(self):
        mode = N.MODES[0]
        mutations = (
            lambda rows: outcome(rows).update(raw=False),
            lambda rows: outcome(rows).update(raw=-1),
            lambda rows: outcome(rows).update(returned=False),
            lambda rows: outcome(rows).update(native_policy=True),
            lambda rows: outcome(rows)['input']['xyz'].__setitem__(0, 14),
            lambda rows: outcome(rows)['input'].update(null_world=0),
            lambda rows: outcome(rows)['prepared_class']['scheduled'].update(sequence='ffffffffffffffff'),
            lambda rows: outcome(rows)['prepared_class']['entity'].update(next_id=99),
            lambda rows: outcome(rows)['prepared_class'].update(item_use_stat=99),
            lambda rows: outcome(rows).update(extra='unparsed'),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index): self.rejected(mode, mutation)

    def test_before_raw_planes_random_and_actual_source_flags(self):
        mode = N.MODES[0]
        def prepared_plane(rows):
            data = bytearray(W.plane_arrays(N.profile_for(mode), False)[0]['planes']['height'])
            data[0] ^= 99
            states(rows)[0]['chunk_planes'][0]['planes']['height'] = runs(data)
        mutations = (
            lambda rows: states(rows)[0]['world_random'].__setitem__(0, '0000000000000000'),
            lambda rows: states(rows)[0]['math_random'].__setitem__(1, 0),
            lambda rows: states(rows)[0]['ore_statics'].update(lit=73),
            lambda rows: states(rows)[0].update(provider_null=True),
            lambda rows: states(rows)[0].update(world_access_null=True),
            prepared_plane,
            lambda rows: states(rows)[0].update(item_use_stat=True),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index): self.rejected(mode, mutation)

    def test_callback_order_body_and_stopped_random_prefix(self):
        mode = 'ore-left-73'
        def reverse(rows):
            callbacks = [entry for entry in rows if entry[0] == 'ORE_EVENT ']
            indices = [i for i, entry in enumerate(rows) if entry[0] == 'ORE_EVENT ']
            for index, value in zip(indices, reversed(callbacks)): rows[index] = value
        mutations = (reverse,
            lambda rows: events(rows)[0]['position'].__setitem__(0, '0000000000000000'),
            lambda rows: events(rows)[0]['world_random'].__setitem__(0, '0000000000000000'),
            lambda rows: events(rows)[0].update(name='smoke'),
            lambda rows: rows.pop(next(i for i, entry in enumerate(rows) if entry[0] == 'ORE_EVENT ')),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index): self.rejected(mode, mutation)
        mode = 'ore-particle-error-3'
        def resume_after_throw(rows):
            for snapshot in states(rows, True):
                snapshot['world_random'] = copy.deepcopy(self.originals['ore-left-73']['after']['world_random'])
        self.rejected(mode, resume_after_throw)

    def test_complete_planes_lighting_and_stable_cleanup(self):
        mode = 'ore-height-prepared'
        for plane in N.N.PLANES:
            def change(rows, plane=plane):
                data = bytearray(W.plane_arrays(N.profile_for(mode), True)[0]['planes'][plane])
                data[-1] ^= 123
                for snapshot in states(rows, True):
                    snapshot['chunk_planes'][0]['planes'][plane] = runs(data)
            with self.subTest(plane=plane): self.rejected(mode, change)
        self.rejected(mode, lambda rows: states(rows)[-1]['target'].__setitem__(0, 73))
        def lighting(rows):
            for snapshot in states(rows, True):
                snapshot['lighting']['regions'].append(dict(kind='Sky', bounds=[0]*6))
        self.rejected(mode, lighting)
        self.rejected(mode, lambda rows: states(rows)[1]['chunk_planes'].pop())

    def test_exact_scheduling_random_lcg_and_complete_footprint(self):
        mode = 'ore-world-scheduled-74'
        self.rejected(mode, lambda rows: states(rows)[0]['scheduled'].update(sequence='0000000000000000'))
        self.rejected(mode, lambda rows: states(rows)[0]['scheduled']['ordered'][0].update(block=73))
        mode = 'ore-world-random-player'
        def traversal(rows):
            for snapshot in states(rows, True): snapshot['random_work']['candidates'].reverse()
        def lcg(rows):
            for snapshot in states(rows, True): snapshot['random_work'].update(g=0)
        self.rejected(mode, traversal)
        self.rejected(mode, lcg)
        self.rejected(mode, lambda rows: outcome(rows)['world_tick'].update(block_ticks=0))
        self.rejected(mode, lambda rows: states(rows)[1]['chunk_planes'].pop())
        self.rejected(mode, lambda rows: states(rows)[1]['players'][0]['position'].__setitem__(0, '0000000000000000'))

    def test_source_null_witness_and_reached_precedence(self):
        modes = ('ore-world-random-null', 'ore-random-provider-null', 'ore-provider-before-unlit-static-null',
                 'ore-unlit-static-null', 'ore-lit-static-null', 'ore-tick-unlit-static-null',
                 'ore-tick-lit-static-null', 'ore-tick-null-world-74', 'ore-access-null',
                 'ore-tick-access-null-74', 'ore-tick-access-null-cell-74')
        for mode in modes:
            self.assertIn(mode, N.MODES)
            def lose_witness(rows):
                outcome(rows)['source_sites'] = dict(step=None, mutation=None)
            with self.subTest(mode=mode): self.rejected(mode, lose_witness)
        mode = 'ore-tick-null-argument-73'
        self.rejected(mode, lambda rows: outcome(rows).update(returned=False, raw=1,
            exception='java/lang/NullPointerException', native_policy=False))
        mode = 'ore-particle-before-lit-static-null'
        self.rejected(mode, lambda rows: outcome(rows).update(exception='java/lang/NullPointerException'))

    def test_real_false_activation_item_and_captured_mining_block(self):
        mode = 'ore-right-73'
        self.rejected(mode, lambda rows: outcome(rows).update(result=True))
        mode = 'ore-right-use'
        mutations = (
            lambda rows: outcome(rows)['continuation'].update(activated=True),
            lambda rows: outcome(rows)['continuation'].update(item_entered=False),
            lambda rows: outcome(rows)['continuation'].update(count_after=1),
            lambda rows: states(rows)[1]['held_stack'].update(count=1),
            lambda rows: states(rows)[1].update(item_use_stat=states(rows)[0]['item_use_stat']),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index): self.rejected(mode, mutation)
        self.rejected('ore-mining', lambda rows: outcome(rows)['continuation'].update(block_id=74))
        self.rejected('ore-mining', lambda rows: outcome(rows)['continuation'].update(initial_hook_returned=False))
        self.rejected('ore-right-use-particle-error', lambda rows: outcome(rows)['continuation'].update(item_entered=True))
        self.rejected('ore-right-use-sound-error', lambda rows: states(rows)[1]['held_stack'].update(count=0))

    def test_actual_receipt_identity_busy_hold_and_single_retirement(self):
        retained = [mode for mode in N.MODES if N.receipt_expected(N.profile_for(mode))]
        self.assertTrue(any('mining' in mode for mode in retained))
        self.assertTrue(any('right-use' in mode for mode in retained))
        self.assertTrue(any('world-scheduled' in mode for mode in retained))
        for mode in retained:
            def missing(rows): rows[:] = [entry for entry in rows if entry[0] != 'ORE_RECEIPT ']
            with self.subTest(mode=mode): self.rejected(mode, missing)
        mode = retained[0]
        for key, value in (('busy_before', 0), ('before_holds', 1), ('after_busy_holds', 1),
                           ('after_call_holds', 2), ('same_identity', False), ('selected_source', False),
                           ('exception', 'java/lang/NullPointerException'), ('retire', 9), ('attempts', 1)):
            def change(rows, key=key, value=value):
                next(item for prefix, item in rows if prefix == 'ORE_RECEIPT ')[key] = value
            with self.subTest(key=key): self.rejected(mode, change)


if __name__ == '__main__': unittest.main()
