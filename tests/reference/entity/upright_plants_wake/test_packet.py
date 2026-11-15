"""Reject changed original input, recursive effect and capture evidence."""
import copy
import unittest

import packet as W


class UprightPlantsWakePacket(unittest.TestCase):
    def test_bound_original_packet(self):
        value = W.check()
        self.assertEqual(value, W.read_reference())
        self.assertEqual(len(value['rows']), 21)
        self.assertEqual(len(W.class_pins()['classes']), 43)

    def test_finite_refusals(self):
        original = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.update(schema='UPRIGHT_PLANTS_WAKE_V0'),
            lambda p: p.update(scope='full native parity'), lambda p: p['rows'].reverse(),
            lambda p: p['rows'].pop(), lambda p: p['rows'][0].update(extra=0),
            lambda p: p['rows'][0].update(result=0), lambda p: p['rows'][0]['prepared'].update(extra=0),
            lambda p: p['rows'][0]['prepared']['profile'].update(id=True),
            lambda p: p['rows'][0]['prepared']['profile'].update(columns=3),
            lambda p: p['rows'][9]['prepared']['profile'].update(water_direction=1),
            lambda p: p['rows'][10]['prepared']['post_setup_cell_assignments'].reverse(),
            lambda p: p['rows'][0]['receiver']['neighbor'].update(**{'class': 'wb'}),
            lambda p: p['rows'][0]['receiver']['survival'].update(**{'class': 'ri'}),
            lambda p: p['rows'][6]['receiver']['placement'].update(descriptor='(I)Z'),
            lambda p: p['rows'][6]['receiver']['drop_item'].update(**{'class': 'uu'}),
            lambda p: p['rows'][0]['before'].update(sleeping=False),
            lambda p: p['rows'][0]['before']['light_inputs'].update(chunk_sky_seen_static=True),
            lambda p: p['rows'][19]['after']['light_inputs'].update(chunk_sky_seen_static=True),
            lambda p: p['rows'][0]['before']['scheduled'].update(initialized=True),
            lambda p: p['rows'][0]['after']['scheduled'].update(sequence='0000000000000000'),
            lambda p: p['rows'][4]['after']['scheduled'].update(sequence='0000000000000002'),
            lambda p: p['rows'][10]['after']['scheduled'].update(sequence='0000000000000006'),
            lambda p: p['rows'][10]['after']['scheduled'].update(ordered=[{}]),
            lambda p: p['rows'][6]['after']['scheduled'].update(members=[{}]),
            lambda p: p['rows'][0]['after'].update(world_o=True),
            lambda p: p['rows'][10]['after'].update(water_cell=[8, 63, 8, 9, 0]),
            lambda p: p['rows'][11]['after']['position'].__setitem__(2, '401e000000000000'),
            lambda p: p['rows'][9]['after']['light_inputs'].update(sky_nibble=True),
            lambda p: p['rows'][0]['after']['planes'].update(blocks='0' * 64),
            lambda p: p['rows'][19]['after']['planes'].update(metadata='0' * 64),
            lambda p: p['rows'][0]['after'].update(plant=[81, 0]),
            lambda p: p['rows'][9]['after'].update(plant=[0, 0]),
            lambda p: p['rows'][4]['after']['column'].__setitem__(1, [0, 0]),
            lambda p: p['rows'][5]['after']['column'].__setitem__(2, [0, 0]),
            lambda p: p['rows'][4]['after'].update(sleeping=False),
            lambda p: p['rows'][2]['after'].update(timer=True),
            lambda p: p['rows'][3]['after'].update(world_entities=2),
            lambda p: p['rows'][3]['after']['items'].pop(),
            lambda p: p['rows'][3]['after']['items'][1].update(stack=[338, 1, 0]),
            lambda p: p['rows'][17]['after']['items'][0].update(stack=[83, 1, 0]),
            lambda p: p['rows'][17]['after']['items'][0].update(delay=True),
            lambda p: p['rows'][17]['after']['items'][0].update(position=['0' * 16] * 3),
            lambda p: p['rows'][17]['after']['items'][0].update(phase='0' * 8),
            lambda p: p['rows'][17]['after']['items'][0].update(motion=['0' * 16] * 3),
            lambda p: p['rows'][3]['after']['random'][2].__setitem__(0, '0000000000000000'),
            lambda p: p['rows'][0]['after']['world_random'].__setitem__(1, True),
            lambda p: p['rows'][18]['events'].reverse(),
            lambda p: p['rows'][17]['events'][3]['column'].__setitem__(0, [83, 0]),
            lambda p: p['rows'][17]['events'][4].update(xyz=[7, 64, 8]),
            lambda p: p['rows'][18]['events'][3]['stack'][2].update(method='other'),
            lambda p: p['rows'][10]['events'][1].update(world_o=False),
            lambda p: p['rows'][10]['events'][1].update(water_cell=[8, 63, 8, 9, 0]),
            lambda p: p['rows'][10]['events'][1].update(xyz=[8, 63, 8, 8, 64, 8]),
            lambda p: p['rows'][20]['prepared']['profile'].update(range_error=False),
            lambda p: p['rows'][20]['after'].update(world_o=False),
            lambda p: p['rows'][20]['after'].update(sleeping=False),
            lambda p: p['rows'][20]['after']['scheduled'].update(sequence='0000000000000007'),
            lambda p: p['rows'][20]['after']['scheduled'].update(sequence='0000000000000009'),
            lambda p: p['rows'][20].update(result=False),
            lambda p: p['rows'][20]['exception_stack'][0].update(method='a'),
            lambda p: p['rows'][4]['exception_stack'][0].update(method='other'),
            lambda p: p['rows'][0].update(exception='java/lang/NullPointerException'),
            lambda p: p['rows'][1].update(exception_message='other'),
            lambda p: p['rows'][5]['exception_stack'].reverse(),
        )
        for index, change in enumerate(changes):
            damaged = copy.deepcopy(original)
            change(damaged)
            with self.subTest(index=index), self.assertRaises(ValueError): W.validate_finite(damaged)

    def test_provenance_refusals(self):
        original = W.P.load(W.HERE / 'provenance.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.update(schema='UPRIGHT_PLANTS_WAKE_PROVENANCE_V0'),
            lambda p: p.update(status='PASS'), lambda p: p.update(archive_sha256='0' * 64),
            lambda p: p.update(finite_sha256='0' * 64), lambda p: p.update(capture_sha256='x'),
            lambda p: p.update(class_count=True), lambda p: p.update(class_count=37),
            lambda p: p.update(rows_per_vm=19), lambda p: p.update(inputs_unchanged=False),
            lambda p: p['sources'].update(extra='0' * 64), lambda p: p['sources'].pop('profiles.json'),
            lambda p: p['sources'].update(**{'packet.py': '0' * 64}),
            lambda p: p['shared'].pop('setup'), lambda p: p['helpers'].pop('UprightPlantsWakeObservation.class'),
            lambda p: p['binaries'].update(client='0' * 64), lambda p: p['methods'].pop('fd.f(III)Lln;'),
            lambda p: p['original'].pop(), lambda p: p['original'][0].update(exit=1),
            lambda p: p['original'][0].update(timed_out=True),
            lambda p: p['original'][0].update(pipe_capture_complete=False),
            lambda p: p['original'][0].update(output_overflow=True),
            lambda p: p['original'][0].update(stdout_bytes=0),
            lambda p: p['original'][0].update(timeout_seconds=71),
            lambda p: p['original'][1].update(stdout_sha256='0' * 64),
            lambda p: p['processes']['compile'].update(exit=True),
            lambda p: p['processes']['compile'].update(stderr_bytes=1),
            lambda p: p['processes'].pop('bytecode'),
        )
        for index, change in enumerate(changes):
            damaged = copy.deepcopy(original)
            change(damaged)
            with self.subTest(index=index), self.assertRaises(ValueError): W.validate_provenance(damaged)

    def test_method_and_class_pin_refusals(self):
        original = W.P.load(W.HERE / 'upright-plant-pins.json')
        changes = (lambda p: p.update(extra=0), lambda p: p.update(archive_sha256='0' * 64),
                   lambda p: p['classes'].pop('ln'), lambda p: p['classes'].update(extra={}),
                   lambda p: p['classes']['or'].update(class_bytes=True),
                   lambda p: p['classes']['ri'].update(super='wb'),
                   lambda p: p['classes']['ln']['methods'][0].update(code_length=True),
                   lambda p: p['methods']['fd.f(III)Lln;'].update(code_length=True),
                   lambda p: p['methods'].pop('ri.h(Lfd;III)V'))
        for index, change in enumerate(changes):
            damaged = copy.deepcopy(original)
            change(damaged)
            with self.subTest(index=index), self.assertRaises(ValueError): W.validate_class_pins(damaged)

    def test_json_transport(self):
        for text in ('{"a":0,"a":1}', '{"v":NaN}', '{"v":Infinity}', '{"v":-Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError): W.P.loads(text)

    def test_declared_recursive_prefixes(self):
        cases = {p['mode']: W.model(p) for p in W.profiles()}
        for prefix in ('cactus', 'reeds'):
            whole = cases[prefix + '-column3']
            self.assertEqual(len(whole['items']), 3)
            self.assertEqual(whole['removed_count'], 3)
            self.assertEqual(whole['draws'], 12)
            self.assertEqual([e['kind'] for e in whole['events']], ['cell', 'added', 'cell', 'added', 'cell', 'added', 'cell'])
            second = cases[prefix + '-column3-publication-error-second']
            self.assertEqual(len(second['items']), 2)
            self.assertEqual(second['removed_count'], 1)
            self.assertEqual(second['draws'], 8)
            middle = cases[prefix + '-column3-removal-error-middle']
            self.assertEqual(middle['removed_count'], 2)
            self.assertTrue(middle['removal_error'])
        for mode in ('reeds-west-water', 'reeds-east-water', 'reeds-north-water', 'reeds-south-water',
                     'reeds-flowing-water', 'reeds-stacked'):
            self.assertTrue(cases[mode]['supported'])
            self.assertEqual(cases[mode]['draws'], 0)
        self.assertFalse(cases['reeds-sand']['supported'])
        self.assertEqual(cases['cactus-column3']['schedule_requests'], 1)
        self.assertEqual(cases['cactus-removal-error']['schedule_requests'], 0)
        self.assertEqual(cases['reeds-east-water']['schedule_requests'], 2)
        self.assertEqual([e['kind'] for e in cases['reeds-east-water']['events']], ['cell', 'range'])
        self.assertEqual(W.exit_position(next(p for p in W.profiles() if p['mode'] == 'reeds-north-water'), 0), (7.5, 9.5))
        stopped = cases['reeds-east-water-range-error']
        self.assertFalse(stopped['normal'])
        self.assertTrue(stopped['range_error'])
        self.assertTrue(stopped['world_o'])
        self.assertEqual(stopped['schedule_requests'], 1)
        self.assertEqual(stopped['draws'], 0)
        self.assertEqual(stopped['items'], [])


if __name__ == '__main__':
    unittest.main()
