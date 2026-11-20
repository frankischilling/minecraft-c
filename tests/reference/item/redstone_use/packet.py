"""Admit the sealed original Redstone Item-use packet without game/JVM assets.

Frame numbers below are actual Java StackTraceElement line numbers, never JVM
bytecode PCs or native traces. Entropy is checked within each owned World only.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
MANIFEST_SHA256 = '12252f225755dd065720278af328d5e3b0935ddaab267ab49db4d612c2db673f'
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
METHODS = {
    ('ie', 'a', '(Liz;Lgs;Lfd;IIII)Z'): (132, '18fec8fec2578d30360e9b9e5b78efe0090b8aa64182e430e78de5fe25c4664e'),
    ('iz', 'a', '(Lgs;Lfd;IIII)Z'): (40, '3882fefba3a2359d4da00e1cb91bda09a75995ba0f650640f3157dd01a414cfd'),
    ('sm', 'a', '(Lfd;III)Z'): (11, 'b04c7c52c0e7d14ba3b9cfc7735b1d2e738465d24eb7259bb77719a2ccd0352b'),
}
ROW_KEYS = set(('index name values result exception stack_before stack_after clicked_before clicked_after '
                'target_before target_after changes queues_before queues_after set_before set_after events '
                'unexpected_adds remote world_o registered_block_class registered_block_id registered_item_class '
                'registered_item_slot registered_owners_same used331_delta used332_delta health_before health_after '
                'world_random_same world_rng_unchanged').split())
SET_KEYS = set(('set_id map_id table_id size capacity threshold mod_count load_factor_bits output_flag roots nodes pending').split())


def need(value, message):
    if not value:
        raise ValueError(message)


def unique(pairs):
    result = {}
    for k, v in pairs:
        need(k not in result, 'duplicate JSON key: ' + k)
        result[k] = v
    return result


def loads(text):
    return json.loads(text, object_pairs_hook=unique)


def load(path):
    return loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def integer(v):
    return type(v) is int and -2**31 <= v < 2**31


def same(actual, expected):
    if type(actual) is not type(expected):
        return False
    if isinstance(actual, dict):
        return actual.keys() == expected.keys() and all(same(actual[k], expected[k]) for k in actual)
    if isinstance(actual, list):
        return len(actual) == len(expected) and all(same(a, b) for a, b in zip(actual, expected))
    return actual == expected


def wrap(v):
    return (v + 2**31) % 2**32 - 2**31


def lf(raw):
    need(raw.endswith(b'\n') and b'\r' not in raw, 'exact LF projection')
    return raw.decode('ascii').splitlines()


def profiles(path, cohort):
    result = []
    for line in lf(Path(path).read_bytes()):
        cells = line.split('\t')
        need(len(cells) == (17 if cohort == 'use' else 7), 'closed input width')
        need(re.fullmatch('[a-z0-9-]+', cells[0]), 'canonical input name')
        need(all(re.fullmatch('0|-?[1-9][0-9]*', x) for x in cells[1:]), 'canonical input integers')
        values = [int(x) for x in cells[1:]]
        need(all(integer(x) for x in values), 'Java input range')
        result.append({'name': cells[0], 'values': values})
    need(len(result) == (42 if cohort == 'use' else 12) and
         len({x['name'] for x in result}) == len(result), 'complete unique frozen roster')
    return result


def scene(p, use):
    result = {}
    def put(x, y, z, block):
        if 0 <= x <= 31 and 0 <= z <= 31 and 0 <= y <= 127:
            result[x, y, z] = [block, 0]
    if use:
        put(p[7], wrap(p[8]-1), p[9], p[11])
        put(p[4], p[5], p[6], p[10])
        put(p[7], p[8], p[9], p[12])
    else:
        put(p[0], wrap(p[1]-1), p[2], p[4])
        put(p[0], p[1], p[2], p[3])
    return result


def expected(profile, cohort):
    """Finite IE/IZ/SM branch specification, independent of captured outcomes."""
    p, use = profile['values'], cohort == 'use'
    cells = scene(p, use)
    # These are the frozen ordinary registered support IDs, not an ID-only
    # replacement for FD.h: the selected original materials/predicates are fixed.
    support_by_frozen_id = {0: False, 1: True, 18: False, 20: False, 85: False}
    def support(x, y, z):
        below = cells.get((x, wrap(y-1), z), [0, 0])[0]
        need(below in support_by_frozen_id, 'support ID outside this finite source specification')
        return support_by_frozen_id[below]
    exception, frames, placed, stat = 'NONE', [], False, [0, 0]
    if not use:
        result = str(support(*p[:3])).lower()
        if p[5]:
            result, exception = 'UNRETURNED', 'java.lang.NullPointerException'
            frames = [['sm', 'a', 45], ['RedstoneUseObservation', 'main', 253]]
        return {'result': result, 'exception': exception, 'frames': frames, 'cells': cells,
                'target': p[:3], 'stack_before': None, 'stack_after': None,
                'events': [], 'changes': [], 'stats': [None, None]}
    need(len(p) == 16 and p[0] == 331 and p[2] == 0 and p[13] in (0, 1) and
         p[14] in (0, 1, 2, 4) and p[15] in (0, 1, 2, 3), 'declared Item owner/modes')
    before = [p[1], 0, p[0], p[2]]
    after = before.copy()
    target = p[4:7].copy()
    snow = cells.get(tuple(target), [0, 0])[0] == 78
    if not snow:
        dx, dy, dz = {0: (0, -1, 0), 1: (0, 1, 0), 2: (0, 0, -1),
                      3: (0, 0, 1), 4: (-1, 0, 0), 5: (1, 0, 0)}.get(p[3], (0, 0, 0))
        target = [wrap(target[0]+dx), wrap(target[1]+dy), wrap(target[2]+dz)]
    need(target == p[7:10], 'authored target must match the actual IE face/snow operands')
    events, changes = [], []
    if p[14] & 2:
        result, exception = 'UNRETURNED', 'java.lang.NullPointerException'
        frames = [['RedstoneUseObservation', 'main', 251]]
    elif p[14] & 1:
        result, exception = 'UNRETURNED', 'java.lang.NullPointerException'
        frames = [['ie', 'a', 13], ['iz', 'a', 78], ['RedstoneUseObservation', 'main', 251]]
    else:
        blocked = not snow and cells.get(tuple(target), [0, 0])[0] != 0
        result = 'false' if blocked else 'true'
        if not blocked and support(*target):
            after[0] = wrap(after[0]-1)
            placed = all(0 <= target[i] <= bound for i, bound in enumerate((31, 127, 31)))
            if placed:
                old = cells.get(tuple(target), [0, 0])[0]
                events.append({'event': 'cell', 'xyz': target, 'cell': [55, 0], 'stack': after.copy()})
                x, y, z = target
                changes.append([(x >> 4)*2+(z >> 4), 0, ((x & 15) << 11) | ((z & 15) << 7) | y, old, 55])
                if p[15] == 1:
                    result, exception = 'UNRETURNED', 'java.lang.IllegalStateException'
                    frames = [['RedstoneUseObservation$Observer', 'a', 163], ['fd', 'j', 398],
                              ['fd', 'g', 403], ['fd', 'f', 380], ['ie', 'a', 24], ['iz', 'a', 78],
                              ['RedstoneUseObservation', 'main', 251]]
                elif p[15] == 2:
                    after[0] = 99
                    events.append({'event': 'count-store', 'value': 99})
                elif p[15] == 3:
                    after[2] = 332
                    events.append({'event': 'id-store', 'value': 332})
        if result == 'true':
            if p[14] & 4:
                result, exception = 'UNRETURNED', 'java.lang.NullPointerException'
                frames = [['iz', 'a', 79], ['RedstoneUseObservation', 'main', 251]]
            else:
                stat[after[2]-331] = 1
    return {'result': result, 'exception': exception, 'frames': frames, 'cells': cells,
            'target': target, 'stack_before': before, 'stack_after': after, 'events': events,
            'changes': changes, 'stats': stat, 'placed': placed}


def random_state(value):
    need(set(value) == {'seed', 'gaussian', 'have'} and type(value['have']) is bool and
         re.fullmatch('[0-9a-f]{12}', value['seed']) and re.fullmatch('[0-9a-f]{16}', value['gaussian']),
         'complete passive Random state')


def set_state(value):
    need(set(value) == SET_KEYS and integer(value['set_id']) and value['set_id'] > 0 and
         integer(value['map_id']) and value['map_id'] > 0 and value['set_id'] != value['map_id'] and
         all(type(value[k]) is int and value[k] == 0 for k in ('table_id', 'size', 'capacity', 'threshold')) and
         integer(value['mod_count']) and value['mod_count'] >= 0 and value['load_factor_bits'] == '3f400000' and
         value['output_flag'] is True and value['roots'] == [] and value['nodes'] == [] and value['pending'] == [],
         'only the observed empty Set boundary topology is qualified')


def validate_records(cohort, rows, entropy, exceptions, roster):
    use = cohort == 'use'
    need(len(rows) == len(entropy) == len(roster), 'complete row/own-entropy roster')
    failures, counts, previous_queue, previous_set, previous_rng = [], collections.Counter(), [], None, None
    for index, (row, own, profile) in enumerate(zip(rows, entropy, roster)):
        need(set(row) == ROW_KEYS and type(row['index']) is int and row['index'] == index and
             row['name'] == profile['name'] and row['values'] == profile['values'] and
             all(integer(x) for x in row['values']), 'exact typed row/profile identity')
        spec = expected(profile, cohort)
        need((row['result'], row['exception']) == (spec['result'], spec['exception']),
             'only source-reachable outcomes and exception classes are admitted')
        if spec['frames']:
            failures.append({'index': index, 'name': profile['name'], 'exception': spec['exception'], 'frames': spec['frames']})
        need(same(row['stack_before'], spec['stack_before']) and same(row['stack_after'], spec['stack_after']) and
             same([row['used331_delta'], row['used332_delta']], spec['stats']), 'exact consumption/live-ID/stat suffix')
        need(same(row['events'], spec['events']) and same(row['changes'], spec['changes']),
             'actual callback/effect prefix must match its reached source branch')
        target = spec['target']
        before = spec['cells'].get(tuple(target), [0, 0])
        need(same(row['target_before'], before) and same(row['target_after'],
             [55, 0] if spec.get('placed') else before), 'actual target snapshot/input join')
        clicked = spec['cells'].get(tuple(profile['values'][4:7]), [0, 0]) if use else None
        need(same(row['clicked_before'], clicked) and same(row['clicked_after'],
             [55, 0] if use and spec.get('placed') and profile['values'][4:7] == target else clicked),
             'actual clicked snapshot/input join')
        need(row['registered_block_class'] == 'sm' and row['registered_block_id'] == 55 and
             row['registered_item_class'] == ('ie' if use else None) and
             row['registered_item_slot'] == (331 if use else None) and row['registered_owners_same'] is True and
             row['health_before'] == row['health_after'] == (20 if use else None) and
             row['remote'] is (bool(profile['values'][13]) if use else False) and row['world_o'] is False and
             type(row['unexpected_adds']) is int and row['unexpected_adds'] == 0,
             'genuine same-namespace owners and unchanged Player/source flags')
        need(row['world_random_same'] is True and row['world_rng_unchanged'] is True and
             set(own) == {'index', 'name', 'world_before', 'world_after', 'world_random_same', 'unexpected_entities'} and
             type(own['index']) is int and own['index'] == index and own['name'] == profile['name'] and
             own['world_random_same'] is True and own['unexpected_entities'] == [], 'own entropy identity/no publication')
        random_state(own['world_before']); random_state(own['world_after'])
        need(own['world_before'] == own['world_after'] and
             (previous_rng is None or own['world_before'] == previous_rng), 'own World stream continuation; no reset/draw')
        previous_rng = own['world_after']
        for queue in (row['queues_before'], row['queues_after']):
            need(type(queue) is list and all(type(q) is list and len(q) == 7 and q[0] in (0, 1) and
                 all(integer(x) for x in q) for q in queue), 'complete ordered light-region tuples')
        need(row['queues_before'] == previous_queue, 'queue conditioning persists across calls')
        previous_queue = row['queues_after']
        set_state(row['set_before']); set_state(row['set_after'])
        need((previous_set is None or row['set_before'] == previous_set) and
             row['set_before']['set_id'] == row['set_after']['set_id'] and
             row['set_before']['map_id'] == row['set_after']['map_id'] and
             row['set_after']['mod_count'] >= row['set_before']['mod_count'], 'same Set owners and retained modification history')
        previous_set = row['set_after']
        counts[row['result'] + ':' + row['exception']] += 1
    need(same(exceptions, failures), 'exact source/helper boundary frames and exception projection')
    return {'rows': len(rows), 'outcomes': dict(counts), 'queue_regions_at_end': len(previous_queue),
            'set_mod_count_at_end': previous_set['mod_count'], 'own_entropy_only': True}


def allowed_stderr(capture_root):
    path = str(capture_root).replace(chr(92), "/").rstrip("/") + "/classes"
    uri = 'file:' + ('/' if re.match('[A-Za-z]:/', path) else '') + path + '/'
    # These four JDK-native warnings use LF, unlike PrintStream stdout's CRLF.
    return ('WARNING: A terminally deprecated method in java.lang.System has been called\n'
            'WARNING: System::setSecurityManager has been called by RedstoneUseObservation (' + uri + ')\n'
            'WARNING: Please consider reporting this to the maintainers of RedstoneUseObservation\n'
            'WARNING: System::setSecurityManager will be removed in a future release\n').encode('ascii')


def validate_streams(cohort, payload, roster, capture_root):
    need(payload['stderr'] == allowed_stderr(capture_root), 'exact SecurityManager-only stderr; daemon/setup errors refused')
    raw = payload['stdout']
    need(raw.endswith(b'\r\n') and raw == raw.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'),
         'closed original CRLF stdout transport')
    lines = raw.decode('ascii').splitlines()
    need(lines[:2] == ['16 achievements', '151 recipes'], 'exact original Registry bootstrap')
    rows, own, failures, selected, own_lines, exception_lines, cursor = [], [], [], [], [], [], 2
    prefix = 'USE' if cohort == 'use' else 'SUPPORT'
    for _ in roster:
        need(cursor < len(lines) and lines[cursor].startswith(prefix + '\t'), 'one ordered original row')
        line = lines[cursor]; cursor += 1; selected.append(line)
        row = loads(line.split('\t', 1)[1]); rows.append(row)
        need(cursor < len(lines) and lines[cursor].startswith('OWN_ENTROPY\t'), 'one ordered own-entropy record')
        line = lines[cursor]; cursor += 1; own_lines.append(line)
        own.append(loads(line.split('\t', 1)[1]))
        if row['exception'] != 'NONE':
            need(cursor < len(lines) and lines[cursor].startswith('SOURCE_EXCEPTION\t'), 'one reached exception record')
            line = lines[cursor]; cursor += 1; exception_lines.append(line)
            failures.append(loads(line.split('\t', 1)[1]))
    complete = 'COMPLETE\t' + cohort + '\t' + str(len(roster))
    need(lines[cursor:] == ['GUARD\tsource\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes=' +
                            ('4' if cohort == 'use' else '3'),
                            'GUARD\tcontrols\tnetwork=2\tnative=1\texecs=1\tdenied-writes=1', complete],
         'exact original guards/completion; no arbitrary stdout tolerated')
    selected.append(complete)
    for key, projected in (('selected', selected), ('entropy', own_lines), ('exceptions', exception_lines)):
        need(payload[key] == ('\n'.join(projected) + '\n').encode('ascii'), 'full raw/projection join: ' + key)
    summary = validate_records(cohort, rows, own, failures, roster)
    return summary, rows, failures


def inventory(root):
    return {p.relative_to(root).as_posix(): sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def validate_receipt(receipt, payload):
    need(type(receipt['exit']) is int and receipt['exit'] == 0 and
         type(receipt['timeout_seconds']) is int and receipt['timeout_seconds'] == 90 and
         receipt['timed_out'] is False and receipt['pipe_capture_complete'] is True and
         receipt['reader_errors'] == {} and receipt['output_overflow'] is False,
         'actual bounded ordinary VM/pipe completion')
    for channel, limit in (('stdout', 4194304), ('stderr', 1048576)):
        need(type(receipt[channel+'_bytes']) is int and receipt[channel+'_bytes'] == len(payload[channel]) and
             receipt[channel+'_sha256'] == hashlib.sha256(payload[channel]).hexdigest() and
             receipt[channel+'_limit'] == limit and len(payload[channel]) < limit and
             receipt[channel+'_file'] == 'original.' + channel, 'exact retained raw pipe bytes')


AUTHORED = ('.gitattributes', 'RedstoneUseObservation.java', 'capture_reference.py',
            'declared-inputs.json', 'input-freeze.json', 'source-pins.json',
            'support-profiles.tsv', 'use-profiles.tsv')
RUNS = (('use', 0), ('use', 1), ('support', 0), ('support', 1))
PAYLOAD = {'stdout': 'original.stdout', 'stderr': 'original.stderr',
           'selected': 'selected.txt', 'entropy': 'own-entropy.jsonl',
           'exceptions': 'source-exceptions.jsonl'}
HELPERS = {'RedstoneUseObservation.class', 'RedstoneUseObservation$1.class',
           'RedstoneUseObservation$Observer.class'}
SUPPORT = ('tests/reference/client/controller/capture.py',
           'tests/reference/client/controller/classfile.py',
           'tests/reference/client/picking/observe.py', 'tests/tools/client/test_picking.py')
LIBRARIES = (
    'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5')


def join(root, relative):
    """Join recorded paths without consulting the auditing host's filesystem."""
    need(type(root) is str and root and '\n' not in root and '\r' not in root and
         (re.match(r'^[A-Za-z]:\\', root) or root.startswith('/')), 'absolute captured path')
    separator = '\\' if re.match(r'^[A-Za-z]:\\', root) else '/'
    return root.rstrip('/\\') + separator + relative.replace('/', separator)


