"""Reject changed input, callback prefixes, full planes and capture evidence."""
import copy
import unittest

import packet as W


class LavaWakePacket(unittest.TestCase):
    def test_bound_original_packet(self):
        value = W.check()
        self.assertEqual(value, W.read_reference())
        self.assertEqual(len(value['rows']), 29)
        self.assertEqual(len(W.class_pins()['classes']), 46)

    def test_finite_refusals(self):
        original = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.update(schema='LAVA_WAKE_V0'),
            lambda p: p.update(scope='whole-game parity'), lambda p: p['rows'].reverse(),
            lambda p: p['rows'].pop(), lambda p: p['rows'][0].update(extra=0),
            lambda p: p['rows'][0].update(result=0),
            lambda p: p['rows'][0]['prepared'].update(extra=0),
            lambda p: p['rows'][0]['prepared']['profile'].update(id=True),
            lambda p: p['rows'][1]['prepared']['profile'].update(metadata=0),
            lambda p: p['rows'][9]['prepared']['profile'].update(x=7),
            lambda p: p['rows'][12]['prepared']['profile'].update(water_direction=3),
            lambda p: p['rows'][13]['prepared']['post_setup_cell_assignments'].reverse(),
            lambda p: p['rows'][0]['prepared'].update(light_or_sky_getter_overrides=['f']),
            lambda p: p['rows'][0]['receiver']['neighbor'].update(**{'class': 'rp'}),
            lambda p: p['rows'][0]['receiver']['addition'].update(**{'class': 'nx'}),
            lambda p: p['rows'][0]['receiver']['tick_rate'].update(descriptor='(I)I'),
            lambda p: p['rows'][0]['receiver'].update(actual_lava_material=False),
            lambda p: p['rows'][0]['before'].update(sleeping=False),
            lambda p: p['rows'][0]['before'].update(sky_seen=True),
            lambda p: p['rows'][28]['after'].update(sky_seen=True),
            lambda p: p['rows'][0]['before']['scheduled'].update(initialized=True),
            lambda p: p['rows'][5]['after']['scheduled'].update(sequence='0000000000000001'),
            lambda p: p['rows'][6]['before']['scheduled'].update(sequence='0000000000000000'),
            lambda p: p['rows'][13]['after']['scheduled'].update(ordered=[{}]),
            lambda p: p['rows'][13]['after']['scheduled'].update(members=[{}]),
            lambda p: p['rows'][27]['after']['scheduled'].update(sequence='000000000000000f'),
            lambda p: p['rows'][0]['after'].update(world_o=True),
            lambda p: p['rows'][26]['after'].update(world_o=False),
            lambda p: p['rows'][28]['after'].update(water_cell=[6,64,8,9,0]),
            lambda p: p['rows'][5]['after'].update(lava=[11,5]),
            lambda p: p['rows'][10]['after']['position'].__setitem__(2, '401e000000000000'),
            lambda p: p['rows'][0]['before']['lighting'].update(depth=1),
            lambda p: p['rows'][28]['after']['lighting'].update(depth=True),
            lambda p: p['rows'][13]['after']['lighting']['regions'].reverse(),
            lambda p: p['rows'][13]['after']['lighting']['regions'][3]['bounds'].__setitem__(1,64),
            lambda p: p['rows'][23]['after']['lighting'].update(regions=[{'kind':'Sky','bounds':[7,64,8,7,65,8]}]),
            lambda p: p['rows'][0]['after']['planes'].update(blocks='0'*64),
            lambda p: p['rows'][6]['after']['planes'].update(metadata='0'*64),
            lambda p: p['rows'][25]['after']['planes'].update(height=p['rows'][0]['after']['planes']['height']),
            lambda p: p['rows'][23]['after']['planes'].update(sky=p['rows'][5]['after']['planes']['sky']),
            lambda p: p['rows'][0]['after']['planes'].update(block_light='0'*64),
            lambda p: p['rows'][16]['after'].update(sleeping=False),
            lambda p: p['rows'][16]['after'].update(timer=True),
            lambda p: p['rows'][5]['after'].update(world_entities=2),
            lambda p: p['rows'][5]['after'].update(items=[{}]),
            lambda p: p['rows'][5]['after']['world_random'].__setitem__(1,True),
            lambda p: p['rows'][5]['after']['random'][2].__setitem__(0,'0'*16),
            lambda p: p['rows'][5]['events'].pop(), lambda p: p['rows'][0]['events'].reverse(),
            lambda p: p['rows'][20]['events'][11].update(world_o=False),
            lambda p: p['rows'][1]['events'][1].update(lava=[4,1]),
            lambda p: p['rows'][5]['events'][10].update(lava=[11,5]),
            lambda p: p['rows'][13]['events'][3].update(water_cell=[6,64,8,9,0]),
            lambda p: p['rows'][0]['events'][3].update(pitch='40000000'),
            lambda p: p['rows'][0]['events'][3].update(volume='3f800000'),
            lambda p: p['rows'][0]['events'][3].update(name='random.hurt'),
            lambda p: p['rows'][0]['events'][3]['world_random'].__setitem__(0,'0'*16),
            lambda p: p['rows'][0]['events'][4]['math_random'].__setitem__(0,'0'*16),
            lambda p: p['rows'][0]['events'][4]['position'].__setitem__(1,'4050000000000000'),
            lambda p: p['rows'][5]['events'][12]['motion'].__setitem__(0,'3ff0000000000000'),
            lambda p: p['rows'][0]['events'][1].update(xyz=[7,64,8,7,64,8]),
            lambda p: p['rows'][0]['events'][3]['stack'][2].update(method='other'),
            lambda p: p['rows'][0].update(exception='java/lang/NullPointerException'),
            lambda p: p['rows'][15].update(exception_message='other'),
            lambda p: p['rows'][28].update(result=False),
            lambda p: p['rows'][28]['exception_stack'][0].update(method='a'),
            lambda p: p['rows'][22]['exception_stack'].reverse(),
        )
        for index, change in enumerate(changes):
            damaged = copy.deepcopy(original)
            change(damaged)
            with self.subTest(index=index), self.assertRaises(ValueError): W.validate_finite(damaged)

    def test_provenance_refusals(self):
        original = W.P.load(W.HERE / 'provenance.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.update(schema='LAVA_WAKE_PROVENANCE_V0'),
            lambda p: p.update(status='PASS'), lambda p: p.update(archive_sha256='0'*64),
            lambda p: p.update(finite_sha256='0'*64), lambda p: p.update(capture_sha256='x'),
            lambda p: p.update(class_count=True), lambda p: p.update(class_count=43),
            lambda p: p.update(rows_per_vm=26), lambda p: p.update(inputs_unchanged=False),
            lambda p: p['sources'].update(extra='0'*64), lambda p: p['sources'].pop('profiles.json'),
            lambda p: p['sources'].update(**{'packet.py':'0'*64}),
            lambda p: p['shared'].pop('setup'), lambda p: p['helpers'].pop('LavaWakeObservation.class'),
            lambda p: p['binaries'].update(client='0'*64), lambda p: p['methods'].pop('rp.i(Lfd;III)V'),
            lambda p: p['original'].pop(), lambda p: p['original'][0].update(exit=1),
            lambda p: p['original'][0].update(timed_out=True),
            lambda p: p['original'][0].update(pipe_capture_complete=False),
            lambda p: p['original'][0].update(output_overflow=True),
            lambda p: p['original'][0].update(stdout_bytes=0),
            lambda p: p['original'][0].update(timeout_seconds=71),
            lambda p: p['original'][1].update(stdout_sha256='0'*64),
            lambda p: p['processes']['compile'].update(exit=True),
            lambda p: p['processes']['compile'].update(stderr_bytes=1),
            lambda p: p['processes'].pop('bytecode'),
        )
        for index, change in enumerate(changes):
            damaged = copy.deepcopy(original)
            change(damaged)
            with self.subTest(index=index), self.assertRaises(ValueError): W.validate_provenance(damaged)

    def test_method_and_class_pin_refusals(self):
        original = W.P.load(W.HERE / 'lava-method-pins.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.update(archive_sha256='0'*64),
            lambda p: p['classes'].pop('sg'), lambda p: p['classes'].update(extra={}),
            lambda p: p['classes']['sg'].update(class_bytes=True),
            lambda p: p['classes']['st'].update(super='uu'),
            lambda p: p['classes']['eb']['methods'][0].update(code_length=True),
            lambda p: p['methods']['rp.i(Lfd;III)V'].update(code_length=True),
            lambda p: p['methods'].pop('st.a(IIIIII)Z'),
        )
        for index, change in enumerate(changes):
            damaged = copy.deepcopy(original)
            change(damaged)
            with self.subTest(index=index), self.assertRaises(ValueError): W.validate_class_pins(damaged)

    def test_json_transport(self):
        for text in ('{"a":0,"a":1}', '{"v":NaN}', '{"v":Infinity}', '{"v":-Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError): W.P.loads(text)

    def test_declared_effect_prefixes(self):
        cases = {p['mode']: W.model(p) for p in W.profiles()}
        for metadata in range(5):
            result = cases['lava-obsidian' if metadata == 0 else 'lava-cobblestone-meta'+str(metadata)]
            self.assertEqual(result['lava'], [49 if metadata == 0 else 4, 0])
            self.assertEqual((result['world_draws'],result['math_draws']), (2,32))
        for mode in ('lava-hot-meta5','lava-hot-meta15','lava-hot-still-water'):
            result = cases[mode]
            self.assertEqual((result['world_draws'],result['math_draws']), (4,64))
            self.assertEqual(result['schedule_requests'],2)
            self.assertEqual([e['kind'] for e in result['events']].count('sound'),2)
        for mode in ('lava-no-contact-meta0','lava-no-contact-meta5'):
            self.assertEqual((cases[mode]['world_draws'],cases[mode]['math_draws']), (0,0))
        early = cases['lava-hot-range-error']
        self.assertEqual(early['lighting'], {'depth':0,'regions':[]})
        self.assertEqual(early['heights'],{})
        self.assertTrue(early['world_o'])
        self.assertEqual(early['schedule_requests'],0)
        late = cases['lava-hot-late-range-error']
        self.assertEqual(late['schedule_requests'],1)
        self.assertTrue(late['world_o'])
        self.assertEqual((late['world_draws'],late['math_draws']), (4,64))
        nested = cases['lava-obsidian-still-water-late-range-error']
        self.assertEqual(nested['schedule_requests'],1)
        self.assertEqual(nested['water_cell'],[6,64,8,8,0])
        self.assertEqual((nested['world_draws'],nested['math_draws']), (0,0))
        self.assertEqual(W.exit_position(cases['lava-obsidian-north-water']), (7.5,9.5))
        for profile in W.profiles():
            self.assertEqual({k:len(v) for k,v in W.plane_arrays(profile,True).items()},
                             dict(blocks=32768,metadata=16384,sky=16384,block_light=16384,height=256))


if __name__ == '__main__':
    unittest.main()
