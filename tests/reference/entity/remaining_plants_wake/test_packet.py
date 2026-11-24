"""Reject closed-packet, source-order, Random and ownership evidence corruptions."""
import copy
import unittest

import packet as W


class RemainingPlantsWakePacket(unittest.TestCase):
    def test_bound_original_packet(self):
        value = W.check()
        self.assertEqual(value, W.read_reference())
        self.assertEqual(len(value['rows']), 25)
        self.assertEqual(len(W.class_pins()['classes']), 38)

    def test_finite_refusals(self):
        value = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.update(schema='REMAINING_PLANTS_WAKE_V0'),
            lambda p: p.update(scope='full native parity'), lambda p: p['rows'].reverse(),
            lambda p: p['rows'].pop(), lambda p: p['rows'][0].update(extra=0),
            lambda p: p['rows'][0].update(result=0), lambda p: p['rows'][0]['prepared'].update(extra=0),
            lambda p: p['rows'][1]['prepared']['profile'].update(id=True),
            lambda p: p['rows'][1]['prepared']['profile'].update(floor=12),
            lambda p: p['rows'][4]['prepared']['profile'].update(world_seed=4660),
            lambda p: p['rows'][11]['prepared']['profile'].update(publication_error_at=1),
            lambda p: p['rows'][12]['prepared']['profile'].update(removal_error=0),
            lambda p: p['rows'][13]['prepared'].update(post_setup_sky_nibble_assignment=[7, 64, 8, 15]),
            lambda p: p['rows'][0]['prepared'].update(light_or_sky_getter_overrides=[[7, 64, 8, 15]]),
            lambda p: p['rows'][0]['receiver'].update(extra=0),
            lambda p: p['rows'][0]['receiver']['drop_item'].update(**{'class': 'uu'}),
            lambda p: p['rows'][3]['receiver']['drop_chance'].update(**{'class': 'uu'}),
            lambda p: p['rows'][14]['receiver']['survival'].update(**{'class': 'wb'}),
            lambda p: p['rows'][14]['receiver']['substrate'].update(descriptor='(Lfd;III)Z'),
            lambda p: p['rows'][0]['before'].update(sleeping=False),
            lambda p: p['rows'][0]['before']['light_inputs'].update(chunk_sky_seen_static=True),
            lambda p: p['rows'][13]['before']['light_inputs'].update(chunk_sky_seen_static=False),
            lambda p: p['rows'][13]['before']['light_inputs'].update(sky_nibble=True),
            lambda p: p['rows'][13]['after']['light_inputs'].update(height=65),
            lambda p: p['rows'][16]['after']['light_inputs'].update(world_f_sky_subtraction=0),
            lambda p: p['rows'][0]['after']['light_inputs'].update(fd_m_chunk_subtraction_literal=5),
            lambda p: p['rows'][0]['after']['light_inputs'].update(extra=0),
            lambda p: p['rows'][1]['after'].update(plant=[32, 0]),
            lambda p: p['rows'][0]['after'].update(plant=[0, 0]),
            lambda p: p['rows'][13]['after'].update(plant_floor=[0, 0]),
            lambda p: p['rows'][10]['after'].update(plant=[0, 0]),
            lambda p: p['rows'][12]['after'].update(plant=[59, 7]),
            lambda p: p['rows'][11]['after'].update(sleeping=False),
            lambda p: p['rows'][12]['after'].update(timer=True),
            lambda p: p['rows'][9]['after'].update(world_entities=2),
            lambda p: p['rows'][1]['after'].update(extra=0),
            lambda p: p['rows'][4]['after']['items'].append(copy.deepcopy(p['rows'][15]['after']['items'][0])),
            lambda p: p['rows'][9]['after']['items'].pop(),
            lambda p: p['rows'][7]['after']['items'][0].update(stack=[295, 1, 0]),
            lambda p: p['rows'][7]['after']['items'][1].update(stack=[296, 1, 0]),
            lambda p: p['rows'][15]['after']['items'][0].update(stack=[40, 1, 0]),
            lambda p: p['rows'][15]['after']['items'][0].update(delay=True),
            lambda p: p['rows'][15]['after']['items'][0].update(position=['0' * 16] * 3),
            lambda p: p['rows'][15]['after']['items'][0].update(phase='0' * 8),
            lambda p: p['rows'][15]['after']['items'][0].update(motion=['0' * 16] * 3),
            lambda p: p['rows'][15]['after']['items'][0].update(inserted=False),
            lambda p: p['rows'][9]['after']['world_random'].__setitem__(0, '0' * 16),
            lambda p: p['rows'][9]['after']['random'][1].__setitem__(0, '0' * 16),
            lambda p: p['rows'][9]['after']['random'][2].__setitem__(0, '0' * 16),
            lambda p: p['rows'][13]['after']['player_random'].__setitem__(1, True),
            lambda p: p['rows'][4]['before']['planes'].update(sky='0' * 64),
            lambda p: p['rows'][4]['after']['planes'].update(metadata='0' * 64),
            lambda p: p['rows'][9]['events'].reverse(),
            lambda p: p['rows'][9]['events'][1].update(plant=[0, 0]),
            lambda p: p['rows'][9]['events'][2].update(floor=[60, 0]),
            lambda p: p['rows'][9]['events'][2]['item'].update(stack=[296, 1, 0]),
            lambda p: p['rows'][9]['events'][1]['stack'][0].update(line=True),
            lambda p: p['rows'][9]['events'][1]['stack'][0].update(extra=0),
            lambda p: p['rows'][12]['events'].pop(),
            lambda p: p['rows'][12].update(exception='NONE'),
            lambda p: p['rows'][11].update(exception_message='other'),
            lambda p: p['rows'][11]['exception_stack'][0].update(**{'class': 'OtherObserver'}),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_finite(damaged)

    def test_provenance_refusals(self):
        value = W.P.load(W.HERE / 'provenance.json')
        changes = (
            lambda p: p.update(extra=0), lambda p: p.pop('schema'),
            lambda p: p.update(schema='REMAINING_PLANTS_WAKE_PROVENANCE_V0'),
            lambda p: p.update(status='PASS_NATIVE'), lambda p: p.update(class_count=True),
            lambda p: p.update(class_count=37), lambda p: p.update(rows_per_vm=24),
            lambda p: p.update(inputs_unchanged=1), lambda p: p.update(helpers_unchanged=False),
            lambda p: p.update(class_names_unchanged=False), lambda p: p.update(class_pins_unchanged=0),
            lambda p: p.update(finite_sha256='0' * 64), lambda p: p.update(capture_sha256='invalid'),
            lambda p: p['sources'].pop('RemainingPlantsWakeObservation.java'),
            lambda p: p['sources'].update(extra='0' * 64),
            lambda p: p['shared'].update(family_finite='0' * 64),
            lambda p: p['helpers'].update(extra='0' * 64),
            lambda p: p['helpers'].pop('RemainingPlantsWakeObservation$Profile.class'),
            lambda p: p['binaries'].update(client='0' * 64),
            lambda p: p['methods']['tl.g(Lfd;III)Z'].update(code_length=True),
            lambda p: p['methods']['ni.a(Lfd;IIIIF)V'].update(code_sha256='0' * 64),
            lambda p: p['original'].pop(), lambda p: p['original'][0].update(extra=0),
            lambda p: p['original'][0].update(exit=False), lambda p: p['original'][0].update(exit=1),
            lambda p: p['original'][0].update(timeout_seconds=True),
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
            lambda p: p['processes'].pop('compile'),
            lambda p: p['processes'].update(extra={}),
            lambda p: p['processes']['compile'].update(exit=True),
            lambda p: p['processes']['java'].update(timeout_seconds=70),
            lambda p: p['processes']['bytecode'].update(pipe_capture_complete=False),
            lambda p: p['processes']['compile'].update(stderr_bytes=1),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_provenance(damaged)

    def test_class_and_method_bindings(self):
        self.assertEqual(len(W.class_pins()['classes']), 38)
        for key, size in (('jb.d(I)Z', 16), ('jb.a(ILjava/util/Random;)I', 2),
                          ('ni.a(Lfd;IIIIF)V', 171), ('ni.a(ILjava/util/Random;)I', 15),
                          ('tl.g(Lfd;III)Z', 49), ('tl.d(I)Z', 6), ('gm.<clinit>()V', 2668)):
            with self.subTest(key=key):
                self.assertEqual(W.method_pins()[key]['code_length'], size)
        value = W.P.load(W.HERE / 'remaining-plant-pins.json')
        changes = (lambda p: p.update(extra=0), lambda p: p.update(archive_sha256='0' * 64),
                   lambda p: p['classes'].pop('gm'), lambda p: p['classes'].update(extra={}),
                   lambda p: p['classes']['jb'].update(class_bytes=True),
                   lambda p: p['classes']['ni'].update(super='uu'),
                   lambda p: p['classes']['tl']['methods'][0].update(code_length=True),
                   lambda p: p['classes']['gm']['fields'][0].update(access=False),
                   lambda p: p['classes']['ni']['methods'][0].update(name='other'),
                   lambda p: p['methods']['tl.g(Lfd;III)Z'].update(code_length=True),
                   lambda p: p['methods'].pop('gm.<clinit>()V'))
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(value)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_class_pins(damaged)

    def test_json_transport(self):
        for text in ('{"a":0,"a":1}', '{"v":NaN}', '{"v":Infinity}', '{"v":-Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                W.P.loads(text)

    def test_declared_outcomes_and_early_stop(self):
        cases = {p['mode']: W.model(p) for p in W.profiles()}
        self.assertEqual(cases['dead-bush-unsupported']['draws'], 1)
        self.assertEqual(cases['dead-bush-unsupported']['items'], [])
        for label in ('1', '1234', '7fffffff'):
            self.assertEqual(cases['crop-unsupported-stage0-seed' + label]['draws'], 4)
            self.assertEqual(cases['crop-unsupported-stage0-seed' + label]['items'], [])
        self.assertEqual(len(cases['crop-unsupported-stage7-seed1']['items']), 2)
        self.assertEqual(len(cases['crop-unsupported-stage7-seed1234']['items']), 2)
        self.assertEqual(len(cases['crop-unsupported-stage7-seed7fffffff']['items']), 4)
        self.assertEqual(cases['crop-publication-error-first']['draws'], 4)
        # One chance draw, three Wheat offsets, a rejected Seed selector,
        # the accepted Seed selector and its three offsets precede publication.
        self.assertEqual(cases['crop-publication-error-second']['draws'], 9)
        self.assertTrue(cases['crop-publication-error-second']['publication_error'])
        self.assertTrue(cases['crop-removal-error']['removal_error'])


if __name__ == '__main__':
    unittest.main()
