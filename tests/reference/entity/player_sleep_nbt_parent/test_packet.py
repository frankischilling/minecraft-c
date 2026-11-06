"""Standalone bounded normal/-O controls; no original/native/graphics runtime."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import time
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('sleep_nbt_parent_packet_controls', HERE / 'packet.py')
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def replace(value, path, new):
    """Copy ancestors only; all other original graph branches stay shared."""
    if not path:
        return new
    result = value.copy()
    result[path[0]] = replace(value[path[0]], path[1:], new)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.work_dir is not None:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        P.require(not args.work_dir.resolve().is_relative_to(HERE), 'controls output outside packet')
    started = time.monotonic()
    before = {p.name: P.digest(p) for p in HERE.iterdir() if p.is_file()}
    checks = []
    with tempfile.TemporaryDirectory(prefix='sleep-nbt-parent-controls-', dir=args.work_dir) as work:
        fixture = Path(work) / 'packet'
        shutil.copytree(HERE, fixture)
        original = {p.name: p.read_bytes() for p in fixture.iterdir()}
        report_path = Path(work) / 'controls.json'
        report = {'schema': 'PLAYER_SLEEP_NBT_PARENT_CONTROLS_V1', 'status': 'FAIL',
                  'mode': 'normal' if __debug__ else 'optimized', 'checks': checks,
                  'original_inputs_before_sha256': before, 'original_runtime': False,
                  'native_runtime': False, 'performance_claim': False}
        def bounded():
            P.require(time.monotonic() - started < 90, 'unchanged90s control watchdog')
        def reject(name, function):
            bounded()
            try:
                function()
            except ValueError:
                checks.append(name)
            else:
                raise ValueError('corruption admitted: ' + name)
        def write_proof(value):
            (fixture / 'provenance.json').write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')
        def proof_variant(name, value):
            write_proof(value)
            try:
                reject(name, lambda: P.read_reference(fixture))
            finally:
                (fixture / 'provenance.json').write_bytes(original['provenance.json'])
        try:
            graph, witness, profiles = P._read(fixture)
            checks.append('portable_positive_full40_both_VM_witnesses')
            proof = P.loads(original['provenance.json'].decode())
            source = P.loads(original['source-pins.json'].decode())
            helper = P.loads(original['helper-pins.json'].decode())
            for name in sorted(P.FILES):
                (fixture / name).write_bytes(original[name] + b' ')
                try:
                    reject('actual_reader_changed_artifact:' + name, lambda: P.read_reference(fixture))
                    if name in P.ORIGINAL:
                        altered = replace(proof, ('current_artifact_sha256', name), P.digest(fixture / name))
                        proof_variant('actual_reader_retagged_original:' + name, altered)
                finally:
                    (fixture / name).write_bytes(original[name])
            missing = 'finite.json'
            (fixture / missing).unlink()
            try:
                reject('actual_reader_missing_original_file', lambda: P.read_reference(fixture))
            finally:
                (fixture / missing).write_bytes(original[missing])
            (fixture / 'unexpected.json').write_text('{}')
            try:
                reject('actual_reader_extra_file', lambda: P.read_reference(fixture))
            finally:
                (fixture / 'unexpected.json').unlink()
            for key in proof['execution_producer_input_sha256']:
                proof_variant('historical_producer:' + key,
                              replace(proof, ('execution_producer_input_sha256', key), '0' * 64))
            for key in proof['execution_helper_sha256']:
                proof_variant('historical_helper:' + key,
                              replace(proof, ('execution_helper_sha256', key), '0' * 64))
            for prefix in ('jdk/', 'sdk/'):
                key = next(k for k in proof['execution_binary_input_sha256'] if k.startswith(prefix))
                proof_variant('historical_binary:' + prefix,
                              replace(proof, ('execution_binary_input_sha256', key), '0' * 64))
            renamed = proof['execution_binary_input_sha256'].copy()
            key = next(iter(renamed))
            renamed['renamed/' + key] = renamed.pop(key)
            proof_variant('historical_binary_inventory_renamed', replace(proof, ('execution_binary_input_sha256',), renamed))
            missing_map = proof['execution_input_sha256'].copy()
            missing_map.pop(next(iter(missing_map)))
            proof_variant('historical_complete_map_drop', replace(proof, ('execution_input_sha256',), missing_map))
            extra_map = {**proof['execution_input_sha256'], 'invented/input': '0' * 64}
            proof_variant('historical_complete_map_extra', replace(proof, ('execution_input_sha256',), extra_map))
            proof_variant('historical_writer_replaced_by_current_unexecuted_reproducer',
                          replace(proof, ('execution_producer_input_sha256', 'producer/capture_matrix.py'),
                                  P.digest(HERE / 'capture_reference.py')))
            for name, path, value in (
                    ('native_credit', ('scope', 'native'), True),
                    ('loaded_credit', ('scope', 'loaded_client'), True),
                    ('whole_restart_credit', ('scope', 'whole_restart'), True),
                    ('initializer_entered_credit', ('scope', 'initializer_execution_extent'), 'ENTERED'),
                    ('dirty_scope_erased', ('executed_source_scope', 'working_tree_dirty'), False),
                    ('dirty_bool_to_int', ('executed_source_scope', 'working_tree_dirty'), 1),
                    ('native_bool_to_int', ('scope', 'native'), 0),
                    ('source_scope_count', ('executed_source_scope', 'inputs'), 5528),
                    ('old_fail_relabel', ('historical_failed_protocol_status',), 'PASS'),
                    ('raw_unavailable_relabel', ('old33_37_raw_products', 'inherited_qualification_claimed'), True),
                    ('terminal_guard_erased', ('guards', 'actual_processes_terminal'), False),
                    ('terminal_bool_to_int', ('guards', 'actual_processes_terminal'), 1),
                    ('VM_proof_hash', ('original_vm_records', 0, 'raw_stdout_sha256'), '0' * 64),
                    ('VM_count', ('original_vm_records', 1, 'rows'), 39),
                    ('limit_relaxation', ('bounds', 'case_calls'), 513)):
                proof_variant(name, replace(proof, path, value))
            unknown = {**proof, 'unknown': True}
            proof_variant('unknown_provenance_key', unknown)
            text = original['provenance.json'].decode()
            (fixture / 'provenance.json').write_text(text.replace('{', '{"schema":"duplicate",', 1))
            try:
                reject('duplicate_provenance_key_actual_reader', lambda: P.read_reference(fixture))
            finally:
                (fixture / 'provenance.json').write_bytes(original['provenance.json'])
            reject('duplicate_JSON_key', lambda: P.loads('{"a":1,"a":2}'))
            reject('nonfinite_JSON', lambda: P.loads('{"a":NaN}'))

            # Direct semantic controls share untouched ancestors; changed
            # original artifact/provenance controls above always use _read.
            first_wake = next(i for i, c in enumerate(graph['rows'][0]['journal']['calls'])
                              if c['site']['class'] == 'gs' and c['site']['descriptor'] == '(ZZZ)V')
            checkpoint = next(i for i, e in enumerate(graph['rows'][0]['journal']['events']) if e['kind'] == 'CHECKPOINT93')
            fault_row = next(i for i, row in enumerate(graph['rows']) if row['journal']['first_exception'] is not None)
            fault_event = next(i for i, e in enumerate(graph['rows'][fault_row]['journal']['events']) if e['kind'] == 'EXCEPTION')
            cases = (
                ('row_identity', ('rows', 0, 'case'), True),
                ('history_unqualified_erased', ('rows', 35, 'within_call_only'), []),
                ('source_flags_not_genuine', ('rows', 0, 'journal', 'calls', first_wake, 'arguments'), [False, True, False]),
                ('original93_rebound', ('rows', 0, 'journal', 'events', checkpoint, 'site', 'pc'), 94),
                ('source_total_order', ('rows', 0, 'journal', 'events', 1, 'tick'), 0),
                ('source_call_identity', ('rows', 0, 'journal', 'calls', 1, 'id'), 0),
                ('first_cause_PC', ('rows', fault_row, 'journal', 'events', fault_event, 'site', 'pc'), 999999),
                ('fault_stack_caller_PC', ('rows', fault_row, 'journal', 'events', fault_event, 'stack', 1, 'pc'), 999999),
                ('native_scope_claim', ('scope', 'native'), True),
                ('draw_scope_claim', ('scope', 'original_draw'), True),
                ('initializer_execution_claim', ('initializer_prefix', 0, 'execution_extent'), 'ENTERED'),
                ('position_rawbits', ('rows', 0, 'after', 'position', 0), 'not-double'),
                ('health_bool_alias', ('rows', 0, 'after', 'living', 0), True),
                ('plane_hash_extent', ('rows', 0, 'after', 'planes', 'blocks'), '0'),
                ('array_alias_flag_type', ('rows', 0, 'after', 'main_same'), 0))
            for name, path, value in cases:
                changed = replace(graph, path, value)
                reject('semantic:' + name, lambda changed=changed: P.V.finite(changed, profiles, source, helper))
            for name, path, value in (
                    ('Random_changed', ('observations', 0, 'rows', 0, 'after', 'random', 0, 0), 'changed'),
                    ('old_Stack_changed', ('observations', 0, 'rows', 0, 'after', 'old_main', 0, 'fields', 'iz.a'), 6),
                    ('source_unselected_App_changed', ('observations', 0, 'rows', 0, 'after', 'actor_fields', 'dc.b'), {'class': 'invented', 'identity': 999}),
                    ('source_unselected_World_changed', ('observations', 0, 'rows', 0, 'after', 'world_fields', 'fd.B'), True),
                    ('published_sleeper_J_erased', ('observations', 0, 'rows', 33, 'before', 'selected', 'world_J'), False),
                    ('coordinate_alias_erased', ('observations', 0, 'rows', 0, 'after', 'selected', 'Q_same'), True),
                    ('constructor_scope_rebound', ('comparison_scope', 'constructor_entropy'), 'absolute_expected_value')):
                changed = replace(witness, path, value)
                reject('within_call:' + name, lambda changed=changed: P.V.witnesses(changed, graph, profiles, source))
            report['status'] = 'PASS'
        except BaseException as error:
            report['error'] = {'type': type(error).__name__, 'message': str(error)}
            raise
        finally:
            after = {p.name: P.digest(p) for p in HERE.iterdir() if p.is_file()}
            report['original_inputs_after_sha256'] = after
            report['original_inputs_unchanged'] = before == after
            report['wall_seconds'] = time.monotonic() - started
            report['count'] = len(checks)
            if before != after or report['wall_seconds'] >= 90:
                report['status'] = 'FAIL'
            # Keep the receipt outside TemporaryDirectory so failed attempts
            # survive ordinary fixture cleanup.
            destination = (args.work_dir if args.work_dir is not None else Path(tempfile.gettempdir())) / (
                'sleep-nbt-parent-controls-' + str(time.time_ns()) + '.json')
            destination.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
            P.require(report['status'] == 'PASS', 'controls failed; actual receipt retained: ' + str(destination))
    print('PASS: ' + str(len(checks)) + ' standalone packet controls; full40 original graph/2 witnesses; no runtime/performance credit')


if __name__ == '__main__':
    main()
