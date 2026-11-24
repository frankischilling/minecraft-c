"""Pure original/native Dispenser component decoding; never launches a process."""

from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import re
import types

SCOPE = 'GENUINE_SH_COMPONENT_ONLY_NO_SUCCESSFUL_GS_SHIFT_QUALIFICATION'

CORE = ('sh', 'dw', 'gp', 'ix', 'az', 'ow', 'iz', 'lw', 'gs', 'js')

LIBRARY_SHAS = ('c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5')

ROW_KEYS = {'ordinal', 'case', 'operation', 'player_present', 'az_present', 'slot', 'button',
            'layout', 'base', 'before', 'returned', 'result', 'throwable', 'after'}

ORIGINAL_ONLY = ('shift-null-gs-left', 'shift-null-gs-right')

MAX_STREAM = 4 * 1024 * 1024

def need(value, message):
    if not value:
        raise ValueError(message)

def pin(path):
    path = Path(path)
    size = path.stat().st_size
    need(size <= 512 * 1024 * 1024, 'Bounded ordinary input file')
    sha = hashlib.sha256()
    count = 0
    with path.open('rb') as stream:
        while True:
            block = stream.read(65536)
            if not block:
                break
            count += len(block)
            need(count <= 512 * 1024 * 1024, 'Bounded complete input stream')
            sha.update(block)
    need(count == size, 'Stable complete file length during pin')
    return {'bytes': count, 'sha256': sha.hexdigest()}

def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result

def parse(raw):
    def bad(value):
        raise ValueError('Nonfinite JSON: ' + value)
    return json.loads(raw, object_pairs_hook=unique, parse_constant=bad)

def bounded(path, maximum=MAX_STREAM):
    with Path(path).open('rb') as stream:
        raw = stream.read(maximum + 1)
    need(len(raw) <= maximum, 'Bounded complete file: ' + str(path))
    return raw

def closed(value, names, label):
    need(type(value) is dict and set(value) == set(names), 'Closed ' + label)

def equal(actual, expected, label):
    # JSON's bool/int equivalence must not turn a wrong typed receipt into a join.
    need(type(actual) is type(expected), 'Exact type: ' + label)
    if type(expected) is dict:
        closed(actual, expected, label)
        for key in expected:
            equal(actual[key], expected[key], label + '.' + key)
    elif type(expected) is list:
        need(len(actual) == len(expected), 'Exact extent: ' + label)
        for index, item in enumerate(expected):
            equal(actual[index], item, label + '[' + str(index) + ']')
    else:
        need(actual == expected, 'Exact value: ' + label)

def declarations(raw):
    need(type(raw) is bytes and 0 < len(raw) <= 8192 and raw.endswith(b'\n') and b'\r' not in raw,
         'Complete LF declaration')
    rows = []
    for line in raw.decode('ascii').splitlines():
        parts = line.split('\t')
        need(len(parts) == 6 and parts[1] in ('0', '1') and parts[2] in ('0', '1'), 'Case declaration')
        need(parts[3] in ('layout', 'getter', 'usable', 'click'), 'Bounded operation')
        need(re.fullmatch('[a-z0-9-]+', parts[0]) is not None, 'Case identifier')
        rows.append({'case': parts[0], 'player_present': parts[1] == '1', 'az_present': parts[2] == '1',
                     'operation': parts[3], 'slot': int(parts[4]), 'button': int(parts[5])})
    need(len(rows) == 18 and len({row['case'] for row in rows}) == 18, 'Exact unique 18 cases')
    return rows

def expected_layout(case):
    result = []
    for index in range(45):
        if index < 9:
            n, x, y, owner = index, 62 + index % 3 * 18, 17 + index // 3 * 18, 'AZ'
        elif index < 36:
            n, x, y, owner = index, 8 + (index - 9) % 9 * 18, 84 + (index - 9) // 9 * 18, 'IX'
        else:
            n, x, y, owner = index - 36, 8 + (index - 36) * 18, 142, 'IX'
        present = case['az_present'] if owner == 'AZ' else case['player_present']
        result.append({'index': index, 'owner': owner if present else None,
                       'inventory_index': n, 'x': x, 'y': y, 'class': 'gp'})
    return result

