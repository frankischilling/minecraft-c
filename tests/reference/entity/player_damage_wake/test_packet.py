"""Exercise packet admission and comparison discrimination without launching Java or native."""
import copy
import hashlib
from pathlib import Path
import tempfile

import packet as P
import run_native as R

checks = 0


def rejected(action, label):
    global checks
    try:
        action()
    except (ValueError, OSError):
        checks += 1
        return
    raise ValueError('accepted authored corruption: ' + label)


def changed(value):
    if value is None: return False
    if type(value) is bool: return not value
    if type(value) is int: return value + 1
    if type(value) is str: return value + 'x'
    if type(value) is list:
        result = copy.deepcopy(value)
        if result: result[0] = changed(result[0])
        else: result.append(None)
        return result
    result = copy.deepcopy(value)
    name = next(iter(result)); result[name] = changed(result[name])
    return result


def main():
    reference = P.read_reference()
    proof = P.load(P.HERE / 'provenance.json')
    for name in proof:
        corrupt = copy.deepcopy(proof); del corrupt[name]
        rejected(lambda corrupt=corrupt: P.provenance(corrupt), 'missing provenance ' + name)
    for name in ('rows_per_vm', 'returns_per_vm', 'exceptions_per_vm', 'selected_scalars_per_vm',
                 'input_unchanged', 'helpers_unchanged', 'schema', 'status', 'archive_sha256'):
        corrupt = copy.deepcopy(proof); corrupt[name] = changed(corrupt[name])
        rejected(lambda corrupt=corrupt: P.provenance(corrupt), 'changed provenance ' + name)
    for name in ('rows_per_vm', 'returns_per_vm', 'exceptions_per_vm', 'selected_scalars_per_vm'):
        corrupt = copy.deepcopy(proof); corrupt[name] = True
        rejected(lambda corrupt=corrupt: P.provenance(corrupt), 'Boolean observation count ' + name)
    for index in range(2):
        for name in ('exit', 'timed_out', 'pipe_capture_complete', 'output_overflow', 'stdout_bytes', 'stderr_bytes'):
            corrupt = copy.deepcopy(proof)
            corrupt['original'][index][name] = (0 if name.endswith('_bytes') else changed(corrupt['original'][index][name]))
            rejected(lambda corrupt=corrupt: P.provenance(corrupt), 'incomplete original receipt ' + name)
    for text in ('{"x":0,"x":1}', '{"x":NaN}', '{"x":Infinity}', '{"x":-Infinity}'):
        rejected(lambda text=text: P.loads(text), 'invalid JSON')
    baseline = reference['rows']
    for key in baseline[0]['before']:
        corrupt = copy.deepcopy(baseline)
        del corrupt[0]['before'][key]
        rejected(lambda corrupt=corrupt: P.validate_rows(corrupt), 'missing state ' + key)
    for case in (0, 12):
        corrupt = copy.deepcopy(baseline); corrupt[case]['result'] = 1
        rejected(lambda corrupt=corrupt: P.validate_rows(corrupt), 'wrong result type')
        corrupt = copy.deepcopy(baseline); corrupt[case]['case'] = True
        rejected(lambda corrupt=corrupt: P.validate_rows(corrupt), 'Boolean row index')
    for name in ('position', 'motion', 'dimensions', 'sleep_offsets', 'bed', 'spawn', 'private_coordinate', 'bounds', 'original_bounds'):
        corrupt = copy.deepcopy(baseline); corrupt[0]['before'][name] = []
        rejected(lambda corrupt=corrupt: P.validate_rows(corrupt), 'truncated state vector ' + name)
    for name in ('timer', 'health', 'hurt_time', 'world_players', 'world_entities'):
        corrupt = copy.deepcopy(baseline); corrupt[0]['before'][name] = True
        rejected(lambda corrupt=corrupt: P.validate_rows(corrupt), 'Boolean integer ' + name)
    for value in ([], [[0, 0, 1]], [[0, 32769, 1]], [[0, 1, 0]], [[0, 1, 1], [0, 2, 2]], [[0, 1, 1], [1, 1, 1]], [[False, 1, 1]]):
        if value == []:
            P.require(R.plane(value, 32768) == hashlib.sha256(bytes(32768)).hexdigest(), 'empty plane reconstructs all bytes')
        else:
            rejected(lambda value=value: R.plane(value, 32768), 'bad native byte runs')
    actual = [{key: copy.deepcopy(row[key]) for key in R.ROW_KEYS} for row in baseline]
    count = R.compare(actual, reference)
    for case in range(32):
        for name in ('case', 'input', 'result', 'exception'):
            corrupt = copy.deepcopy(actual); corrupt[case][name] = changed(corrupt[case][name])
            rejected(lambda corrupt=corrupt: R.compare(corrupt, reference), 'changed outcome/input')
    for phase in ('before', 'after'):
        for name in actual[0][phase]:
            corrupt = copy.deepcopy(actual); corrupt[0][phase][name] = changed(corrupt[0][phase][name])
            rejected(lambda corrupt=corrupt: R.compare(corrupt, reference), 'changed selected state ' + name)
    with tempfile.TemporaryDirectory(prefix='player-damage-wake-reader-') as folder:
        root = Path(folder)
        for name in (*P.SOURCES, 'finite.json', 'provenance.json'):
            (root / name).write_bytes((P.HERE / name).read_bytes())
        P.require(P.exact(P.read_reference(root), reference), 'unchanged copied packet')
        for name in (*P.SOURCES, 'finite.json'):
            path = root / name; original = path.read_bytes(); path.write_bytes(original + b'\n')
            rejected(lambda: P.read_reference(root), 'changed sealed member ' + name)
            path.write_bytes(original)
        product = root / 'authored-large-build-product'
        block = b'build-product\0' * 4096
        expected = hashlib.sha256()
        with product.open('wb') as stream:
            for unused in range(160):
                stream.write(block); expected.update(block)
        P.require(product.stat().st_size > 8 * 1024 * 1024 and
                  R.input_digests([product]) == {str(product): expected.hexdigest()},
                  'stream complete large build products independently of packet admission')
        try:
            P.digest(product)
        except ValueError:
            pass
        else:
            raise ValueError('packet size bound changed with product hashing')
    print('Player damage wake packet:', checks, 'authored rejections;', count, 'baseline selected scalar comparisons; no native launch')


if __name__ == '__main__':
    main()
