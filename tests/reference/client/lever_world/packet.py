"""Read finite original Lever rendering evidence without executing Java."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_packet(directory=HERE):
    provenance = json.loads((directory / 'provenance.json').read_text(encoding='ascii'))
    assert provenance['status'] == 'PASS' and provenance['calls_per_vm'] == 32
    assert provenance['ordinary_vms'] == 2 and provenance['same_selected_rows'] is True
    assert digest(directory / 'selected.txt') == provenance['selected_sha256']
    for name, pin in provenance['captured_helpers_sha256'].items():
        assert digest(directory / name) == pin, 'captured helper changed: ' + name
    pins = json.loads((directory / 'source-pins.json').read_text(encoding='ascii'))
    assert pins['jar_sha256'] == 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
    assert len(pins['classes_sha256']) == 12
    rows = (directory / 'selected.txt').read_text(encoding='ascii').splitlines()
    assert len(rows) == 32
    for index, row in enumerate(rows):
        fields = row.split('\t')
        metadata, override = index // 2, -1 if index % 2 == 0 else 17
        assert len(fields) == 9 and fields[:7] == ['LEVER', str(metadata), str(override),
            'true', str(override), 'false', '8']
        assert re.fullmatch('[0-9a-f]{16}(,[0-9a-f]{16}){5}', fields[7])
        assert re.fullmatch('[0-9a-f]{8}(,[0-9a-f]{8}){575}', fields[8])
    return rows