def expected_snapshot(case):
    tile = player = None
    stacks = {}
    seeded = case['operation'] != 'layout'
    if case['az_present']:
        slots = [None] * 9
        if seeded:
            slots[0] = 'A'
            stacks['A'] = [1, 7, 3, 0]
        tile = {'slots': slots, 'world_null': True, 'random_constructed': True,
                'random_class': 'java.util.Random'}
    if case['player_present']:
        main = [None] * 36
        if seeded:
            main[9], main[0] = 'B', 'H'
            stacks.update(B=[4, 6, 2, 0], H=[5, 2, 1, 0])
        player = {'main': main, 'armor': [None] * 4, 'cursor': None, 'selected': 0,
                  'changed': False, 'player_null': True}
    return {'tile': tile, 'player': player, 'stacks': stacks}

def expected_failure(case):
    name = case['case']
    if name in ('get-negative', 'get-end'):
        return 'java.lang.IndexOutOfBoundsException', [('java.util.ArrayList', 'get'), ('dw', 'a')]
    if name in ('get-null-az', 'get-null-ix'):
        return 'java.lang.NullPointerException', [('gp', 'a'), ('dw', 'a')]
    if name == 'usable-null-az':
        return 'java.lang.NullPointerException', [('sh', 'b')]
    if name == 'usable-null-world':
        return 'java.lang.NullPointerException', [('az', 'a_'), ('sh', 'b')]
    if name in ORIGINAL_ONLY:
        return 'java.lang.NullPointerException', [('dw', 'a')]
    return None

def throwable_graph(value, expected):
    if expected is None:
        equal(value, None, 'No Throwable on actual return')
        return
    closed(value, ('root', 'nodes'), 'Throwable graph')
    equal(value['root'], 0, 'Throwable root')
    need(type(value['nodes']) is list and len(value['nodes']) == 1, 'Exact complete simple actual Throwable graph')
    node = value['nodes'][0]
    closed(node, ('id', 'class', 'message', 'cause', 'suppressed', 'frames'), 'Throwable node')
    equal(node['id'], 0, 'Throwable node ID')
    equal(node['class'], expected[0], 'Actual Throwable class')
    need(node['message'] is None or type(node['message']) is str, 'Raw Throwable message retained')
    equal(node['cause'], None, 'No introduced cause')
    equal(node['suppressed'], [], 'No introduced suppression')
    frames = node['frames']
    need(type(frames) is list and 1 <= len(frames) <= 128, 'Bounded complete stack')
    for frame in frames:
        closed(frame, ('class', 'method', 'file', 'line', 'native'), 'Stack frame')
        need(all(type(frame[k]) is str for k in ('class', 'method')) and
             (frame['file'] is None or type(frame['file']) is str) and
             type(frame['line']) is int and type(frame['native']) is bool, 'Raw actual stack frame types')
    actual = [(frame['class'], frame['method']) for frame in frames]
    expected_chain = expected[1]
    # JDK index checks legitimately precede ArrayList.get, but no extra original
    # frame is admitted. Source PCs are a separate static-Code attribution.
    source = [(c, m) for c, m in actual if c in CORE]
    equal(source, [(c, m) for c, m in expected_chain if c in CORE], 'Exact reached Source stack')
    positions = []
    for pair in expected_chain:
        matches = [index for index, item in enumerate(actual) if item == pair]
        need(len(matches) == 1, 'Exactly one expected actual stack frame ' + repr(pair))
        positions.append(matches[0])
    need(positions == sorted(positions), 'Actual exception frame order')
    equal(actual[-2:], [('DispenserContainerObservation', 'observe'),
                       ('DispenserContainerObservation', 'main')], 'Actual observer boundary stack')
    need(all(c in CORE or c == 'DispenserContainerObservation' or c.startswith(('java.', 'jdk.'))
             for c, _ in actual), 'No replacement source/callback frame')

