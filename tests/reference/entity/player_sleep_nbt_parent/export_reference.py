"""Export a sealed private original cohort to a new flat packet template.

This copies authored producers and finite observations, never archives, class
files, bytecode, process commands, absolute input paths or raw attempt trees.
It does not run Java or update an existing qualified packet.
"""
import argparse
import hashlib
import json
from pathlib import Path


def need(value, message):
    if not value:
        raise ValueError(message)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def commitment(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def write_new(path, data):
    with Path(path).open('xb') as output:
        output.write(data)


def export(capture, output, repo):
    capture, output, repo = (Path(p).resolve() for p in (capture, output, repo))
    if output.is_relative_to(repo):
        need(output.is_relative_to(repo / '.local') and output != repo / '.local',
             'export refuses public repository output')
    need(output.is_dir() and not (output / 'provenance.json').exists(), 'new private authored template only')
    report = load(capture / 'capture.json')
    need(report['status'] == 'PASS_ORIGINAL_FULL_NBT_COMPONENT_ONLY' and
         report['all_inputs_unchanged'] is True and report['all_recorded_processes_terminal'] is True and
         report['runner_within_bound'] is True and report['declared_working_tree_dirty'] is True and
         report['declared_public_source_inputs'] == 5529, 'actual completed declared-dirty original cohort')
    files = {p.relative_to(capture).as_posix(): sha(p) for p in sorted(capture.rglob('*'))
             if p.is_file() and p.name != 'capture.json' and '__pycache__' not in p.parts}
    need(files == report['artifact_sha256'], 'complete sealed private artifact inventory')
    for key in ('input', 'protected', 'helper', 'generated'):
        need(report[key + '_before_sha256'] == report[key + '_after_sha256'], 'actual before/after guard ' + key)
    finite = load(capture / 'finite.json')
    need(sha(capture / 'finite.json') == report['finite_sha256'], 'actual selected graph')
    for number in range(2):
        need(load(capture / ('projection-' + str(number) + '.json')) == finite,
             'both actual original projections equal selected graph')
    producer = capture / 'producer'
    for name in ('PlayerNbtSleepMatrixObservation.java', 'PlayerNbtSleepMatrixJdi.java',
                 'profiles.json', 'profiles.tsv', 'class-names.txt'):
        need(sha(producer / name) == report['input_before_sha256']['producer/' + name],
             'actual historical authored input: ' + name)
        write_new(output / name, (producer / name).read_bytes())
    write_new(output / 'finite.json', (capture / 'finite.json').read_bytes())
    write_new(output / 'helper-pins.json', (capture / 'helper-pins.json').read_bytes())
    write_new(output / 'executed-source-inputs.json', (capture / 'declared-public-inputs.json').read_bytes())
    source = load(producer / 'source-pins.json')
    selected_classes = set((producer / 'class-names.txt').read_text(encoding='ascii').splitlines())
    selected_classes.update(code['class'] for code in finite['live_code'] if code['class'] in source['classes'])
    selected_classes.update(('ep', 'hk'))
    selected_source = {'archive_sha256': source['archive_sha256'],
                       'classes': {name: source['classes'][name] for name in sorted(selected_classes)}}
    write_new(output / 'source-pins.json', (json.dumps(selected_source, indent=2) + '\n').encode())
    observations = []
    vm_records = []
    for vm in range(2):
        raw = []
        for line in (capture / ('vm-' + str(vm)) / 'target.stdout').read_text(encoding='utf-8').splitlines():
            if line.startswith('NBT_SLEEP_MATRIX '):
                row = json.loads(line[len('NBT_SLEEP_MATRIX '):])
                raw.append({'case': row['case'], 'before': row['before'], 'after': row['after']})
        need(len(raw) == 40, 'all forty actual before/after witnesses')
        observations.append({'vm': vm, 'rows': raw})
        vm_records.append({'vm': vm, 'rows': 40, 'returns': 29, 'throws': 11,
                           'calls': sum(len(row['journal']['calls']) for row in finite['rows']),
                           'events': sum(len(row['journal']['events']) for row in finite['rows']),
                           'Code': len(finite['live_code']),
                           'raw_stdout_sha256': sha(capture / ('vm-' + str(vm)) / 'target.stdout'),
                           'raw_stderr_sha256': sha(capture / ('vm-' + str(vm)) / 'target.stderr'),
                           'raw_journal_sha256': sha(capture / ('vm-' + str(vm)) / 'journal.jsonl')})
    witnesses = {'schema': 'PLAYER_SLEEP_NBT_PARENT_WITNESSES_V1', 'observations': observations,
                 'comparison_scope': {'constructor_entropy': 'within_call_only', 'Random': 'within_call_only',
                                      'source_unselected_fields': 'within_call_preservation',
                                      'identity_numbers': 'equivalence_only'}}
    write_new(output / 'witnesses.json', (json.dumps(witnesses, separators=(',', ':')) + '\n').encode())
    historical = report['input_before_sha256']
    binary = report['binary_input_sha256']
    producers = {k: v for k, v in historical.items() if k.startswith(('producer/', 'support/'))}
    helpers = report['helper_before_sha256']
    controls = []
    for mode in ('normal', 'optimized'):
        # Only bounded counts and historical proof hashes are public, not the
        # private before/after input maps inside a control-process receipt.
        controls.append({'mode': mode, 'checks': 11,
                         'receipt_sha256': historical['protocol-control/' + mode + '.json'],
                         'scope': 'copied_failed_stream_protocol_only_no_new_VM'})
    provenance = {
        'schema': 'PLAYER_SLEEP_NBT_PARENT_PROVENANCE_V1',
        'scope': {'original_vms': 2, 'rows_per_vm': 40, 'native': False, 'loaded_client': False,
                  'original_draw': False, 'whole_restart': False, 'natural_night': False,
                  'initializer_execution_extent': 'UNOBSERVED_UNQUALIFIED'},
        'historical_capture_receipt_sha256': sha(capture / 'capture.json'),
        'historical_failed_protocol_receipt_sha256': report['failed_session2_receipt_sha256'],
        'historical_failed_protocol_status': 'FAIL',
        'historical_preflight_receipt_sha256': report['preflight_receipt_sha256'],
        'execution_input_sha256': historical,
        'execution_producer_input_sha256': producers,
        'execution_binary_input_sha256': binary,
        'execution_helper_sha256': helpers,
        'executed_source_scope': {'repository_head': report['repository_head'],
                                 'working_tree_dirty': True, 'inputs': 5529,
                                 'file': 'executed-source-inputs.json',
                                 'sha256': report['declared_public_source_sha256']},
        'protected_old_and_current_closure': {'files': len(report['protected_before_sha256']),
                                             'sha256': commitment(report['protected_before_sha256']),
                                             'raw_paths_published': False},
        'old33_37_raw_products': report['old33_37_raw_products'],
        'guards': {'execution_inputs_unchanged': True, 'protected_inputs_unchanged': True,
                   'compiled_helpers_unchanged': True, 'generated_pins_unchanged': True,
                   'actual_processes_terminal': True},
        'original_vm_records': vm_records, 'bounds': report['bounds'],
        'protocol_controls': controls,
        'current_tool_scope': 'packet/export/reproduction tools; no inherited original or native qualification'}
    # All current code/data pins are distinct from the historical executed map.
    current = {p.name: sha(p) for p in sorted(output.iterdir()) if p.is_file()}
    need(all(not p.is_dir() for p in output.iterdir()), 'flat authored/data packet only')
    provenance['current_artifact_sha256'] = current
    write_new(output / 'provenance.json', (json.dumps(provenance, indent=2) + '\n').encode())
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repo-root', type=Path, required=True)
    args = parser.parse_args()
    export(args.capture, args.output, args.repo_root)


if __name__ == '__main__':
    main()
