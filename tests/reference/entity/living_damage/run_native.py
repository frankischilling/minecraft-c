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
from capture_reference import SOURCES, digest, ensure, read_json, rows, strict_equal
from capture import capture, require_success


def expected_rows(packet):
    ensure(type(packet) is dict and set(packet) == {'rows', 'source_sha256'} and
           type(packet['rows']) is list and type(packet['source_sha256']) is dict and
           set(packet['source_sha256']) == set(SOURCES), 'Living finite packet schema')
    ensure(packet['source_sha256'] == {name: digest(HERE / name) for name in SOURCES},
           'Living capture input pins changed; recapture required')
    return rows('\n'.join('LIVING_DAMAGE ' + json.dumps(row) for row in packet['rows']))


def compare(actual, expected):
    ensure(len(actual) == len(expected), 'native/original Living row count differs')
    for index, (a, b) in enumerate(zip(actual, expected)):
        for key in b:
            # These are deliberately different executions, with an explicitly
            # bounded common state checkpoint rather than Throwable parity.
            if b['profile'] == 19 and key in ('result', 'exception', 'completion'):
                continue
            if b['profile'] == 21 and key == 'exception':
                continue
            ensure(strict_equal(a.get(key), b[key]),
                   f'native/original Living case{index} field {key} differs: native {a.get(key)!r}; original {b[key]!r}')
        if b['profile'] == 19:
            ensure(a['result'] is None and a['exception'] == 'UNIMPLEMENTED' and a['completion'] is None and
                   b['result'] is True and b['exception'] == 'NONE' and b['completion']['state']['dying'] is True and
                   a['state']['dying'] is False, 'actual original death completion/native death-entry limit differs')
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
              root / 'src/entity/mob/tick/wolf_attack.c', root / 'tests/support/java_random.h']
    inputs += sorted(path for path in (output / 'resources').rglob('*') if path.is_file())
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': '27 unchanged-original DL/LS and GI close-bite CPU observations over genuine fd/Sheep/Wolf. '
                       'Exact primitive returns/state/Random histories, NPE prefixes. Fatal compares source sound checkpoint before '
                       'LS.b(SN)316 only; original full death retained separately. Throwing original sound observer and native IO '
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
        actual = rows('\n'.join('LIVING_DAMAGE ' + line for line in native_text.splitlines()), native=True)
        report['rows_sha256'] = digest(data_file)
        report['outcome_pairs'] = [{'case': index, 'native': a['exception'], 'original': b['exception'],
                                    'scope': b['scope']} for index, (a, b) in enumerate(zip(actual, expected))
                                   if b['profile'] in (19, 21)]
        report['mismatches'] = [{'case': index, 'native': a, 'original': b}
                               for index, (a, b) in enumerate(zip(actual, expected))
                               if any(not strict_equal(a[key], b[key]) for key in b
                                      if not (b['profile'] == 19 and key in ('result', 'exception', 'completion') or
                                              b['profile'] == 21 and key == 'exception'))]
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
