"""Replay native Animal death primitives against unchanged-original observations."""
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
from validate import audit_entropy, projection
from capture import capture, require_success


def expected_rows(packet):
    ensure(type(packet) is dict and set(packet) == {'rows', 'source_sha256'} and
           type(packet['rows']) is list and type(packet['source_sha256']) is dict and
           set(packet['source_sha256']) == set(SOURCES), 'Animal finite packet schema')
    ensure(packet['source_sha256'] == {name: digest(HERE / name) for name in SOURCES},
           'Animal capture input pins changed; recapture required')
    observed = rows('\n'.join('ANIMAL_DEATH ' + json.dumps(row) for row in packet['rows']))
    audit_entropy(observed)
    return observed


def compare(actual, expected):
    ensure(len(actual) == len(expected), 'native/original animal row count differs')
    native_audit, original_audit = audit_entropy(actual), audit_entropy(expected)
    ensure(native_audit == original_audit, 'native/original independently audited entropy census differs')
    for index, (a, b) in enumerate(zip(actual, expected)):
        if b['profile'] in (33, 34):
            ensure(a['exception'] == 'IO' and b['exception'] == 'CALLBACK', 'distinct actual publication failure categories')
        left, right = projection(a), projection(b)
        for key in right:
            ensure(strict_equal(left.get(key), right[key]),
                   f'native/original animal case{index} field {key} differs: native {left.get(key)!r}; original {right[key]!r}')
    return {'native': native_audit, 'original': original_audit}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='animal-damage-reference-', dir=args.work_dir.resolve()))
    root = HERE.parents[3]
    support = root / 'tests/tools/entity/test_mob_construction.py'
    spec = importlib.util.spec_from_file_location('animal_declared_resources', support)
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
              root / 'tests/entity/mob/test_animal_death.c', root / 'tests/world/runtime/test_world_random_tick.c']
    for directory in (root / 'src', root / 'include', root / 'tests/support'):
        inputs += sorted(path for path in directory.rglob('*') if path.suffix in ('.c', '.h'))
    inputs += sorted(path for path in (output / 'resources').rglob('*') if path.is_file())
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': '43 unchanged-original animal fatal damage/direct death/actual GI bite calls. '
                       'Exact deterministic states, Stack/Item fields, callbacks and real World/Chunk memberships. '
                       'Each native/original Math stream, constructor fields and default-Random count is independently audited; '
                       'raw Item private clock seeds remain separate. Original publication Throwable and native IO are distinct '
                       'categories at the same completed state boundary. Other overrides, whole gameplay, audio, GL and pixels excluded.'}
    try:
        data_file = output / 'native.ndjson'
        receipt = capture([str(executable), str(output / 'resources'), str(worlds), '--reference', str(data_file)],
                          output, 'native', 150, os.environ.copy(), output)
        report['native'] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt['stdout_file']).read_text(encoding='utf-8')
        ensure(re.fullmatch(r'PASS: [1-9][0-9]* Animal death source-order/ownership checks', text.splitlines()[-1]),
               'native Animal normal completion')
        ensure(data_file.is_file() and data_file.stat().st_size <= 512 * 1024, 'bounded native Animal primitive rows')
        native_text = data_file.read_text(encoding='utf-8')
        actual = rows('\n'.join('ANIMAL_DEATH ' + line for line in native_text.splitlines()), native=True)
        report['rows_sha256'] = digest(data_file)
        report['outcome_pairs'] = [{'case': a['case'], 'native': a['exception'], 'original': b['exception']}
                                   for a, b in zip(actual, expected) if b['profile'] in (33, 34)]
        report['mismatches'] = [{'case': a['case'], 'native': a, 'original': b}
                               for a, b in zip(actual, expected) if not strict_equal(projection(a), projection(b))]
        report['independent_entropy_audits'] = compare(actual, expected)
        report.update(status='PASS', rows=43)
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report['input_after_sha256']
        if not same:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'Animal native executable/source/reference/resource inputs changed')
    print('Animal death: 43 finite unchanged-original CPU rows passed; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
