"""Java-free validation of the declared unchanged-original Dye cohort."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
ROW_KEYS = set('index name inputs channel result exception stack_before stack_after '
               'item_random_before item_random_after world_random_before world_random_after '
               'gm_b_is_world_after used_delta remote_after observer_cells notifications changes '
               'blocks_metadata_after'.split())
LENGTHS = (32768, 16384, 16384, 16384, 256)


def require(value, message):
    if not value:
        raise ValueError('Dye packet: ' + message)


def canonical(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON key ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def integer(value, low=-(1 << 31), high=(1 << 31) - 1):
    return type(value) is int and low <= value <= high


def random(value):
    require(type(value) is dict and set(value) == {'seed_bits', 'gaussian_bits', 'have_gaussian'},
            'actual Random state fields')
    require(re.fullmatch('[0-9a-f]{12}', value['seed_bits']) and
            re.fullmatch('[0-9a-f]{16}', value['gaussian_bits']) and
            type(value['have_gaussian']) is bool, 'typed Random words')


def stack(value):
    require(type(value) is list and len(value) == 4 and all(integer(v) for v in value),
            'four signed Stack words')


def initial_arrays(p):
    planes = [[bytearray(n) for n in LENGTHS] for _ in range(4)]
    for chunk in planes:
        chunk[2][:] = bytes([15]) * LENGTHS[2]
        chunk[4][:] = bytes([64]) * LENGTHS[4]

    def put(x, y, z, block, metadata=0):
        if not (0 <= x < 32 and 0 <= y < 128 and 0 <= z < 32):
            return
        chunk = planes[(x >> 4) * 2 + (z >> 4)]
        index = ((x & 15) << 11) | ((z & 15) << 7) | y
        chunk[0][index] = block
        shift = (index & 1) * 4
        chunk[1][index >> 1] = (chunk[1][index >> 1] & ~(15 << shift)) | ((metadata & 15) << shift)

    for x in range(32):
        for z in range(32):
            put(x, 63, z, 3 if p[13] == 1 else 2)
    if p[13] == 1:
        put(p[4], 63, p[6], 2)
    if p[7] == 59:
        put(p[4], p[5] - 1, p[6], 60)
    put(p[4], p[5], p[6], p[7], p[8])
    if p[13] in (2, 3):
        put(p[4], p[5] + 1, p[6], 1)
    return planes


def validate_row(row, declared, index, frozen_digest=None):
    require(type(row) is dict and set(row) == ROW_KEYS, 'closed observation schema')
    p = declared['inputs']
    require(row['index'] == index and type(row['index']) is int and row['name'] == declared['name'] and
            row['inputs'] == p and len(p) == 15, 'declared profile and row order')
    require(row['channel'] == ('direct' if p[0] == 0 else 'wrapper'), 'separate direct/wrapper channel')
    require(row['result'] in ('true', 'false', 'UNRETURNED') and
            row['exception'] in ('NONE', 'java.lang.NullPointerException', 'java.lang.IllegalStateException') and
            (row['result'] == 'UNRETURNED') == (row['exception'] != 'NONE'), 'exact return/exception outcome')
    stack(row['stack_before'])
    stack(row['stack_after'])
    require(row['stack_before'] == [p[1], 5, 351, p[2]], 'fresh declared original Stack')
    for key in ('item_random_before', 'item_random_after', 'world_random_before', 'world_random_after'):
        random(row[key])
    require(type(row['gm_b_is_world_after']) is bool and type(row['remote_after']) is bool and
            row['remote_after'] == bool(p[9]) and integer(row['used_delta'], 0, 1) and
            integer(row['observer_cells'], 0), 'typed alias/statistic/World observations')
    require(row['used_delta'] == (1 if p[0] == 1 and row['result'] == 'true' else 0),
            'real registered wrapper statistic; no direct award or failed-call award')
    require(row['gm_b_is_world_after'] == (p[14] == 2 and row['observer_cells'] > 0),
            'declared live GM.b owner alias')
    require(type(row['notifications']) is list, 'ordered callback list')
    cells = 0
    aliases = 0
    count_stores = 0
    for event in row['notifications']:
        require(type(event) is dict and type(event.get('event')) is str, 'typed callback event')
        kind = event['event']
        if kind == 'cell':
            require(set(event) == {'event', 'position', 'stack', 'item_random', 'world_random'} and
                    len(event['position']) == 3 and all(integer(v) for v in event['position']), 'actual cell callback')
            stack(event['stack'])
            random(event['item_random'])
            random(event['world_random'])
            cells += 1
        elif kind == 'range':
            require(set(event) == {'event', 'coordinates'} and len(event['coordinates']) == 6 and
                    all(integer(v) for v in event['coordinates']), 'actual range callback')
        elif kind == 'gm_b_alias_world':
            require(event == {'event': kind, 'after_cell': 1} and cells == 1 and p[14] == 2,
                    'first-cell live GM.b alias order')
            aliases += 1
        elif kind == 'authored_count_store':
            require(event == {'event': kind, 'value': 99} and cells == 1 and p[14] == 4,
                    'first-cell declared count-store order')
            count_stores += 1
        else:
            require(kind in ('sound', 'particle', 'add', 'remove', 'refresh', 'record', 'tile', 'level'),
                    'known observer interface member')
    require(cells == row['observer_cells'] and aliases == int(row['gm_b_is_world_after']) and
            count_stores == int(p[14] == 4 and cells > 0), 'complete ordered callback summary')
    require(type(row['changes']) is list and type(row['blocks_metadata_after']) is list,
            'full five-array changes and Block/metadata map')
    planes = initial_arrays(p)
    seen = set()
    previous = None
    for change in row['changes']:
        require(type(change) is list and len(change) == 5 and all(integer(v, 0) for v in change),
                'typed complete byte change')
        chunk, plane, offset, old, new = change
        require(chunk < 4 and plane < 5 and offset < LENGTHS[plane] and old <= 255 and new <= 255 and
                old != new, 'bounded changed byte')
        identity = (chunk, plane, offset)
        require(identity not in seen and (previous is None or previous < identity), 'unique source-ordered changes')
        require(planes[chunk][plane][offset] == old, 'observed old byte matches exact declared setup')
        seen.add(identity)
        previous = identity
        planes[chunk][plane][offset] = new
    coordinates = set()
    previous = None
    for cell in row['blocks_metadata_after']:
        require(type(cell) is list and len(cell) == 5 and all(integer(v, 0) for v in cell), 'typed full cell map')
        x, y, z, block, metadata = cell
        require(x < 32 and y < 128 and z < 32 and 1 <= block <= 255 and metadata <= 15,
                'bounded complete cell projection')
        identity = (x, y, z)
        order = (x, z, y)
        require(identity not in coordinates and (previous is None or previous < order), 'unique source-ordered cells')
        coordinates.add(identity)
        previous = order
        chunk = planes[(x >> 4) * 2 + (z >> 4)]
        offset = ((x & 15) << 11) | ((z & 15) << 7) | y
        require(chunk[0][offset] == block and (chunk[1][offset >> 1] >> ((offset & 1) * 4)) & 15 == metadata,
                'Block/metadata map agrees with complete observed byte changes')
    require(len(coordinates) == sum(len(c[0]) - c[0].count(0) for c in planes), 'all nonair cells represented')
    if frozen_digest is not None:
        require(canonical(row) == frozen_digest, 'unchanged frozen original row identity')


def audit(directory=HERE):
    manifest = load(directory / 'manifest.json')
    require(manifest['format'] == 1 and manifest['client_jar_sha256'] == JAR, 'verified original client identity')
    for name, value in manifest['files_sha256'].items():
        require('/' not in name and '\\' not in name and ':' not in name and
                hashlib.sha256((directory / name).read_bytes()).hexdigest() == value, 'frozen artifact bytes: ' + name)
    declared = load(directory / 'declared-inputs.json')
    transported = [{'name': line.split('\t')[0], 'inputs': list(map(int, line.split('\t')[1:]))}
                   for line in (directory / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
    require(declared['rows'] == transported and len(transported) == 39 and
            all(row['inputs'][0] == 1 for row in transported), 'separate wrapper-only declared operand transport')
    selected = (directory / 'selected.jsonl').read_text(encoding='ascii').splitlines()
    require(len(selected) == 40 and selected[-1].startswith(
        'DYE_COMPLETE rows=39 network=0 native=0 execs=0 denied-writes=0 ') and
        selected[-1].endswith(' game-code-replacements=0'), 'complete guarded original cohort')
    rows = [json.loads(line.split('\t', 1)[1]) for line in selected[:-1]]
    for index, row in enumerate(rows):
        require(selected[index].startswith('DYE\t'), 'original row prefix')
        validate_row(row, transported[index], index, manifest['row_sha256'][index])
    provenance = load(directory / 'capture-provenance.json')
    require(provenance['status'] == 'PASS' and provenance['profiles'] == 39 and provenance['ordinary_fresh_vms'] == 2 and
            provenance['inputs_unchanged'] and provenance['helpers_unchanged'] and
            provenance['inputs_before'] == provenance['inputs_after'] and
            provenance['original_game_class_replacements'] == 0 and len(provenance['runs']) == 2,
            'two completed unchanged-original VMs')
    require(re.search(r'[A-Za-z]:[\\/]|/home/|file://', json.dumps(provenance)) is None,
            'portable metadata projection')
    for repeat, run in enumerate(provenance['runs']):
        raw = (directory / ('ordinary-' + str(repeat) + '.stdout')).read_bytes()
        require(run['returncode'] == 0 and hashlib.sha256(raw).hexdigest() == run['stdout_sha256'] ==
                provenance['raw_stdout_sha256'], 'exact repeated raw ordinary stdout')
        require([line for line in raw.decode('ascii').splitlines() if line.startswith('DYE')] == selected,
                'declared LF selected-output projection')
        require(all(name == 'session.lock' or run['world_after'].get(name) == value
                    for name, value in run['world_before'].items()), 'private supplied saves unchanged except lock')
    for name in ('DyeObservation.java', 'profiles.tsv', 'declared-inputs.json', 'source-pins.json'):
        require(hashlib.sha256((directory / name).read_bytes()).hexdigest() ==
                provenance['inputs_before']['authored/' + name], 'successful frozen authored input: ' + name)
    require(hashlib.sha256((directory / 'original-capture-driver.py').read_bytes()).hexdigest() ==
            provenance['inputs_before']['authored/capture.py'], 'successful frozen capture driver identity')
    outcomes = load(directory / 'source-outcomes.json')
    actual_exceptions = [(r['index'], r['name'], r['exception']) for r in rows if r['exception'] != 'NONE']
    require(actual_exceptions == [(r['index'], r['name'], r['exception_class'])
                                  for r in outcomes['expected_exceptions']] and len(actual_exceptions) == 9,
            'all nine primary expected original exception outcomes retained')
    require(all(r['native_exception_class_equivalence'] == 'UNQUALIFIED'
                for r in outcomes['expected_exceptions']), 'no invented native exception equivalence')
    pins = load(directory / 'source-pins.json')
    require(pins['client_jar_sha256'] == JAR and len(pins['classes']) == 18, 'whole original classes and Code hash pins')
    method = [m for m in pins['classes']['km']['methods']
              if m['name'] == 'a' and m['descriptor'] == '(Liz;Lgs;Lfd;IIII)Z']
    require(len(method) == 1 and method[0]['code_bytes'] == 380 and method[0]['code_sha256'] ==
            '328115dac944a8a82bada64e04fb356129112540a4a38cce16fa80bb48021fa5', 'exact complete KM body identity')
    support = directory / 'supporting-interleaved-78'
    for name, wanted in manifest['supporting_files_sha256'].items():
        require(not name.startswith('/') and '..' not in Path(name).parts and ':' not in name and
                hashlib.sha256((support / name).read_bytes()).hexdigest() == wanted,
                'exact preserved supporting artifact: ' + name)
    spec = importlib.util.spec_from_file_location('dye_supporting_original_packet', support / 'packet.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    supporting = module.audit(support)
    require(supporting['profiles'] == 78 and supporting['expected_exceptions'] == 14 and
            supporting['native_comparisons'] == 0, 'distinct complete supporting original cohort')
    require((directory / 'DyeObservation.java').read_bytes() == (support / 'DyeObservation.java').read_bytes(),
            'identical successful helper bytes across distinct original cohorts')
    return {'profiles': 39, 'direct_calls': 0, 'stack_wrappers': 39, 'ordinary_vms': 2,
            'expected_exceptions': 9, 'supporting_interleaved_profiles': 78,
            'supporting_expected_exceptions': 14, 'total_ordinary_vms': 4,
            'native_comparisons': 0, 'read_only_jdi_runs': 0}


def read_packet(directory=HERE):
    """Return the 39 literal independently captured IZ rows after full audit."""
    directory = Path(directory)
    audit(directory)
    return [json.loads(line.split('\t', 1)[1])
            for line in (directory / 'selected.jsonl').read_text(encoding='ascii').splitlines()
            if line.startswith('DYE\t')]


if __name__ == '__main__':
    print(audit())
