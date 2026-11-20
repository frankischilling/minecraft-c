"""Portable Dispenser packet audit. No Java, assets, network or process launch."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('dispenser_component_model', HERE / 'model.py')
M = importlib.util.module_from_spec(spec)
spec.loader.exec_module(M)
FILES = ('DispenserContainerObservation.java', 'cases.tsv', 'source-pins.json', 'finite.json',
         'model.py', 'capture.py', 'publish.py', 'audit.py', 'test_packet.py', 'README.md', 'packet.json')
OPTIONS = ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS', 'JDK_JAVAC_OPTIONS', 'CLASSPATH')
JAR_SHA = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
JAVA_SHA = '59649870e7b73dbcb9c3908b81da5c59a3039593fcf17d69553166864862ee16'
CASES_SHA = '235f4dc408d26ac3dd5702c055ba9e7bb998dd816913ddc64872f5d1f2b64fe5'
PINS_SHA = 'e1476d2007f63fcc88c833d6e8722fa2fe0c43ca50502a093ff17b76e29e8bd4'
DATA_SHA = '82e6f574488af2346a1dea04815c929df024a53c0091167d9e82a2db5a9e92f9'
PRIVATE = re.compile(r'(?i)(?:[a-z]:[\\/]|\\\\|/home/|/users/|/mnt/|\.local[\\/])')


def load(path):
    return M.parse(M.bounded(path))


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf8', newline='\n')


def strings(value):
    if type(value) is str:
        yield value
    elif type(value) is dict:
        for key, item in value.items():
            yield key
            yield from strings(item)
    elif type(value) is list:
        for item in value:
            yield from strings(item)


def privacy(value):
    for text in strings(value):
        M.need(PRIVATE.search(text) is None, 'No private path/capture identity in public data')


def pin_shape(value):
    M.closed(value, ('bytes', 'sha256'), 'Digest metadata')
    M.need(type(value['bytes']) is int and value['bytes'] >= 0 and
           type(value['sha256']) is str and re.fullmatch('[0-9a-f]{64}', value['sha256']) is not None,
           'Actual finite byte count/SHA256 metadata')


def native_streams(data):
    stdout = '\n'.join(data['native_bootstrap'] + ['SH_NATIVE_ROW ' + json.dumps(row)
        for row in data['native_rows']] + ['SH_NATIVE_COMPLETE 16']) + '\n'
    lines = []
    for detail, retired in zip(data['native_details'], data['native_retirements']):
        lines += ['SH_NATIVE_DETAIL ' + json.dumps(detail), 'SH_NATIVE_RETIRE ' + json.dumps(retired)]
    return stdout.encode('utf8'), ('\n'.join(lines) + '\nSH_NATIVE_DETAIL_COMPLETE 16\n').encode('utf8')


def validate_data(data, declared):
    M.closed(data, ('schema', 'status', 'scope', 'original_rows', 'native_rows', 'native_details',
        'native_retirements', 'native_bootstrap', 'original_only_cases', 'historical_lineage', 'scope_limits'), 'Finite dataset')
    M.equal(data['schema'], 'dispenser_component_reference_v1', 'Dataset schema')
    M.equal(data['status'], 'HISTORICAL_PRIVATE_ORIGINAL_AND_NATIVE_V2_OBSERVATIONS', 'Historical-only dataset')
    M.equal(data['original_only_cases'], list(M.ORIGINAL_ONLY), 'Explicit original-only cases')
    cases = M.declarations(declared)
    M.need(len(data['original_rows']) == 18, 'All original rows retained')
    for index, (row, case) in enumerate(zip(data['original_rows'], cases)):
        M.validate_row(row, case, index)
    M.need(all(type(data[key]) is list and len(data[key]) == 16 for key in
               ('native_rows', 'native_details', 'native_retirements')), 'Complete actual native V2 rows')
    summary = M.join(data['original_rows'], *native_streams(data))
    lineage = data['historical_lineage']
    M.closed(lineage, ('original', 'native_v2', 'public_reproducer'), 'Historical lineage groups')
    M.equal(lineage['original']['fresh_vms'], 2, 'Historical fresh original VM count')
    M.equal(lineage['original']['cases_each'], 18, 'Historical original cases')
    M.equal(lineage['original']['runtime_entry_trace_or_JDI_live_Code_claim'], False, 'No invented JDI trace')
    M.equal(lineage['original']['implicit_option_self_guard_omission'], 'JDK_JAVAC_OPTIONS', 'Historical guard omission')
    M.equal(set(lineage['original']['all_five_actual_options']), set(OPTIONS), 'Complete measured historical options')
    for value in lineage['original']['all_five_actual_options'].values():
        M.equal(value, {'present': False, 'empty': True}, 'Actual historical supplemental environment')
    M.equal(lineage['original']['execution_helpers']['DispenserContainerObservation.java']['sha256'], JAVA_SHA,
            'Exact measured authored Java source')
    M.equal(lineage['native_v2']['cases'], 16, 'Historical native case count')
    M.equal(lineage['native_v2']['source_files'], 6664, 'Historical source closure scope')
    M.equal(lineage['native_v2']['actual_compiler_link_product_inputs'], 195, 'Historical native input closure scope')
    M.equal(lineage['native_v2']['natural_exit'], 0, 'Historical natural exit')
    M.equal(lineage['native_v2']['kill_requested'], False, 'No forced historical success')
    M.equal(lineage['public_reproducer'], {'status': 'POSTEXECUTION_PUBLIC_TOOLS_AUTHORED_NOT_RUNTIME_EXECUTED',
        'historical_capture_rebound_to_public_tools': False}, 'No public-tool historical rebinding')
    privacy(data)
    return summary


def validate_pins(pins):
    M.closed(pins, ('schema', 'status', 'jar', 'loaded_class_names', 'classes'), 'Archive identity metadata')
    M.equal(pins['jar']['sha256'], JAR_SHA, 'Owned original archive identity')
    M.equal(pins['loaded_class_names'], list(M.CORE), 'Observed class-resource inventory')
    pin_shape(pins['jar'])
    M.need(len(pins['classes']) == 15, 'Selected static Class/Code inventory')
    for name, item in pins['classes'].items():
        M.need(re.fullmatch('[a-z]+', name) is not None, 'Class name metadata')
        M.closed(item, ('class_bytes', 'class_sha256', 'major', 'minor', 'methods'), 'Class metadata without bodies')
        M.need(type(item['class_bytes']) is int and item['class_bytes'] > 0 and
               re.fullmatch('[0-9a-f]{64}', item['class_sha256']) is not None, 'Class digest metadata')
        for method in item['methods']:
            M.need(set(method) in ({'name', 'descriptor', 'access'},
                {'name', 'descriptor', 'access', 'code_bytes', 'code_sha256'}), 'Method metadata without instructions')
            M.need(type(method['name']) is str and type(method['descriptor']) is str and type(method['access']) is int,
                   'Method identity types')
            if 'code_sha256' in method:
                M.need(type(method['code_bytes']) is int and method['code_bytes'] > 0 and
                       re.fullmatch('[0-9a-f]{64}', method['code_sha256']) is not None, 'Code digest/length only')
    privacy(pins)


def audit(directory=HERE):
    directory = Path(directory).resolve(strict=True)
    M.equal(sorted(path.name for path in directory.iterdir() if path.is_file()), sorted(FILES), 'Closed public packet file inventory')
    M.need(not any(path.is_symlink() or path.is_dir() for path in directory.iterdir()), 'No undeclared directories/symlink inputs')
    packet = load(directory / 'packet.json')
    M.closed(packet, ('schema', 'status', 'files', 'shared_inputs', 'scope'), 'Public manifest')
    M.equal(packet['schema'], 'dispenser_component_packet_v1', 'Packet schema')
    M.equal(packet['status'], 'POSTEXECUTION_PUBLIC_REPRODUCER_NOT_RUNTIME_QUALIFIED', 'Public authoring status')
    M.equal(set(packet['files']), set(FILES) - {'packet.json'}, 'Every public source/data/readme rooted')
    for name, expected in packet['files'].items():
        pin_shape(expected)
        M.equal(M.pin(directory / name), expected, 'Actual packet bytes ' + name)
        raw = M.bounded(directory / name)
        M.need(b'\r' not in raw and raw.endswith(b'\n'), 'Explicit UTF8/LF text ' + name)
        raw.decode('utf8', errors='strict')
    M.equal(M.pin(directory / 'DispenserContainerObservation.java')['sha256'], JAVA_SHA, 'Unchanged measured Java')
    M.equal(M.pin(directory / 'cases.tsv')['sha256'], CASES_SHA, 'Unchanged declarations')
    M.equal(M.pin(directory / 'source-pins.json')['sha256'], PINS_SHA, 'Immutable original Class/Code digest metadata')
    M.equal(M.pin(directory / 'finite.json')['sha256'], DATA_SHA, 'Immutable historical scalar/provenance projection')
    expected_shared = {'tests/reference/client/controller/capture.py', 'tests/reference/client/controller/classfile.py',
                       'tests/reference/client/player_statistics/support.py'}
    M.equal(set(packet['shared_inputs']), expected_shared, 'Only declared tracked shared dependencies')
    for value in packet['shared_inputs'].values():
        pin_shape(value)
    privacy(packet)
    validate_pins(load(directory / 'source-pins.json'))
    summary = validate_data(load(directory / 'finite.json'), M.bounded(directory / 'cases.tsv'))
    return {'status': 'PASS_PORTABLE_PACKET_STRUCTURE_HASHES_AND_SCALARS_ONLY', 'files': len(FILES),
        'original_cases': 18, 'historical_original_vms': 2, 'native_cases': 16, 'native_summary': summary,
        'public_controller_runtime_qualified': False, 'raw_process_provenance_reopened': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-stdout', type=Path)
    parser.add_argument('--native-stderr', type=Path)
    args = parser.parse_args()
    result = audit()
    if args.native_stdout is not None or args.native_stderr is not None:
        M.need(args.native_stdout is not None and args.native_stderr is not None, 'Both complete native streams required')
        result['supplied_native_semantic_join'] = M.join(load(HERE / 'finite.json')['original_rows'],
            M.bounded(args.native_stdout), M.bounded(args.native_stderr))
        result['supplied_native_process_qualification'] = False
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
