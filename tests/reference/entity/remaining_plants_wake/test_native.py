"""Corruption controls for the native transport and original pairing boundary."""
import copy
import json
import unittest

import run_native as N

W, P = N.W, N.P


def plane_runs(profile, removed):
    """Author the declared raw fixture planes for reader controls, never native answers."""
    values = {key: bytearray(length) for key, length in N.PLANES.items()}
    def nibble(name, index, value):
        shift = (index & 1) * 4
        array = values[name]
        array[index >> 1] = (array[index >> 1] & ~(15 << shift)) | value << shift
    def cell(x, y, z, block, metadata):
        index = (x * 16 + z) * 128 + y
        values['blocks'][index] = block
        nibble('metadata', index, metadata)
    for x in range(16):
        for z in range(16):
            cell(x, 63, z, 2, 0)
            for y in range(64, 128): nibble('sky', (x * 16 + z) * 128 + y, 15)
    nibble('sky', (7 * 16 + 8) * 128 + 64, profile['light'])
    cell(8, 64, 8, 26, 8)
    cell(8, 64, 7, 26, 0)
    cell(7, 64, 8, 0 if removed else profile['id'], 0 if removed else profile['metadata'])
    cell(7, 63, 8, profile['floor'], 0)
    values['height'][:] = bytes([64]) * 256
    result = {}
    for name, data in values.items():
        result[name] = []
        start = 0
        while start < len(data):
            end = start + 1
            while end < len(data) and data[end] == data[start]: end += 1
            if data[start]: result[name].append([start, end - start, data[start]])
            start = end
    return result


def native_item(original):
    return {**N.selected_item(original), 'random': ['0000000000000001', False, '0000000000000000']}


def fixture(mode, originals):
    profile = N.profile_for(mode)
    original = originals[profile['mode']]
    predicted = W.model(profile)
    missing = mode.endswith('-math-missing')
    failed_math = missing and bool(predicted['items'])
    normal = predicted['normal'] and not failed_math
    source = original['after']
    if failed_math:
        source = originals['crop-publication-error-first']['after']
    snapshot = {key: copy.deepcopy(source[key]) for key in N.STATE_FIELDS}
    items = [native_item(value) for value in original['after']['items']]
    if failed_math:
        random = W.RandomModel(profile['world_seed'])
        random.floating()
        if profile['id'] == 59 and profile['metadata'] != 7:
            for _ in range(3):
                if random.bounded(15) <= profile['metadata']: break
        for _ in range(3): random.floating()
        snapshot.update(plant=[profile['id'], profile['metadata']], plant_floor=[profile['floor'], 0],
                        world_random=random.observation(), world_entities=1)
        items = []
        draws = random.draws
    else: draws = predicted['draws']
    math = (W.model({**profile, 'publication_error_at': 1})['math_random'] if failed_math else
            original['after']['random'][2])
    snapshot.update(phase='returned' if normal else 'diagnostic', pending=not normal,
                    light=[profile['light'], 0, 64], world_f=5, sky_seen=True,
                    math_random=dict(present=True, phase=2, state=math[0], cache=math[1], gaussian=math[2]),
                    items=items)
    events = []
    item_index = 0
    for index, event in enumerate(original['events'][:1] if failed_math else original['events']):
        added = event['kind'] == 'added'
        entry = {key: copy.deepcopy(event[key]) for key in ('kind', 'head', 'plant', 'sleeping')}
        if added:
            item_index += 1
            entry['item'] = native_item(event['item'])
            entry['world_seed'] = W.model({**profile, 'publication_error_at': item_index})['world_random'][0]
        else:
            entry['xyz'] = copy.deepcopy(event['xyz'])
            entry['world_seed'] = (snapshot['world_random'][0] if index else
                                   W.RandomModel(profile['world_seed']).observation()[0])
        events.append(entry)
    snapshot.update(cells=sum(event['kind'] == 'cell' for event in events), event_count=len(events))
    outcome = dict(mode=mode, input={key: value for key, value in profile.items() if key != 'mode'},
                   continued=normal, raw=0 if normal or failed_math else 1, pc=35, wake_entered=True,
                   result=False if normal else None,
                   exception='NONE' if normal or failed_math else 'java/lang/IllegalStateException',
                   math_failed=failed_math, model_draws=draws)
    outcome['input'].update(math_owned=not missing, initial_sky_seen=mode != N.MODES[0])
    planes = plane_runs(profile, snapshot['plant'][0] == 0)
    rows = [('PLANT_WAKE_OUTCOME ', outcome), ('PLANT_WAKE_STATE ', snapshot),
            ('PLANT_WAKE_PLANES ', dict(phase=snapshot['phase'], planes=planes)),
            ('PLANT_WAKE_EVENTS ', events)]
    for phase in ('native-close', 'cleanup', 'retired'):
        rows.append(('PLANT_WAKE_STATE ', {**copy.deepcopy(snapshot), 'phase': phase,
                                          'pending': not normal if phase == 'native-close' else False}))
        rows.append(('PLANT_WAKE_PLANES ', dict(phase=phase, planes=copy.deepcopy(planes))))
    return rows