def validate_row(row, case, ordinal):
    closed(row, ROW_KEYS, 'SH row')
    for key, value in case.items():
        equal(row[key], value, 'Case ' + key)
    equal(row['ordinal'], ordinal, 'Ordered case ordinal')
    equal(row['layout'], expected_layout(case), 'Actual 45 GP layout and owner identities')
    equal(row['base'], {'window': 0, 'transaction': 0, 'slots': 45, 'snapshots': 45,
          'snapshots_null': True, 'listeners': 0, 'blocked': 0,
          'collections_distinct': True, 'captured_az_same': True}, 'DW base constructor fields')
    expected = expected_snapshot(case)
    equal(row['before'], expected, 'Genuine constructor/public-set prestate')
    equal(row['after'], row['before'], 'No state change on reached operation')
    failure = expected_failure(case)
    equal(row['returned'], failure is None, 'Actual return/failure')
    wanted = {'get-az': 'A', 'get-ix-main': 'B', 'get-ix-hotbar': 'H'}.get(case['case'])
    equal(row['result'], wanted, 'Returned exact input Stack alias, empty or failed result')
    throwable_graph(row['throwable'], failure)

def read_rows(raw, declared, pins, jar):
    need(type(raw) is bytes and len(raw) <= MAX_STREAM and raw.endswith(b'\n'), 'Complete bounded stdout')
    lines = raw.decode('utf8', errors='strict').splitlines()
    need(len(lines) == 21 and lines[0].startswith('SH_ORIGINS ') and
         all(line.startswith('SH_ROW ') for line in lines[1:19]) and
         lines[19].startswith('SH_GUARD ') and lines[20].startswith('SH_COMPLETE '),
         'Exact ordered stdout protocol; no skipped/retried/preamble rows')
    origin = parse(lines[0][len('SH_ORIGINS '):])
    equal(origin, {'jar': str(Path(jar).resolve(strict=True)), 'jar_sha256': pins['jar']['sha256'],
                  'classes': {key: {'bytes': pins['classes'][key]['class_bytes'],
                                   'sha256': pins['classes'][key]['class_sha256']} for key in CORE}},
          'Actual original class resources/common source archive')
    rows = [parse(line[len('SH_ROW '):]) for line in lines[1:19]]
    cases = declarations(declared)
    for index, (row, case) in enumerate(zip(rows, cases)):
        validate_row(row, case, index)
    equal(parse(lines[19][len('SH_GUARD '):]), {'network': 0, 'native_links': 0, 'execs': 0,
          'writes': 0, 'deletes': 0, 'replacements': 0, 'installed': True}, 'Actual guard counters')
    equal(parse(lines[20][len('SH_COMPLETE '):]), {'cases': 18,
          'declaration_sha256': hashlib.sha256(declared).hexdigest(), 'scope': SCOPE}, 'Terminal scope/declaration')
    return rows

def warnings(classes):
    uri = Path(classes).resolve().as_uri().replace('file:///', 'file:/') + '/'
    return ['WARNING: A terminally deprecated method in java.lang.System has been called',
        'WARNING: System::setSecurityManager has been called by DispenserContainerObservation (' + uri + ')',
        'WARNING: Please consider reporting this to the maintainers of DispenserContainerObservation',
        'WARNING: System::setSecurityManager will be removed in a future release']

def common_projection(row):
    """Explicit native comparison domain; raw Java-only diagnostics stay retained."""
    need(row['case'] not in ORIGINAL_ONLY, 'No null-GS click promotion to native GS callback qualification')
    snapshot = parse(json.dumps(row['before']))
    if snapshot['tile'] is not None:
        del snapshot['tile']['random_class']
    return {'ordinal': row['ordinal'], 'case': row['case'], 'layout': row['layout'], 'base': row['base'],
            'before': snapshot, 'after': snapshot, 'returned': row['returned'], 'result': row['result'],
            'exception_class': None if row['throwable'] is None else row['throwable']['nodes'][0]['class']}

