"""Corruption controls for Cactus/Reeds native observations and original pairing."""
import copy
import json
import unittest

import run_native as N

W, P = N.W, N.P


def plane_runs(profile, removed_count):
    values = {key: bytearray(size) for key, size in N.N.PLANES.items()}
    def nibble(name, offset, value):
        shift = (offset & 1) * 4
        array = values[name]
        array[offset >> 1] = (array[offset >> 1] & ~(15 << shift)) | value << shift
    def cell(x, y, z, block, metadata=0):
        offset = (x * 16 + z) * 128 + y
        values['blocks'][offset] = block
        nibble('metadata', offset, metadata)
    for x in range(16):
        for z in range(16):
            cell(x, 63, z, 2)
            for y in range(64, 128): nibble('sky', (x * 16 + z) * 128 + y, 15)
    cell(8, 64, 8, 26, 8)
    cell(8, 64, 7, 26)
    cell(7, 63, 8, profile['floor'])
    for level in range(removed_count, profile['columns']): cell(7, 64 + level, 8, profile['id'])
    if profile['water_direction'] >= 0:
        x, z = W.WATER[profile['water_direction']]
        # East water sits immediately below the Bed and reaches its actual
        # still-to-flowing neighbor conversion during the head notification.
        cell(7 + x, 63, 8 + z, 8 if profile['water_direction'] == 1 else profile['water_id'])
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
    return {**N.N.selected_item(original), 'random': ['0000000000000001', False, '0000000000000000']}


def fixture(mode, originals):
    profile = N.profile_for(mode)
    original = originals[profile['mode']]
    missing = mode.endswith('-math-missing')
    failed_math = missing and bool(original['after']['items'])
    source = (originals['cactus-publication-error' if profile['id'] == 81 else 'reeds-publication-error']['after']
              if failed_math else original['after'])
    snapshot = {key: copy.deepcopy(source[key]) for key in N.N.STATE_FIELDS}
    cells = original['before']['column'] if failed_math else original['after']['column']
    removed_count = sum(cell[0] == 0 for cell in cells[:profile['columns']])
    normal = original['exception'] == 'NONE' and not failed_math
    items = [] if failed_math else [native_item(item) for item in source['items']]
    if failed_math:
        snapshot.update(plant=[profile['id'], 0], plant_floor=[profile['floor'], 0], world_entities=1)
    math = source['random'][2]
    snapshot.update(phase='returned' if normal else 'diagnostic', pending=not normal,
                    light=[15, 0, 64], world_f=5, sky_seen=False, items=items,
                    math_random=dict(present=True, phase=2, state=math[0], cache=math[1], gaussian=math[2]))
    events = []
    random = W.RandomModel(profile['world_seed'])
    for event in original['events'][:1] if failed_math else original['events']:
        entry = {key: copy.deepcopy(event[key]) for key in ('kind', 'head', 'plant', 'column', 'sleeping', 'world_o', 'water_cell')}
        if event['kind'] == 'added':
            for _ in range(4): random.floating()
            entry['item'] = native_item(event['item'])
        else: entry['xyz'] = copy.deepcopy(event['xyz'])
        entry['world_seed'] = random.observation()[0]
        events.append(entry)
    snapshot.update(cells=sum(event['kind'] == 'cell' for event in events), event_count=len(events))
    inputs = {key: value for key, value in profile.items() if key != 'mode'}
    inputs['math_owned'] = not missing
    inputs['scheduled_before'] = {key: original['before']['scheduled'][key] for key in ('sequence', 'initialized')}
    outcome = dict(mode=mode, input=inputs, continued=normal, raw=0 if normal or failed_math else 1,
                   pc=35, wake_entered=True, result=False if normal else None,
                   exception='NONE' if normal or failed_math else 'java/lang/IllegalStateException', math_failed=failed_math)
    planes = plane_runs(profile, removed_count)
    before = {key: copy.deepcopy(original['before'][key]) for key in N.RUNTIME_FIELDS}
    reached = before if failed_math else {key: copy.deepcopy(original['after'][key]) for key in N.RUNTIME_FIELDS}
    rows = [('UPRIGHT_WAKE_RUNTIME ', dict(phase='before', **before)), ('PLANT_WAKE_OUTCOME ', outcome)]
    for phase in (snapshot['phase'], 'native-close', 'cleanup', 'retired'):
        value = {**copy.deepcopy(snapshot), 'phase': phase,
                 'pending': not normal if phase in (snapshot['phase'], 'native-close') else False}
        rows.extend([('PLANT_WAKE_STATE ', value), ('PLANT_WAKE_PLANES ', dict(phase=phase, planes=copy.deepcopy(planes))),
                     ('UPRIGHT_WAKE_COLUMN ', dict(phase=phase, column=copy.deepcopy(cells))),
                     ('UPRIGHT_WAKE_RUNTIME ', dict(phase=phase, **copy.deepcopy(reached)))])
        if phase == snapshot['phase']: rows.append(('PLANT_WAKE_EVENTS ', events))
    return rows


def output(rows, mode):
    return '\n'.join(prefix + json.dumps(value, separators=(',', ':')) for prefix, value in rows) + \
        '\nPASS: 99 retained upright plant wake checks;mode=' + mode + '\n'