def frozen_files():
    names = set(AUTHORED) | {'capture.json', 'freeze.json', 'provenance.json'}
    names |= {tool + '.' + suffix for tool in ('java', 'javac', 'compile')
              for suffix in ('stdout', 'stderr', 'capture.json')}
    names |= {cohort + '-ordinary-' + str(repeat) + '/' + name
              for cohort, repeat in RUNS for name in (*PAYLOAD.values(), 'original.capture.json')}
    return names


def authored(directory):
    """Source-only admission; never interprets prepared inputs as observations."""
    directory = Path(directory)
    freeze = load(directory / 'input-freeze.json')
    need(sha(directory / 'input-freeze.json') ==
         '915f09e4072e783e284efd182609646bb67f31ffbfc9560736baeac9f93cc0d1',
         'original pre-helper operand freeze')
    need(all(sha(directory / name) == value for name, value in freeze['files'].items()),
         'unchanged pre-helper operands')
    need(sha(directory / 'RedstoneUseObservation.java') ==
         '51cc2610e21c76a8aed6811b421a426230d400ae8e3188f010acc12727909459',
         'byte-identical helper and exception line-number authority')
    pins = load(directory / 'source-pins.json')
    need(pins['client_jar_sha256'] == JAR, 'unchanged original archive')
    for (owner, name, descriptor), (size, code_hash) in METHODS.items():
        methods = [m for m in pins['classes'][owner]['methods']
                   if m['name'] == name and m['descriptor'] == descriptor]
        need(len(methods) == 1 and (methods[0]['code_bytes'], methods[0]['code_sha256']) ==
             (size, code_hash), 'source Item/Stack/predicate Code authority')
    declaration = load(directory / 'declared-inputs.json')
    rosters = {cohort: profiles(directory / (cohort + '-profiles.tsv'), cohort)
               for cohort in ('use', 'support')}
    need(all(same(rosters[c], declaration[c + '_rows']) for c in rosters),
         'frozen declared operands')
    return rosters


