"""Java-free byte, schema and source-identity audit of finite RU/QM tint rows."""
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
FILES = {
    'profiles.tsv': 'cc8185d7fa7041da757fc7b932a0b2519bae275dae7348f174cbccc0c299065f',
    'observations.tsv': '9638e0482c07c9fd4f16075d187bebd018579ea17a63ca9fe656cc0736ddfe04',
    'guards.tsv': 'd8988c6097b88f28352cc03d241fea1c2e06f4376fdb48c772443cceada20ffc',
    'source-pins.json': '66e2900951bb5c84b3eb990186dd9ccb3da8827ad88bcc603c371626e82eb488',
    'schema.json': '01807cf99773846c49e9400490d4b681c7a7a51fb3dc82aaa3b43afc257a91cf',
}
STDOUT = '5f1741b23900354e75920b0011816869072ddcd2fd1cb6aca7819bf668e632a3'
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
GUARDS = ['GUARD\tsource_connect\t0', 'GUARD\tsource_listen\t0', 'GUARD\tsource_link\t0',
          'GUARD\tconnect_denied\t1', 'GUARD\tlisten_denied\t1', 'GUARD\tlink_denied\t1']


def require(value, message):
    if not value:
        raise ValueError('TallGrass tint packet: ' + message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for name, value in pairs:
        require(name not in result, 'duplicate JSON key')
        result[name] = value
    return result


def text(path):
    data = Path(path).read_bytes()
    require(0 < len(data) <= 1024 * 1024 and b'\r' not in data and data.endswith(b'\n'), 'bounded LF transport')
    return data.decode('utf-8')


def load(path):
    return json.loads(text(path), object_pairs_hook=unique)


def integer(value):
    require(re.fullmatch(r'0|-?[1-9][0-9]*', value) is not None, 'canonical integer')
    result = int(value)
    require(-2147483648 <= result <= 2147483647, 'source integer range')
    return result


def profiles(lines):
    require(len(lines) == 28, 'complete input inventory')
    result = []
    for index, line in enumerate(lines):
        cells = line.split('\t')
        require(len(cells) == 9 and cells[0] == 'INPUT', 'input row shape')
        row = [integer(c) for c in cells[1:]]
        require(row[0] == index and row[1] == (1 if index % 2 == 0 else 0), 'input identity and call order')
        require(0 <= row[2] <= 15 and row[3] in (8,700,-700) and row[4] == 64 and
                row[5] in (8,700,-700) and 0 <= row[6] <= 3 and 0 <= row[7] <= 3, 'declared input domains')
        if index % 2:
            require(row[2:] == result[-1][2:], 'paired RU/QM operands')
        result.append(row)
    return result


def floating(value, width):
    require(re.fullmatch('[0-9a-f]{' + str(width) + '}', value) is not None, 'raw floating width')
    number = struct.unpack('>f' if width == 8 else '>d', bytes.fromhex(value))[0]
    require(number == number and abs(number) != float('inf'), 'finite projected floating value')


def rows(lines, inputs):
    require(len(lines) == 28, 'complete observation inventory')
    for index, line in enumerate(lines):
        cells = line.split('\t')
        require(len(cells) == 23 and cells[0] == 'TINT', 'observation row shape')
        require([integer(c) for c in cells[1:9]] == inputs[index], 'observation input binding')
        mode, metadata, fault, palette = inputs[index][1], inputs[index][2], inputs[index][6], inputs[index][7]
        expected = ('NULL' if fault == 1 or (fault in (2,3) and metadata != 0) else
                    'INDEX' if palette == 3 and metadata != 0 else
                    'NULL' if palette == 2 and metadata != 0 else 'OK')
        require(cells[9] == expected, 'source result category')
        require(re.fullmatch('[0-9a-f]{8}', cells[10]) is not None, 'full source color int')
        for value in cells[11:14]:
            floating(value, 8)
        if mode == 0 or expected != 'OK' or metadata == 0:
            require(cells[11:14] == ['3f19999a'] * 3, 'unchanged constructor RGB prefix')
        if mode == 1 or expected != 'OK':
            require(cells[10] == '12345678', 'ordinary color output sentinel')
        elif metadata == 0:
            require(cells[10] == '00ffffff', 'actual dead metadata branch')
        require(cells[14] == str(int(mode == 1 and expected == 'OK')), 'actual returned particle alias')
        require(all(c in ('0','1') for c in cells[15:17]), 'actual static array presence')
        require(cells[15] == ('0' if index < 8 or 20 <= index < 24 else '1') and
                cells[16] == ('0' if index < 10 else '1'), 'reached IA/JH array states')
        for channel in cells[17:19]:
            values = channel.split(',')
            require(len(values) == 3, 'complete retained climate array')
            for value in values:
                floating(value, 16)
        require(cells[19:23] == ['1'] * 4, 'reused climate aliases and unchanged World Random')
    return lines


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(set(provenance) == set(('schema status scope fresh_vms rows_per_vm inputs_unchanged '
            'source_inputs runtime_inputs files runs guard_scope world_scope private_capture_sha256 limits').split()),
            'exact provenance fields')
    require(re.search(r'[A-Za-z]:[\\/]|/home/|file://', json.dumps(provenance)) is None,
            'no private paths in provenance')
    require(provenance['schema'] == 1 and type(provenance['schema']) is int and
            provenance['status'] == 'PASS' and provenance['fresh_vms'] == 2 and
            provenance['rows_per_vm'] == 28 and provenance['inputs_unchanged'] is True, 'actual capture completion')
    require(set(provenance['files']) == set(FILES), 'exact pinned file inventory')
    for name, expected in FILES.items():
        data = text(directory / name)
        require(digest(directory / name) == expected and provenance['files'][name] ==
                {'bytes': len(data.encode()), 'sha256': expected}, 'byte-pinned ' + name)
    require(len(provenance['runs']) == 2, 'two fresh original VMs')
    for run in provenance['runs']:
        require(type(run['exit']) is int and run['exit'] == 0 and run['timed_out'] is False and
                run['pipe_capture_complete'] is True and run['reader_errors'] == {} and
                run['output_overflow'] is False and run['stdout_sha256'] == STDOUT and
                re.fullmatch('[0-9a-f]{64}', run['stderr_sha256']) is not None, 'bounded complete original run')
    require(len(provenance['source_inputs']) == 8 and len(provenance['runtime_inputs']) == 9 and
            provenance['runtime_inputs']['client_jar']['sha256'] == JAR, 'source/runtime role inventory')
    for record in provenance['runtime_inputs'].values():
        require(set(record) == {'bytes','sha256'} and type(record['bytes']) is int and
                record['bytes'] > 0 and re.fullmatch('[0-9a-f]{64}', record['sha256']) is not None,
                'runtime input bytes and hash')
    for name, record in provenance['source_inputs'].items():
        require(not name.startswith('/') and ':' not in name and '\\' not in name and
                all(p not in ('','.','..') for p in name.split('/')), 'repository-relative input')
        path = ROOT / name
        require(path.stat().st_size == record['bytes'] and digest(path) == record['sha256'], 'unchanged observation source')
    pins = load(directory / 'source-pins.json')
    require(pins['client_jar_sha256'] == JAR and len(pins['classes']) == 14, 'unchanged original class inventory')
    schema = load(directory / 'schema.json')
    require(schema['schema'] == 1 and len(schema['columns']) == 23 and schema['rows'] == 28, 'schema identity')
    require(text(directory / 'guards.tsv').splitlines() == GUARDS, 'zero source attempts and actual positive denials')
    prepared = profiles(text(directory / 'profiles.tsv').splitlines())
    observed = rows(text(directory / 'observations.tsv').splitlines(), prepared)
    return {'profiles': prepared, 'rows': observed, 'fresh_vms': 2}


if __name__ == '__main__':
    packet = read_packet()
    print({'rows': len(packet['rows']), 'fresh_vms': packet['fresh_vms'], 'status': 'PASS'})