class NativeReader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.originals = {row['mode']: row for row in W.read_reference()['rows']}

    def run_case(self, rows, mode):
        return N.validate(N.parse(output(rows, mode), mode), mode, self.originals)

    def test_all_profiles(self):
        paired = 0
        for mode in N.MODES:
            with self.subTest(mode=mode):
                paired += self.run_case(fixture(mode, self.originals), mode)['matched'] is True
        self.assertEqual(len(N.MODES), 25)
        self.assertEqual(paired, 22)

    def test_column_and_item_corruptions(self):
        mode = 'cactus-column3-publication-error-second'
        changes = (
            lambda rows: rows[1][1]['input'].update(columns=True),
            lambda rows: rows[1][1].update(pc=34),
            lambda rows: rows[1][1].update(raw=False),
            lambda rows: rows[1][1].update(result=False),
            lambda rows: rows[2][1].update(pending=False),
            lambda rows: rows[2][1]['items'][1].update(yaw='00000000'),
            lambda rows: rows[2][1]['math_random'].update(state='0000000000000000'),
            lambda rows: rows[4][1]['column'].__setitem__(2, [0, 0]),
            lambda rows: rows[6][1][2]['column'].__setitem__(1, [0, 0]),
            lambda rows: rows[6][1][1].update(world_seed='0000000000000000'),
            lambda rows: rows[6][1].reverse(),
            lambda rows: rows[9][1]['column'].__setitem__(1, [0, 0]),
            lambda rows: rows[11][1].update(pending=True),
            lambda rows: rows[15][1]['items'][0]['random'].__setitem__(0, '0000000000000002'),
            lambda rows: rows[12][1]['planes']['height'][0].__setitem__(2, 65),
            lambda rows: rows[17][1].update(phase='cleanup'),
            lambda rows: rows[5][1]['scheduled'].update(sequence='0000000000000000'),
            lambda rows: rows[10][1].update(world_o=True),
            lambda rows: rows.pop(4),
            lambda rows: rows.append(copy.deepcopy(rows[-1])),
        )
        for index, change in enumerate(changes):
            rows = fixture(mode, self.originals)
            change(rows)
            with self.subTest(index=index), self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_closed_json_and_output(self):
        mode = 'reeds-west-water'
        text = output(fixture(mode, self.originals), mode)
        values = (text.replace('"raw":0', '"raw":0,"raw":0', 1),
                  text.replace('"world_f":5', '"world_f":NaN', 1), text + 'EXTRA\n',
                  text.rsplit('\nPASS:', 1)[0], text.replace('mode=' + mode, 'mode=wrong'),
                  '16 achievements\n' + text)
        for index, altered in enumerate(values):
            with self.subTest(index=index), self.assertRaises(ValueError): N.parse(altered, mode)

    def test_missing_math_completed_prefix(self):
        mode = 'reeds-column3-math-missing'
        for key, value in (('world_entities', 2), ('timer', 0), ('plant', [0, 0]),
                           ('world_random', ['0000000000000001', False, '0000000000000000'])):
            rows = fixture(mode, self.originals)
            for index in (2, 7, 11, 15): rows[index][1][key] = copy.deepcopy(value)
            with self.subTest(key=key), self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        for index in (2, 7, 11, 15): rows[index][1]['math_random'].update(state=W.RandomModel(0).observation()[0])
        with self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_exact_scheduling_and_range_effects(self):
        mode = 'reeds-east-water'
        changes = (
            lambda rows: rows[0][1]['scheduled'].update(initialized=False),
            lambda rows: rows[1][1]['input']['scheduled_before'].update(sequence='0000000000000000'),
            lambda rows: rows[6][1][-1].update(world_o=False),
            lambda rows: rows[6][1][-1]['water_cell'].__setitem__(3, 9),
            lambda rows: rows[6][1][-1].update(xyz=[8, 63, 8]),
            lambda rows: [rows[index][1]['scheduled'].update(sequence='0000000000000000') for index in (5, 10, 14, 18)],
            lambda rows: [rows[index][1].update(world_o=True) for index in (5, 10, 14, 18)],
            lambda rows: [rows[index][1]['scheduled']['members'].append(
                dict(xyz=[8, 63, 8], block=8, time='0000000000000005', ordinal='0000000000000005'))
                          for index in (5, 10, 14, 18)],
        )
        for index, change in enumerate(changes):
            rows = fixture(mode, self.originals)
            change(rows)
            with self.subTest(index=index), self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_range_exception_preserves_interrupted_state(self):
        mode = 'reeds-east-water-range-error'
        for field, value in (('world_o', False), ('water_cell', [8, 63, 8, 9, 0])):
            rows = fixture(mode, self.originals)
            for index in (5, 10, 14, 18): rows[index][1][field] = copy.deepcopy(value)
            with self.subTest(field=field), self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        before = int(self.originals[mode]['before']['scheduled']['sequence'], 16)
        for index in (5, 10, 14, 18):
            rows[index][1]['scheduled']['sequence'] = f'{before + 2:016x}'
        with self.assertRaises(ValueError): self.run_case(rows, mode)
        rows = fixture(mode, self.originals)
        for index in (2, 7, 11, 15): rows[index][1].update(sleeping=False, timer=0, world_sleeping=False)
        with self.assertRaises(ValueError): self.run_case(rows, mode)

    def test_plane_extent_and_run_controls(self):
        for rows in ([[0, 0, 1]], [[0, 2, 1], [1, 1, 2]], [[0, 1, 1], [1, 1, 1]],
                     [[0, 1, 0]], [[0, 1, 256]], [[0, True, 1]], [[16, 1, 1]]):
            with self.subTest(rows=rows), self.assertRaises(ValueError): N.N.plane(rows, 16)


if __name__ == '__main__':
    unittest.main()
