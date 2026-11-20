"""Compare the native World projection with the retained original Bed packet."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from validate import (COMPARE_SCOPE, JAR, RECEIPT, SCOPE, SOURCES, compare, digest, integer, items,
                      keys, load, loads, packet, planes, profiles, projection, random_state,
                      require, utf16, word)
from capture_reference import HELPERS, LIBRARIES

_spec = importlib.util.spec_from_file_location('bed_interaction_native_capture', HERE.parents[1] / 'client/controller/capture.py')
_capture = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_capture)


def provenance(value, finite_digest):
    expected = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'source_sha256', 'tool_sha256',
                'library_sha256', 'helper_sha256', 'original', 'jdi', 'jdi_target', 'profiles_per_vm',
                'query_count', 'code_methods', 'game_code_methods', 'class_count', 'input_unchanged',
                'helper_unchanged', 'reviewed_observations_equal', 'scope'}
    keys(value, expected, 'Bed provenance exact schema')
    require(value['schema'] == 'BED_INTERACTION_CAPTURE_V1' and value['status'] == 'PASS' and
            value['archive_sha256'] == JAR and value['finite_sha256'] == finite_digest and
            value['input_unchanged'] is True and value['helper_unchanged'] is True and
            value['reviewed_observations_equal'] is True and value['scope'] == SCOPE,
            'Bed provenance source/continuity/scope')
    for name, number in (('profiles_per_vm', 85), ('query_count', 408), ('code_methods', 20),
                         ('game_code_methods', 19), ('class_count', 24)):
        require(integer(value[name]) and value[name] == number, 'Bed provenance extent ' + name)
    compare(value['source_sha256'], {name: digest(HERE / name) for name in SOURCES}, 'Bed original producer source pins')
    compare(value['library_sha256'], LIBRARIES, 'Bed original SDK pins')

    def hashes(group, names):
        require(type(group) is dict and set(group) == set(names) and all(word(v, 64) for v in group.values()),
                'Bed provenance hash inventory')

    hashes(value['tool_sha256'], ('java.exe', 'javac.exe'))
    hashes(value['helper_sha256'], HELPERS)
    hashes(value['jdi_target'], ('calls.jsonl', 'target.stdout', 'target.stderr'))
    require(type(value['original']) is list and len(value['original']) == 2, 'Bed original independent VM count')

    def receipt(row, stem, timeout):
        keys(row, RECEIPT, 'Bed bounded receipt exact schema')
        require(integer(row['exit']) and row['exit'] == 0 and integer(row['timeout_seconds']) and
                row['timeout_seconds'] == timeout and row['timed_out'] is False and
                row['pipe_capture_complete'] is True and type(row['reader_errors']) is dict and
                row['reader_errors'] == {} and row['output_overflow'] is False,
                'Bed process completed without truncation/timeout/reader failure')
        for stream, limit in (('stdout', 4194304), ('stderr', 1048576)):
            require(row[stream + '_file'] == stem + '.' + stream and
                    integer(row[stream + '_limit']) and row[stream + '_limit'] == limit and
                    integer(row[stream + '_bytes'], 0, limit) and word(row[stream + '_sha256'], 64),
                    'Bed bounded stream extent/fingerprint')
        require(row['stdout_bytes'] > 0, 'Bed original nonempty completion')

    for index, row in enumerate(value['original']):
        receipt(row, 'original-' + str(index), 180)
    receipt(value['jdi'], 'jdi-controller', 240)


def finite():
    proof = load(HERE / 'provenance.json')
    provenance(proof, digest(HERE / 'finite.json'))
    value = packet(load(HERE / 'finite.json'))
    return projection(value)


def native_rows(text, original=None):
    source = packet(load(HERE / 'finite.json')) if original is None else packet(original)
    return _native_rows_validated(text, source)


def _native_rows_validated(text, source):
    # Internal parser core: source has already passed the full packet validator.
    result = [loads(line[len('BED_INTERACTION '):]) for line in text.splitlines()
              if line.startswith('BED_INTERACTION ')]
    require(len(result) == 85, 'Bed complete native row count')
    for row, captured, profile in zip(result, source['rows'], profiles()):
        number, kind, metadata, partner, remote = profile
        common = {'case', 'kind', 'metadata', 'partner', 'remote', 'input_cells', 'output_cells',
                  'random_before', 'random_after'}
        more = {'id', 'metadata_after', 'partner_id', 'items'} if not kind else {
            'returned', 'origin_id', 'resolved_id', 'resolved_metadata', 'daytime', 'provider_allows',
            'player_unchanged', 'sleep_result', 'message_keys'}
        keys(row, common | more, 'Bed native exact row schema')
        require(integer(row['case']) and row['case'] == number and
                row['kind'] == ('activate' if kind else 'neighbor') and integer(row['metadata']) and
                row['metadata'] == metadata and type(row['partner']) is bool and row['partner'] == bool(partner) and
                type(row['remote']) is bool and row['remote'] == bool(remote), 'Bed native profile/order/types')
        require(planes(row['input_cells']) == captured['input_planes'] and
                planes(row['output_cells']) == captured['output_planes'], 'Bed native complete planes differ from original')
        random_state(row['random_before']); random_state(row['random_after'])
        if not kind:
            require(all(integer(row[k], 0, 255) for k in ('id', 'partner_id')) and
                    integer(row['metadata_after'], 0, 15), 'Bed native neighbor scalar types')
            items(row['items'])
        else:
            require(row['returned'] is True and row['daytime'] is True and row['provider_allows'] is True and
                    all(integer(row[k], 0, 255) for k in ('origin_id', 'resolved_id')) and
                    integer(row['resolved_metadata'], 0, 15) and type(row['message_keys']) is list and
                    all(utf16(v) for v in row['message_keys']), 'Bed native activation scalar types')
            if 64 <= number < 80:
                require(row['player_unchanged'] is True and row['sleep_result'] == 'cw.c' and
                        len(row['message_keys']) == 1, 'Bed actual native CW/Player/message boundary')
            else:
                require(row['player_unchanged'] is None and row['sleep_result'] is None and
                        row['message_keys'] == [], 'Bed native unreached Player children')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    expected = finite()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='bed-interaction-', dir=args.work_dir.resolve()))
    resources = output / 'resources'
    for name, text in {'lang/en_US.lang': 'achievement.openInventory=Inventory\nachievement.openInventory.desc=Use %1$s\nkey.inventory=Inventory\n',
                       'lang/stats_US.lang': '', 'lang/en_GB.lang': '', 'achievement/map.txt': ''}.items():
        path = resources / name; path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
    inputs = [executable, Path(__file__).resolve(), HERE / 'finite.json', HERE / 'provenance.json',
              *[HERE / name for name in SOURCES]]
    before = {str(path.resolve()): digest(path) for path in inputs}
    result = _capture.capture([str(executable), 'bed-interaction', str(output / 'worlds').encode('utf-8').hex(),
                               str(resources).encode('utf-8').hex()], output, 'native', 180)
    _capture.require_success(result, empty_stderr=True)
    text = (output / result['stdout_file']).read_text(encoding='utf-8')
    lines = text.splitlines()
    footer = re.fullmatch(r'Bed interaction: ([1-9][0-9]*) checks', lines[-1] if lines else '')
    require(footer is not None and all(line.startswith('BED_INTERACTION ') or
            line in ('16 achievements', '151 recipes') for line in lines[:-1]), 'Bed complete native body/control output')
    comparisons = compare(native_rows(text), expected, 'Bed native/original World projection')
    after = {str(path.resolve()): digest(path) for path in inputs}
    require(before == after, 'Bed replay inputs unchanged')
    report = {'status': 'PASS', 'rows': 85, 'scalar_comparisons': comparisons,
              'native_checks': int(footer[1]), 'native': result,
              'input_before_sha256': before, 'input_after_sha256': after, 'scope': COMPARE_SCOPE}
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Bed interaction: ' + str(comparisons) + ' original World scalar comparisons; report=' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
