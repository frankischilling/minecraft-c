"""Validate original records and reject damaged packets without Java or asserts."""
import copy
import unittest

import packet as W


class TorchWakePacket(unittest.TestCase):
    def test_bound_packet(self):
        value = W.check()
        self.assertEqual(value, W.read_reference())
        self.assertEqual(value['schema'], W.SCHEMA)

    def test_damaged_finite(self):
        value = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p.update(extra=0),
            lambda p: p.update(schema='TORCH_WAKE_V0'),
            lambda p: p.update(scope='native parity complete'),
            lambda p: p['rows'].reverse(),
            lambda p: p['rows'].pop(),
            lambda p: p['rows'][0].update(extra=0),
            lambda p: p['rows'][0].update(result=0),
            lambda p: p['rows'][0]['before'].update(sleeping=False),
            lambda p: p['rows'][0]['after'].update(timer=True),
            lambda p: p['rows'][0]['after'].update(sleeping=True),
            lambda p: p['rows'][1]['after'].update(torch=[0, 0]),
            lambda p: p['rows'][0]['after'].update(torch_support=[1, 0]),
            lambda p: p['rows'][0]['after'].update(torch_floor=[2, 0]),
            lambda p: p['rows'][0]['after'].update(torch_floor=[True, 0]),
            lambda p: p['rows'][0]['after'].update(foot=[26, 16]),
            lambda p: p['rows'][0]['after'].update(unselected=0),
            lambda p: p['rows'][0]['after'].update(position=['0' * 16] * 3),
            lambda p: p['rows'][1]['after'].update(position=['0' * 16] * 3),
            lambda p: p['rows'][0]['after'].update(dimensions=['0' * 8] * 3),
            lambda p: p['rows'][0]['after']['items'][0].update(delay=True),
            lambda p: p['rows'][0]['after']['items'][0].update(stack=[50, True, 0]),
            lambda p: p['rows'][0]['after']['items'][0].update(stack=[65, 1, 0]),
            lambda p: p['rows'][0]['after']['items'][0].update(inserted=False),
            lambda p: p['rows'][0]['after']['items'][0].update(removed=True),
            lambda p: p['rows'][0]['after']['items'][0].update(extra=0),
            lambda p: p['rows'][0]['after']['items'][0].update(motion=[0, 0, 0]),
            lambda p: p['rows'][0]['after'].update(world_random=['f' * 16, False, '0' * 16]),
            lambda p: p['rows'][0]['after'].update(player_random=p['rows'][0]['after']['world_random']),
            lambda p: p['rows'][0]['after']['random'][1].__setitem__(1, 0),
            lambda p: p['rows'][0]['events'].reverse(),
            lambda p: p['rows'][0]['events'][0].update(extra=0),
            lambda p: p['rows'][0]['events'][1].update(torch=[0, 0]),
            lambda p: p['rows'][0]['events'][1].update(west=[1, 0]),
            lambda p: p['rows'][0]['events'][1].update(floor=[2, 0]),
            lambda p: p['rows'][0]['events'][1]['stack'][0].update(line='0'),
            lambda p: p['rows'][0]['events'][1]['stack'][0].update(line=True),
            lambda p: p['rows'][0]['events'][2]['stack'].pop(5),
            lambda p: p['rows'][0]['events'][1].update(sleeping=False),
            lambda p: p['rows'][1].update(exception_stack=[]),
            lambda p: p['rows'][1].update(exception='NONE'),
            lambda p: p['rows'][1].update(exception_message='different callback'),
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
            lambda p: p.update(extra=0),
            lambda p: p.pop('schema'),
            lambda p: p.update(schema='TORCH_WAKE_PROVENANCE_V0'),
            lambda p: p.update(status='PASS_NATIVE'),
            lambda p: p.update(class_count=True),
            lambda p: p.update(rows_per_vm=3),
            lambda p: p.update(inputs_unchanged=1),
            lambda p: p.update(helpers_unchanged=False),
            lambda p: p.update(class_names_unchanged=False),
            lambda p: p.update(class_pins_unchanged=1),
            lambda p: p.update(finite_sha256='0' * 64),
            lambda p: p.update(capture_sha256='invalid'),
            lambda p: p['sources'].pop('TorchWakeObservation.java'),
            lambda p: p['sources'].update(extra='0' * 64),
            lambda p: p['sources'].update(packet_py='0' * 64),
            lambda p: p['shared'].update(setup='0' * 64),
            lambda p: p['helpers'].update(extra='0' * 64),
            lambda p: p['helpers'].update({'TorchWakeObservation.class': 'invalid'}),
            lambda p: p['binaries'].update(client='0' * 64),
            lambda p: p['binaries'].update(java='invalid'),
            lambda p: p['methods']['vm.b(Lfd;IIII)V'].update(code_length=True),
            lambda p: p['methods']['vm.h(Lfd;III)Z'].update(code_sha256='0' * 64),
            lambda p: p['original'].pop(),
            lambda p: p['original'][0].update(extra=0),
            lambda p: p['original'][0].update(exit=False),
            lambda p: p['original'][0].update(exit=1),
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
        for name in ('vm.b(Lfd;IIII)V', 'vm.i(Lfd;III)Z', 'vm.a(Lfd;III)Z', 'vm.h(Lfd;III)Z'):
            self.assertIn(name, pins)
        self.assertEqual(pins['vm.b(Lfd;IIII)V']['code_length'], 169)
        self.assertEqual(len(W.class_pins()['classes']), 31)

    def test_json_transport(self):
        for value in ('{"schema":0,"schema":1}', '{"value":NaN}', '{"value":Infinity}', '{"value":-Infinity}'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                W.P.loads(value)


if __name__ == '__main__':
    unittest.main()
