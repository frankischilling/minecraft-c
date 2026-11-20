"""Validate actual Flower records and refuse damaged packets without Java."""
import copy
import unittest

import packet as W


class FlowerWakePacket(unittest.TestCase):
    def test_bound_packet(self):
        value = W.check()
        self.assertEqual(value, W.read_reference())
        self.assertEqual(value['schema'], W.SCHEMA)

    def test_damaged_finite(self):
        value = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p.update(extra=0),
            lambda p: p.update(schema='FLOWER_WAKE_V0'),
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
            lambda p: p['rows'][0]['after'].update(flower=[0, 0]),
            lambda p: p['rows'][1]['after'].update(flower=[37, 0]),
            lambda p: p['rows'][2]['after'].update(flower=[0, 0]),
            lambda p: p['rows'][0]['after'].update(flower_floor=[0, 0]),
            lambda p: p['rows'][1]['after'].update(flower_floor=[False, 0]),
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
            lambda p: p['rows'][1]['after']['items'][0].update(stack=[37, True, 0]),
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
            lambda p: p['rows'][1]['events'][1].update(flower=[0, 0]),
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
            lambda p: p.update(schema='FLOWER_WAKE_PROVENANCE_V0'),
            lambda p: p.update(status='PASS_NATIVE'), lambda p: p.update(class_count=True),
            lambda p: p.update(rows_per_vm=2), lambda p: p.update(inputs_unchanged=1),
            lambda p: p.update(helpers_unchanged=False), lambda p: p.update(class_names_unchanged=False),
            lambda p: p.update(class_pins_unchanged=1), lambda p: p.update(finite_sha256='0' * 64),
            lambda p: p.update(capture_sha256='invalid'),
            lambda p: p['sources'].pop('FlowerWakeObservation.java'),
            lambda p: p['sources'].update(extra='0' * 64), lambda p: p['shared'].update(setup='0' * 64),
            lambda p: p['helpers'].update(extra='0' * 64),
            lambda p: p['helpers'].update({'FlowerWakeObservation.class': 'invalid'}),
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
                           ('fd.l(III)Z', 23), ('lm.c(IIII)I', 54), ('wi.a(III)I', 54)):
            with self.subTest(name=name):
                self.assertEqual(pins[name]['code_length'], size)
        self.assertEqual(len(W.class_pins()['classes']), 32)

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


if __name__ == '__main__':
    unittest.main()
