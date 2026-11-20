"""Replay native Bed helper/wrapper results and ordered queries against the original packet."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import re
import tempfile
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from validate import digest, journals, load, profiles, queries, require, rows, unique
from capture_reference import JAR, RECEIPT, SOURCES
_support = importlib.util.spec_from_file_location('bed_native_capture', HERE.parents[1] / 'client/controller/capture.py')
_capture = importlib.util.module_from_spec(_support); _support.loader.exec_module(_capture)


def provenance(value, finite_digest):
    keys = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'source_sha256', 'tool_sha256',
            'helper_sha256', 'original', 'jdi', 'jdi_target', 'profiles_per_vm', 'query_count',
            'code_methods', 'input_unchanged', 'helper_unchanged', 'scope'}
    require(type(value) is dict and set(value) == keys and value['schema'] == 'BED_SPAWN_CAPTURE_V1' and
            value['status'] == 'PASS' and value['archive_sha256'] == JAR and
            value['finite_sha256'] == finite_digest and value['input_unchanged'] is True and
            value['helper_unchanged'] is True and type(value['profiles_per_vm']) is int and
            value['profiles_per_vm'] == 30 and type(value['code_methods']) is int and
            value['code_methods'] == 7 and type(value['query_count']) is int and value['query_count'] > 0 and
            value['scope'] == 'Prepared genuine FD and normal Map-provider/Chunks; direct VE helper and GS wrapper. Read-only original JDI observes caller-level ordered queries. No loaded gameplay or Bed use/sleep/wake qualification.',
            'Bed provenance schema/declarations')
    require(value['source_sha256'] == {name: digest(HERE / name) for name in SOURCES}, 'Bed original producer input pins')
    def hashes(group, names):
        require(type(group) is dict and set(group) == set(names) and all(type(d) is str and
                re.fullmatch('[0-9a-f]{64}', d) for d in group.values()), 'Bed provenance fingerprints')
    hashes(value['tool_sha256'], ('java.exe', 'javac.exe'))
    hashes(value['helper_sha256'], ('BedSpawnObservation.class', 'BedSpawnObservation$Guard.class', 'BedSpawnJdi.class'))
    hashes(value['jdi_target'], ('target.stdout', 'target.stderr', 'queries.jsonl'))
    require(type(value['original']) is list and len(value['original']) == 2, 'Bed independent original VM count')
    def receipt(row, stem, timeout):
        require(type(row) is dict and set(row) == set(RECEIPT) and type(row['exit']) is int and
                row['exit'] == 0 and row['timeout_seconds'] == timeout and type(row['timeout_seconds']) is int and
                row['timed_out'] is False and row['pipe_capture_complete'] is True and row['reader_errors'] == {} and
                row['output_overflow'] is False and row['stdout_limit'] == 4194304 and row['stderr_limit'] == 1048576,
                'Bed completed bounded process receipt')
        for stream in ('stdout', 'stderr'):
            require(row[stream + '_file'] == stem + '.' + stream and type(row[stream + '_bytes']) is int and
                    0 <= row[stream + '_bytes'] <= row[stream + '_limit'] and
                    type(row[stream + '_sha256']) is str and re.fullmatch('[0-9a-f]{64}', row[stream + '_sha256']),
                    'Bed bounded stream extent/fingerprint')
        require(row['stdout_bytes'] > 0, 'Bed nonempty source process completion')
    for index, row in enumerate(value['original']):
        receipt(row, 'original-' + str(index), 90)
    receipt(value['jdi'], 'jdi-controller', 180)


def finite():
    value = load(HERE / 'finite.json')
    proof = load(HERE / 'provenance.json')
    provenance(proof, digest(HERE / 'finite.json'))
    require(type(value) is dict and set(value) == {'schema', 'rows', 'journals', 'code'} and
            value['schema'] == 'BED_SPAWN_FINITE_V1', 'Bed finite packet schema')
    original = rows('\n'.join('BED_SPAWN ' + json.dumps(row) for row in value['rows']))
    encoded = '\n'.join(json.dumps(row) for row in [*value['code'], *[
        {'event': 'journal', 'case': row['case'], 'queries': row['journal']} for row in value['journals']]])
    trace, code = journals(encoded)
    require(trace == value['journals'] and code == value['code'] and
            sum(len(row['journal']) for row in trace) == proof['query_count'], 'Bed retained query/Code inventory')
    return [{'case': row['case'], 'result': row['result'], 'journal': event['journal']}
            for row, event in zip(original, trace)]


def native_rows(text):
    value = [json.loads(line[len('BED_SPAWN '):], object_pairs_hook=unique)
             for line in text.splitlines() if line.startswith('BED_SPAWN ')]
    require(len(value) == 30, 'Bed complete native row count')
    for index, row in enumerate(value):
        require(type(row) is dict and set(row) == {'case', 'result', 'journal'} and
                type(row['case']) is int and row['case'] == index, 'Bed native row schema/order')
        require(row['result'] is None or (type(row['result']) is list and len(row['result']) == 3 and
                all(type(x) is int and -(1 << 31) <= x < (1 << 31) for x in row['result'])), 'Bed native position result')
        queries(row['journal'], profiles()[index])
    return value


def compare(actual, expected):
    require(len(actual) == len(expected) == 30, 'Bed comparison extent')
    comparisons = 0
    def visit(left, right, path):
        nonlocal comparisons
        require(type(left) is type(right), 'Bed value type differs: ' + path)
        if isinstance(right, dict):
            require(left.keys() == right.keys(), 'Bed keys differ: ' + path)
            for key in right:
                visit(left[key], right[key], path + '/' + key)
        elif isinstance(right, list):
            require(len(left) == len(right), 'Bed sequence length differs: ' + path)
            for index, (a, b) in enumerate(zip(left, right)):
                visit(a, b, path + '/' + str(index))
        else:
            comparisons += 1
            require(left == right, 'Bed scalar differs: ' + path + ': native=' + str(left) + '; original=' + str(right))
    for index, (left, right) in enumerate(zip(actual, expected)):
        visit(left, right, str(index))
    return comparisons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    expected = finite()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='bed-spawn-', dir=args.work_dir.resolve()))
    resources = output / 'resources'
    for name, text in {'lang/en_US.lang': 'achievement.openInventory=Inventory\nachievement.openInventory.desc=Use %1$s\nkey.inventory=Inventory\n',
            'lang/stats_US.lang': '', 'lang/en_GB.lang': '', 'achievement/map.txt': ''}.items():
        path = resources / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(text, encoding='utf-8')
    inputs = [executable, Path(__file__).resolve(), HERE / 'finite.json', HERE / 'provenance.json',
              *[HERE / name for name in SOURCES]]
    before = {str(path.resolve()): digest(path) for path in inputs}
    result = _capture.capture([str(executable), 'bed-spawn', str(output / 'worlds').encode('utf-8').hex(),
        str(resources).encode('utf-8').hex(), str((HERE / 'profiles.tsv').resolve())], output, 'native', 180)
    _capture.require_success(result, empty_stderr=True)
    text = (output / result['stdout_file']).read_text(encoding='utf-8')
    lines = text.splitlines()
    footer = re.fullmatch('Bed spawn: ([1-9][0-9]*) checks', lines[-1] if lines else '')
    require(footer is not None and all(line.startswith('BED_SPAWN ') or line in ('16 achievements', '151 recipes')
            for line in lines[:-1]), 'Bed completed native body/control output')
    actual = native_rows(text); comparisons = compare(actual, expected)
    after = {str(path.resolve()): digest(path) for path in inputs}
    require(before == after, 'Bed replay inputs unchanged')
    report = {'status': 'PASS', 'rows': len(actual), 'scalar_comparisons': comparisons,
        'native_checks': int(footer[1]), 'native': result, 'input_before_sha256': before,
        'input_after_sha256': after,
        'scope': '30 prepared original VE/GS results and complete selected caller-level World/provider query journals. Original plane/Random preservation remains separate from authored native owner controls; no complete Bed gameplay or loaded respawn qualification.'}
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Bed spawn: ' + str(comparisons) + ' original scalar comparisons; report=' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
