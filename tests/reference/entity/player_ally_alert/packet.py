"""Bounded portable admission for unchanged-original Player ally-alert evidence."""
import hashlib
import json
from pathlib import Path
import re

from observation import need

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SCOPE = '40 prepared complete unchanged-original calls: 32 Player damage calls and eight direct ally-alert calls; no ticks, frames, loaded combat, original presentation or FPS qualification.'
ORIGINAL_SOURCES = ('PlayerAllyAlertObservation.java', 'PlayerAllyAlertJdi.java', 'capture_reference.py',
                    'profiles.tsv', 'class-names.txt', 'source-pins.json')
PROCESSING_SOURCES = ('observation.py', 'packet.py', 'export_reference.py')
SOURCES = ORIGINAL_SOURCES + PROCESSING_SOURCES


def digest(path):
    path = Path(path)
    need(path.stat().st_size <= 8 * 1024 * 1024, 'bounded packet member')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unique(items):
    result = {}
    for key, value in items:
        need(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def loads(text):
    return json.loads(text, object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError('nonfinite JSON')))


def load(path):
    digest(path)
    return loads(Path(path).read_text(encoding='utf-8'))


def exact(left, right):
    if type(left) is not type(right):
        return False
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(exact(left[key], right[key]) for key in left)
    if isinstance(left, list):
        return len(left) == len(right) and all(exact(a, b) for a, b in zip(left, right))
    return left == right


def scalars(value):
    if isinstance(value, dict):
        return sum(scalars(item) for item in value.values())
    if isinstance(value, list):
        return sum(scalars(item) for item in value)
    return 1


def profiles(directory=HERE):
    values = [[int(word) for word in line.split()] for line in (directory / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
    need(len(values) == 40 and all(len(row) == 8 and row[0] == index and row[1] in (0, 1, 2)
                                  and 0 <= row[2] <= 7 and 0 <= row[3] <= 20 for index, row in enumerate(values)), 'declared ordered profiles')
    return values


def summary(rows):
    return {'calls': len(rows), 'damage_calls': sum(row['input'][1] == 0 for row in rows),
            'direct_calls': sum(row['input'][1] != 0 for row in rows),
            'normal_returns': sum(row['exception'] == 'NONE' for row in rows),
            'damage_true': sum(row['result'] is True for row in rows),
            'damage_false': sum(row['result'] is False for row in rows),
            'exceptions': sum(row['exception'] != 'NONE' for row in rows), 'selected_scalars': scalars(rows)}


def read(directory=HERE):
    directory = Path(directory)
    proof = load(directory / 'provenance.json')
    need(type(proof) is dict and set(proof) == {'schema', 'scope', 'archive_sha256', 'original_sources',
         'processing_sources', 'finite_sha256', 'capture_sha256', 'runs', 'summary'}, 'closed provenance')
    need(proof['schema'] == 'PLAYER_ALLY_ALERT_PROVENANCE_V1' and proof['scope'] == SCOPE and
         proof['archive_sha256'] == JAR, 'original authority/scope')
    for name, expected in (('original_sources', ORIGINAL_SOURCES), ('processing_sources', PROCESSING_SOURCES)):
        need(type(proof[name]) is dict and set(proof[name]) == set(expected), 'separate source inventories')
        for path, pin in proof[name].items():
            need(type(pin) is str and re.fullmatch('[0-9a-f]{64}', pin) is not None and digest(directory / path) == pin, 'bound source ' + path)
    need(digest(directory / 'capture-receipt.json') == proof['capture_sha256'], 'exact historical capture receipt')
    capture = load(directory / 'capture-receipt.json')
    need(capture['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and capture['profiles_per_vm'] == 40 and
         capture['input_before_sha256'] == capture['input_after_sha256'] and
         capture['helper_before_sha256'] == capture['helper_after_sha256'] and capture['binary_sha256']['client'] == JAR and
         all(capture['public_source_sha256'][name] == pin for name, pin in proof['original_sources'].items()), 'immutable actual original producers')
    pins = load(directory / 'source-pins.json')
    need(pins['archive'] == {'sha256': JAR, 'bytes': 1465375} and
         list(pins['classes']) == (directory / 'class-names.txt').read_text(encoding='ascii').splitlines(), 'original class inventory')
    need(digest(directory / 'finite.json') == proof['finite_sha256'], 'recorded finite output identity')
    value = load(directory / 'finite.json')
    need(type(value) is dict and set(value) == {'schema', 'scope', 'rows', 'traces', 'entropy'} and
         value['schema'] == 'PLAYER_ALLY_ALERT_FINITE_V1' and value['scope'] == SCOPE, 'finite scope')
    rows = value['rows']
    need(type(rows) is list and len(rows) == 40, 'complete original row inventory')
    for index, (row, profile) in enumerate(zip(rows, profiles(directory))):
        need(type(row['case']) is int and row['case'] == index and exact(row['input'], profile), 'row/profile identity')
        need(row['exception'] in ('NONE', 'java/lang/NullPointerException', 'java/lang/ClassCastException') and
             (type(row['result']) is bool if row['exception'] == 'NONE' and profile[1] == 0 else row['result'] is None), 'actual source outcome')
    need(exact(summary(rows), proof['summary']) and len(value['traces']) == len(value['entropy']) == 2,
         'actual finite count/evidence inventory')
    need(len(proof['runs']) == 4 and [row['kind'] for row in proof['runs']] == ['ordinary', 'ordinary', 'debugger', 'debugger'], 'four actual original VMs')
    return value, proof


def compare_rows(expected, actual):
    need(type(actual) is list and len(actual) == len(expected) == 40, 'complete transport inventory')
    for index, (left, right) in enumerate(zip(expected, actual)):
        compare_row(left, right)
    return scalars(expected)


def compare_row(expected, actual):
    need(exact(expected, actual), 'selected original disagreement in case ' + str(expected['case']))
    return scalars(expected)