def output(rows, mode):
    return '\n'.join(prefix + json.dumps(value, separators=(',', ':')) for prefix, value in rows) + \
        '\nPASS: 99 retained plant wake checks;mode=' + mode + '\n'


class NativeReader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.originals = {row['mode']: row for row in W.read_reference()['rows']}

    def run_case(self, rows, mode):
        return N.validate(N.parse(output(rows, mode), mode), mode, self.originals)

    def test_all_declared_profiles(self):
        paired = 0
        for mode in N.MODES:
            with self.subTest(mode=mode):
                result = self.run_case(fixture(mode, self.originals), mode)
                paired += result['matched'] is True
        self.assertEqual(paired, 27)  # Two missing-Math profiles also pair their no-Item outcomes.

    def test_native_corruptions(self):
        mode = 'crop-publication-error-second'
        changes = (
            lambda rows: rows[0][1].update(raw=False),
            lambda rows: rows[0][1].update(result=False),
            lambda rows: rows[0][1]['input'].update(world_seed=True),
            lambda rows: rows[0][1].update(pc=34),
            lambda rows: rows[1][1].update(phase='returned'),
            lambda rows: rows[1][1].update(pending=False),
            lambda rows: rows[1][1].update(health=True),
            lambda rows: rows[1][1]['items'][0].update(yaw='00000000'),
            lambda rows: rows[1][1]['items'][0]['motion'].__setitem__(0, '0000000000000000'),
            lambda rows: rows[1][1]['math_random'].update(state='0000000000000001'),
            lambda rows: rows[3][1][1].update(world_seed='0000000000000000'),
            lambda rows: rows[3][1].reverse(),
            lambda rows: rows[4][1]['items'][0]['random'].__setitem__(0, '0000000000000002'),
            lambda rows: rows[6][1].update(pending=True),
            lambda rows: rows[8][1].update(timer=0),
            lambda rows: rows[2][1]['planes']['blocks'][0].__setitem__(2, 0),
            lambda rows: rows[5][1]['planes']['height'][0].__setitem__(2, 65),
            lambda rows: rows[9][1]['planes'].update(extra=[]),
            lambda rows: rows.append(copy.deepcopy(rows[-1])),
            lambda rows: rows.pop(2),
        )
        for index, change in enumerate(changes):
            rows = fixture(mode, self.originals)
            change(rows)
            with self.subTest(index=index), self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_closed_stdout_and_json(self):
        mode = N.MODES[0]
        text = output(fixture(mode, self.originals), mode)
        changes = (text.replace('"raw":0', '"raw":0,"raw":0', 1),
                   text.replace('"world_f":5', '"world_f":NaN', 1),
                   text.removesuffix('\n').rsplit('\n', 1)[0], text + 'EXTRA\n',
                   text.replace('mode=' + mode, 'mode=wrong'),
                   '16 achievements\n16 achievements\n' + text)
        for index, altered in enumerate(changes):
            with self.subTest(index=index), self.assertRaises(ValueError): N.parse(altered, mode)

    def test_missing_math_prefix_corruptions(self):
        mode = 'mushroom39-unsupported-math-missing'
        for key, value in (('timer', 0), ('plant', [0, 0]), ('world_entities', 2),
                           ('world_random', ['0000000000000001', False, '0000000000000000'])):
            rows = fixture(mode, self.originals)
            for index in (1, 4, 6, 8): rows[index][1][key] = copy.deepcopy(value)
            with self.subTest(key=key), self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        initial = W.RandomModel(0).observation()
        for index in (1, 4, 6, 8):
            rows[index][1]['math_random'].update(state=initial[0], cache=initial[1], gaussian=initial[2])
        with self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_plane_run_boundaries(self):
        for rows in ([[0, 0, 1]], [[-1, 1, 1]], [[0, 2, 1], [1, 1, 2]],
                     [[0, 1, 1], [1, 1, 1]], [[0, 1, 0]], [[0, 1, 256]], [[0, True, 1]],
                     [[16, 1, 1]]):
            with self.subTest(rows=rows), self.assertRaises(ValueError): N.plane(rows, 16)


if __name__ == '__main__':
    unittest.main()
