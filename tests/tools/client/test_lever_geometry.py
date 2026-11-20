"""Original geometry selection and independent scene operand corruption controls."""
import unittest

import lever_geometry as geometry


def batch(vertices):
    # Inactive NW backing words are deliberately distinct from the original;
    # the advertised arrays7 consumes only position, UV and packed color.
    return b''.join(vertex + bytes.fromhex('aabbccdd11223344') for vertex in vertices)


def fixture(ao=True, light=0, state=0):
    rows = geometry.packet().read_packet()
    lever = [] if state < 0 else geometry.expected_vertices(rows, ao, light, state, 69)
    dust = geometry.expected_vertices(rows, ao, light, state, 55)
    return bytes(32) + batch(lever) + bytes(64) + batch(dust) + bytes(32)


def cells(powered=False, present=True):
    raw = bytearray((geometry.PACKET / 'scene-input.bin').read_bytes())
    at = (3 << 11) | (2 << 7) | 65
    raw[at] = 69 if present else 0
    raw[32768 + at // 2] = (raw[32768 + at // 2] & 15) | (
        ((12 if powered else 4) if present else 0) << 4)
    at = (4 << 11) | (2 << 7) | 65
    raw[32768 + at // 2] = (raw[32768 + at // 2] & 15) | ((15 if powered else 0) << 4)
    return bytes(raw)


class LeverGeometryTests(unittest.TestCase):
    def test_complete_grid(self):
        for ao in (False, True):
            for light in range(16):
                for state in (-1, 0, 1):
                    result = geometry.check_geometry(fixture(ao, light, state), ao, light,
                                                     state > 0, state >= 0)
                    self.assertEqual(result['dust']['vertices'], 12)
                    if state >= 0:
                        self.assertEqual(result['lever']['vertices'], 66)
                        self.assertEqual(result['lever']['start_vertex'], 1)
                    self.assertEqual(result['dust']['start_vertex'], 3 if state < 0 else 69)

    def test_changed_vertex_words(self):
        raw = fixture()
        for vertex in (1, 10, 31, 66, 69, 80):
            for word in range(6):
                broken = bytearray(raw)
                broken[vertex * 32 + word * 4] ^= 1
                with self.subTest(vertex=vertex, word=word), self.assertRaises(AssertionError):
                    geometry.check_geometry(bytes(broken), True, 0, False)

    def test_duplicate_cell(self):
        raw = fixture()
        lever = geometry.expected_vertices(geometry.packet().read_packet(), True, 0, 0, 69)
        with self.assertRaises(AssertionError):
            geometry.check_geometry(raw + batch(lever), True, 0, False)

    def test_wrong_profile(self):
        raw = fixture()
        for ao, light, powered in ((False, 0, False), (True, 1, False), (True, 0, True)):
            with self.subTest(ao=ao, light=light, powered=powered), self.assertRaises(AssertionError):
                geometry.check_geometry(raw, ao, light, powered)
        with self.assertRaises(AssertionError):
            geometry.check_geometry(raw[:-1], True, 0, False)

    def test_source_operands(self):
        for powered, present in ((False, False), (False, True), (True, True)):
            raw = cells(powered, present)
            self.assertEqual(geometry.check_cells(raw, powered, present)['cells'], 150)
            at = (2 << 11) | (1 << 7) | 65
            for offset in (at, 32768 + at // 2, 49152 + at // 2, 65536 + at // 2):
                broken = bytearray(raw)
                broken[offset] ^= 16 if offset != at else 1
                with self.subTest(offset=offset), self.assertRaises(AssertionError):
                    geometry.check_cells(bytes(broken), powered, present)
            outside = bytearray(raw)
            outside[(15 << 11) | (15 << 7)] ^= 1
            self.assertEqual(geometry.check_cells(bytes(outside), powered, present)['cells'], 150)


if __name__ == '__main__':
    unittest.main()
