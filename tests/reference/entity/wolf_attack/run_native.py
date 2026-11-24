"""Replay native Wolf attack primitives against unchanged-original observations."""
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
           set(packet['source_sha256']) == set(SOURCES), 'Wolf finite packet schema')
    ensure(packet['source_sha256'] == {name: digest(HERE / name) for name in SOURCES},
           'Wolf capture input pins changed; recapture required')
    return rows('\n'.join('WOLF_ATTACK ' + json.dumps(row) for row in packet['rows']))


def compare(actual, expected):
    ensure(len(actual) == len(expected), 'native/original Wolf row count differs')
    for index, (a, b) in enumerate(zip(actual, expected)):
        for key in b:
            ensure(strict_equal(a.get(key), b[key]),
                   f'native/original Wolf case{index} field {key} differs: native {a.get(key)!r}; original {b[key]!r}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='wolf-attack-reference-', dir=args.work_dir.resolve()))
    root = HERE.parents[3]
    support = root / 'tests/tools/entity/test_mob_construction.py'
    spec = importlib.util.spec_from_file_location('wolf_declared_resources', support)
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
              root / 'tests/entity/test_wolf_attack.c', root / 'tests/world/runtime/test_world_random_tick.c',
              root / 'src/entity/mob/tick/wolf_attack.c', root / 'src/entity/mob/tick/tick.c',
              root / 'src/entity/mob/tick/internal.h', root / 'tests/support/java_random.h']
    inputs += sorted(path for path in (output / 'resources').rglob('*') if path.is_file())
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': '24 exact unchanged-original GI.a(SN,F) CPU rows over actual GI/JU World=NULL owners. '
                       'Authored native fixture resources/independent private Random seed/bounds/positions. '
                       'Full Living hurt, World query/scheduling, GL and pixel parity excluded.'}
    try:
        data_file = output / 'native.ndjson'
        receipt = capture([str(executable), str(output / 'resources'), str(worlds), '--reference', str(data_file)],
                          output, 'native', 150, os.environ.copy(), output)
        report['native'] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt['stdout_file']).read_text(encoding='utf-8')
        ensure(re.fullmatch(r'PASS: [1-9][0-9]* Wolf attack source-component checks', text.splitlines()[-1]),
               'native Wolf normal completion')
        ensure(data_file.is_file() and data_file.stat().st_size <= 128 * 1024, 'bounded native Wolf primitive rows')
        native_text = data_file.read_text(encoding='utf-8')
        actual = rows('\n'.join('WOLF_ATTACK ' + line for line in native_text.splitlines()))
        report['rows_sha256'] = digest(data_file)
        report['mismatches'] = [{'case': index, 'native': a, 'original': b}
                                for index, (a, b) in enumerate(zip(actual, expected)) if not strict_equal(a, b)]
        compare(actual, expected)
        report.update(status='PASS', rows=24)
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report['input_after_sha256']
        if not same:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'Wolf native executable/source/reference/resource inputs changed')
    print('Wolf attack: 24 exact unchanged-original CPU rows passed; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
