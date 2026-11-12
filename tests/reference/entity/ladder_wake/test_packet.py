"""Exercise actual schema and damaged finite observations in both Python modes."""
import copy
import unittest

import packet as W


class LadderWakePacket(unittest.TestCase):
    def test_bound_packet(self):
        W.check()

    def test_damaged_observations(self):
        finite = W.P.load(W.HERE / 'finite.json')
        changes = (
            lambda p: p.update(extra=0),
            lambda p: p.update(schema='LADDER_WAKE_V0'),
            lambda p: p['rows'].reverse(),
            lambda p: p['rows'][0].update(result=0),
            lambda p: p['rows'][0]['after'].update(timer=True),
            lambda p: p['rows'][0]['after'].update(sleeping=True),
            lambda p: p['rows'][1]['after'].update(ladder=[0, 0]),
            lambda p: p['rows'][0]['after'].update(unselected=0),
            lambda p: p['rows'][0]['after']['items'][0].update(delay=True),
            lambda p: p['rows'][0]['after']['items'][0].update(stack=[65, True, 0]),
            lambda p: p['rows'][0]['after']['items'][0].update(inserted=False),
            lambda p: p['rows'][0]['after'].update(world_random=['f' * 16, False, '0' * 16]),
            lambda p: p['rows'][0]['after'].update(player_random=p['rows'][0]['after']['world_random']),
            lambda p: p['rows'][0]['events'].reverse(),
            lambda p: p['rows'][0]['events'][0].update(extra=0),
            lambda p: p['rows'][0]['events'][1].update(ladder=[0, 0]),
            lambda p: p['rows'][0]['events'][1]['stack'][0].update(line='0'),
            lambda p: p['rows'][0]['events'][2]['stack'].pop(5),
            lambda p: p['rows'][0]['events'][1].update(sleeping=False),
            lambda p: p['rows'][1].update(exception_stack=[]),
        )
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                damaged = copy.deepcopy(finite)
                change(damaged)
                with self.assertRaises(ValueError):
                    W.validate_finite(damaged)

    def test_json_transport(self):
        for value in ('{"schema":0,"schema":1}', '{"value":NaN}', '{"value":Infinity}'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                W.P.loads(value)


if __name__ == '__main__':
    unittest.main()
