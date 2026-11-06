"""Validate observations; captured values are never used to execute source calls."""
import collections
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlparse

from support import require


def read_rows(path):
    require(path.stat().st_size <= 134217728, 'bounded observation file ' + str(path))
    return [json.loads(line) for line in path.read_text().splitlines()]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def references(value):
    if isinstance(value, dict):
        if set(value) == {'class', 'identity'}:
            yield value
        else:
            for item in value.values():
                yield from references(item)
    elif isinstance(value, list):
        for item in value:
            yield from references(item)


def bridge_map(record):
    result = {}
    actual_ids = set()
    for item in record['identity_bridge']:
        identity, actual = item['identity'], item['actual']
        require(identity not in result and actual['oid'] not in actual_ids, 'one-to-one actual identity bridge')
        result[identity] = actual
        actual_ids.add(actual['oid'])
    return result


def same_class(binary_name, jdi_name):
    """Exact Java array-name encoding conversion; retain both raw spellings."""
    dimensions = 0
    while jdi_name.endswith('[]'):
        dimensions += 1
        jdi_name = jdi_name[:-2]
    if not dimensions:
        return binary_name == jdi_name
    primitive = {'boolean': 'Z', 'byte': 'B', 'char': 'C', 'short': 'S',
                 'int': 'I', 'long': 'J', 'float': 'F', 'double': 'D'}
    expected = '[' * dimensions + primitive.get(jdi_name, 'L' + jdi_name + ';')
    return binary_name == expected