def tool_receipt(receipt, payload, name, timeout):
    adapted = dict(receipt)
    adapted['timeout_seconds'] = 90
    adapted['stdout_file'], adapted['stderr_file'] = 'original.stdout', 'original.stderr'
    validate_receipt(adapted, payload)
    need(type(receipt['timeout_seconds']) is int and receipt['timeout_seconds'] == timeout and
         all(receipt[c + '_file'] == name + '.' + c for c in ('stdout', 'stderr')),
         'actual bounded tool receipt')


def validate(directory=HERE):
    directory = Path(directory)
    need(re.fullmatch('[0-9a-f]{64}', MANIFEST_SHA256) is not None,
         'packet has not been sealed after genuine public capture')
    need(sha(directory / 'manifest.json') == MANIFEST_SHA256, 'sealed public manifest')
    manifest = load(directory / 'manifest.json')
    need(set(manifest) == {'format', 'client_jar_sha256', 'files_sha256'} and
         type(manifest['format']) is int and manifest['format'] == 1 and
         manifest['client_jar_sha256'] == JAR and set(manifest['files_sha256']) == frozen_files(),
         'closed public artifact roster')
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()}
    need(actual == frozen_files() | {'manifest.json', 'packet.py', 'test_packet.py', 'README.md'},
         'closed on-disk packet; no compiled helpers or original assets')
    for name, value in manifest['files_sha256'].items():
        need(re.fullmatch('[0-9a-f]{64}', value) and sha(directory / name) == value,
             'changed sealed packet file: ' + name)
    return validate_evidence(directory)


