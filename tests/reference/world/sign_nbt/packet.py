"""Validate retained original Sign component observations without a JVM."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
RAW = {'main.stdout': (267675, '49424fd1624d8582be567130b1d4fef3bff26b0cfd8ce53f25b1d69823cf1411'),
       'null-lines.stdout': (24729, '9d34b8ab299312b0fd015f5640738d9f5b69ccdbe2b3caab6da4d622d6693167')}
FROZEN = {'SignNbtObservation.java': (23455, 'f52c6c9b5b4bd77ce4df4979e4605603b114590410f0c0a7a69795be4e15df47'),
          'NullSignWriteObservation.java': (3079, '07ab86f5c7cd12c2112ec1106a98e4b248f946a93b63d8c934ed1aafdc59b08c'),
          'inspect_classes.py': (4549, '81274b0dcf91e7f40f08194054f9ae678460ea4e2a118ac93c222ae9ece78475'),
          'source-pins.json': (54990, '727e72f95ad19082644a16c56ca8d65d8369965676cd990d71b176b10a089ffe')}
RECORDED = {'provenance.json': (22539, '86f4c5895022d93d526071c336a2a5442552218f9d00bc0a39df8ccbda1dc6e6'),
            'declaration.json': (1524, '7daec5a20d70922b397921dc10e653adb9731da424d54b4d28a8cd4987a70f74'),
            'audit-summary.json': (17228, 'a03421b06719d7179984f9e3a024accaead1d1dfcfcb30141e5f1a11f0a32518')}
FILES = {'.gitattributes', 'README.md', *RAW, *FROZEN, 'provenance.json', 'declaration.json',
         'audit-summary.json', 'packet.py', 'test_packet.py', 'capture_reference.py', 'publication.json'}
MAIN_CASES = ['constructor-defaults', 'missing-all-text', 'mixed-missing-text', 'length-14',
              'length-15', 'length-16', 'supplementary-inside-15', 'supplementary-cut-at-15',
              'literal-unpaired-units', 'color-and-spacing', 'missing-coordinates',
              'unknown-input-and-existing-output', *['late-wrong-text-' + str(i) for i in range(1, 5)],
              *['late-wrong-coordinate-' + c for c in 'xyz'], 'null-nbt-read', 'null-nbt-write',
              'writer-public-overlong-lines']
NULL_CASES = ['public-null-line-' + str(i) for i in range(4)]
CTOR, READ, WRITE = 'yk.<init>()V', 'yk.a(Lnu;)V', 'yk.b(Lnu;)V'
REF = re.compile(r'R(?:0|[1-9][0-9]{0,2})\Z')
HEX = re.compile(r'(?:[0-9a-f]{4})*\Z')
SHA = re.compile(r'[0-9a-f]{64}\Z')


def need(condition, message):
    if not condition:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def pin(raw):
    return {'bytes': len(raw), 'sha256': digest(raw)}


def unique(pairs):
    value = dict(pairs)
    need(len(value) == len(pairs), 'duplicate JSON key')
    return value


def parse(raw):
    def invalid(value):
        raise ValueError('nonfinite JSON number: ' + value)
    return json.loads(raw, object_pairs_hook=unique, parse_constant=invalid)


def integer(value, low=-(1 << 31), high=(1 << 31) - 1):
    need(type(value) is int and low <= value <= high, 'bounded integer')


def keys(value, expected):
    need(type(value) is dict and set(value) == set(expected), 'complete exact object keys')


def reference(value):
    need(type(value) is str and REF.fullmatch(value) is not None and int(value[1:]) < 512,
         'bounded output-only reference label')


def string(value, aliases):
    if value is None:
        return
    keys(value, ('ref', 'units', 'length'))
    reference(value['ref'])
    integer(value['length'], 0, 4096)
    need(type(value['units']) is str and HEX.fullmatch(value['units']) is not None and
         len(value['units']) == value['length'] * 4, 'complete unmodified UTF-16 units')
    previous = aliases.setdefault(value['ref'], (value['units'], value['length']))
    need(previous == (value['units'], value['length']), 'immutable String alias changed')


def state(value, aliases):
    keys(value, ('ref', 'class', 'world', 'xyz', 'invalid', 'lines_array', 'edit_row', 'private_c'))
    reference(value['ref'])
    need(value['class'] == 'yk' and value['world'] is None, 'actual component-only YK receiver')
    need(type(value['xyz']) is list and len(value['xyz']) == 3, 'all coordinates')
    for number in value['xyz']:
        integer(number)
    need(type(value['invalid']) is bool and type(value['private_c']) is bool, 'actual Boolean fields')
    integer(value['edit_row'])
    array = value['lines_array']
    keys(array, ('ref', 'length', 'items'))
    reference(array['ref'])
    need(type(array['length']) is int and array['length'] == 4 and
         type(array['items']) is list and len(array['items']) == 4, 'complete four-line array')
    for item in array['items']:
        string(item, aliases)


def tag(value, aliases, depth=0):
    if value is None:
        return
    need(depth <= 4 and type(value) is dict, 'complete bounded NBT graph')
    kind = value.get('type')
    need(type(kind) is int and kind in (1, 3, 8, 10), 'actual declared concrete NBT kind')
    keys(value, ('ref', 'class', 'type', 'name', *(
        ('encounter_key_units', 'logical_entries') if kind == 10 else ('value',))))
    reference(value['ref'])
    need(value['class'] == {1: 'qp', 3: 'pp', 8: 'xb', 10: 'nu'}[kind], 'concrete tag/Class agreement')
    string(value['name'], aliases)
    if kind == 10:
        encounter, entries = value['encounter_key_units'], value['logical_entries']
        need(type(encounter) is list and type(entries) is dict and len(entries) <= 32 and
             len(encounter) == len(entries) and len(set(encounter)) == len(encounter) and
             set(encounter) == set(entries) and list(entries) == sorted(entries),
             'complete separate raw Map encounter order and logical entries')
        for name, child in entries.items():
            need(type(name) is str and HEX.fullmatch(name) is not None, 'complete UTF-16 Map key')
            tag(child, aliases, depth + 1)
            need(child['name'] is not None and child['name']['units'] == name, 'actual tag name/key join')
    elif kind == 8:
        string(value['value'], aliases)
    else:
        integer(value['value'], -128 if kind == 1 else -(1 << 31), 127 if kind == 1 else (1 << 31) - 1)


def throwable(value, nodes=None):
    if value is None:
        return
    if nodes is None:
        nodes = []
    if set(value) == {'cycle_to'}:
        integer(value['cycle_to'], 0, len(nodes) - 1)
        return
    keys(value, ('node', 'class', 'message', 'frames', 'cause', 'suppressed'))
    need(type(value['node']) is int and value['node'] == len(nodes) and len(nodes) < 16,
         'complete Throwable graph IDs')
    nodes.append(value['node'])
    need(type(value['class']) is str and (value['message'] is None or type(value['message']) is str),
         'actual Throwable type/message')
    need(type(value['frames']) is list and len(value['frames']) <= 128 and
         type(value['suppressed']) is list and len(value['suppressed']) <= 16, 'complete Throwable extent')
    for frame in value['frames']:
        keys(frame, ('class', 'method', 'file', 'line', 'native', 'loader', 'module', 'module_version'))
        need(type(frame['class']) is str and type(frame['method']) is str and type(frame['native']) is bool,
             'actual stack-frame fields')
        integer(frame['line'])
        for field in ('file', 'loader', 'module', 'module_version'):
            need(frame[field] is None or type(frame[field]) is str, 'complete nullable stack metadata')
    throwable(value['cause'], nodes)
    for child in value['suppressed']:
        throwable(child, nodes)


def unit_key(value):
    return ''.join(f'{ord(char):04x}' for char in value)


def line_units(value):
    return [None if item is None else item['units'] for item in value['lines_array']['items']]


def call_semantics(row):
    before, after = row['before'], row['after']
    method = row['method']
    if method == CTOR:
        need(before is None and row['returned'] is True and row['failure'] is None and
             after['xyz'] == [0, 0, 0] and after['edit_row'] == -1 and after['invalid'] is False and
             after['private_c'] is True and line_units(after) == ['', '', '', ''] and
             len({item['ref'] for item in after['lines_array']['items']}) == 1, 'actual constructor defaults/aliases')
        return
    source, target = row['nbt_before'], row['nbt_after']
    need(before['ref'] == after['ref'] and before['world'] == after['world'] and
         before['invalid'] == after['invalid'] and before['edit_row'] == after['edit_row'] and
         before['lines_array']['ref'] == after['lines_array']['ref'], 'unchanged inherited/array fields')
    if method == READ:
        need(source == target and after['private_c'] is False, 'read-only input and early c=false prefix')
        expected_xyz, expected_lines = list(before['xyz']), line_units(before)
        failure = None
        if source is None:
            failure = 'java.lang.NullPointerException'
        else:
            entries = source['logical_entries']
            for index, name in enumerate('xyz'):
                item = entries.get(unit_key(name))
                if item is not None and item['type'] != 3:
                    failure = 'java.lang.ClassCastException'
                    break
                expected_xyz[index] = 0 if item is None else item['value']
            if failure is None:
                for index in range(4):
                    item = entries.get(unit_key('Text' + str(index + 1)))
                    if item is not None and item['type'] != 8:
                        failure = 'java.lang.ClassCastException'
                        break
                    units = '' if item is None else item['value']['units']
                    expected_lines[index] = units[:15 * 4]
        need(after['xyz'] == expected_xyz and line_units(after) == expected_lines,
             'ordered actual base/text assignments and UTF-16 substring prefix')
        need(row['returned'] is (failure is None) and
             (failure is None or row['failure']['class'] == failure), 'actual reader outcome')
    else:
        need(before == after, 'writer preserves every observed Tile field')
        if source is None:
            need(target is None and row['returned'] is False and
                 row['failure']['class'] == 'java.lang.NullPointerException', 'actual null-Compound writer')
            return
        entries = dict(source['logical_entries'])
        for name, number in zip('xyz', before['xyz']):
            actual = target['logical_entries'][unit_key(name)]
            need(actual['type'] == 3 and actual['value'] == number, 'actual base writer coordinates')
        actual_id = target['logical_entries'][unit_key('id')]
        need(actual_id['type'] == 8 and actual_id['value']['units'] == unit_key('Sign'), 'actual base writer ID')
        written = {unit_key(name) for name in ('id', 'x', 'y', 'z')}
        failure = None
        for index, item in enumerate(before['lines_array']['items']):
            if item is None:
                failure = 'java.lang.IllegalArgumentException'
                break
            name = unit_key('Text' + str(index + 1))
            written.add(name)
            actual = target['logical_entries'][name]
            need(actual['type'] == 8 and actual['value'] == item, 'full actual writer String/ref without truncation')
        need(row['returned'] is (failure is None), 'actual writer completion')
        if failure is not None:
            need(row['failure']['class'] == failure and row['failure']['message'] == 'Empty string not allowed' and
                 [(frame['class'], frame['method']) for frame in row['failure']['frames'][:3]] ==
                 [('xb', '<init>'), ('nu', 'a'), ('yk', 'b')], 'actual null-line failure/callee stack')
        actual = target['logical_entries']
        need(set(actual) == set(entries) | written, 'complete mutable target key set')
        for name in set(entries) - written:
            need(actual[name] == entries[name], 'unknown/current/later output fields survive failure prefix')


def read_rows(raw):
    need(type(raw) is bytes and len(raw) <= 4194304 and raw.endswith(b'\r\n') and
         raw.count(b'\n') == raw.count(b'\r\n'), 'complete original Windows CRLF stdout')
    rows = []
    for line in raw.decode('ascii').splitlines():
        kind, payload = line.split(' ', 1)
        need(kind in {'SIGN_NBT_' + word for word in ('BEGIN', 'CASE', 'CALL', 'PUBLIC_PRESTATE', 'COMPLETE')},
             'declared complete record kind')
        rows.append((kind[9:], parse(payload)))
    return rows


def validate_rows(rows, cohort, pins):
    main = cohort == 'main'
    cases = MAIN_CASES if main else NULL_CASES
    need(cohort in ('main', 'null-lines') and len(rows) == (126 if main else 17), 'complete fixed row census')
    need(rows[-1][0] == 'COMPLETE' and (rows[0][0] == 'BEGIN' if main else rows[0][0] == 'CASE'),
         'complete ordered capture boundaries')
    aliases, states, seen, calls, public = {}, {}, [], [], []
    current = None
    per_case = {}
    for index, (kind, row) in enumerate(rows):
        if kind == 'BEGIN':
            keys(row, ('schema', 'jar_sha256', 'scope', 'vm', 'observer'))
            need(main and index == 0 and type(row['schema']) is int and row['schema'] == 1 and row['jar_sha256'] == JAR and
                 row['vm'] == {'runtime': '17.0.17+10', 'vm': 'OpenJDK 64-Bit Server VM',
                               'vm_version': '17.0.17+10', 'os': 'Windows 10', 'arch': 'amd64'},
                 'actual archive/VM declaration')
        elif kind == 'CASE':
            keys(row, ('index', 'case', 'scope', 'reference_labels'))
            need(type(row['index']) is int and row['index'] == len(seen) and row['case'] == cases[len(seen)],
                 'ordered unique case/index join')
            current = row['case']
            seen.append(current)
            per_case[current] = []
            aliases, states = {}, {}
        elif kind == 'CALL':
            need(row['case'] == current and row['method'] in (CTOR, READ, WRITE), 'entered case/method join')
            expected = ('case', 'phase', 'method', 'before', 'returned', 'failure', 'after')
            keys(row, expected if row['method'] == CTOR else (*expected, 'nbt_before', 'nbt_after'))
            need(type(row['returned']) is bool and (row['failure'] is None) is row['returned'], 'explicit outcome/Throwable join')
            if row['before'] is not None:
                state(row['before'], aliases)
                need(states.get(row['before']['ref']) == row['before'], 'actual prior receiver continuation')
            state(row['after'], aliases)
            if row['method'] != CTOR:
                tag(row['nbt_before'], aliases)
                tag(row['nbt_after'], aliases)
            throwable(row['failure'])
            call_semantics(row)
            states[row['after']['ref']] = row['after']
            calls.append(row)
            per_case[current].append((row['phase'], row['method'], row['returned']))
        elif kind == 'PUBLIC_PRESTATE':
            keys(row, ('case', 'fields', 'source_call', 'before', 'after') if main else ('case', 'null_index', 'after'))
            need(row['case'] == current and current in ('writer-public-overlong-lines', *NULL_CASES),
                 'declared separate public component prestate only')
            state(row['after'], aliases)
            if main:
                need(row['fields'] == ['yk.a'] and row['source_call'] is False and
                     states.get(row['before']['ref']) == row['before'] and
                     [item['length'] for item in row['after']['lines_array']['items']] == [16, 31, 16, 16],
                     'declared public overlong array assignment')
            else:
                slot = int(current[-1])
                need(type(row['null_index']) is int and row['null_index'] == slot and
                     row['after']['xyz'] == [12, -30, 51] and
                     line_units(row['after']) == [None if i == slot else '' for i in range(4)],
                     'declared public null-line prestate')
            states[row['after']['ref']] = row['after']
            public.append(current)
        else:
            need(index == len(rows) - 1, 'single terminal completion')
    need(seen == cases and public == (['writer-public-overlong-lines'] if main else NULL_CASES),
         'complete fixed case/public-prestate inventory')
    for name in cases:
        ctor = [('constructor', CTOR, True)]
        roundtrip = [('write-fresh', WRITE, True), ('write-existing', WRITE, True),
                     ('roundtrip-constructor', CTOR, True), ('roundtrip-read', READ, True)]
        if not main:
            expected = ctor + [('write', WRITE, False)]
        elif name.startswith('late-wrong-'):
            expected = ctor + [('first-read', READ, True), ('second-read', READ, False)]
        elif name.startswith('null-nbt-'):
            expected = ctor + [('read-null', READ, False) if name.endswith('read') else ('write-null', WRITE, False)]
        else:
            expected = ctor + ([] if name in ('constructor-defaults', 'writer-public-overlong-lines') else [('read', READ, True)]) + roundtrip
        need(per_case[name] == expected, 'fixed direct-call phases without omitted failure/continuation')
    complete = rows[-1][1]
    keys(complete, ('schema', 'cases', 'case_count', 'constructor_calls', 'read_calls', 'write_calls',
                    'rows_before_complete', 'class_origins', 'guards', 'completed_script') if main else
         ('completed_script', 'cases', 'constructor_calls', 'write_calls', 'denied', 'class_origins'))
    need(complete['completed_script'] is True and complete['cases'] == cases and
         complete['constructor_calls'] == sum(row['method'] == CTOR for row in calls) == (35 if main else 4) and
         complete['write_calls'] == sum(row['method'] == WRITE for row in calls) == (27 if main else 4),
         'actual selected-call completion counters')
    if main:
        need(type(complete['schema']) is int and complete['schema'] == 1 and complete['case_count'] == 22 and complete['read_calls'] == 39 and
             complete['rows_before_complete'] == 125 and complete['guards'] == dict.fromkeys(
                 ('network', 'native_link', 'exec', 'write', 'delete', 'explicit_exit'), 0) and
             all(type(value) is int for value in complete['guards'].values()), 'complete guards/row counters')
    else:
        need(type(complete['denied']) is int and complete['denied'] == 0, 'zero supplemental external attempts')
    origins = complete['class_origins']
    need(set(origins) == ({'yk', 'ow', 'nu', 'ij', 'pp', 'xb', 'qp'} if main else {'yk', 'ow', 'nu', 'ij', 'xb', 'pp'}),
         'all actually observed Class origins; static classes are separate')
    for name, origin in origins.items():
        need(origin == {'bytes': pins['classes'][name]['class_bytes'],
                        'sha256': pins['classes'][name]['class_sha256']}, 'actual unchanged archive Class origin')
    return {'cases': len(cases), 'calls': len(calls), 'rows': len(rows),
            'failed_calls': sum(not row['returned'] for row in calls), 'observed_classes': sorted(origins)}


def files(directory=HERE):
    return {path.name: path.read_bytes() for path in directory.iterdir() if path.is_file()}


def healthy(receipt):
    need(receipt['exit'] == 0 and type(receipt['exit']) is int and receipt['timed_out'] is False and
         receipt['pipe_capture_complete'] is True and receipt['reader_errors'] == {} and
         receipt['output_overflow'] is False and receipt['timeout_seconds'] == 60 and
         receipt['stdout_limit'] == 4194304 and receipt['stderr_limit'] == 1048576, 'complete bounded transport receipt')


def audit(blobs):
    need(set(blobs) == FILES, 'exact public packet files; no classes/JAR/assets')
    manifest = parse(blobs['publication.json'])
    keys(manifest, ('schema', 'files'))
    need(type(manifest['schema']) is int and manifest['schema'] == 1 and
         set(manifest['files']) == FILES - {'publication.json'}, 'complete publication manifest')
    for name in FILES - {'publication.json'}:
        need(pin(blobs[name]) == manifest['files'][name], 'published file bytes: ' + name)
    for name, (length, sha) in {**RAW, **FROZEN, **RECORDED}.items():
        need(pin(blobs[name]) == {'bytes': length, 'sha256': sha}, 'frozen observed bytes: ' + name)
    pins = parse(blobs['source-pins.json'])
    need(pins['client_jar_sha256'] == JAR and len(pins['classes']) == 14 and
         sum('code_sha256' in method for value in pins['classes'].values() for method in value['methods']) == 114 and
         pins['inspector_sha256'] == FROZEN['inspect_classes.py'][1], 'complete static archive/Code inventory')
    result = {name: validate_rows(read_rows(blobs[name + '.stdout']), name, pins) for name in ('main', 'null-lines')}
    declaration = parse(blobs['declaration.json'])
    need(declaration['main_cases'] == MAIN_CASES and declaration['null_line_cases'] == NULL_CASES and
         declaration['main_calls'] == {'constructors': 35, 'reads': 39, 'writes': 27} and
         declaration['supplement_calls'] == {'constructors': 4, 'reads': 0, 'writes': 4} and
         declaration['public_prestate_cases'] == ['writer-public-overlong-lines', *NULL_CASES], 'fixed authored declaration')
    provenance = parse(blobs['provenance.json'])
    need(provenance['schema'] == 1 and provenance['status'] == 'PASS' and
         provenance['hidden_jvm_option_environment'] == dict.fromkeys(
             ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'), 'absent-or-empty') and
         [cohort['name'] for cohort in provenance['cohorts']] == ['main', 'null-lines'], 'actual declared process cohorts')
    for cohort in provenance['cohorts']:
        name = cohort['name']
        need(cohort['status'] == 'PASS' and cohort['raw_outputs_equal'] is True and
             (cohort['cases_per_vm'], cohort['selected_calls_per_vm'], cohort['rows_per_vm']) ==
             ((22, 101, 126) if name == 'main' else (4, 8, 17)), 'cohort inventory')
        inputs = cohort['inputs']
        need(len(inputs) == (12 if name == 'main' else 11) and all(
            value['before'] == value['after'] for value in inputs.values()), 'all retained input brackets')
        need(inputs['b1.7.3.jar']['before']['sha256'] == JAR and inputs['source-pins-v2.json']['before'] == pin(blobs['source-pins.json']),
             'actual JAR/static input identities')
        for source in ('SignNbtObservation.java', 'inspect_classes.py', *(() if name == 'main' else ('NullSignWriteObservation.java',))):
            need(inputs[source]['before'] == pin(blobs[source]), 'actual executed authored source input')
        products = cohort['compiled_helper_products']
        expected_products = {'SignNbtObservation.class', 'SignNbtObservation$1.class', 'SignNbtObservation$SourceCall.class'}
        if name != 'main':
            expected_products |= {'NullSignWriteObservation.class', 'NullSignWriteObservation$1.class'}
        need(set(products) == expected_products and all(type(value['bytes']) is int and value['bytes'] > 0 and
             SHA.fullmatch(value['sha256']) for value in products.values()), 'actual helper-only compiled inventory')
        healthy(cohort['compiler'])
        need(cohort['compiler']['stdout_bytes'] == cohort['compiler']['stderr_bytes'] == 0 and
             cohort['compiler']['stdout_sha256'] == cohort['compiler']['stderr_sha256'] == digest(b''), 'complete empty compiler streams')
        sources = ['SignNbtObservation.java'] + ([] if name == 'main' else ['NullSignWriteObservation.java'])
        expected_compile = ['SELECTED_JAVAC', '--release', '17', *(['-encoding', 'UTF-8'] if name == 'main' else []),
                            '-cp', 'UNCHANGED_CLIENT_JAR', '-d', 'AUTHORED_CLASSES', *sources]
        need(cohort['compiler']['command_roles'] == expected_compile, 'actual compiler arguments')
        processes = cohort['processes']
        need(len(processes) == 4 and [row['mode'] for row in processes] == ['interpreter', 'default'] * 2,
             'four fresh ordinary VMs in actual order')
        for row in processes:
            healthy(row)
            need({'bytes': row['stdout_bytes'], 'sha256': row['stdout_sha256']} == pin(blobs[name + '.stdout']) and
                 row['stdout_artifact'] == name + '.stdout' and row['stderr_artifact'] is None and
                 row['stderr_bytes'] == processes[0]['stderr_bytes'] and row['stderr_sha256'] == processes[0]['stderr_sha256'],
                 'all four actual raw stream hashes and lengths')
            expected_command = ['SELECTED_JAVA', '-Xmx128m', '-XX:-UsePerfData',
                                *(['-Xint'] if row['mode'] == 'interpreter' else []), '-cp',
                                'AUTHORED_CLASSES;UNCHANGED_CLIENT_JAR',
                                'SignNbtObservation' if name == 'main' else 'NullSignWriteObservation', 'UNCHANGED_CLIENT_JAR']
            need(row['command_roles'] == expected_command, 'ordinary actual runtime command without extra flags')
        need(len(cohort['stderr_warning_lines_with_private_uri_replaced']) == 4 and all(
            line.startswith('WARNING:') for line in cohort['stderr_warning_lines_with_private_uri_replaced']), 'declared redacted warning text only')
    audit_report = parse(blobs['audit-summary.json'])['audits']
    need(set(audit_report) == {'independent-capture-audit-v1.json', 'null-write-root-prefix-audit.json'} and all(
        value['sanitized_report']['status'] == 'PASS' for value in audit_report.values()), 'both actual retained-byte audits')
    main_audit = audit_report['independent-capture-audit-v1.json']['sanitized_report']
    need(main_audit['findings'] == [] and main_audit['inputs_rehashed'] == 12 and main_audit['products_rehashed'] == 3 and
         main_audit['processes']['fresh_vms'] == 4 and main_audit['processes']['raw_outputs_equal'] is True,
         'independent retained-byte audit scope')
    return {'status': 'PASS', 'scope': 'Original Sign component packet only; no JVM/native/game execution', 'cohorts': result}


if __name__ == '__main__':
    print(json.dumps(audit(files()), sort_keys=True))