def projection_join(rows, raw):
    # Input must be separately tied to a completed native process with stable
    # real linked products/compile inputs. This pure function grants no receipt.
    need(len(raw) <= MAX_STREAM and raw.endswith(b'\n'), 'Complete bounded native projection')
    lines = raw.decode('utf8', errors='strict').splitlines()
    expected = [common_projection(row) for row in rows if row['case'] not in ORIGINAL_ONLY]
    need(len(lines) == 17 and lines[-1] == 'SH_NATIVE_COMPLETE 16', 'Exact native terminal/inventory')
    actual = []
    for line in lines[:-1]:
        need(line.startswith('SH_NATIVE_ROW '), 'Native projection protocol')
        actual.append(parse(line[len('SH_NATIVE_ROW '):]))
    equal(actual, expected, 'Actual native component projection')
    return {'rows': len(actual), 'original_only_cases': list(ORIGINAL_ONLY),
            'scope': 'COMPONENT_PROJECTION_ONLY_REQUIRES_INDEPENDENT_NATIVE_PROCESS_AND_PRODUCT_RECEIPTS'}

R = types.SimpleNamespace(**globals())

PREFIX = [b'151 recipes', b'16 achievements']

NONE_SITE = {'class': None, 'method': None, 'descriptor': None, 'pc': -1}

def site(owner, method, descriptor, pc):
    return {'class': owner, 'method': method, 'descriptor': descriptor, 'pc': pc}

def empty_registry(sentinel=0):
    return {'status': 0, 'message': None, 'initializer_pc': sentinel, 'block_id': sentinel,
            'constructor_class': None, 'constructor_pc': sentinel, 'profile_status': 0,
            'active_initializer_class': None, 'active_initializer_pc': sentinel,
            'item_argument': sentinel, 'format_error': {'status': 0, 'offset': 0, 'length': 0,
            'value': 0, 'flags': 0, 'conversion': 0, 'date_time': False}}

def empty_inventory():
    return {'status': 0, 'operation': None, 'source_exception': False, 'exception_class': None,
            'frames': [], 'mutation_reached': False, 'notification_reached': False,
            'stack_status': 0, 'stack': {'status': 0, 'field': None, 'nbt': {'status': 0, 'offset': 0}},
            'registry_status': 0, 'registry': empty_registry(), 'matching_status': 0,
            'chest_present': False, 'furnace_present': False, 'dispenser_present': False}

def validate_inventory(value, status, frames, exception):
    expected = empty_inventory()
    expected.update(status=status, frames=frames, source_exception=exception is not None,
                    exception_class=exception)
    R.closed(value, expected, 'Complete current inventory error fields')
    # Actual human-readable operation text is retained exactly in raw evidence;
    # it is not a Java Throwable message or a cross-language equality claim.
    R.need(value['operation'] is None or type(value['operation']) is str, 'Actual operation text')
    if status != 0:
        R.need(type(value['operation']) is str and value['operation'], 'Reached failure operation text')
    expected['operation'] = value['operation']
    R.equal(value, expected, 'Actual source diagnostics and absent descendant causes')

