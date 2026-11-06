"""Strict unchanged-original UQ/SW/NH component packet, without draw qualification."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[4]
FOLDER = 'tests/reference/client/gui_ingame_chat'
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SCOPE = ('Original UQ/SW/NH add/translate/tick/clear components; prepared original kq App and '
         'prepared original SJ widths7/space4; authored ASCII FP and lang resources. No complete '
         'App/Font construction, rendering, loaded World, respawn or gameplay qualification.')
COMPLETION = 'CHAT_ORIGINAL vm=1 network=0 natives=0 writes=0 prepared-app=1 prepared-font=1 draw=0'
SOURCES = ('ChatObservation.java', 'capture_reference.py', 'packet.py', 'source-pins.json',
           'inputs/font.txt', 'inputs/lang/en_US.lang', 'inputs/lang/stats_US.lang',
           '../controller/capture.py', '../controller/classfile.py')
COUNTS = (1, 1, 1, 1, 2, 3, 2, 1, 1, 1, 1, 50, 2, 0)
MODS = (1, 3, 5, 7, 10, 14, 17, 19, 21, 23, 25, 78, 81, 82)


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate packet key')
        result[key] = value
    return result


def read(path):
    require(path.is_file() and path.stat().st_size <= 1048576, 'bounded chat packet file')
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique,
                      parse_constant=lambda value: require(False, 'finite packet JSON'))


def closed(value, keys, message):
    require(type(value) is dict and set(value) == set(keys), message)


def validate_rows(rows):
    require(type(rows) is list and len(rows) == 81 and all(type(row) is str for row in rows),
            '81 original chat rows')
    index = 0
    for profile, count in enumerate(COUNTS):
        capacity = 10 if profile < 11 else 73
        h, j = (0, 0) if profile < 12 else (1, 1)
        require(rows[index] == f'CHAT_CASE {profile} {count} {capacity} {MODS[profile]} {h} {j}',
                'exact original chat case order/List state')
        index += 1
        for line in range(count):
            age = (-2147483648 if line == 0 else 200) if profile == 12 else 0
            match = re.fullmatch(rf'CHAT_LINE {profile} {line} {age} (null|text) (-|(?:[0-9a-f]{{4}}){{1,256}})', rows[index])
            require(match is not None and (match[1] != 'null' or match[2] == '-'),
                    'original UTF16/age line shape')
            index += 1
    require(index == 81, 'closed original chat row extent')
    return rows


def source_digests(root=ROOT):
    directory = root / FOLDER
    return {name: digest(directory / name) for name in SOURCES}


def load(root=ROOT):
    directory = root / FOLDER
    finite, proof = read(directory / 'finite.json'), read(directory / 'provenance.json')
    closed(finite, ('format', 'client_jar_sha256', 'scope', 'rows'), 'closed chat finite packet')
    require(type(finite['format']) is int and finite['format'] == 1 and
            finite['client_jar_sha256'] == JAR and finite['scope'] == SCOPE, 'chat finite identity/scope')
    rows = validate_rows(finite['rows'])
    closed(proof, ('schema', 'client_jar_sha256', 'scope', 'rows', 'finite_sha256',
                   'source_sha256', 'helper_sha256', 'tools', 'vms', 'input_unchanged',
                   'helper_unchanged'), 'closed original chat proof')
    require(proof['schema'] == 'GUI_INGAME_CHAT_CAPTURE_V1' and
            proof['client_jar_sha256'] == JAR and proof['scope'] == SCOPE and
            type(proof['rows']) is int and proof['rows'] == 81, 'chat proof identity/scope')
    require(proof['finite_sha256'] == digest(directory / 'finite.json'), 'chat finite byte pin')
    require(proof['source_sha256'] == source_digests(root), 'chat observer/source/resource pins')
    require(proof['input_unchanged'] is True and proof['helper_unchanged'] is True,
            'original chat input/helper continuity')
    closed(proof['helper_sha256'], ('ChatObservation.class', 'ChatObservation$1.class'), 'closed compiled chat helpers')
    closed(proof['tools'], ('java', 'javac'), 'closed original JDK tools')
    require(all(type(pin) is str and re.fullmatch('[0-9a-f]{64}', pin)
                for pin in (*proof['helper_sha256'].values(), *proof['tools'].values())),
            'exact original helper/tool digests')
    expected = ('\n'.join([*rows, COMPLETION]) + '\n').encode('ascii')
    stdout_sha = hashlib.sha256(expected).hexdigest()
    require(type(proof['vms']) is list and len(proof['vms']) == 2, 'two original VMs')
    for vm in proof['vms']:
        closed(vm, ('exit', 'timed_out', 'output_overflow', 'pipe_capture_complete',
                    'stdout_bytes', 'stdout_sha256', 'stderr_bytes', 'stderr_sha256'),
               'closed original VM receipt')
        require(type(vm['exit']) is int and vm['exit'] == 0 and vm['timed_out'] is False and
                vm['output_overflow'] is False and vm['pipe_capture_complete'] is True and
                type(vm['stdout_bytes']) is int and vm['stdout_bytes'] == len(expected) and
                vm['stdout_sha256'] == stdout_sha and type(vm['stderr_bytes']) is int and
                0 <= vm['stderr_bytes'] <= 1048576 and type(vm['stderr_sha256']) is str and
                re.fullmatch('[0-9a-f]{64}', vm['stderr_sha256']), 'complete original component VM')
    return rows
