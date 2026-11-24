"""Offline Sign packet integrity and semantic corruption controls."""
import copy
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('sign_nbt_packet_controls', HERE / 'packet.py')
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def reject(operation, name):
    try:
        operation()
    except (ValueError, KeyError, TypeError, IndexError, UnicodeError):
        return name
    raise RuntimeError('corruption accepted: ' + name)


def encoded(value):
    return (json.dumps(value, indent=2) + '\n').encode()


def repaired(blobs, name, raw):
    changed = dict(blobs)
    changed[name] = raw
    manifest = P.parse(changed['publication.json'])
    manifest['files'][name] = P.pin(raw)
    changed['publication.json'] = encoded(manifest)
    return changed


def row(rows, name, phase):
    return next(value for kind, value in rows if kind == 'CALL' and value['case'] == name and value['phase'] == phase)


def main():
    blobs = P.files()
    result = P.audit(blobs)
    rejected = []
    for name in ('main.stdout', 'null-lines.stdout', 'SignNbtObservation.java', 'source-pins.json'):
        bad = dict(blobs)
        bad[name] += b' '
        rejected.append(reject(lambda: P.audit(bad), 'unrepaired published bytes: ' + name))
        bad = repaired(blobs, name, bad[name])
        rejected.append(reject(lambda: P.audit(bad), 'repaired manifest cannot alter observed bytes: ' + name))
    for name in ('provenance.json', 'declaration.json', 'audit-summary.json'):
        bad = repaired(blobs, name, blobs[name] + b'\n')
        rejected.append(reject(lambda: P.audit(bad), 'repaired manifest cannot rewrite historical metadata: ' + name))
    bad = dict(blobs)
    del bad['main.stdout']
    rejected.append(reject(lambda: P.audit(bad), 'missing original cohort'))
    bad = dict(blobs)
    bad['original.class'] = b'not an admitted public artifact'
    rejected.append(reject(lambda: P.audit(bad), 'unexpected binary artifact'))
    rejected.append(reject(lambda: P.parse(b'{"x":1,"x":2}'), 'duplicate JSON key'))
    rejected.append(reject(lambda: P.parse(b'{"x":NaN}'), 'nonfinite JSON number'))
    rejected.append(reject(lambda: P.read_rows(blobs['main.stdout'][:-2]), 'missing terminal line ending'))
    rejected.append(reject(lambda: P.read_rows(blobs['main.stdout'].replace(b'\r\n', b'\n')), 'changed actual transport line endings'))
    provenance = P.parse(blobs['provenance.json'])
    for field, value in (('exit', 1), ('exit', False), ('timed_out', True),
                         ('pipe_capture_complete', False), ('reader_errors', {'stdout': 'read failed'}),
                         ('output_overflow', True), ('timeout_seconds', 61),
                         ('stdout_limit', 8388608), ('stderr_limit', 2097152)):
        bad = copy.deepcopy(provenance['cohorts'][0]['processes'][0])
        bad[field] = value
        rejected.append(reject(lambda: P.healthy(bad), 'direct transport receipt refusal: ' + field + '=' + repr(value)))
    pins = P.parse(blobs['source-pins.json'])
    main_rows = P.read_rows(blobs['main.stdout'])
    null_rows = P.read_rows(blobs['null-lines.stdout'])

    def semantic(name, change, cohort='main'):
        rows = copy.deepcopy(main_rows if cohort == 'main' else null_rows)
        change(rows)
        rejected.append(reject(lambda: P.validate_rows(rows, cohort, pins), name))

    semantic('omitted actual call', lambda rows: rows.pop(3))
    semantic('reordered call phases', lambda rows: rows.__setitem__(slice(2, 4), [rows[3], rows[2]]))
    semantic('wrong case index', lambda rows: rows[1][1].__setitem__('index', 1))
    semantic('Boolean substituted for integer case index', lambda rows: rows[1][1].__setitem__('index', False))
    semantic('undeclared record field', lambda rows: rows[1][1].__setitem__('hidden', 'not observed'))
    semantic('early reader c assignment removed', lambda rows: row(rows, 'null-nbt-read', 'read-null')['after'].__setitem__('private_c', True))
    semantic('wrong coordinate failure prefix', lambda rows: row(rows, 'late-wrong-coordinate-z', 'second-read')['after']['xyz'].__setitem__(0, 101))
    semantic('wrong text failure suffix', lambda rows: row(rows, 'late-wrong-text-2', 'second-read')['after']['lines_array']['items'].__setitem__(2, None))
    semantic('wrong 15-unit split surrogate prefix', lambda rows: row(rows, 'supplementary-cut-at-15', 'read')['after']['lines_array']['items'][0].__setitem__('units', '0051' * 14 + 'de00'))
    semantic('incomplete String length', lambda rows: row(rows, 'length-16', 'read')['after']['lines_array']['items'][0].__setitem__('length', 16))
    semantic('writer truncates the actual public overlong line', lambda rows: row(rows, 'writer-public-overlong-lines', 'write-fresh')['nbt_after']['logical_entries'][P.unit_key('Text1')]['value'].__setitem__('length', 15))
    semantic('wrong read return/Throwable join', lambda rows: row(rows, 'late-wrong-text-1', 'second-read').__setitem__('returned', True))
    semantic('missing raw Map encounter key', lambda rows: row(rows, 'constructor-defaults', 'write-fresh')['nbt_after']['encounter_key_units'].pop())
    semantic('wrong original tag Class', lambda rows: row(rows, 'constructor-defaults', 'write-fresh')['nbt_after']['logical_entries'][P.unit_key('Text1')].__setitem__('class', 'pp'))
    semantic('constructor empty aliases changed', lambda rows: row(rows, 'constructor-defaults', 'constructor')['after']['lines_array']['items'][1].__setitem__('ref', 'R40'))
    semantic('public prestate disguised as Source call', lambda rows: next(value for kind, value in rows if kind == 'PUBLIC_PRESTATE').__setitem__('source_call', True))
    semantic('wrong guard attempt count', lambda rows: rows[-1][1]['guards'].__setitem__('network', 1))
    semantic('Boolean substituted for guard count', lambda rows: rows[-1][1]['guards'].__setitem__('network', False))
    semantic('missing actual Class origin', lambda rows: rows[-1][1]['class_origins'].pop('yk'))
    semantic('changed original Class identity', lambda rows: rows[-1][1]['class_origins']['yk'].__setitem__('sha256', '0' * 64))
    semantic('shortened terminal call count', lambda rows: rows[-1][1].__setitem__('constructor_calls', 34))
    semantic('wrong null-line Throwable', lambda rows: row(rows, 'public-null-line-2', 'write')['failure'].__setitem__('class', 'java.lang.NullPointerException'), 'null-lines')
    semantic('wrong null-line message', lambda rows: row(rows, 'public-null-line-2', 'write')['failure'].__setitem__('message', None), 'null-lines')
    semantic('wrong actual callee stack', lambda rows: row(rows, 'public-null-line-2', 'write')['failure']['frames'][0].__setitem__('class', 'yk'), 'null-lines')
    semantic('null writer changed Tile', lambda rows: row(rows, 'public-null-line-2', 'write')['after'].__setitem__('edit_row', 0), 'null-lines')
    semantic('null writer replaced later seeded text', lambda rows: row(rows, 'public-null-line-2', 'write')['nbt_after']['logical_entries'].pop(P.unit_key('Text4')), 'null-lines')
    semantic('public null slot declaration changed', lambda rows: next(value for kind, value in rows if kind == 'PUBLIC_PRESTATE').__setitem__('null_index', 3), 'null-lines')
    semantic('supplement denied attempt hidden', lambda rows: rows[-1][1].__setitem__('denied', 1), 'null-lines')
    result.update(corruption_controls=len(rejected), rejected=rejected,
                  scope='Offline historical packet and corruption controls only; no JVM/native/game execution')
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
