"""Validate frozen Food operands, ordinary observations and source identities."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def require(value, message):
    if not value:
        raise ValueError(message)


def rows(text, prefix):
    selected = [line.split('\t') for line in text.splitlines() if line.startswith(prefix + '\t')]
    width = 14 if prefix == 'FOOD' else 15
    require(len(selected) == 14, 'fourteen closed original profiles')
    for index, row in enumerate(selected):
        require(len(row) == width and row[0] == prefix and row[1] == str(index),
                'typed row identity/order/width')
        for field in [*range(1, 10), width - 2]:
            require(re.fullmatch('-?[0-9]+', row[field]) is not None and
                    -(1 << 31) <= int(row[field]) < (1 << 31), 'signed Java scalar')
        require(row[10] in ('true', 'false') and row[-1] in ('true', 'false') and
                row[-3] in ('NONE', 'java.lang.NullPointerException'), 'closed outcome transport')
        if prefix == 'CONTROLLERFOOD':
            require(row[11] in ('true', 'false', 'UNRETURNED'), 'controller return value')
    return selected


def validate(directory=HERE):
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    require(manifest['format'] == 1 and manifest['client_jar_sha256'] == JAR, 'pinned client archive')
    for name, wanted in manifest['files_sha256'].items():
        require(hashlib.sha256((directory / name).read_bytes()).hexdigest() == wanted,
                'changed frozen packet file: ' + name)
    inputs = json.loads((directory / 'declared-inputs.json').read_text(encoding='utf-8'))
    profiles = [list(map(int, line.split('\t')))
                for line in (directory / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
    require(inputs['rows'] == profiles and len(profiles) == 14 and
            all(len(p) == 7 and p[0] in (260, 282, 297, 319, 320, 322, 349, 350, 357) and
                p[5] in (0, 1) and p[6] in (0, 1) for p in profiles), 'closed operand inventory')
    require(inputs['world_argument'] is None and inputs['damage'] == -(1 << 31) and
            inputs['animation'] == 9 and inputs['controller_selected'] == 2,
            'declared shared source operands')
    expected = []
    for role, prefix in [('wrapper', 'FOOD'), ('controller', 'CONTROLLERFOOD')]:
        text = (directory / (role + '.tsv')).read_text(encoding='ascii')
        observed = rows(text, prefix)
        require(len(text.splitlines()) == 15 and re.fullmatch(
            prefix + r'_COMPLETE rows=14 network=0 native=0 execs=0 denied-writes=0 '
            r'game-code-replacements=0 world-writes=[0-9]+', text.splitlines()[-1]) is not None,
            'ordinary completion and external-effect guard')
        raw = (directory / (role + '.stdout')).read_text(encoding='ascii')
        require('\n'.join(line for line in raw.splitlines() if line.startswith(prefix)) + '\n' == text,
                'raw ordinary output selection')
        provenance = json.loads((directory / (role + '-provenance.json')).read_text())
        require(provenance['status'] == 'PASS' and provenance['inputs_unchanged'] and
                provenance['ordinary_fresh_vms'] == 2 and provenance['profiles'] == 14 and
                provenance['client_jar_sha256'] == JAR and
                len(set(provenance['stdout_sha256'])) == 1 and
                hashlib.sha256((directory / (role + '.stdout')).read_bytes()).hexdigest() ==
                provenance['stdout_sha256'][0], 'two completed unchanged ordinary VMs')
        for index, row in enumerate(observed):
            require(int(row[2]) == profiles[index][0] and
                    (row[-1] == 'true') == bool(profiles[index][6]), 'profile/output association')
        expected.extend(observed)
    pins = json.loads((directory / 'source-pins.json').read_text())
    require(pins['client_jar_sha256'] == JAR and len(pins['classes']) == 12,
            'separate whole-class and Code identities')
    for owner, name, descriptor in [('yw', 'a', '(Liz;Lfd;Lgs;)Liz;'),
                                     ('bz', 'a', '(Liz;Lfd;Lgs;)Liz;'), ('ls', 'c', '(I)V'),
                                     ('iz', 'a', '(Lfd;Lgs;)Liz;'), ('ob', 'a', '(Lgs;Lfd;Liz;)Z')]:
        methods = [method for method in pins['classes'][owner]['methods']
                   if method['name'] == name and method['descriptor'] == descriptor]
        require(len(methods) == 1 and methods[0]['code_bytes'] > 0 and
                re.fullmatch('[0-9a-f]{64}', methods[0]['code_sha256']), 'selected method Code pin')
    return expected