def validate_detail(value, original):
    R.closed(value, {'ordinal', 'case', 'status', 'has_call', 'usable_sentinel', 'inventory',
        'interaction', 'call_state', 'constructor_pc', 'inventory_calls', 'cursor_calls'}, 'Native detail')
    name = original['case']
    operation = original['operation']
    failure = original['throwable'] is not None
    status = (3 if name in ('get-negative', 'get-end') else 2) if failure else 0
    has_call = operation in ('getter', 'click')
    exception = None if not failure else original['throwable']['nodes'][0]['class'].replace('.', '/')
    for key, expected in {'ordinal': original['ordinal'], 'case': name, 'status': status,
            'has_call': has_call, 'usable_sentinel': True, 'constructor_pc': 173,
            'inventory_calls': 0, 'cursor_calls': 0}.items():
        R.equal(value[key], expected, 'Native detail ' + key)
    frames = []
    if operation == 'usable':
        frames = [site('sh', 'b', '(Lgs;)Z', 5)]
        if name == 'usable-null-world':
            frames.append(site('az', 'a_', '(Lgs;)Z', 16))
    validate_inventory(value['inventory'], status if operation == 'usable' else 0,
                       frames, exception if operation == 'usable' else None)
    interaction = value['interaction']
    R.closed(interaction, {'status', 'operation', 'exception_class', 'native_policy', 'source_exception',
        'source_error', 'site', 'child', 'dependency', 'transfer', 'inventory', 'registry', 'stack'}, 'Interaction error')
    target_site = child = transfer = NONE_SITE
    if operation == 'getter':
        pc = 5 if status == 3 else 19 if failure else 22
        target_site = transfer = site('dw', 'a', '(I)Liz;', pc)
        child = site('dw', 'a', '(I)Liz;', 5) if status == 3 else site('gp', 'a', '()Liz;', 8 if failure else 13)
    elif operation == 'click':
        # Current native instrumentation records the reached branch at3 before
        # its early return. This is not a claim of observed original return694.
        target_site = site('dw', 'a', '(IIZLgs;)Liz;', 3)
    wanted = {'status': status if has_call else 0, 'exception_class': exception if has_call else None,
        'native_policy': not (has_call and failure), 'source_exception': has_call and failure,
        'source_error': False, 'site': target_site, 'child': child, 'dependency': NONE_SITE,
        'transfer': transfer, 'registry': empty_registry(-1), 'stack': 0}
    for key, expected in wanted.items():
        R.equal(interaction[key], expected, 'Actual interaction ' + key)
    R.need(interaction['operation'] is None or type(interaction['operation']) is str, 'Raw interaction operation')
    if has_call and failure:
        R.need(type(interaction['operation']) is str and interaction['operation'], 'Actual failed interaction operation')
    validate_inventory(interaction['inventory'], 0, [], None)
    R.equal(value['call_state'], {'returned': has_call and not failure, 'active': False,
        'source_pc': target_site['pc'] if has_call else 0, 'shift_calls': 1 if operation == 'getter' else 0,
        'drop_calls': 0, 'steps': 1 if operation == 'click' else 0, 'cursor_cleared': False}, 'Actual completed call state')

def prior_join(original_rows, stdout, stderr):
    R.need(type(stdout) is bytes and len(stdout) <= R.MAX_STREAM and stdout.endswith(b'\n') and
           type(stderr) is bytes and len(stderr) <= R.MAX_STREAM and stderr.endswith(b'\n'), 'Bounded complete native streams')
    # Exact observed Registry fixture prefix remains part of the physical raw
    # stdout; only this explicitly reported representation split is admitted.
    lines = stdout.splitlines()
    R.equal(lines[:2], PREFIX, 'Exact actual native Registry bootstrap prefix')
    projection = b'\n'.join(lines[2:]) + b'\n'
    summary = R.projection_join(original_rows, projection)
    cases = [row for row in original_rows if row['case'] not in R.ORIGINAL_ONLY]
    detail_lines = stderr.decode('utf8', errors='strict').splitlines()
    R.need(len(detail_lines) == 33 and detail_lines[-1] == 'SH_NATIVE_DETAIL_COMPLETE 16', 'Exact complete diagnostic and retirement inventory')
    details = []
    for index, original in enumerate(cases):
        line, retirement = detail_lines[index * 2:index * 2 + 2]
        R.need(line.startswith('SH_NATIVE_DETAIL ') and retirement.startswith('SH_NATIVE_RETIRE '), 'Exact ordered native diagnostic protocol')
        value = R.parse(line[len('SH_NATIVE_DETAIL '):])
        validate_detail(value, original)
        R.equal(R.parse(retirement[len('SH_NATIVE_RETIRE '):]),
                {'ordinal': original['ordinal'], 'case': original['case'], 'retired': True}, 'Actual owner retirement')
        details.append(value)
    return {'projection': summary, 'diagnostic_rows': len(details), 'retired_case_owners': len(details),
            'raw_native_bootstrap_lines': [line.decode('ascii') for line in PREFIX],
            'native_source_pc_scope': 'ACTUAL_NATIVE_INSTRUMENTATION_AND_STATIC_CODE_ATTRIBUTION_NOT_ORIGINAL_JDI_PCS',
            'native_process_product_scope': 'REQUIRES_SEPARATE_ACTUAL_PROCESS_BUILD_INPUT_AND_PRODUCT_CLOSURE'}

