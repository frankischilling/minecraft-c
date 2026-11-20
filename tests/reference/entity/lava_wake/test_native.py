"""Corruption controls for Lava presentation, partial effects and native lifetimes."""
import copy
import hashlib
import json
import unittest

import run_native as N

W, P = N.W, N.P


def plane_runs(arrays):
    result = {}
    for name, data in arrays.items():
        result[name] = []
        start = 0
        while start < len(data):
            end = start + 1
            while end < len(data) and data[end] == data[start]: end += 1
            if data[start]: result[name].append([start, end - start, data[start]])
            start = end
    return result


def native_state(source, arrays, math, phase, pending, cells, event_count):
    snapshot = {key: copy.deepcopy(source[key]) for key in N.COMMON_FIELDS}
    offset = (7 * 16 + 8) * 128 + 64
    floor = offset - 1
    def nibble(name, index): return (arrays[name][index >> 1] >> ((index & 1) * 4)) & 15
    snapshot.update(phase=phase, pending=pending,
                    plant=[arrays['blocks'][offset], nibble('metadata', offset)],
                    plant_floor=[arrays['blocks'][floor], nibble('metadata', floor)],
                    light=[nibble('sky', offset), nibble('block_light', offset), arrays['height'][(8 << 4) | 7]],
                    world_f=5, sky_seen=source['sky_seen'],
                    math_random=dict(present=True, phase=2, state=math[0], cache=math[1], gaussian=math[2]),
                    items=[], cells=cells, event_count=event_count)
    return snapshot


def fixture(mode, originals):
    profile, _, control = N.mode_spec(mode)
    original = originals[profile['mode']]
    selected = N.control_expected(control, original, originals) if control in (1, 3, 4) else N.original_projection(original)
    arrays = W.plane_arrays(profile, True)
    P.require(set(arrays) == set(N.N.PLANES) and
              all(len(data) == N.N.PLANES[name] and hashlib.sha256(data).hexdigest() == selected['planes'][name]
                  for name, data in arrays.items()), 'reader-control authored planes match actual original hashes')
    normal = selected['exception'] == 'NONE' and control not in (1, 3, 4)
    phase = 'returned' if normal else 'diagnostic'
    # The shared native transport also retains its fixed (7,64,8) diagnostics.
    # The Lava projection uses the independently declared actual target instead.
    events = copy.deepcopy(selected['events'])
    snapshot = native_state(selected['state'], arrays, selected['math_random'], phase, not normal,
                            sum(event['kind'] == 'cell' for event in events), len(events))
    before = {key: copy.deepcopy(original['before'][key]) for key in N.RUNTIME_FIELDS}
    reached = {key: copy.deepcopy(selected['state'][key]) for key in N.RUNTIME_FIELDS}
    outcome = dict(mode=mode, input=N.prepared(profile), control=control,
                   scheduled_before={key: before['scheduled'][key] for key in ('sequence', 'initialized')},
                   continued=normal, raw=0 if normal or control in (1, 3, 4) else 1, pc=35, wake_entered=True,
                   result=selected['result'], exception=selected['exception'],
                   native_status=5 if control == 1 else 9 if control in (3, 4) else 0)
    before_arrays = W.plane_arrays(profile, False)
    P.require(all(hashlib.sha256(data).hexdigest() == original['before']['planes'][name]
                  for name, data in before_arrays.items()), 'reader-control declared before planes match actual original')
    before_state = native_state(original['before'], before_arrays, original['before']['random'][2], 'before', False, 0, 0)
    rows = [('PLANT_WAKE_STATE ', before_state),
            ('PLANT_WAKE_PLANES ', dict(phase='before', planes=plane_runs(before_arrays))),
            ('LAVA_WAKE_RUNTIME ', dict(phase='before', **before)), ('LAVA_WAKE_OUTCOME ', outcome)]
    planes = plane_runs(arrays)
    for current in (phase, 'native-close', 'cleanup', 'retired'):
        rows.extend([('PLANT_WAKE_STATE ', {**copy.deepcopy(snapshot), 'phase': current,
                                          'pending': not normal if current in (phase, 'native-close') else False}),
                     ('PLANT_WAKE_PLANES ', dict(phase=current, planes=copy.deepcopy(planes))),
                     ('LAVA_WAKE_RUNTIME ', dict(phase=current, **copy.deepcopy(reached)))])
        if current == phase:
            rows.append(('LAVA_WAKE_EVENTS ', events))
            if control in (3, 4):
                rows.append(('LAVA_WAKE_DESCENDANT ', dict(raw_callback=0, callback_kind='sound' if control == 3 else 'particle',
                    first_close=9, child_pop=0, second_close=0, parent_slot=1, parent_generation=11,
                    child_slot=2, child_generation=12, source_exception=False, explicit_child_pops=1)))
    return rows


