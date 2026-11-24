"""Portable packet integrity, entropy arithmetic and corruption-refusal controls."""
import copy
import json
from pathlib import Path
import shutil
import tempfile

import packet as P
from observation import MASK, MULTIPLIER


def refused(action):
    try:
        action()
    except (ValueError, TypeError, KeyError, IndexError, AttributeError, OSError):
        return
    raise AssertionError('corruption was admitted')


def entropy(value):
    checks = 0
    for run_index, run in enumerate(value['constructor_evidence']):
        P.need(len(run) == 28, 'complete passive constructor evidence')
        for row in run:
            for constructor in row['constructors']:
                clocks = constructor['entropy']
                P.need([clock['method'] for clock in clocks] ==
                       ['java.util.Random.seedUniquifier', 'java.lang.System.nanoTime'],
                       'actual unchanged default Random constructor')
                uniquifier, nano = [int(clock['value']['J'], 16) for clock in clocks[-2:]]
                P.need(constructor['random'] == [f'{(uniquifier ^ nano ^ MULTIPLIER) & MASK:016x}',
                                                 False, '0000000000000000'],
                       'actual default-constructor private Random seed')
                state = int(constructor['math_before'][0], 16)
                for _ in range(8):
                    state = (state * MULTIPLIER + 11) & MASK
                P.need(constructor['math_after'] == [f'{state:016x}', False, '0000000000000000'],
                       'four source Math.random draws per real Mob constructor')
                checks += 2
            published = {entity['id']: entity for entity in value['rows'][row['case']]['after']['entities']
                         if entity['id'] >= 1000}
            for constructor in row['constructors']:
                if run_index != 0:
                    continue
                if constructor['id'] not in published:
                    continue
                entity = published[constructor['id']]
                # Successful primary predicates draw32/8; ambient then draws two floats.
                # A mounted Skeleton is the helper's fresh constructor and draws neither.
                draws = 0 if entity['mount_id'] is not None else (2 if row['case'] == 25 else 4)
                state = int(constructor['random'][0], 16)
                for _ in range(draws):
                    state = (state * MULTIPLIER + 11) & MASK
                P.need(entity['random'] == [f'{state:016x}', False, '0000000000000000'],
                       'actual published Mob eligibility/ambient private stream')
                checks += 1
    return checks


def main():
    value, provenance = P.read()
    proof_path = P.HERE / 'capture-receipt.json'
    P.need(P.digest(proof_path) == provenance['capture']['sha256'], 'exact retained original capture receipt')
    proof = P.load(proof_path)
    P.need(proof['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and
           proof['input_before_sha256'] == proof['input_after_sha256'] and
           proof['helper_before_sha256'] == proof['helper_after_sha256'] and
           proof['profiles_per_vm'] == 28 and proof['binary_sha256']['client'] == P.JAR and
           all(proof['public_source_sha256'][name] == pin for name, pin in provenance['original_sources'].items()),
           'actual original products and immutable producer/helper identity')
    checks = entropy(value)
    actual = copy.deepcopy(value['rows'])
    for index in (6, 8):
        actual[index].update(returned=False, result=None, print_pending=True, source_pc=362,
                             exception='java/lang/NullPointerException' if index == 8 else 'java/lang/NoSuchMethodException')
    count = P.compare_rows(value['rows'], actual)
    rejected = 0
    # Mutate every selected top-level scalar and representative structured readback in every case.
    for index, row in enumerate(actual):
        for name in ('case', 'result', 'exception'):
            bad = copy.deepcopy(row)
            bad[name] = 'CORRUPT'
            if index in (6, 8) and name in ('result', 'exception'):
                continue
            refused(lambda bad=bad: P.compare_row(value['rows'][index], bad)); rejected += 1
        for phase in ('before', 'after'):
            for name in row[phase]:
                if row[phase][name] is None:
                    continue
                bad = copy.deepcopy(row)
                bad[phase][name] = None
                refused(lambda bad=bad: P.compare_row(value['rows'][index], bad)); rejected += 1
        for name in row['counters']:
            bad = copy.deepcopy(row)
            bad['counters'][name] += 1
            refused(lambda bad=bad: P.compare_row(value['rows'][index], bad)); rejected += 1
    for bad in ([], actual[:-1], actual + actual[:1], actual[::-1]):
        refused(lambda bad=bad: P.compare_rows(value['rows'], bad)); rejected += 1
    for text in ('{"a":0,"a":1}', '{"a":NaN}', '{"a":Infinity}'):
        refused(lambda text=text: P.loads(text)); rejected += 1
    with tempfile.TemporaryDirectory(prefix='nightmare-packet-') as temporary:
        folder = Path(temporary)
        for name in (*P.SOURCES, 'expected.json', 'provenance.json', 'capture-receipt.json'):
            shutil.copyfile(P.HERE / name, folder / name)
        P.read(folder)
        for name in (*P.SOURCES, 'expected.json', 'capture-receipt.json'):
            path = folder / name
            original = path.read_bytes()
            path.write_bytes(original + b' ')
            refused(lambda: P.read(folder)); rejected += 1
            path.write_bytes(original)
        original = (folder / 'provenance.json').read_bytes()
        for name in provenance:
            bad = copy.deepcopy(provenance)
            bad[name] = None
            (folder / 'provenance.json').write_text(json.dumps(bad), encoding='utf-8')
            refused(lambda: P.read(folder)); rejected += 1
        (folder / 'provenance.json').write_bytes(original)
        path = folder / 'expected.json'
        with path.open('wb') as stream:
            stream.truncate(8 * 1024 * 1024 + 1)
        refused(lambda: P.read(folder)); rejected += 1
    print(f'Nightmare packet: {checks} constructor arithmetic checks; {count} shared scalars; {rejected} corruption refusals')


if __name__ == '__main__':
    main()
