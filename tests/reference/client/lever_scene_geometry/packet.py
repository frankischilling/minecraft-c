"""Validate original scene rows and the independently authored plane inputs."""
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
PLANE_LENGTHS = {'Blocks': 32768, 'Data': 16384, 'SkyLight': 16384,
                 'BlockLight': 16384, 'HeightMap': 256}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_packet(directory=HERE):
    receipt = json.loads((directory / 'provenance.json').read_text(encoding='ascii'))
    assert receipt['status'] == 'PASS' and receipt['ordinary_vms'] == 2
    assert receipt['calls_per_vm'] == 160 and receipt['same_selected_rows'] is True
    assert receipt['nw_input']['translation_bits'] == [
        '0000000000000000', 'c050000000000000', '0000000000000000']
    assert receipt['nw_input']['normal'] is False and receipt['nw_input']['normal_word'] == '00000000'
    assert receipt['nw_input']['compiled_invocation'] == 'nw.b(DDD)V'
    assert digest(directory / 'selected.txt') == receipt['selected_sha256']
    for name, pin in receipt['captured_helpers_sha256'].items():
        assert digest(directory / name) == pin, 'captured input changed: ' + name
    pins = json.loads((directory / 'source-pins.json').read_text(encoding='ascii'))
    assert pins['jar_sha256'] == 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
    assert len(pins['classes_sha256']) == 13
    planes = (directory / 'scene-input.bin').read_bytes()
    origin = json.loads((directory / 'scene-input-origin.json').read_text(encoding='ascii'))
    assert len(planes) == sum(PLANE_LENGTHS.values()) == 82176
    offset = 0
    for name, size in PLANE_LENGTHS.items():
        assert hashlib.sha256(planes[offset:offset + size]).hexdigest() == origin['planes_sha256'][name]
        offset += size
    expected = [(light, ao, state, block) for light in range(16) for ao in range(2)
                for state in (-1, 0, 1) for block in ((55,) if state < 0 else (69, 55))]
    rows = (directory / 'selected.txt').read_text(encoding='ascii').splitlines()
    assert len(rows) == len(expected) == 160
    for row, key in zip(rows, expected):
        light, ao, state, block = key
        fields = row.split('\t')
        assert len(fields) == 11 and fields[:9] == ['SCENE', str(light), str(ao), str(state),
            str(block), 'true', '-1', 'false', '8' if block == 69 else '0']
        assert re.fullmatch('[0-9a-f]{16}(,[0-9a-f]{16}){5}', fields[9])
        words = 528 if block == 69 else 96
        assert re.fullmatch('[0-9a-f]{8}(,[0-9a-f]{8}){' + str(words - 1) + '}', fields[10])
        packed = fields[10].split(',')
        # Calibrate the declared section translation independently of the
        # generated receipt. Integer Java literals previously selected the
        # normal overload instead of b(DDD); both faults must be rejected.
        assert all(word == '00000000' for index, word in enumerate(packed) if index % 8 >= 6)
        for word in packed[1::8]:
            y = struct.unpack('<f', struct.pack('<I', int(word, 16)))[0]
            assert y == 1.015625 if block == 55 else 1.0 <= y <= 2.0
    return dict(zip(expected, rows))