def text(rows, mode):
    return '\n'.join(prefix + json.dumps(value, separators=(',', ':')) for prefix, value in rows) + \
        '\nPASS: 99 retained Lava wake checks;mode=' + mode + '\n'


def records(rows, prefix):
    return [value for actual, value in rows if actual == prefix]


class NativeReader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.originals = {row['mode']: row for row in W.read_reference()['rows']}

    def run_case(self, rows, mode):
        return N.validate(N.parse(text(rows, mode), mode), mode, self.originals)

    def test_all_originals_and_native_controls(self):
        self.assertEqual(len(W.MODES), 29)
        self.assertEqual(len(N.MODES), 34)
        for mode in N.MODES:
            with self.subTest(mode=mode): self.assertTrue(self.run_case(fixture(mode, self.originals), mode)['matched'])

    def test_outcome_and_input_corruptions(self):
        mode = 'lava-obsidian-sound-error'
        changes = (
            lambda rows: rows[3][1]['input'].update(metadata=True),
            lambda rows: rows[3][1]['input'].update(xyz=[7, 64, 9]),
            lambda rows: rows[3][1].update(raw=False),
            lambda rows: rows[3][1].update(raw=0),
            lambda rows: rows[3][1].update(pc=34),
            lambda rows: rows[3][1].update(result=False),
            lambda rows: rows[3][1].update(exception='NONE'),
            lambda rows: rows[3][1].update(native_status=9),
            lambda rows: rows[3][1]['scheduled_before'].update(sequence='ffffffffffffffff'),
            lambda rows: rows[2][1]['scheduled'].update(initialized=1),
        )
        for index, change in enumerate(changes):
            rows = fixture(mode, self.originals)
            change(rows)
            with self.subTest(index=index), self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_actual_before_state_planes_and_duplicate_fields(self):
        mode = 'lava-obsidian'
        changes = (
            lambda rows: rows[0][1].update(idle=99),
            lambda rows: rows[0][1]['position'].__setitem__(0, '0000000000000000'),
            lambda rows: rows[0][1]['world_random'].__setitem__(0, '0000000000000000'),
            lambda rows: rows[0][1]['player_random'].__setitem__(0, '0000000000000000'),
            lambda rows: rows[0][1]['math_random'].update(state='0000000000000000'),
            lambda rows: rows[0][1]['math_random'].update(present=False),
            lambda rows: rows[1][1]['planes']['height'][0].__setitem__(2, 65),
            lambda rows: rows[0][1].update(sky_seen=True),
            lambda rows: rows[4][1].update(sky_seen=True),
            lambda rows: rows[0][1].update(pending=True),
            lambda rows: rows[0][1].update(event_count=1),
        )
        for index, change in enumerate(changes):
            rows = fixture(mode, self.originals)
            change(rows)
            with self.subTest(index=index), self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_complete_presentation_params_and_random(self):
        mode = 'lava-hot-meta5'
        changes = (
            lambda events: next(event for event in events if event['kind'] == 'sound').update(volume='00000000'),
            lambda events: next(event for event in events if event['kind'] == 'sound').update(pitch='00000000'),
            lambda events: next(event for event in events if event['kind'] == 'particle').update(name='smoke'),
            lambda events: next(event for event in events if event['kind'] == 'particle')['position'].__setitem__(0, '0000000000000000'),
            lambda events: next(event for event in events if event['kind'] == 'particle')['motion'].__setitem__(1, '8000000000000000'),
            lambda events: next(event for event in events if event['kind'] == 'particle')['math_random'].__setitem__(0, '0000000000000000'),
            lambda events: next(event for event in events if event['kind'] == 'sound')['world_random'].__setitem__(0, '0000000000000000'),
            lambda events: events.reverse(),
        )
        for index, change in enumerate(changes):
            rows = fixture(mode, self.originals)
            change(rows[7][1])
            with self.subTest(index=index), self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        events = rows[7][1]
        del events[-2]
        for snapshot in records(rows, 'PLANT_WAKE_STATE ')[1:]:
            snapshot['event_count'] = len(events)
            snapshot['cells'] = sum(event['kind'] == 'cell' for event in events)
        with self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_retained_world_lighting_and_cleanup(self):
        mode = 'lava-hot-late-range-error'
        for field, value in (('world_o', False), ('lava', [11, 5]), ('sky_seen', True)):
            rows = fixture(mode, self.originals)
            for runtime in records(rows, 'LAVA_WAKE_RUNTIME ')[1:]: runtime[field] = copy.deepcopy(value)
            with self.subTest(field=field), self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        for runtime in records(rows, 'LAVA_WAKE_RUNTIME ')[1:]: runtime['lighting']['regions'] = []
        with self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        for runtime in records(rows, 'LAVA_WAKE_RUNTIME ')[1:]: runtime['scheduled']['sequence'] = 'ffffffffffffffff'
        with self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        records(rows, 'PLANT_WAKE_STATE ')[2]['world_random'][0] = '0000000000000000'
        with self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        records(rows, 'PLANT_WAKE_PLANES ')[3]['planes']['height'][0][2] ^= 1
        with self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_native_scope_and_fp_prefixes(self):
        for mode in ('lava-obsidian-sound-descendant', 'lava-obsidian-particle-descendant'):
            for field, value in (('first_close', 0), ('raw_callback', 9), ('explicit_child_pops', 2),
                                 ('source_exception', True), ('child_slot', 1), ('parent_generation', 0)):
                rows = fixture(mode, self.originals)
                records(rows, 'LAVA_WAKE_DESCENDANT ')[0][field] = value
                with self.subTest(mode=mode, field=field), self.assertRaises(ValueError): self.run_case(rows, mode)
            rows = fixture(mode, self.originals)
            rows[3][1].update(exception='java/lang/IllegalStateException')
            with self.assertRaises(ValueError): self.run_case(rows, mode)
        mode = 'lava-obsidian-fp-refusal'
        rows = fixture(mode, self.originals)
        for snapshot in records(rows, 'PLANT_WAKE_STATE ')[1:]: snapshot['world_random'][0] = self.originals['lava-obsidian-sound-error']['after']['world_random'][0]
        with self.assertRaises(ValueError): self.run_case(rows, mode)
        mode = 'lava-obsidian-no-math-helper'
        rows = fixture(mode, self.originals)
        for snapshot in records(rows, 'PLANT_WAKE_STATE ')[1:]: snapshot['math_random']['state'] = W.RandomModel(0).observation()[0]
        with self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_closed_json_and_completion(self):
        mode = 'lava-obsidian'
        output = text(fixture(mode, self.originals), mode)
        changes = (output.replace('"raw":0', '"raw":0,"raw":0', 1),
                   output.replace('"world_f":5', '"world_f":NaN', 1), output + 'EXTRA\n',
                   output.rsplit('\nPASS:', 1)[0], output.replace('mode=' + mode, 'mode=wrong'),
                   '16 achievements\n' + output)
        for index, altered in enumerate(changes):
            with self.subTest(index=index), self.assertRaises(ValueError): N.parse(altered, mode)

    def test_lossless_plane_extent_controls(self):
        for rows in ([[0, 0, 1]], [[0, 2, 1], [1, 1, 2]], [[0, 1, 1], [1, 1, 1]],
                     [[0, 1, 0]], [[0, 1, 256]], [[0, True, 1]], [[16, 1, 1]]):
            with self.subTest(rows=rows), self.assertRaises(ValueError): N.N.plane(rows, 16)


if __name__ == '__main__':
    unittest.main()
