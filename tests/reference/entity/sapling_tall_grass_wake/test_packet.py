"""Validate actual PlantFamily records and refuse damaged packets without Java."""
import copy
import unittest

import packet as W


class PlantFamilyWakePacket(unittest.TestCase):
    def test_bound_packet(self):
        value = W.check()
        self.assertEqual(value, W.read_reference())
        self.assertEqual(value['schema'], W.SCHEMA)

    def test_damaged_finite(self):
        value = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p.update(extra=0),
            lambda p: p.update(schema='SAPLING_TALL_GRASS_WAKE_V0'),
            lambda p: p.update(scope='native parity complete'),
            lambda p: p['rows'].reverse(),
            lambda p: p['rows'].pop(),
            lambda p: p['rows'][0].update(extra=0),
            lambda p: p['rows'][0].update(result=0),
            lambda p: p['rows'][0]['prepared'].update(extra=0),
            lambda p: p['rows'][0]['prepared']['shared_setup'].__setitem__(2, True),
            lambda p: p['rows'][1]['prepared']['post_setup_cell_assignments'][1].__setitem__(3, 2),
            lambda p: p['rows'][0]['prepared'].update(light_or_sky_overrides=[[7, 64, 8, 15]]),
            lambda p: p['rows'][0]['before'].update(sleeping=False),
            lambda p: p['rows'][1]['after'].update(timer=True),
            lambda p: p['rows'][1]['after'].update(sleeping=True),
            lambda p: p['rows'][0]['after'].update(plant=[0, 0]),
            lambda p: p['rows'][1]['after'].update(plant=[37, 0]),
            lambda p: p['rows'][2]['after'].update(plant=[0, 0]),
            lambda p: p['rows'][0]['after'].update(plant_floor=[0, 0]),
            lambda p: p['rows'][1]['after'].update(plant_floor=[False, 0]),
            lambda p: p['rows'][0]['after'].update(foot=[26, 16]),
            lambda p: p['rows'][0]['after'].update(unselected=0),
            lambda p: p['rows'][0]['after'].update(position=['0' * 16] * 3),
            lambda p: p['rows'][2]['after'].update(position=['0' * 16] * 3),
            lambda p: p['rows'][0]['after'].update(dimensions=['0' * 8] * 3),
            lambda p: p['rows'][0]['after']['light_inputs'].update(extra=0),
            lambda p: p['rows'][0]['after']['light_inputs'].update(sky_nibble=True),
            lambda p: p['rows'][0]['after']['light_inputs'].update(sky_nibble=14),
            lambda p: p['rows'][0]['after']['light_inputs'].update(block_light_nibble=1),
            lambda p: p['rows'][0]['after']['light_inputs'].update(height=65),
            lambda p: p['rows'][0]['after']['light_inputs'].update(world_f_sky_subtraction=0),
            lambda p: p['rows'][0]['after']['light_inputs'].update(fd_m_chunk_subtraction_literal=5),
            lambda p: p['rows'][0]['after']['light_inputs'].update(chunk_sky_seen_static=False),
            lambda p: p['rows'][0]['before']['light_inputs'].update(chunk_sky_seen_static=0),
            lambda p: p['rows'][0]['after']['items'].append(p['rows'][1]['after']['items'][0]),
            lambda p: p['rows'][1]['after']['items'][0].update(delay=True),
            lambda p: p['rows'][1]['after']['items'][0].update(stack=[6, True, 2]),
            lambda p: p['rows'][1]['after']['items'][0].update(stack=[50, 1, 0]),
            lambda p: p['rows'][1]['after']['items'][0].update(inserted=False),
            lambda p: p['rows'][1]['after']['items'][0].update(removed=True),
            lambda p: p['rows'][1]['after']['items'][0].update(extra=0),
            lambda p: p['rows'][1]['after']['items'][0].update(motion=[0, 0, 0]),
            lambda p: p['rows'][0]['after'].update(world_random=['00002f08d408505d', False, '0' * 16]),
            lambda p: p['rows'][1]['after'].update(player_random=p['rows'][1]['after']['world_random']),
            lambda p: p['rows'][1]['after']['random'][1].__setitem__(1, 0),
            lambda p: p['rows'][1]['after']['planes'].update(sky='0' * 64),
            lambda p: p['rows'][1]['events'].reverse(),
            lambda p: p['rows'][1]['events'][0].update(extra=0),
            lambda p: p['rows'][1]['events'][1].update(plant=[0, 0]),
            lambda p: p['rows'][1]['events'][1].update(floor=[2, 0]),
            lambda p: p['rows'][1]['events'][1]['stack'][0].update(line='0'),
            lambda p: p['rows'][1]['events'][1]['stack'][0].update(extra=0),
            lambda p: p['rows'][1]['events'][1]['stack'][7].update(method='wrong'),
            lambda p: p['rows'][2].update(exception='java/lang/NullPointerException'),
            lambda p: p['rows'][2].update(exception_message='other'),
            lambda p: p['rows'][2]['exception_stack'][0].update(**{'class': 'other'}),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_finite(damaged)

    def test_damaged_provenance(self):
        value = W.P.load(W.HERE / 'provenance.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.pop('schema'),
            lambda p: p.update(schema='SAPLING_TALL_GRASS_WAKE_PROVENANCE_V0'),
            lambda p: p.update(status='PASS_NATIVE'), lambda p: p.update(class_count=True),
            lambda p: p.update(rows_per_vm=2), lambda p: p.update(inputs_unchanged=1),
            lambda p: p.update(helpers_unchanged=False), lambda p: p.update(class_names_unchanged=False),
            lambda p: p.update(class_pins_unchanged=1), lambda p: p.update(finite_sha256='0' * 64),
            lambda p: p.update(capture_sha256='invalid'),
            lambda p: p['sources'].pop('PlantFamilyWakeObservation.java'),
            lambda p: p['sources'].update(extra='0' * 64), lambda p: p['shared'].update(setup='0' * 64),
            lambda p: p['helpers'].update(extra='0' * 64),
            lambda p: p['helpers'].update({'PlantFamilyWakeObservation.class': 'invalid'}),
            lambda p: p['binaries'].update(client='0' * 64), lambda p: p['binaries'].update(java='invalid'),
            lambda p: p['methods']['wb.b(Lfd;IIII)V'].update(code_length=True),
            lambda p: p['methods']['fd.m(III)I'].update(code_sha256='0' * 64),
            lambda p: p['original'].pop(), lambda p: p['original'][0].update(extra=0),
            lambda p: p['original'][0].update(exit=False), lambda p: p['original'][0].update(exit=1),
            lambda p: p['original'][0].update(timeout_seconds=True),
            lambda p: p['original'][0].update(timeout_seconds=120),
            lambda p: p['original'][0].update(timed_out=True),
            lambda p: p['original'][0].update(pipe_capture_complete=False),
            lambda p: p['original'][0].update(output_overflow=True),
            lambda p: p['original'][0].update(reader_errors={'stdout': ['lost']}),
            lambda p: p['original'][0].update(command_sha256='invalid'),
            lambda p: p['original'][0].update(stdout_file='../outside'),
            lambda p: p['original'][0].update(stdout_bytes=True),
            lambda p: p['original'][0].update(stdout_bytes=0),
            lambda p: p['original'][0].update(stdout_limit=1024),
            lambda p: p['original'][0].update(stderr_bytes=1048577),
            lambda p: p['original'][0].update(stdout_sha256='invalid'),
            lambda p: p['original'][1].update(stdout_sha256='0' * 64),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_provenance(damaged)

    def test_reached_method_bindings(self):
        pins = W.method_pins()
        for name, size in (('wb.b(Lfd;IIII)V', 21), ('wb.h(Lfd;III)V', 40),
                           ('wb.g(Lfd;III)Z', 47), ('wb.d(I)Z', 36), ('fd.m(III)I', 40),
                           ('fd.l(III)Z', 23), ('lm.c(IIII)I', 54), ('wi.a(III)I', 54), ('he.b_(I)I', 4), ('ru.a(ILjava/util/Random;)I', 18),
                           ('uu.a(ILjava/util/Random;)I', 5), ('uu.b_(I)I', 2)):
            with self.subTest(name=name):
                self.assertEqual(pins[name]['code_length'], size)
        self.assertEqual(len(W.class_pins()['classes']), 34)

    def test_closed_class_records(self):
        value = W.class_pins()['classes']['wb']
        changes = (lambda p: p.update(extra=0), lambda p: p.update(class_bytes=True),
                   lambda p: p.update(super='vm'), lambda p: p['fields'].append({'access': True}),
                   lambda p: p['methods'][0].update(access=False),
                   lambda p: p['methods'][0].update(code_length=True))
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.class_record(damaged, 'wb', 1313,
                        'c843c6873acbc51586f64468ab2635341caf44109f7ae4ec410a414dca3543da', 'uu')

    def test_json_transport(self):
        for value in ('{"schema":0,"schema":1}', '{"value":NaN}',
                      '{"value":Infinity}', '{"value":-Infinity}'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                W.P.loads(value)

    def test_family_semantics(self):
        value = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p['rows'].__setitem__(7, copy.deepcopy(p['rows'][4])),
            lambda p: p['rows'][6].update(mode='tallgrass-unsupported-seed7fffffff'),
            lambda p: p['rows'][7]['prepared']['profile'].update(world_seed=1),
            lambda p: p['rows'][7]['prepared'].update(world_random_reseed=True),
            lambda p: p['rows'][3]['prepared']['profile'].update(observer_error=0),
            lambda p: p['rows'][0]['receiver'].update(extra=0),
            lambda p: p['rows'][0]['receiver'].update(**{'class': 'wb'}),
            lambda p: p['rows'][4]['receiver']['drop_item'].update(**{'class': 'uu'}),
            lambda p: p['rows'][1]['receiver']['drop_metadata'].update(**{'class': 'uu'}),
            lambda p: p['rows'][4]['receiver']['drop_item'].update(descriptor='(I)I'),
            lambda p: p['rows'][4]['receiver']['drop_item'].update(extra=0),
            lambda p: p['rows'][2]['prepared']['profile'].update(observer_error=False),
            lambda p: p['rows'][1]['after']['items'][0].update(stack=[6, 1, 0]),
            lambda p: p['rows'][4]['after']['items'][0].update(stack=[31, 1, 1]),
            lambda p: p['rows'][4]['after']['items'][0].update(stack=[295, 1, True]),
            lambda p: p['rows'][4]['after']['items'][0].update(stack=[295, 2, 0]),
            lambda p: p['rows'][7]['after']['items'].append(copy.deepcopy(p['rows'][4]['after']['items'][0])),
            lambda p: p['rows'][7]['events'].insert(1, copy.deepcopy(p['rows'][4]['events'][1])),
            lambda p: p['rows'][6]['after'].update(items=[]),
            lambda p: p['rows'][4]['after'].update(plant=[31, 1]),
            lambda p: p['rows'][5]['after'].update(plant=[0, 0]),
            lambda p: p['rows'][5]['after'].update(world_sleeping=False),
            lambda p: p['rows'][5].update(result=False),
            lambda p: p['rows'][7].update(exception='java/lang/IllegalStateException'),
            lambda p: p['rows'][5]['events'].append(copy.deepcopy(p['rows'][4]['events'][2])),
            lambda p: p['rows'][4]['events'][1].update(plant=[0, 0]),
            lambda p: p['rows'][7]['events'][1].update(plant=[31, 1]),
            lambda p: p['rows'][4]['events'][1]['item'].update(stack=[6, 1, 2]),
            lambda p: p['rows'][6]['after']['world_random'].__setitem__(0, W.RANDOM_AFTER[4]),
            lambda p: p['rows'][7]['after']['world_random'].__setitem__(0, W.RANDOM_AFTER[6]),
            lambda p: p['rows'][4]['after']['random'][2].__setitem__(0, '0000000000000000'),
            lambda p: p['rows'][3]['before']['light_inputs'].update(chunk_sky_seen_static=False),
            lambda p: p['rows'][5]['exception_stack'][0].update(**{'class': 'InventedObserver'}),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_finite(damaged)

    def test_added_class_bindings(self):
        value = W.P.load(W.HERE / 'plant-family-pins.json')
        W.validate_class_pins(value)
        changes = (
            lambda p: p.update(extra=0), lambda p: p.update(archive_sha256='0' * 64),
            lambda p: p['classes'].pop('he'), lambda p: p['classes'].update(extra={}),
            lambda p: p['classes']['ru'].update(class_bytes=True),
            lambda p: p['classes']['he'].update(super='uu'),
            lambda p: p['classes']['he']['methods'][0].update(name='invented'),
            lambda p: p['classes']['ru']['methods'][0].update(access=False),
            lambda p: p['classes']['ru']['methods'][0].update(code_length=True),
            lambda p: p['classes']['ru']['methods'][0].update(code_sha256='0' * 64),
            lambda p: p['methods']['he.b_(I)I'].update(code_length=True),
            lambda p: p['methods']['ru.a(ILjava/util/Random;)I'].update(code_sha256='0' * 64),
            lambda p: p['methods'].pop('uu.b_(I)I'),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_class_pins(damaged)

    def test_all_process_receipts(self):
        proof = W.P.load(W.HERE / 'provenance.json')
        changes = (
            lambda p: p['processes'].pop('compile'),
            lambda p: p['processes'].update(extra={}),
            lambda p: p['processes']['compile'].update(exit=True),
            lambda p: p['processes']['java'].update(timeout_seconds=70),
            lambda p: p['processes']['bytecode'].update(pipe_capture_complete=False),
            lambda p: p['processes']['compile'].update(timed_out=True),
            lambda p: p['processes']['javac'].update(output_overflow=True),
            lambda p: p['processes']['javap'].update(reader_errors={'stderr': ['lost']}),
            lambda p: p['processes']['bytecode'].update(stdout_file='../bytecode.stdout'),
            lambda p: p['processes']['compile'].update(stdout_bytes=False),
            lambda p: p['processes']['compile'].update(stderr_bytes=1),
            lambda p: p['processes']['compile'].update(stdout_sha256='0' * 64),
            lambda p: p['processes']['java'].update(stderr_limit=1048577),
            lambda p: p['methods']['ru.a(ILjava/util/Random;)I'].update(code_length=17),
            lambda p: p['shared'].update(flower_packet='0' * 64),
            lambda p: p['helpers'].pop('PlantFamilyWakeObservation$Profile.class'),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(proof)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_provenance(damaged)


if __name__ == '__main__':
    unittest.main()
