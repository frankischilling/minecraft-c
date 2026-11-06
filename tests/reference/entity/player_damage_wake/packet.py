"""Strict, Java-free reader for the finite sleeping-Player damage packet."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SCHEMA = 'PLAYER_DAMAGE_WAKE_V1'
SCOPE = '32 prepared unchanged-original CPU component calls; no ticks, frames, natural sleep or sustained gameplay qualification.'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
SOURCES = ('PlayerDamageWakeObservation.java', 'capture_reference.py', 'packet.py', 'profiles.tsv', 'source-pins.json')
STATE_KEYS = {'position', 'motion', 'dimensions', 'sleep_offsets', 'sleeping', 'timer', 'bed', 'spawn', 'private_coordinate',
              'bounds', 'original_bounds', 'world_same', 'bounds_same', 'bed_same', 'spawn_same', 'private_same',
              'health', 'previous_health', 'hurt_resistance', 'hurt_time', 'hurt_maximum', 'previous_damage', 'idle',
              'attacked', 'removed', 'armor_remainder', 'world_sleeping', 'remote', 'world_players', 'world_entities',
              'head', 'planes', 'random', 'statistics', 'sounds'}


def require(condition, message):
    if not condition: raise ValueError(message)


def digest(path):
    path = Path(path)
    require(path.stat().st_size <= 8 * 1024 * 1024, 'bounded packet member')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def loads(text):
    return json.loads(text, object_pairs_hook=pairs,
                      parse_constant=lambda word: (_ for _ in ()).throw(ValueError('nonfinite JSON number: ' + word)))


def load(path):
    path = Path(path)
    require(path.stat().st_size <= 8 * 1024 * 1024, 'bounded JSON member')
    return loads(path.read_text(encoding='utf-8'))


def exact(left, right):
    if type(left) is not type(right): return False
    if isinstance(left, dict): return left.keys() == right.keys() and all(exact(left[key], right[key]) for key in left)
    if isinstance(left, list): return len(left) == len(right) and all(exact(a, b) for a, b in zip(left, right))
    return left == right


def scalars(value):
    if isinstance(value, dict): return sum(scalars(item) for item in value.values())
    if isinstance(value, list): return sum(scalars(item) for item in value)
    return 1


def integer(value): return type(value) is int and -(1 << 31) <= value < (1 << 31)
def word(value, digits): return type(value) is str and re.fullmatch('[0-9a-f]{' + str(digits) + '}', value) is not None


def vector(value, count, item):
    return type(value) is list and len(value) == count and all(item(v) for v in value)


def profiles():
    value = [[int(word) for word in line.split()] for line in (HERE / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
    require(len(value) == 32 and all(len(row) == 5 and row[0] == i and 0 <= row[1] < 4 and
            row[3] in (0, 4) and 0 <= row[4] <= 13 for i, row in enumerate(value)), 'profile transport')
    return value


def state(value):
    require(type(value) is dict and value.keys() == STATE_KEYS, 'selected state fields')
    for name in ('position', 'motion'): require(vector(value[name], 3, lambda v: word(v, 16)), name)
    require(vector(value['dimensions'], 3, lambda v: word(v, 8)) and
            vector(value['sleep_offsets'], 2, lambda v: word(v, 8)), 'Player binary32 fields')
    for name in ('bed', 'spawn', 'private_coordinate'):
        require(value[name] is None or vector(value[name], 3, integer), 'coordinate ' + name)
    for name in ('bounds', 'original_bounds'):
        require(value[name] is None or vector(value[name], 6, lambda v: word(v, 16)), 'bounds ' + name)
    for name in ('sleeping', 'world_same', 'bounds_same', 'bed_same', 'spawn_same', 'private_same', 'attacked', 'removed', 'world_sleeping', 'remote'):
        require(type(value[name]) is bool, 'Boolean ' + name)
    for name in ('timer', 'health', 'previous_health', 'hurt_resistance', 'hurt_time', 'hurt_maximum', 'previous_damage', 'idle', 'armor_remainder', 'world_players', 'world_entities'):
        require(integer(value[name]), 'int ' + name)
    require(vector(value['head'], 2, integer) and 0 <= value['head'][0] <= 255 and 0 <= value['head'][1] < 16, 'Bed cell')
    require(type(value['planes']) is dict and set(value['planes']) == {'blocks', 'metadata', 'sky', 'block_light', 'height'} and
            all(word(v, 64) for v in value['planes'].values()), 'complete Chunk plane digests')
    require(vector(value['random'], 3, lambda v: vector(v, 3, lambda item: True)), 'Randoms')
    for random in value['random']:
        require(word(random[0], 16) and int(random[0], 16) < (1 << 48) and type(random[1]) is bool and word(random[2], 16), 'Random fields')
    statistics = value['statistics']
    require(type(statistics) is dict and set(statistics) == {'total', 'unsent', 'dirty'} and type(statistics['dirty']) is bool, 'statistic maps')
    for name in ('total', 'unsent'):
        rows = statistics[name]
        require(type(rows) is list and all(vector(row, 2, integer) for row in rows) and
                all(rows[i][0] < rows[i+1][0] for i in range(len(rows)-1)), 'ordered statistic entries')
    require(type(value['sounds']) is list and len(value['sounds']) <= 1, 'sound count')
    for sound in value['sounds']:
        require(type(sound) is dict and set(sound) == {'name', 'position', 'volume', 'pitch'} and sound['name'] == 'random.hurt' and
                vector(sound['position'], 3, lambda v: word(v, 16)) and word(sound['volume'], 8) and word(sound['pitch'], 8), 'sound event')


def validate_rows(rows):
    require(type(rows) is list and len(rows) == 32, 'original call count')
    for index, (row, profile) in enumerate(zip(rows, profiles())):
        require(type(row) is dict and set(row) == {'schema', 'case', 'input', 'result', 'exception', 'exception_frames', 'before', 'after'} and
                row['schema'] == 'PLAYER_DAMAGE_WAKE_ROW_V1' and type(row['case']) is int and row['case'] == index and
                exact(row['input'], profile), 'row identity')
        require(row['exception'] in ('NONE', 'java/lang/NullPointerException') and
                (type(row['result']) is bool if row['exception'] == 'NONE' else row['result'] is None), 'actual call outcome')
        require(type(row['exception_frames']) is list and len(row['exception_frames']) <= 8 and
                all(type(name) is str and re.fullmatch('[A-Za-z0-9_$.]+', name) for name in row['exception_frames']) and
                (len(row['exception_frames']) == 0) == (row['exception'] == 'NONE'), 'actual exception stack')
        state(row['before']); state(row['after'])


def fingerprints(value, names):
    require(type(value) is dict and set(value) == set(names) and all(word(v, 64) for v in value.values()), 'closed SHA256 inventory')


def provenance(proof):
    names = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'public_source_sha256', 'support_sha256',
             'binary_sha256', 'helper_sha256', 'original', 'rows_per_vm', 'returns_per_vm', 'exceptions_per_vm',
             'selected_scalars_per_vm', 'input_unchanged', 'helpers_unchanged'}
    require(type(proof) is dict and set(proof) == names, 'closed provenance')
    require(proof['schema'] == 'PLAYER_DAMAGE_WAKE_PROVENANCE_V1' and
            proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == JAR, 'provenance status/archive')
    for name, count in (('rows_per_vm', 32), ('returns_per_vm', 25), ('exceptions_per_vm', 7), ('selected_scalars_per_vm', 4794)):
        require(type(proof[name]) is int and proof[name] == count, 'typed observation count ' + name)
    require(proof['input_unchanged'] is True and proof['helpers_unchanged'] is True, 'captured input/helper preservation')
    require(word(proof['finite_sha256'], 64), 'finite packet hash shape')
    fingerprints(proof['public_source_sha256'], SOURCES)
    fingerprints(proof['support_sha256'], ('capture.py', 'classfile.py', 'ConstructorJson.java'))
    fingerprints(proof['binary_sha256'], ('client', 'java', 'javac', *LIBRARIES))
    require(proof['binary_sha256']['client'] == JAR and all(proof['binary_sha256'][name] == pin for name, pin in LIBRARIES.items()), 'original SDK/archive fingerprints')
    fingerprints(proof['helper_sha256'], ('ConstructorJson.class', 'PlayerDamageWakeObservation.class',
        'PlayerDamageWakeObservation$Guard.class', 'PlayerDamageWakeObservation$Observer.class'))
    receipts = proof['original']
    require(type(receipts) is list and len(receipts) == 2, 'two fresh original VM receipts')
    receipt_keys = {'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete', 'reader_errors', 'output_overflow',
                    'stdout_file', 'stderr_file', 'stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit'}
    for index, receipt in enumerate(receipts):
        require(type(receipt) is dict and set(receipt) == receipt_keys and type(receipt['exit']) is int and receipt['exit'] == 0 and
                type(receipt['timeout_seconds']) is int and receipt['timeout_seconds'] == 120 and
                receipt['timed_out'] is False and receipt['pipe_capture_complete'] is True and receipt['reader_errors'] == {} and
                receipt['output_overflow'] is False, 'complete bounded original capture')
        for stream, limit in (('stdout', 4194304), ('stderr', 1048576)):
            require(type(receipt[stream + '_limit']) is int and receipt[stream + '_limit'] == limit and
                    type(receipt[stream + '_bytes']) is int and 0 < receipt[stream + '_bytes'] <= limit and
                    receipt[stream + '_file'] == 'original-' + str(index) + '.' + stream and
                    word(receipt[stream + '_sha256'], 64), 'original captured stream extent/hash')


def read_reference(directory=HERE):
    directory = Path(directory)
    proof = load(directory / 'provenance.json')
    provenance(proof)
    require(digest(directory / 'finite.json') == proof['finite_sha256'], 'finite packet digest')
    for name, expected in proof['public_source_sha256'].items(): require(digest(directory / name) == expected, 'executed source ' + name)
    support = {'capture.py': HERE.parents[1] / 'client/controller/capture.py',
               'classfile.py': HERE.parent / 'player/classfile.py', 'ConstructorJson.java': HERE.parent / 'player/ConstructorJson.java'}
    for name, expected in proof['support_sha256'].items(): require(digest(support[name]) == expected, 'executed support ' + name)
    value = load(directory / 'finite.json')
    require(type(value) is dict and set(value) == {'schema', 'scope', 'rows'} and value['schema'] == SCHEMA and value['scope'] == SCOPE, 'packet schema/scope')
    validate_rows(value['rows'])
    require(sum(row['exception'] == 'NONE' for row in value['rows']) == proof['returns_per_vm'] and
            sum(row['exception'] != 'NONE' for row in value['rows']) == proof['exceptions_per_vm'] and
            sum(scalars(row) for row in value['rows']) == proof['selected_scalars_per_vm'], 'actual selected observation counts')
    return value
