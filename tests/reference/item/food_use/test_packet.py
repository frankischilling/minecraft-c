import copy
import unittest

import packet


class FoodPacket(unittest.TestCase):
    def test_frozen_inputs_and_ordinary_rows(self):
        self.assertEqual(len(packet.validate()), 28)

    def test_row_transport_rejects_changed_identity_and_width(self):
        rows = packet.validate()[:14]
        for column, value in [(0, 'CONTROLLERFOOD'), (1, '99'), (3, '2147483648'),
                              (10, '1'), (11, 'java.lang.IllegalStateException')]:
            changed = copy.deepcopy(rows)
            changed[0][column] = value
            with self.assertRaises(ValueError):
                packet.rows('\n'.join('\t'.join(r) for r in changed), 'FOOD')


if __name__ == '__main__':
    unittest.main()
