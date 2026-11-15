"""Reject corrupted Fire effects, source prefixes, traversal and cleanup transport."""
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
    P.require(type(arrays) is list and len(arrays) == len(value['chunk_planes']), 'independent complete Fire plane model')
    for target, chunk in zip(value['chunk_planes'], arrays):
        P.require(P.exact(target['xyz'], chunk['xyz']) and set(chunk['planes']) == set(N.N.PLANES), 'independent Chunk coordinates')
        for name, data in chunk['planes'].items():
            P.require(len(data) == N.N.PLANES[name] and hashlib.sha256(data).hexdigest() == target['planes'][name],
                      'independently derived Fire plane matches actual original')
        target['planes'] = {name: runs(data) for name, data in chunk['planes'].items()}
    return value


def fixture(mode, originals):
    profile = N.profile_for(mode)
    original = originals[mode]
    exception = N.exception_class(original)
    normal = exception is None
    source_sites = dict(null=exception == 'java/lang/NullPointerException', array_index=False,
                        query_null=False, query_pc=0, provider_pc=0,
                        fire=dict.fromkeys(('added', 'neighbor', 'tick', 'can_place', 'neighbors', 'flammable',
                                            'encouragement', 'spread', 'burn'), 0))
    hook = dict.fromkeys(('class', 'method')) | {'pc': 0}
    if not normal:
        frame = next(entry for entry in original['exception_stack'] if entry['class'] in ('yq', 'nx', 'ah', 'fd', 'ak'))
        hook.update({'class': frame['class'], 'method': frame['method']})
    if source_sites['null']:
        origin = W.model(profile, original['prepared_class'])['exception_origin']
        if origin['class'] == 'yq':
            source_sites['fire']['flammable' if origin['method'] == 'c' else 'burn'] = origin['pc']
        else:
            source_sites.update(query_null=True, provider_pc=origin['pc'])
    work = None
    if profile['operation'] == 'world-tick':
        chunks = 361 if profile['random_player'] else 0
        work = dict(pc=198 if normal else 190, scheduled_returned=normal, random_returned=normal,
                    chunks=chunks if normal else 0, samples=chunks * 80 if normal else 0,
                    block_ticks=1 if chunks and normal else 0)
    outcome = dict(mode=mode, input=copy.deepcopy(profile), prepared_class=copy.deepcopy(original['prepared_class']),
                   raw=0 if normal else 1, returned=normal, result=original['result'], exception=exception,
                   native_policy=False, hook=hook, cause_hook=dict.fromkeys(('class', 'method')) | {'pc': 0},
                   source_sites=source_sites, world_tick=work)
    records = [('FIRE_STATE ', raw_state(profile, original['before'], 'before', False))]
    for event in original['events']:
        records.append(('FIRE_EVENT ', {key: copy.deepcopy(value) for key, value in event.items()
                                        if key not in ('stack', 'source_stack')}))
    records.append(('FIRE_OUTCOME ', outcome))
    for phase in ('after', 'retired', 'cleanup'):
        records.append(('FIRE_STATE ', raw_state(profile, original['after'], phase, True)))
        if phase == 'retired' and N.receipt_expected(profile):
            records.append(('FIRE_RECEIPT ', dict(busy_before=9, before_holds=2, after_busy_holds=2, after_call_holds=1,
                retire=0, same_identity=True, selected_source=True, exception=exception, attempts=2)))
    return records


def text(records, mode):
    return '\n'.join(prefix + json.dumps(value, separators=(',', ':')) for prefix, value in records) + \
           '\nPASS: 101 World Fire checks;mode=' + mode + '\n'


def states(records, after=False):
    values = [value for prefix, value in records if prefix == 'FIRE_STATE ']
    return values[1:] if after else values


def events(records):
    return [value for prefix, value in records if prefix == 'FIRE_EVENT ']


