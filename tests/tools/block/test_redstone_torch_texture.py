"""Compare the actual metadata texture API with the sealed original packet."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = ROOT / 'tests/reference/client/redstone_torch_texture'
PRELUDE = (b'16 achievements\r\n151 recipes\r\n' if os.name == 'nt'
           else b'16 achievements\n151 recipes\n')
BOOL_KEYS = {'self_is_db', 'dust_is_sm', 'same_registry', 'array_same', 'named_same', 'fields_unchanged', 'restored'}
INT_KEYS = {'index', 'status', 'out_before', 'out', 'saved_self', 'saved_dust', 'entered_self', 'entered_dust',
            'after_self', 'after_dust', 'restored_self', 'restored_dust', 'self_id', 'dust_id'}
ROW_KEYS = BOOL_KEYS | INT_KEYS | {'name', 'values'}
FINISH_KEYS = {'rows', 'checks', 'stable', 'resources_ok', 'owners_restored', 'retired'}
DETAIL_KEYS = {'checks', 'returning', 'direct_sm', 'null_aw', 'bypass', 'invalid', 'retired'}


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    sys.modules[name] = result
    spec.loader.exec_module(result)
    return result


C = module('db_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
N = module('db_native_products', ROOT / 'tests/tools/client/native_product_capture.py')


def need(condition, message):
    if not condition:
        raise ValueError(message)


def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def invalid_constant(value):
    raise ValueError('non-JSON constant: ' + value)


def loads(value):
    return json.loads(value, object_pairs_hook=unique, parse_constant=invalid_constant)


def same(left, right):
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(same(left[k], right[k]) for k in left)
    if type(left) is list:
        return len(left) == len(right) and all(same(a, b) for a, b in zip(left, right))
    return left == right


def tree(root):
    return {p.relative_to(root).as_posix(): C.digest(p) for p in sorted(root.rglob('*')) if p.is_file()}


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='ascii', newline='\n')


def admit_original():
    packet = module('redstone_torch_texture_packet', SOURCE / 'packet.py')
    return packet.read_packet(SOURCE)


def expected_row(fields):
    """Compare scalar/field facts only; original declaring owners stay original."""
    return {'index': int(fields[1]), 'name': fields[2], 'values': list(map(int, fields[3:9])),
            'status': 0, 'out_before': -123456789, 'out': int(fields[13]),
            'saved_self': int(fields[9]), 'saved_dust': int(fields[10]),
            'entered_self': int(fields[11]), 'entered_dust': int(fields[12]),
            'after_self': int(fields[11]), 'after_dust': int(fields[12]),
            'restored_self': int(fields[9]), 'restored_dust': int(fields[10]),
            'self_id': int(fields[3]), 'dust_id': int(fields[18]), **{k: True for k in BOOL_KEYS}}


def lines_after_bootstrap(raw):
    need(raw.startswith(PRELUDE), 'exact native two-line Registry bootstrap')
    payload = raw[len(PRELUDE):]
    lines = payload.decode('ascii').splitlines()
    need(payload == ('\n'.join(lines) + '\n').encode('ascii'), 'exact native LF record transport')
    return lines


def parse_native(raw):
    lines = lines_after_bootstrap(raw)
    need(len(lines) == 161 and all(line.startswith('DB_NATIVE\t') for line in lines[:-1]) and
         lines[-1].startswith('DB_NATIVE_FINISH\t'), 'closed ordered native row/finish roster')
    rows = [loads(line.split('\t', 1)[1]) for line in lines[:-1]]
    for index, row in enumerate(rows):
        need(type(row) is dict and set(row) == ROW_KEYS and all(type(row[k]) is bool for k in BOOL_KEYS) and
             all(type(row[k]) is int and -2**31 <= row[k] < 2**31 for k in INT_KEYS) and
             row['index'] == index and type(row['name']) is str and re.fullmatch('[a-z0-9-]+', row['name']) and
             type(row['values']) is list and len(row['values']) == 6 and
             all(type(v) is int and -2**31 <= v < 2**31 for v in row['values']), 'closed typed native scalar row')
    finish = loads(lines[-1].split('\t', 1)[1])
    need(type(finish) is dict and set(finish) == FINISH_KEYS and type(finish['rows']) is int and
         finish['rows'] == 160 and type(finish['checks']) is int and finish['checks'] > 0 and
         all(finish[k] is True for k in FINISH_KEYS - {'rows', 'checks'}), 'native continuity and checked retirement')
    return rows, finish


def compare_native(raw, original):
    need(type(original) is list and len(original) == 160, 'complete admitted original scalar roster')
    rows, finish = parse_native(raw)
    for index, (row, fields) in enumerate(zip(rows, original)):
        expected = expected_row(fields)
        if not same(row, expected):
            diff = {k: {'native': row[k], 'original_or_contract': expected[k]}
                    for k in expected if not same(row[k], expected[k])}
            raise ValueError('scalar row ' + str(index) + ' ' + fields[2] + ': ' + json.dumps(diff, sort_keys=True))
    return {'rows': len(rows), 'checks': finish['checks'], 'native_owner_contract': True,
            'original_declaring_class_proof_compared_as_native_trace': False}


def parse_detail(raw):
    lines = lines_after_bootstrap(raw)
    need(len(lines) == 1 and lines[0].startswith('DB_DETAIL_FINISH\t'), 'closed detail control roster')
    value = loads(lines[0].split('\t', 1)[1])
    need(type(value) is dict and set(value) == DETAIL_KEYS and type(value['checks']) is int and value['checks'] > 0 and
         all(value[k] is True for k in DETAIL_KEYS - {'checks'}), 'actual candidate-only detail predicates and retirement')
    return value


def controls(original):
    """Synthetic native decoder controls in memory; never capture evidence."""
    rows = [expected_row(fields) for fields in original]
    finish = {'rows': 160, 'checks': 1, 'stable': True, 'resources_ok': True, 'owners_restored': True, 'retired': True}
    def encode(changed, ending=finish):
        return PRELUDE + ('\n'.join(['DB_NATIVE\t' + json.dumps(r) for r in changed] +
                                   ['DB_NATIVE_FINISH\t' + json.dumps(ending)]) + '\n').encode('ascii')
    compare_native(encode(rows), original)
    cases = []
    for key, value in (('out', rows[0]['out'] + 1), ('status', 1), ('index', True), ('out', True),
                       ('self_id', True), ('same_registry', 1), ('dust_is_sm', False), ('named_same', False),
                       ('fields_unchanged', False), ('restored_self', 7), ('values', [True, *rows[0]['values'][1:]])):
        changed = copy.deepcopy(rows)
        changed[0][key] = value
        cases.append(encode(changed))
    changed = copy.deepcopy(rows); changed[0]['extra'] = 1; cases.append(encode(changed))
    cases.extend([encode(rows[1:]), encode([rows[1], rows[0], *rows[2:]]), encode([*rows, rows[-1]]),
                  encode(rows).replace(b'"status": 0', b'"status": 0, "status": 0', 1),
                  encode(rows).replace(b'"out": ', b'"out": NaN, "ignored": ', 1),
                  encode(rows)[len(PRELUDE):], b'extra\n' + encode(rows), encode(rows) + b'extra\n',
                  encode(rows, {**finish, 'retired': False}), encode(rows, {**finish, 'checks': True}),
                  encode(rows).replace(b'151 recipes', b'151 recipe', 1)])
    for index, raw in enumerate(cases):
        try:
            compare_native(raw, original)
        except (ValueError, KeyError, TypeError):
            continue
        raise AssertionError('native corruption accepted: ' + str(index))
    return {'status': 'PASS_DECODER_CONTROLS_ONLY', 'mutation_cases': len(cases),
            'native_processes': 0, 'synthetic_control_data_is_capture_evidence': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls-only', action='store_true')
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--detail', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    original = admit_original()
    if args.controls_only:
        need(not args.execute and not args.detail and args.executable is None and args.work_dir is None,
             'controls-only starts no native process')
        print(json.dumps(controls(original), indent=2))
        return
    need(args.execute and args.executable is not None and args.work_dir is not None,
         'explicit native release, executable and append-only output required')
    executable = args.executable.resolve(strict=True)
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='redstone-torch-texture-', dir=work))
    authored = [Path(__file__).resolve(), *SOURCE.rglob('*'),
                * (ROOT / 'tests/block/registry/redstone_torch').iterdir(),
                ROOT / 'tests/item/registry/test_runtime.c',
                ROOT / 'tests/item/registry/block_static_controls.inc',
                ROOT / 'tests/block/registry/static_index_cases.h',
                ROOT / 'tests/reference/client/controller/capture.py',
                ROOT / 'tests/tools/client/native_product_capture.py']
    inputs = {str(p.resolve()): C.digest(p) for p in authored if p.is_file()}

    command = [str(executable)] if args.detail else [str(executable), str(SOURCE / 'profiles.tsv')]
    report = {'status': 'FAIL', 'mode': 'candidate-only-detail' if args.detail else 'shared-public-scalar',
              'inputs_before': inputs, 'command': command}
    try:
        report['products'] = N.capture_native_products(command, output)
        receipt = C.capture(command, output, 'native', 90, os.environ.copy(), output)
        report['capture'] = receipt
        C.require_success(receipt, empty_stderr=True)
        need(type(receipt['exit']) is int and receipt['exit'] == 0, 'actual native return success')
        raw = (output / 'native.stdout').read_bytes()
        report['comparison'] = parse_detail(raw) if args.detail else compare_native(raw, original)
        report['status'] = 'PASS'
    except N.NativeProductCaptureError as error:
        report['product_failure'] = error.receipt
        report['error'] = str(error)
    except Exception as error:
        report['error'] = type(error).__name__ + ': ' + str(error)
    finally:
        try:
            admit_original()
            report['original_unchanged'] = True
            report['inputs_after'] = {name: C.digest(Path(name)) for name in inputs}
            report['inputs_unchanged'] = report['inputs_after'] == inputs
            products = report.get('products', {}).get('products', [])
            report['products_unchanged'] = bool(products) and all(
                C.digest(Path(item['source'])) == item['before']['sha256'] ==
                C.digest(output / item['artifact']) for item in products)
            if not report['inputs_unchanged'] or not report['products_unchanged']:
                report['status'] = 'FAIL'
        except Exception as error:
            report.update(status='FAIL', continuity_error=type(error).__name__ + ': ' + str(error))
        write(output / 'run.json', report)
    need(report['status'] == 'PASS', report.get('error', report.get('continuity_error', 'native qualification failed')))
    print(json.dumps({'status': report['status'], 'comparison': report['comparison'], 'report': str(output / 'run.json')}, indent=2))


if __name__ == '__main__':
    main()