def validate_evidence(directory):
    """Semantic layer for corruption controls; public callers use read_packet."""
    directory = Path(directory)
    rosters = authored(directory)
    report, freeze, provenance = [load(directory / name)
                                  for name in ('capture.json', 'freeze.json', 'provenance.json')]
    need(set(provenance) == {'format', 'capture_root', 'prepared_root', 'repository_root',
         'paths', 'authored_files_sha256', 'capture_sha256', 'freeze_sha256'} and
         type(provenance['format']) is int and provenance['format'] == 1 and
         provenance['capture_sha256'] == sha(directory / 'capture.json') and
         provenance['freeze_sha256'] == sha(directory / 'freeze.json'), 'actual capture authority')
    capture, prepared, repository = [provenance[k] for k in
                                     ('capture_root', 'prepared_root', 'repository_root')]
    need(join(repository, 'tests/reference/item/redstone_use') == prepared,
         'actual public helper destination, not a private predecessor')
    paths = provenance['paths']
    need(set(paths) == {'java', 'javac', 'jvm', 'jmod', 'modules', 'libraries', 'java_dlls'} and
         len(paths['libraries']) == 4 and len(set(paths['libraries'])) == 4 and
         len(paths['java_dlls']) == len(set(paths['java_dlls'])), 'closed actual JDK/library roles')
    jar = join(repository, 'b1.7.3.jar')
    expected_inputs = {jar, *paths['libraries'], paths['java'], paths['javac'], paths['jvm'],
                       paths['jmod'], paths['modules'], *paths['java_dlls'],
                       *(join(prepared, n) for n in AUTHORED), *(join(repository, n) for n in SUPPORT)}
    before = report['inputs_before']
    need(report['status'] == 'PASS' and report['inputs_unchanged'] is True and
         report['helpers_unchanged'] is True and same(before, report['inputs_after']) and
         same(before, freeze['inputs_before']) and set(before) == expected_inputs and
         all(re.fullmatch('[0-9a-f]{64}', value) for value in before.values()),
         'complete exact execution closure and before/after continuity')
    need(before[jar] == JAR and [before[p] for p in paths['libraries']] == list(LIBRARIES) and
         before[paths['jmod']] == load(directory / 'source-pins.json')['jdk_module_sha256'],
         'actual original/JDK/library identities')
    need(set(provenance['authored_files_sha256']) == set(AUTHORED) and all(
         sha(directory / n) == provenance['authored_files_sha256'][n] == before[join(prepared, n)]
         for n in AUTHORED), 'actual public authored closure')
    helpers = report['helpers_before']
    need(set(helpers) == HELPERS and same(helpers, report['helpers_after']) and all(
         re.fullmatch('[0-9a-f]{64}', value) for value in helpers.values()),
         'only unchanged three compiled authored helper classes')
    separator = ';' if re.match(r'^[A-Za-z]:\\', capture) else ':'
    cp = separator.join([join(capture, 'classes'), jar, *paths['libraries']])
    for name, timeout in (('java', 10), ('javac', 10), ('compile', 30)):
        payload = {c: (directory / (name + '.' + c)).read_bytes() for c in ('stdout', 'stderr')}
        receipt = load(directory / (name + '.capture.json'))
        need(same(receipt, report[name]), 'actual tool report/receipt join')
        tool_receipt(receipt, payload, name, timeout)
        if name == 'compile':
            need(receipt['command'] == [paths['javac'], '-encoding', 'UTF-8', '-implicit:none',
                 '-cp', cp, '-d', join(capture, 'classes'), join(prepared, 'RedstoneUseObservation.java')] and
                 payload == {'stdout': b'', 'stderr': b''}, 'exact sole helper compilation')
        else:
            need(receipt['command'] == [paths[name], '-version'] and b'17.0.17' in
                 payload['stdout'] + payload['stderr'], 'qualified actual tool version')
    need([(r['cohort'], r['repeat']) for r in report['runs']] == list(RUNS),
         'all four independent outcomes in declared order')
    results, summaries, logical, frames = {}, {}, {}, {}
    for run in report['runs']:
        cohort, repeat = run['cohort'], run['repeat']
        name = cohort + '-ordinary-' + str(repeat)
        folder = directory / name
        payload = {key: (folder / file).read_bytes() for key, file in PAYLOAD.items()}
        receipt = load(folder / 'original.capture.json')
        need(same(receipt, run['capture']) and run['passed'] is True, 'raw receipt/report join')
        validate_receipt(receipt, payload)
        need(receipt['command'] == [paths['java'], '-Xint', '-Xverify:all', '-Xmx256m',
             '-Djava.awt.headless=true', '-Djava.security.manager=allow', '--add-opens',
             'java.base/java.util=ALL-UNNAMED', '--add-opens', 'java.base/java.lang=ALL-UNNAMED',
             '-cp', cp, 'RedstoneUseObservation', jar, join(capture, name + '/world'),
             join(prepared, cohort + '-profiles.tsv'), cohort], 'ordinary source call; no injected repair/agent')
        summary, rows, failures = validate_streams(cohort, payload, rosters[cohort], capture)
        need(type(run['rows']) is int and run['rows'] == len(rows) and
             type(run['source_exceptions']) is int and run['source_exceptions'] == len(failures) and
             run['selected_sha256'] == hashlib.sha256(payload['selected']).hexdigest(), 'actual summary join')
        before, after = run['world_before'], run['world_after']
        need(set(before) == set(after) == {'level.dat', 'region/r.0.0.mcr', 'session.lock'} and
             same(before, freeze['world_inputs'][cohort + '-' + str(repeat)]) and
             all(re.fullmatch('[0-9a-f]{64}', x) for x in [*before.values(), *after.values()]) and
             {k: v for k, v in before.items() if k != 'session.lock'} ==
             {k: v for k, v in after.items() if k != 'session.lock'}, 'saved input continuity; only session lock may differ')
        summaries[name], logical[cohort, repeat], frames[cohort, repeat] = summary, payload['selected'], payload['exceptions']
        if repeat == 0:
            results[cohort] = rows
    need(all(logical[c, 0] == logical[c, 1] and frames[c, 0] == frames[c, 1]
             for c in ('use', 'support')), 'independent repeated logical effects and source/helper boundaries')
    return {'status': 'PASS', 'runs': summaries, 'rows': results,
            'scope': 'original-only; own World entropy and empty Set boundaries; no native/JDI/GUI qualification'}


def read_packet(directory=HERE):
    return validate(directory)['rows']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', nargs='?', type=Path, default=HERE)
    parser.add_argument('--authored-only', action='store_true')
    args = parser.parse_args()
    if args.authored_only:
        print(json.dumps({'status': 'AUTHORED_ONLY_NOT_CAPTURED',
                          'rows': {k: len(v) for k, v in authored(args.directory).items()}}))
    else:
        print(json.dumps(validate(args.directory), indent=2))