V1 = types.SimpleNamespace(validate_detail=validate_detail, join=prior_join)

def prior_detail(value, original):
    """Check every new field, then retain exact old values for verified V1 gates."""
    value = copy.deepcopy(value)
    for key in ('inventory',):
        inventory = value[key]
        R.equal(inventory.pop('dispenser_diagnostic_owner_id_is_zero'), True, 'Actual zero inventory diagnostic owner')
        R.equal(inventory.pop('exception_class_from_dispenser'), False, 'Actual static/unreached name origin')
    inventory = value['interaction']['inventory']
    R.equal(inventory.pop('dispenser_diagnostic_owner_id_is_zero'), True, 'Actual zero nested inventory owner')
    R.equal(inventory.pop('exception_class_from_dispenser'), False, 'Actual nested static/unreached name origin')
    R.equal(value['interaction'].pop('exception_class_dispenser_owner_id_is_zero'), True,
            'Actual zero interaction exception-name owner')
    present = original['az_present']
    R.equal(value.pop('adapter_diagnostic_owner_present'), present, 'Actual adapter witness presence')
    R.equal(value.pop('adapter_diagnostic_owner_stable'), present, 'Actual live adapter witness stability')
    # Absence has no native object-identity or cross-language alias credit.
    # Every remaining actual value is passed unchanged, including all Source
    # diagnostics, aliases, enum/status, prefix and retirement data.
    V1.validate_detail(value, original)
    return value

def join(original_rows, stdout, stderr):
    R.need(type(stderr) is bytes and len(stderr) <= R.MAX_STREAM and stderr.endswith(b'\n'),
           'Bounded complete typed-witness native stderr')
    lines = stderr.decode('utf8', errors='strict').splitlines()
    R.need(len(lines) == 33 and lines[-1] == 'SH_NATIVE_DETAIL_COMPLETE 16',
           'Exact complete V2 diagnostic/retirement inventory')
    cases = [row for row in original_rows if row['case'] not in R.ORIGINAL_ONLY]
    prior = []
    for index, original in enumerate(cases):
        line, retirement = lines[index * 2:index * 2 + 2]
        R.need(line.startswith('SH_NATIVE_DETAIL '), 'Exact typed-witness diagnostic protocol')
        actual = R.parse(line[len('SH_NATIVE_DETAIL '):])
        previous = prior_detail(actual, original)
        prior.extend(['SH_NATIVE_DETAIL ' + json.dumps(previous), retirement])
    prior.append(lines[-1])
    # This is an explicit additive diagnostic projection after all seven new
    # native fields were checked. Raw V2 bytes remain retained and hashed; no
    # old values or stdout bytes are repaired, normalized, ignored or replaced.
    result = V1.join(original_rows, stdout, ('\n'.join(prior) + '\n').encode('utf8'))
    result['typed_witness_observations'] = {'inventory_errors_with_zero_owner_id': 32,
        'inventory_errors_with_receipt_derived_name': 0, 'interactions_with_zero_name_owner_id': 16,
        'present_stable_adapter_witnesses': sum(row['az_present'] for row in cases),
        'absent_adapters_receive_identity_credit': False,
        'scope': 'ABSENCE_OF_RECEIPT_DERIVED_ERRORS_AND_LIVE_ADAPTER_ID_STABILITY_ONLY',
        'retained_WorldAccess_name_expiry_qualified': False}
    return result
