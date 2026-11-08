"""Bounded, Java-free admission and exact selected-state comparison for complete CQ calls."""
import hashlib
import json
from pathlib import Path
import re

from observation import deterministic, need

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
ORIGINAL_SOURCES = ('NightmareObservation.java', 'NightmareJdi.java', 'capture_reference.py',
                    'profiles.tsv', 'class-names.txt', 'source-pins.json')
PROCESSING_SOURCES = ('observation.py', 'packet.py', 'export_reference.py')
SOURCES = ORIGINAL_SOURCES + PROCESSING_SOURCES
SCOPE = '28 prepared complete unchanged-original CQ nightmare component calls; no natural ticks, rendered frames or sustained gameplay qualification.'


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
    need(Path(path).stat().st_size <= 8 * 1024 * 1024, 'bounded JSON member')
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
    need(len(values) == 28 and all(len(row) == 8 and row[0] == index for index, row in enumerate(values)),
         'ordered finite profile transport')
    return values


def read(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    need(set(provenance) == {'schema', 'scope', 'archive_sha256', 'original_sources', 'processing_sources', 'expected_sha256',
                            'capture', 'runs', 'summary'}, 'provenance inventory')
    need(provenance['schema'] == 'NIGHTMARE_PROVENANCE_V1' and provenance['scope'] == SCOPE and
         provenance['archive_sha256'] == JAR, 'original authority and scope')
    need(set(provenance['original_sources']) == set(ORIGINAL_SOURCES) and
         set(provenance['processing_sources']) == set(PROCESSING_SOURCES), 'separate original/current processing inventories')
    for name, value in {**provenance['original_sources'], **provenance['processing_sources']}.items():
        need(re.fullmatch('[0-9a-f]{64}', value) is not None and digest(directory / name) == value,
             'current producer identity: ' + name)
    pins = load(directory / 'source-pins.json')
    need(pins['archive'] == {'sha256': JAR, 'bytes': 1465375}, 'unchanged original archive')
    need([*pins['classes']] == (directory / 'class-names.txt').read_text(encoding='ascii').splitlines(),
         'resolved original class inventory')
    need(digest(directory / 'expected.json') == provenance['expected_sha256'], 'recorded finite output identity')
    value = load(directory / 'expected.json')
    need(set(value) == {'schema', 'scope', 'rows', 'constructor_evidence', 'search_evidence', 'exception_evidence'} and
         value['schema'] == 'NIGHTMARE_FINITE_V1' and value['scope'] == SCOPE, 'finite packet scope')
    declared = profiles(directory)
    need(len(value['rows']) == 28 and all(row['case'] == index and row['input'] == declared[index]
                                        for index, row in enumerate(value['rows'])), 'complete profile observations')
    normalized = [deterministic(row) for row in value['rows']]
    need(provenance['summary'] == summary(normalized), 'finite observed outcome summary')
    need(len(provenance['runs']) == 4 and [run['kind'] for run in provenance['runs']] ==
         ['ordinary', 'ordinary', 'debugger', 'debugger'], 'four independent actual original VMs')
    for run in provenance['runs']:
        need(run['calls'] == 28 and run['returned'] == provenance['summary']['returns'] and
             run['shared_scalars'] == scalars(normalized), 'complete original run readback')
    artifact = provenance['capture']
    need(type(artifact) is dict and set(artifact) == {'name', 'bytes', 'sha256'} and
         artifact['name'] == 'capture.json' and type(artifact['bytes']) is int and
         0 < artifact['bytes'] <= 8 * 1024 * 1024 and
         re.fullmatch('[0-9a-f]{64}', artifact['sha256']) is not None, 'closed retained capture identity')
    proof_path = directory / 'capture-receipt.json'
    need(proof_path.stat().st_size == artifact['bytes'] and digest(proof_path) == artifact['sha256'],
         'exact retained capture receipt')
    proof = load(proof_path)
    need(proof['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and
         proof['input_before_sha256'] == proof['input_after_sha256'] and
         proof['helper_before_sha256'] == proof['helper_after_sha256'] and
         proof['profiles_per_vm'] == 28 and proof['binary_sha256']['client'] == JAR and
         all(proof['public_source_sha256'][name] == pin for name, pin in provenance['original_sources'].items()),
         'unchanged actually captured original producer identity')
    return value, provenance


def summary(rows):
    return {'profiles': len(rows), 'returns': sum(row['returned'] for row in rows),
            'true': sum(row['result'] is True for row in rows),
            'false': sum(row['result'] is False for row in rows),
            'exceptions': sum(row['exception'] != 'NONE' for row in rows),
            'attempts': sum(row['counters']['attempts_started'] for row in rows),
            'searches': sum(row['counters']['paths'] for row in rows),
            'shared_scalars': scalars(rows)}


def compare(actual, directory=HERE):
    expected, _ = read(directory)
    return compare_rows(expected['rows'], actual)


def compare_rows(expected, actual):
    need(type(actual) is list and len(actual) == 28, 'complete native row inventory')
    count = 0
    for original, native in zip(expected, actual):
        count += compare_row(original, native)
    return count


def compare_row(original, native):
    value = {key: (native['exception'] if original['printer_exception'] is not None else None)
             if key == 'printer_exception' else native[key] for key in original}
    expected_value = deterministic(original)
    actual_value = deterministic(value)
    # The real original printer returns; native has no whole-return qualification here.
    if original['printer_exception'] is not None:
        need(not native['returned'] and native['print_pending'] and native['source_pc'] == 362,
             'caught printer remains explicitly pending')
        for name in ('returned', 'result', 'exception'):
            del expected_value[name]
            del actual_value[name]
    need(exact(expected_value, actual_value), 'selected original/native disagreement in case ' + str(original['case']))
    return scalars(expected_value)