def validate(out, pins, jar):
    records = {}
    runtime = {}
    expected_classes = set(pins['classes'])
    for mode in ('normal', 'debugger'):
        directory = out / mode
        rows = read_rows(directory / 'target.ndjson')
        records[mode] = rows
        source_stdout = directory / ('target.stdout' if mode == 'debugger' else 'process.stdout')
        source_stderr = directory / ('target.stderr' if mode == 'debugger' else 'process.stderr')
        stdout_lines = source_stdout.read_text().splitlines()
        stderr_lines = source_stderr.read_text().splitlines()
        warning_streams = [(name, [line for line in lines if line.startswith('WARNING: ')])
                           for name, lines in [('stdout', stdout_lines), ('stderr', stderr_lines)]
                          if any(line.startswith('WARNING: ') for line in lines)]
        require(len(warning_streams) == 1, 'one preserved JDK warning stream')
        warning_stream, warnings = warning_streams[0]
        require(all(line.startswith('WARNING: ') for line in stderr_lines), 'no undeclared target stderr')
        require(len(warnings) == 4
                and warnings[0] == 'WARNING: A terminally deprecated method in java.lang.System has been called'
                and 'System::setSecurityManager has been called by MobDrawTarget' in warnings[1]
                and warnings[2] == 'WARNING: Please consider reporting this to the maintainers of MobDrawTarget'
                and 'System::setSecurityManager will be removed in a future release' in warnings[3], 'only declared JDK17 warning lines')
        bindings = {}
        for line in stdout_lines:
            if line.startswith('WARNING: '):
                continue
            if line.startswith('BIND '):
                _, name, origin, size, digest = line.split()
                require(name not in bindings and name in pins['classes'], 'bound original class inventory')
                expected = pins['classes'][name]
                require(int(size) == expected['class_bytes'] and digest == expected['class_sha256'], 'runtime original class bytes ' + name)
                require(Path(unquote(urlparse(origin).path)).resolve() == jar, 'actual original JAR origin ' + name)
                bindings[name] = {'origin': origin, 'bytes': int(size), 'sha256': digest}
            else:
                require(re.fullmatch(r'\d+ (recipes|achievements)|SHEEP_DRAW [012] 1 pool=\d+ nw=\d+|ORIGINAL_MOB_DRAW complete blocked_network=\d+', line), 'unexpected original stdout ' + line)
        require(set(bindings) == expected_classes, 'complete selected runtime class bindings')
        initialized = set()
        for line in (directory / 'class-init.log').read_text().splitlines():
            match = re.search(r"Initializing '([^']+)'", line)
            if match and match[1] in pins['jar_class_names']:
                initialized.add(match[1])
        require(initialized <= expected_classes, 'every actually initialized original class pinned')
        runtime[mode] = {'bindings': bindings, 'initialized_original_classes': sorted(initialized),
                         'jdk_warning_stream': warning_stream, 'jdk_warning_lines': warnings}
        resources = [row for row in rows if row['event'] == 'resource_input']
        require(len(resources) == 4, 'four authored resource inputs')
        for row in resources:
            path = directory / 'resources' / row['resource']
            require(path.stat().st_size == row['bytes'] and sha(path) == row['sha256'], 'authored resource remains unchanged')
            require(Path(unquote(urlparse(row['runtime_url']).path)).resolve() == path.resolve(), 'real selected resource URL')
        require(not list((directory / 'resources').rglob('*.class')), 'resource-only classpath, no replacement source classes')
        guard = [row for row in rows if row['event'] == 'network_guard']
        require(len(guard) == 1, 'preserved no-network guard outcomes')
        for attempt in guard[0]['blocked_attempts']:
            require('java.net.InetAddress.getLocalHost' in attempt and 'sun.font.' in attempt and 'ji.<init>' in attempt, 'declared Fontconfig hostname lookup only')
    draw = {mode: [r for r in rows if r['event'] == 'draw'] for mode, rows in records.items()}
    texture = {mode: [r for r in rows if r['event'] == 'texture_lookup'] for mode, rows in records.items()}
    require(len(draw['normal']) == len(draw['debugger']) == 3, 'three complete draw observations in both VMs')
    require(texture['normal'] == texture['debugger'] and len(texture['normal']) == 3, 'exact ordinary/JDI texture setup and transfer')
    for index, row in enumerate(texture['normal']):
        require(row['index'] == index and row['name'] == row['cached_name'] and row['gl_error'] == 0, 'real returned texture miss/hit')
        require(row['after'] == row['after_hit'], 'hit leaves actual cache/GL unchanged')
        after = row['after']
        require(after['cache_size'] == index + 1 and not after['clamp'] and not after['blur'], 'real source cache publication and flag cleanup')
        require(after['min'] == after['mag'] == 9728 and after['border'] == 0, 'genuine nearest/borderless upload')
        require((after['width'], after['height']) == ((128, 128) if index == 2 else (64, 32)), 'authored source image extent')
        require(after['wrap_s'] == after['wrap_t'] == (10496 if index == 2 else 10497), 'actual source wrap state')
        require(len(bytes.fromhex(row['transfer_hex'])) == after['width'] * after['height'] * 4, 'complete original transfer bytes')
    device = {mode: next(row for row in rows if row['event'] == 'device') for mode, rows in records.items()}
    require(device['normal'] == device['debugger'], 'same actual GL device')
    checked = []
    for case, (ordinary, debugger) in enumerate(zip(draw['normal'], draw['debugger'])):
        require(ordinary['index'] == debugger['index'] == case, 'authored draw identities/order')
        for key in ('input', 'outcome', 'failure', 'shadow_only', 'baseline_pixels', 'pixels', 'nw_position', 'nw_words', 'gl_error'):
            require(ordinary[key] == debugger[key], 'ordinary/JDI exact draw ' + key)
        require(ordinary['outcome'] == 1 and ordinary['failure'] is None and ordinary['gl_error'] == 0, 'actual complete source return')
        for phase in ('before', 'after'):
            for key in ('graph', 'parts', 'pool', 'pool_storage', 'gl'):
                require(ordinary[phase][key] == debugger[phase][key], 'ordinary/JDI exact ' + phase + '.' + key)
        for record in (ordinary, debugger):
            require(record['before']['entropy'] == record['after']['entropy'], 'actual within-VM draw entropy unchanged')
            require(record['before']['actor'] == record['after']['actor'] and record['before']['camera'] == record['after']['camera'], 'source Entity/watcher fields unchanged by selected draw')
            require(record['before']['pool'] == (0 if case == 0 else record['after']['pool']), 'reuse consumes no additional vectors')
            require(all(p['fields']['ps.n'] and p['is_list'] for p in record['after']['parts']), 'all actual parts compiled and valid')
            if case:
                require(record['before']['parts'] == draw['normal' if record is ordinary else 'debugger'][case-1]['after']['parts'], 'same persistent parts/list identities')
            for image in ('baseline_pixels', 'pixels'):
                path = out / ('normal' if record is ordinary else 'debugger') / record[image]['file']
                require(path.stat().st_size == 256 * 256 * 4 and sha(path) == record[image]['sha256'], 'actual complete RGBA artifact')
        checked.append({'case': case, 'exact': ['selected_graph', 'part_fields_and_lists', 'pool_vectors', 'GL_state', 'final_NW', 'pixels'],
                        'entity_and_entropy_scope': 'Retained raw; unchanged within its own VM. Independent constructor entropy is not equated.'})
    require(draw['normal'][0]['pixels']['sha256'] == draw['normal'][1]['pixels']['sha256'], 'real compile/reuse raster equality')
    events = read_rows(out / 'debugger/checkpoints.ndjson')
    require([e['sequence'] for e in events] == list(range(len(events))), 'complete ordered JDI transport')
    live = {}
    for event in events:
        if event['event'] == 'code-pin':
            name = event['class'].replace('.', '/')
            method = next((m for m in pins['classes'][name]['methods'] if m['name'] == event['method'] and m['descriptor'] == event['descriptor']), None)
            require(method and method['code_bytes'] == event['bytes'] and method['code_sha256'] == event['sha256'], 'actual live original Code')
            key = (event['class'], event['method'], event['descriptor'])
            require(key not in live, 'unique live Code record')
            live[key] = event
    lists = []
    case_records = []
    prior_bridge = None
    for case in range(3):
        selected = [e for e in events if e.get('case') == case]
        require(not any(e['event'] in ('exception', 'random', 'random-constructor', 'clock') for e in selected), 'no active draw Throwable or entropy draws')
        require(sum(e['event'] == 'draw-entry' for e in selected) == sum(e['event'] == 'draw-return' for e in selected) == 1, 'complete actual draw boundaries')
        start = bridge_map(next(e for e in selected if e['event'] == 'draw-entry'))
        end = bridge_map(next(e for e in selected if e['event'] == 'state-complete'))
        require(all(end.get(identity) == actual for identity, actual in start.items()), 'actual graph reference continuity')
        if prior_bridge is not None:
            require(all(start.get(identity) == actual for identity, actual in prior_bridge.items()), 'actual graph identity retained between successive calls')
        prior_bridge = end
        for state, bridge in ((draw['debugger'][case]['before'], start), (draw['debugger'][case]['after'], end)):
            for reference in references(state):
                require(reference['identity'] in bridge and same_class(reference['class'], bridge[reference['identity']]['class']), 'every observed graph reference resolves to actual JVM identity/class')
        stack = []
        current = None
        batches = []
        for event in selected:
            if event['event'] == 'entry':
                stack.append(event)
                if 'batch' in event:
                    batch = event['batch']
                    require(len(batch['words']) == batch['nw.o'] == 8 * batch['nw.h'], 'complete real NW raw buffer words')
                    batches.append(event)
                    if current is not None:
                        current['batches'].append(event)
            elif event['event'] == 'return':
                require(stack and all(stack[-1]['location'][k] == event['location'][k] for k in ('class', 'method', 'descriptor')), 'actual original nested call/return order')
                stack.pop()
            elif event['event'] == 'gl-entry':
                if event['method'] == 'glNewList':
                    require(current is None, 'one genuine open GL list')
                    owner = next((s['receiver'] for s in reversed(stack) if s['location']['class'] == 'ps' and s['location']['method'] == 'd'), None)
                    require(owner is not None, 'actual PS owner at list compilation')
                    current = {'case': case, 'list': event['arguments'][0], 'mode': event['arguments'][1], 'part': owner, 'gl_commands': [], 'batches': []}
                if current is not None:
                    current['gl_commands'].append(event)
            elif event['event'] == 'gl-return' and event['method'] == 'glEndList':
                require(current is not None, 'actual list closes')
                lists.append(current)
                current = None
        require(not stack and current is None, 'draw returns with complete observed child/list nesting')
        require(batches[-1]['batch']['words'] == draw['normal'][case]['nw_words'], 'full shadow batch equals actual final NW buffer')
        case_records.append({'case': case, 'source_entries': sum(e['event'] == 'entry' for e in selected),
                             'gl_entries': sum(e['event'] == 'gl-entry' for e in selected), 'batch_records': batches,
                             'entry_bridge': start, 'after_bridge': end})
    part_count = len(draw['normal'][0]['after']['parts'])
    require(part_count == 12 and len(lists) == part_count and all(len(item['batches']) == 6 for item in lists), 'actual six parts in each of two distinct source models')
    require([len(item['batch_records']) for item in case_records] == [part_count * 6 + 1, 1, 1], 'source compilation and reuse batch topology')
    clocks = []
    uniquifier = nano = None
    for event in events:
        if event['event'] == 'clock':
            if event['kind'] == 'seedUniquifier':
                uniquifier = int(event['value']['J'], 16)
            else:
                nano = int(event['value']['J'], 16)
        elif event['event'] == 'random-constructor' and event['descriptor'] == '()V':
            require(uniquifier is not None and nano is not None, 'actual default Random clock pair')
            require(int(event['seed'], 16) == (uniquifier ^ nano ^ 0x5deece66d) & ((1 << 48)-1), 'actual JDK17 default seed calculation')
            clocks.append({'constructor_case': event['constructor_case'], 'actual': event['actual'], 'U': f'{uniquifier:016x}', 'nano': f'{nano:016x}', 'seed': event['seed']})
            uniquifier = nano = None
    texture_events = [e for e in events if e.get('texture_case', -1) >= 0]
    require({e['texture_case'] for e in texture_events} == set(range(6)), 'all actual texture misses and hits traced')
    for index in range(3):
        miss = [e for e in texture_events if e['texture_case'] == index and e['event'] == 'gl-entry']
        hit = [e for e in texture_events if e['texture_case'] == index + 3 and e['event'] == 'gl-entry']
        require(sum(e['method'] == 'glTexImage2D' for e in miss) == 1 and not hit, 'genuine upload miss and no-upload hit')
        parameters = [e['arguments'] for e in miss if e['method'] == 'glTexParameteri']
        wrap = 10496 if index == 2 else 10497
        require(parameters == [[3553, 10241, 9728], [3553, 10240, 9728],
                               [3553, 10242, wrap], [3553, 10243, wrap]], 'actual ordered source filter and independent S/T wrap stores')
    result = {'scope': 'Finite original Sheep source returns and genuine cached textures following observed original uploads; no native parity.',
              'draw_records': draw['normal'], 'debugger_draw_records': draw['debugger'], 'texture_records': texture['normal'],
              'constructor_contexts': {mode: [r for r in rows if r['event'] == 'constructor'] for mode, rows in records.items()},
              'constructor_events': [e for e in events if e.get('constructor_case', -1) >= 0], 'default_Random_clock_pairs': clocks,
              'source_events': events, 'list_records': lists, 'case_records': case_records, 'runtime': runtime, 'devices': device,
              'ordinary_debugger_checks': checked, 'counts': dict(collections.Counter(e['event'] for e in events)),
              'coverage_limits': ['Prepared raw App; genuine Cow camera; prepared view matrix, not full startup/camera.',
                                  'Only default unsheared white Sheep at partial .375 and the selected genuine shadow component.',
                                  'Three genuine default-pack authored image misses and later cache hits; no arbitrary pack/refresh/download claim.',
                                  'Constructor/default entropy stays raw and independent between VMs; no application profile rebinding.',
                                  'No native parity, other Mob renderer, hurt/color/fire/debug true branch or playability claim.']}
    (out / 'observations.json').write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    return {'sha256': sha(out / 'observations.json'), 'draws': len(draw['normal']), 'textures': len(texture['normal']),
            'lists': len(lists), 'batches': sum(len(item['batch_records']) for item in case_records),
            'live_Code': len(live), 'event_counts': result['counts'], 'initialized_classes': {k: len(v['initialized_original_classes']) for k, v in runtime.items()}}
