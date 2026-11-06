"""Replay native Living damage primitives against unchanged-original observations."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import SOURCES, digest, ensure, profiles, read_json, rows, state, strict_equal, unique_object
from capture import capture, require_success


def expected_rows(packet):
    ensure(type(packet) is dict and set(packet) == {'rows', 'source_sha256'} and
           type(packet['rows']) is list and type(packet['source_sha256']) is dict and
           set(packet['source_sha256']) == set(SOURCES), 'Living finite packet schema')
    ensure(packet['source_sha256'] == {name: digest(HERE / name) for name in SOURCES},
           'Living capture input pins changed; recapture required')
    return rows('\n'.join('LIVING_DAMAGE ' + json.dumps(row) for row in packet['rows']))


def native_rows(text):
    """Validate current native returns without changing the pinned capture reader."""
    found = [json.loads(line[len('LIVING_DAMAGE '):], object_pairs_hook=unique_object)
             for line in text.splitlines() if line.startswith('LIVING_DAMAGE ')]
    expected = [(index, step, profile) for index, profile in enumerate(profiles())
                for step in range(3 if profile[0] == 2 else 1)]
    keys = {'case', 'profile', 'step', 'input', 'amount', 'scope', 'result', 'exception', 'state', 'completion'}
    ensure(len(found) == len(expected) == 27, 'exact Living row count')
    for index, (row, (profile_index, step, profile)) in enumerate(zip(found, expected)):
        ensure(type(row) is dict and set(row) == keys and type(row['case']) is int and row['case'] == index and
               type(row['profile']) is int and row['profile'] == profile_index and type(row['step']) is int and row['step'] == step and
               strict_equal(row['input'], profile), 'Living row identity/schema differs')
        amount = (4, 3, 7)[step] if profile[0] == 2 else profile[1]
        ensure(type(row['amount']) is int and row['amount'] == amount, 'Living amount input differs')
        exception = 'NPE' if profile_index in (9, 10, 20) else 'IO' if profile_index == 21 else 'NONE'
        scope = 'EXCEPTION' if exception != 'NONE' else 'RETURN'
        ensure(row['exception'] == exception and row['scope'] == scope, 'Living source outcome category differs')
        ensure(row['result'] is None if exception != 'NONE' or profile[0] == 1 else type(row['result']) is bool,
               'Living actual result type')
        if profile_index == 19:
            ensure(row['result'] is True, 'Living native completed death return')
        state(row['state'])
        ensure(row['completion'] is None, 'Living native terminal-state scope differs')
    return found


def comparison_row(original):
    if original['profile'] != 19:
        return original
    completion = original['completion']
    ensure(original['scope'] == 'DEATH_ENTRY' and original['result'] is True and original['exception'] == 'NONE' and
           original['state']['dying'] is False and type(completion) is dict and
           set(completion) == {'result', 'exception', 'state'} and completion['result'] is True and
           completion['exception'] == 'NONE' and completion['state']['dying'] is True,
           'actual original death completion/checkpoint differs')
    # The immutable original record contains TWO actual observations. Native
    # emits its terminal state, so compare it to the captured completion,
    # without relabeling the historical sound checkpoint as a native result.
    return {**original, 'scope': 'RETURN', 'state': completion['state'], 'completion': None}


def compare(actual, expected):
    ensure(len(actual) == len(expected), 'native/original Living row count differs')
    for index, (a, b) in enumerate(zip(actual, expected)):
        compared = comparison_row(b)
        for key in compared:
            # The throwing observer and native I/O adapter are deliberately
            # distinct failures with a common reached-state boundary.
            if b['profile'] == 21 and key == 'exception':
                continue
            ensure(strict_equal(a.get(key), compared[key]),
                   f'native/original Living case{index} field {key} differs: native {a.get(key)!r}; original {compared[key]!r}')
        if b['profile'] == 21:
            ensure(a['exception'] == 'IO' and b['exception'] == 'CALLBACK', 'distinct actual World observer failure categories differ')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='living-damage-reference-', dir=args.work_dir.resolve()))
    root = HERE.parents[3]
    support = root / 'tests/tools/entity/test_mob_construction.py'
    spec = importlib.util.spec_from_file_location('living_declared_resources', support)
    resources = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(resources)
    for name, data in resources.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    worlds = output / 'worlds'
    worlds.mkdir()
    expected = expected_rows(read_json(HERE / 'finite.json'))
    inputs = [executable, HERE / 'run_native.py', HERE / 'finite.json', *[HERE / name for name in SOURCES], support,
              root / 'tests/entity/mob/test_living_damage.c', root / 'tests/world/runtime/test_world_random_tick.c',
              root / 'src/entity/mob/damage.c', root / 'src/entity/mob/damage_internal.h',
              root / 'src/entity/mob/death.c', root / 'src/entity/mob/death_internal.h',
              root / 'include/mc/entity/mob/death.h', root / 'src/entity/drop/drop.c',
              root / 'include/mc/entity/drop.h',
              root / 'src/entity/mob/tick/wolf_attack.c', root / 'tests/support/java_random.h']
    inputs += sorted(path for path in (output / 'resources').rglob('*') if path.is_file())
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': '27 unchanged-original DL/LS and GI close-bite CPU observations over genuine fd/Sheep/Wolf. '
                       'Exact primitive returns/state/Random histories, NPE prefixes and complete sheared Sheep fatal return. '
                       'Fatal terminal state compares the original completion; its historical sound checkpoint remains separate. '
                       'Throwing original sound observer and native IO '
                       'adapter compare completed state, with different failure categories preserved. Other overrides, World ticks, '
                       'whole combat/gameplay, GL and pixels excluded.'}
    try:
        data_file = output / 'native.ndjson'
        receipt = capture([str(executable), str(output / 'resources'), str(worlds), '--reference', str(data_file)],
                          output, 'native', 150, os.environ.copy(), output)
        report['native'] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt['stdout_file']).read_text(encoding='utf-8')
        ensure(re.fullmatch(r'PASS: [1-9][0-9]* Living damage source-order/ownership checks', text.splitlines()[-1]),
               'native Living normal completion')
        ensure(data_file.is_file() and data_file.stat().st_size <= 256 * 1024, 'bounded native Living primitive rows')
        native_text = data_file.read_text(encoding='utf-8')
        actual = native_rows('\n'.join('LIVING_DAMAGE ' + line for line in native_text.splitlines()))
        report['rows_sha256'] = digest(data_file)
        report['outcome_pairs'] = [{'case': index, 'native': a['exception'], 'original': b['exception'],
                                    'scope': b['scope']} for index, (a, b) in enumerate(zip(actual, expected))
                                   if b['profile'] == 21]
        report['fatal_comparison'] = {'profile': 19, 'native': 'actual terminal state',
                                      'original': 'actual captured completion',
                                      'historical_sound_checkpoint_preserved': True}
        report['mismatches'] = [{'case': index, 'native': a, 'original': b}
                               for index, (a, b) in enumerate(zip(actual, expected))
                               if any(not strict_equal(a[key], value)
                                      for key, value in comparison_row(b).items()
                                      if not (b['profile'] == 21 and key == 'exception'))]
        compare(actual, expected)
        report.update(status='PASS', rows=27)
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report['input_after_sha256']
        if not same:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'Living native executable/source/reference/resource inputs changed')
    print('Living damage: 27 exact unchanged-original CPU rows passed; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