def outcome(records):
    return next(value for prefix, value in records if prefix == 'FIRE_OUTCOME ')


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
            with self.subTest(mode=mode): self.assertTrue(self.check(fixture(mode, self.originals), mode)['matched'])

    def test_transport_closure_completion_and_duplicate_json(self):
        mode = N.MODES[0]
        value = text(fixture(mode, self.originals), mode)
        corruptions = (value.rstrip('\n'), value + 'noise\n', value.replace('PASS: 101', 'PASS: 0'),
                       value.replace('FIRE_OUTCOME {', 'FIRE_OUTCOME {"mode":"duplicate",', 1),
                       value.replace('FIRE_STATE ', 'FIRE_UNKNOWN ', 1), value + value)
        for index, bad in enumerate(corruptions):
            with self.subTest(index=index), self.assertRaises(ValueError): N.parse(bad, mode)

    def test_literal_inputs_status_and_class_preparation(self):
        mode = N.MODES[0]
        mutations = (
            lambda rows: outcome(rows).update(raw=False),
            lambda rows: outcome(rows).update(returned=False),
            lambda rows: outcome(rows).update(native_policy=True),
            lambda rows: outcome(rows)['input']['xyz'].__setitem__(0, 14),
            lambda rows: outcome(rows)['input'].update(rain=0),
            lambda rows: outcome(rows)['prepared_class']['scheduled'].update(sequence='ffffffffffffffff'),
            lambda rows: outcome(rows)['prepared_class']['entity'].update(next_id=99),
            lambda rows: outcome(rows).update(extra='unparsed'),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index): self.rejected(mode, mutation)

    def test_actual_before_state_planes_and_typed_fields(self):
        mode = N.MODES[0]
        mutations = (
            lambda rows: states(rows)[0]['world_random'].__setitem__(0, '0000000000000000'),
            lambda rows: states(rows)[0]['math_random'].__setitem__(1, 0),
            lambda rows: states(rows)[0]['fire_arrays']['encouragement'].__setitem__(5, 999),
            lambda rows: states(rows)[0]['chunk_planes'][0]['planes']['height'].append([0, 1, 99]),
            lambda rows: states(rows)[0]['chunk_planes'].reverse(),
            lambda rows: states(rows)[0].update(world_o=0),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index): self.rejected(mode, mutation)

    def test_callback_order_deletion_and_committed_prefix(self):
        mode = next(mode for mode in N.MODES if len(self.originals[mode]['events']) >= 3)
        mutations = (
            lambda rows: rows.pop(next(i for i, (prefix, _) in enumerate(rows) if prefix == 'FIRE_EVENT ')),
            lambda rows: events(rows)[0]['world_random'].__setitem__(0, '0000000000000000'),
            lambda rows: events(rows)[0].update(world_o=not events(rows)[0]['world_o']),
            lambda rows: events(rows)[0].update(target=[0, 0]),
        )
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index): self.rejected(mode, mutation)
        records = fixture(mode, self.originals)
        positions = [i for i, (prefix, _) in enumerate(records) if prefix == 'FIRE_EVENT ']
        records[positions[0]], records[positions[-1]] = records[positions[-1]], records[positions[0]]
        with self.assertRaises(ValueError): self.check(records, mode)

    def test_tnt_publication_failure_keeps_real_entity_and_math_draw(self):
        mode = next(profile['mode'] for profile in W.profiles() if profile['added_error_at'] and
                    self.originals[profile['mode']]['after']['entities'])
        def missing_entity(rows):
            for snapshot in states(rows, True):
                snapshot['entities'] = []; snapshot['entity_count'] = snapshot['player_count']
        def rewind_math(rows):
            before = copy.deepcopy(states(rows)[0]['math_random'])
            for snapshot in states(rows, True): snapshot['math_random'] = copy.deepcopy(before)
        def wrong_fuse(rows):
            for snapshot in states(rows, True): snapshot['entities'][0]['fuse'] -= 1
        for index, mutation in enumerate((missing_entity, rewind_math, wrong_fuse)):
            with self.subTest(index=index): self.rejected(mode, mutation)

    def test_world_random_fire_uses_complete_real_iterator_and_lcg(self):
        mode = next(profile['mode'] for profile in W.profiles() if profile['random_player'])
        def lost_candidate(rows):
            for snapshot in states(rows, True): snapshot['random_work']['candidates'].pop()
        def different_lcg(rows):
            for snapshot in states(rows, True): snapshot['random_work']['g'] ^= 1
        def partial_samples(rows): outcome(rows)['world_tick']['samples'] -= 1
        def missing_player(rows):
            for snapshot in states(rows):
                snapshot['players'] = []; snapshot['entity_count'] -= snapshot['player_count']; snapshot['player_count'] = 0
        for index, mutation in enumerate((lost_candidate, different_lcg, partial_samples, missing_player)):
            with self.subTest(index=index): self.rejected(mode, mutation)

    def test_source_null_sites_preserve_getter_before_array_order(self):
        mode = 'fire-tick-b-null'
        for index, mutation in enumerate((
                lambda rows: outcome(rows)['source_sites'].update(null=False),
                lambda rows: outcome(rows)['source_sites']['fire'].update(burn=11),
                lambda rows: outcome(rows)['hook'].update({'class': 'ah', 'method': 'c'}))):
            with self.subTest(index=index): self.rejected(mode, mutation)
        mode = 'fire-neighbor-a-null-provider-null'
        for index, mutation in enumerate((
                lambda rows: outcome(rows)['source_sites'].update(query_null=False),
                lambda rows: outcome(rows)['source_sites'].update(provider_pc=7),
                lambda rows: outcome(rows)['source_sites']['fire'].update(flammable=14))):
            with self.subTest(index=index): self.rejected(mode, mutation)

    def test_retained_world_source_receipt_identity_and_retirement(self):
        modes = [profile['mode'] for profile in W.profiles() if N.receipt_expected(profile)]
        self.assertEqual(len(modes), 3)
        for mode in modes:
            for key, value in (('busy_before', 0), ('before_holds', 1), ('after_busy_holds', 1), ('after_call_holds', 2),
                               ('retire', 9), ('same_identity', False), ('selected_source', False), ('attempts', 3),
                               ('exception', 'java/lang/NullPointerException')):
                def mutation(rows, key=key, value=value):
                    next(record for prefix, record in rows if prefix == 'FIRE_RECEIPT ')[key] = value
                with self.subTest(mode=mode, key=key): self.rejected(mode, mutation)
            records = fixture(mode, self.originals)
            at = next(index for index, (prefix, _) in enumerate(records) if prefix == 'FIRE_RECEIPT ')
            lost = copy.deepcopy(records); lost.pop(at)
            duplicate = copy.deepcopy(records); duplicate.insert(at, copy.deepcopy(duplicate[at]))
            for bad in (lost, duplicate):
                with self.subTest(mode=mode, inventory=True), self.assertRaises(ValueError): self.check(bad, mode)
        mode = 'fire-world-tick-tnt-added-error'
        self.rejected(mode, lambda rows: outcome(rows).update(exception=None, native_policy=True))

    def test_cleanup_stability_and_unvisited_plane_bytes(self):
        mode = N.MODES[0]
        def changed_cleanup(rows): states(rows)[-1]['weather']['rain_time'] += 1
        def changed_all_after(rows):
            for snapshot in states(rows, True): snapshot['chunk_planes'][-1]['planes']['block_light'].append([8192, 1, 15])
        for index, mutation in enumerate((changed_cleanup, changed_all_after)):
            with self.subTest(index=index): self.rejected(mode, mutation)


if __name__ == '__main__':
    unittest.main()
