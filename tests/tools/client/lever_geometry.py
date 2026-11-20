"""Exact original Lever/Dust scene selection within ordinary compiled terrain."""
import hashlib
import importlib.util
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / 'tests/reference/client/lever_scene_geometry'


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def packet():
    spec = importlib.util.spec_from_file_location('lever_scene_packet', PACKET / 'packet.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def expected_vertices(rows, ao, sky_subtraction, state, block):
    require(type(ao) is bool and type(sky_subtraction) is int and 0 <= sky_subtraction < 16,
            'Exact AO and original 0..15 skylight subtraction operands')
    row = rows[(sky_subtraction, int(ao), state, block)]
    words = [int(word, 16) for word in row.split('\t')[-1].split(',')]
    # Compare the reached position/UV/color arrays. The final two words are
    # inactive NW backing cells; the SDK's arrays7 does not consume them.
    return [struct.pack('<6I', *words[index:index + 6]) for index in range(0, len(words), 8)]


def select(raw, vertices, name):
    require(type(raw) is bytes and len(raw) > 0 and len(raw) % 32 == 0,
            'Complete mode4/arrays7 compiled terrain bytes')
    actual = [raw[index:index + 24] for index in range(0, len(raw), 32)]
    starts = [index for index in range(len(actual) - len(vertices) + 1)
              if actual[index:index + len(vertices)] == vertices]
    require(len(starts) == 1, name + ': exactly one complete contiguous original cell geometry')
    return {'start_vertex': starts[0], 'vertices': len(vertices),
            'selected_sha256': hashlib.sha256(b''.join(vertices)).hexdigest(),
            'compared_bytes_per_vertex': 24}


def check_geometry(raw, ao, sky_subtraction, powered, present=True):
    require(type(powered) is bool and type(present) is bool and (present or not powered),
            'Closed before/off/on scene state')
    rows = packet().read_packet()
    state = -1 if not present else int(powered)
    result = {'ao': ao, 'sky_subtraction': sky_subtraction,
              'state': 'before' if state < 0 else 'on' if powered else 'off'}
    if present:
        result['lever'] = select(raw, expected_vertices(rows, ao, sky_subtraction, state, 69), 'Lever')
        require(result['lever']['vertices'] == 66, 'Five visible base faces and six handle faces')
    result['dust'] = select(raw, expected_vertices(rows, ao, sky_subtraction, state, 55), 'Dust')
    require(result['dust']['vertices'] == 12, 'Both six-vertex Dust overlays')
    if present:
        a, b = result['lever'], result['dust']
        require(a['start_vertex'] + a['vertices'] <= b['start_vertex'] or
                b['start_vertex'] + b['vertices'] <= a['start_vertex'], 'Distinct cell extents')
    return result


def check_cells(raw, powered, present=True):
    """Require the full radius2 Block/metadata/light inputs around both cells."""
    require(type(raw) is bytes and len(raw) == 82176, 'All five actual Chunk planes')
    require(type(powered) is bool and type(present) is bool and (present or not powered),
            'Closed scene input state')
    packet().read_packet()
    expected = bytearray((PACKET / 'scene-input.bin').read_bytes())

    def index(x, y, z):
        return (x << 11) | (z << 7) | y

    def nibble(buffer, offset, at):
        return (buffer[offset + at // 2] >> ((at & 1) * 4)) & 15

    def set_nibble(offset, at, value):
        mask = 15 << ((at & 1) * 4)
        expected[offset + at // 2] = ((expected[offset + at // 2] & ~mask) |
                                     (value << ((at & 1) * 4)))

    at = index(3, 65, 2)
    expected[at] = 69 if present else 0
    set_nibble(32768, at, (12 if powered else 4) if present else 0)
    set_nibble(32768, index(4, 65, 2), 15 if powered else 0)
    count = 0
    for x in range(1, 7):
        for y in range(63, 68):
            for z in range(0, 5):
                at = index(x, y, z)
                require(raw[at] == expected[at], 'Actual source Block radius2 input')
                for offset in (32768, 49152, 65536):
                    require(nibble(raw, offset, at) == nibble(expected, offset, at),
                            'Actual source metadata/SkyLight/BlockLight radius2 input')
                count += 1
    return {'cells': count, 'radius': 2, 'bounds': [1, 6, 63, 67, 0, 4],
            'plane_order': ['Blocks', 'Data', 'SkyLight', 'BlockLight', 'HeightMap'],
            'scope': 'Selected radius2 cells; HeightMap and outside cells are retained, not compared.'}
