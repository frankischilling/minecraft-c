"""Validate unchanged original records; constructor entropy is retained, never replayed as execution."""
import collections
import hashlib
import json
import math
from pathlib import Path
import re
import struct
from urllib.parse import unquote, urlparse
from support import require

MODES = ('main', 'null_item', 'null_snow', 'null_world_tick', 'null_world_draw')
FAILURES = {13: ('pb', '<init>', 17), 14: ('pb', '<init>', 42),
            15: ('sn', 'b', 394), 16: ('sn', 'a', 118)}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    require(path.stat().st_size < 536870912, 'bounded complete observation transport')
    return [json.loads(line) for line in path.read_text().splitlines()]


def refs(value):
    if isinstance(value, dict):
        if set(value) == {'class', 'identity'}:
            yield value
        else:
            for child in value.values():
                yield from refs(child)
    elif isinstance(value, list):
        for child in value:
            yield from refs(child)


def bridge(event):
    result = {row['identity']: row['actual'] for row in event['identity_bridge']}
    require(len(result) == len(event['identity_bridge']) == len({v['oid'] for v in result.values()}), 'one-to-one actual JVM identity bridge')
    return result


def same_class(binary, source):
    dimensions = 0
    while source.endswith('[]'):
        dimensions += 1
        source = source[:-2]
    primitive = {'boolean': 'Z', 'byte': 'B', 'char': 'C', 'short': 'S', 'int': 'I', 'long': 'J', 'float': 'F', 'double': 'D'}
    return binary == ('[' * dimensions + primitive.get(source, 'L' + source + ';') if dimensions else source)


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def number(value):
    key, bits = next(iter(value.items()))
    return struct.unpack('>f' if key == 'F' else '>d', bytes.fromhex(bits))[0]


def bits(value, kind):
    return {kind: struct.pack('>f' if kind == 'F' else '>d', value).hex()}


def advance(seed, count):
    for _ in range(count):
        seed = (seed * 0x5DEECE66D + 11) & ((1 << 48) - 1)
    return seed


def signature(row, event, cls, name, descriptor=None):
    loc = row.get('location', {})
    return (row['event'] == event and loc.get('class') == cls and loc.get('method') == name and
            (descriptor is None or loc.get('descriptor') == descriptor))


def image_cache_entropy(row):
    """Separate only the observed JDK file-cache producer, retaining every raw event."""
    expected = [{'class': 'ji', 'method': 'a', 'descriptor': '(Ljava/io/InputStream;)Ljava/awt/image/BufferedImage;', 'pc': 1},
                {'class': 'ji', 'method': 'b', 'descriptor': '(Ljava/lang/String;)I', 'pc': 197},
                {'class': 'dn', 'method': 'a', 'descriptor': '(Lsn;F)V', 'pc': 215}]
    return (row['event'] == 'random' and row['method'] == 'nextLong' and
            row.get('receiver') == {'class': 'java.security.SecureRandom', 'oid': row['oid']} and
            row.get('source_frames') == expected and
            any(f['class'] == 'javax.imageio.stream.FileCacheImageInputStream' and f['method'] == '<init>' and f['pc'] == 76
                for f in row.get('producer_frames', [])) and
            any(f['class'] == 'java.nio.file.TempFileHelper' and f['method'] == 'generatePath' and f['pc'] == 3
                for f in row.get('producer_frames', [])) and
            any(f['class'] == 'java.nio.file.Files' and f['method'] == 'createTempFile' and f['pc'] == 4
                for f in row.get('producer_frames', [])))


def constructors(events):
    """Independent scalar oracle over actual returns, not constructor execution or seed injection."""
    result, pending = [], None
    for row in events:
        if signature(row, 'entry', 'pb', '<init>'):
            require(pending is None, 'nonrecursive actual PB constructors')
            pending = {'entry': row, 'events': []}
        if pending is None:
            continue
        pending['events'].append(row)
        end = signature(row, 'particle-constructor-prefix', 'pb', '<init>') or row['event'] == 'exception'
        if not end:
            continue
        selected = pending['events']
        ancestor = next(x['particle'] for x in selected if signature(x, 'particle-constructor-prefix', 'sn', '<init>'))
        xw = next(x['particle'] for x in selected if signature(x, 'particle-constructor-prefix', 'xw', '<init>'))
        shared = [x for x in selected if x['event'] == 'random' and x['method'] == 'nextDouble' and
                  any(f['class'] == 'xw' and f['method'] == '<init>' for f in x['source_frames'])]
        private = [x for x in selected if x['event'] == 'random' and x['method'] == 'nextFloat' and x['oid'] == ancestor['random']['oid']]
        require(len(shared) == 5 and len(private) == 4 and all(x['oid'] == ancestor['shared_math']['oid'] for x in shared), 'five actual shared and four actual own XW draws')
        require(int(xw['random_seed'], 16) == advance(int(ancestor['random_seed'], 16), 4), 'actual four-draw private LCG delta')
        require(int(xw['shared_math_seed'], 16) == advance(int(ancestor['shared_math_seed'], 16), 10), 'actual five-double shared LCG delta')
        sqrt = [x for x in selected if signature(x, 'entry', 'in', 'a', '(D)F')]
        require(len(sqrt) == 1 and sqrt[0]['sequence'] > shared[-1]['sequence'], 'actual IN sqrt follows all five shared draws')
        d = [number(x['result']) for x in shared]
        p = [number(x['result']) for x in private]
        raw = [float(f32(f32(2 * r - 1) * f32(.4))) for r in d[:3]]
        speed = f32(f32(d[3] + d[4] + 1) * f32(.15))
        length = f32(math.sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]))
        velocity = [((r / length) * speed) * f32(.4) for r in raw]
        velocity[1] += f32(.1)
        fields = xw['fields']
        for name, expected in zip(('sn.aP', 'sn.aQ', 'sn.aR'), velocity):
            require(fields[name] == bits(expected, 'D'), 'observed constructor velocity independent scalar oracle ' + name)
        size = f32(f32(f32(p[2] * f32(.5)) + f32(.5)) * 2)
        expected = {'xw.c': bits(f32(p[0] * 3), 'F'), 'xw.d': bits(f32(p[1] * 3), 'F'), 'xw.g': bits(size, 'F'),
                    'xw.f': int(f32(4 / f32(f32(p[3] * f32(.9)) + f32(.1))))}
        require(all(fields[k] == value for k, value in expected.items()), 'actual jitter/size/lifetime independent scalar oracles')
        icon = [x for x in selected if signature(x, 'return', 'gm', 'a', '(I)I')]
        snow = [x for x in selected if x['event'] == 'field-read' and x['field_class'] == 'uu' and x['field'] == 'aV']
        reads = [x for x in selected if x['event'] == 'field-read' and x['field_class'] == 'uu' and x['field'] == 'bz']
        finished = row['event'] != 'exception'
        if finished:
            final = row['particle']['fields']
            require(len(icon) == len(snow) == len(reads) == 1 and icon[0]['sequence'] < snow[0]['sequence'] < reads[0]['sequence'], 'actual Item icon then Snow reference/gravity read order')
            require(final['xw.b'] == icon[0]['result'] and final['xw.h'] == reads[0]['value'] and final['xw.g'] == bits(f32(size / 2), 'F'), 'actual PB final icon/gravity/half-size stores')
        else:
            require(row['class'] == 'java.lang.NullPointerException' and len(row['particles']) == 1, 'actual failed constructor receiver remains observable')
            final = row['particles'][0]['fields']
            require(final['xw.g'] == fields['xw.g'] and final['xw.h'] == {'F': '00000000'}, 'failed PB retains full ancestor size and zero gravity before final stores')
            if row['location']['pc'] == 17:
                require(not icon and not snow and final['xw.b'] == 0, 'null Item fails before icon/Snow work')
            else:
                require(len(icon) == len(snow) == 1 and snow[0]['value'] is None and not reads and final['xw.b'] == icon[0]['result'], 'null Snow retains actual icon/white prefix before field42')
        result.append({'case': pending['entry']['case'], 'constructor_case': pending['entry']['constructor_case'],
                       'entry': pending['entry'], 'end': row, 'ancestor': ancestor, 'xw': xw, 'shared_draws': shared,
                       'private_draws': private, 'sqrt_entries': sqrt, 'icon_returns': icon, 'snow_reads': snow, 'gravity_reads': reads,
                       'source_scalar_oracles': True, 'constructor_entropy_rebound': False})
        pending = None
    require(pending is None, 'complete constructor outcome transport')
    return result


def validate(out, pins, jar):
    data, event_data, runtime, operation_records, debugger_records = {}, {}, {}, [], []
    for transport in ('normal', 'debugger'):
        data[transport] = {}
        for mode in MODES:
            directory = out / transport / mode
            records = rows(directory / 'target.ndjson')
            data[transport][mode] = records
            stdout = directory / ('target.stdout' if transport == 'debugger' else 'process.stdout')
            stderr = directory / ('target.stderr' if transport == 'debugger' else 'process.stderr')
            lines, errors = stdout.read_text().splitlines(), stderr.read_text().splitlines()
            warnings = [x for x in lines + errors if x.startswith('WARNING: ')]
            require(len(warnings) == 4 and 'ItemParticleTarget' in warnings[1], 'exact declared JDK17 SecurityManager warning family')
            require(all(x.startswith('WARNING: ') for x in errors), 'no undeclared source stderr')
            bindings = {}
            for line in lines:
                if line.startswith('BIND '):
                    _, name, origin, count, digest = line.split()
                    expected = pins['classes'][name]
                    require(name not in bindings and int(count) == expected['class_bytes'] and digest == expected['class_sha256'] and Path(unquote(urlparse(origin).path)).resolve() == jar, 'actual original bytes/JAR runtime origin')
                    bindings[name] = {'origin': origin, 'bytes': int(count), 'sha256': digest}
                else:
                    require(line.startswith('WARNING: ') or re.fullmatch(r'\d+ (recipes|achievements)|ITEM_PARTICLE \d+ -?1 layer2=\d+|ORIGINAL_ITEM_PARTICLE complete \w+', line), 'only declared source stdout; no NW println bypass')
            initialized = set(re.findall("Initializing '([^']+)'", (directory / 'class-init.log').read_text())) & set(pins['jar_class_names'])
            require(set(bindings) == set(pins['classes']) and initialized <= set(bindings), 'complete selected runtime binding/initialization closure')
            runtime[transport + '/' + mode] = {'bindings': bindings, 'initialized_original_classes': sorted(initialized)}
            resources = [r for r in records if r['event'] == 'resource_input']
            require(len(resources) == 2 and not list((directory / 'resources').rglob('*.class')), 'only two authored image resources; no replacement classes')
            for resource in resources:
                path = directory / 'resources' / resource['resource']
                require(sha(path) == resource['sha256'] and Path(unquote(urlparse(resource['runtime_url']).path)).resolve() == path.resolve(), 'actual selected authored resource bytes')
            guard = next(r for r in records if r['event'] == 'network_guard')
            require(all('java.net.InetAddress.getLocalHost' in r and 'sun.font.' in r and 'ji.<init>' in r for r in guard['blocked_attempts']), 'only declared Fontconfig hostname attempts')
            operations = [r for r in records if r['event'] == 'operation']
            (operation_records if transport == 'normal' else debugger_records).extend(operations)
            for record in operations:
                for image in ('baseline_pixels', 'pixels'):
                    path = directory / record[image]['file']
                    require(path.stat().st_size == 262144 and sha(path) == record[image]['sha256'], 'complete actual RGBA artifact')
                require(record['gl_error'] == 0 and record['outcome'] == (-1 if record['case'] in FAILURES else 1), 'actual complete operation/declared Source failure and GL result')
            if transport == 'debugger':
                event_data[mode] = rows(directory / 'checkpoints.ndjson')
    require([r['case'] for r in operation_records] == [r['case'] for r in debugger_records] == list(range(17)), 'all seventeen authored operations in both VMs')
    all_constructors, case_records, live = {}, [], set()
    for mode, events in event_data.items():
        require([e['sequence'] for e in events] == list(range(len(events))), 'complete local-VM event order')
        for event in events:
            if event['event'] == 'code-pin':
                cls = event['class'].replace('.', '/')
                expected = next(m for m in pins['classes'][cls]['methods'] if m['name'] == event['method'] and m['descriptor'] == event['descriptor'])
                require(expected['code_sha256'] == event['sha256'] and expected['code_bytes'] == event['bytes'], 'actual live unchanged original Code')
                live.add((cls, event['method'], event['descriptor']))
        all_constructors[mode] = constructors(events)
        prior = {}
        for record in [r for r in debugger_records if r['input'][1] == mode]:
            case = record['case']
            selected = [e for e in events if e.get('case') == case]
            first = bridge(next(e for e in selected if e['event'] == 'operation-entry'))
            after = bridge(next(e for e in selected if e['event'] == 'state-complete'))
            require(all(after.get(k) == v for k, v in first.items()) and all(first.get(k) == v for k, v in prior.items()), 'actual alias/temporal continuity in each VM')
            prior = after
            for phase, actual in (('before', first), ('after', after)):
                for ref in refs(record[phase]):
                    require(ref['identity'] in actual and same_class(ref['class'], actual[ref['identity']]['class']), 'every selected reference resolves to actual class/JVM identity')
            exceptions = [e for e in selected if e['event'] == 'exception']
            if case in FAILURES:
                cls, method, pc = FAILURES[case]
                require(len(exceptions) == 1 and exceptions[0]['class'] == 'java.lang.NullPointerException' and
                        (exceptions[0]['location']['class'], exceptions[0]['location']['method'], exceptions[0]['location']['pc']) == (cls, method, pc) and len(exceptions[0]['particles']) == 1, 'actual Source Throwable class/BCI/partial receiver')
            else:
                require(not exceptions, 'no undeclared source exception')
            if case in (3, 4, 5, 6, 7, 8, 9, 11, 12, 15, 16):
                require(not any(e['event'] in ('random', 'random-constructor', 'clock') and not image_cache_entropy(e)
                                for e in selected), 'no particle/Math/World/DN update/render draws; only proven ImageIO file-cache entropy separate')
                require(record['before']['streams'] == record['after']['streams'], 'actual update/render streams unchanged')
            batches = [e for e in selected if e['event'] == 'entry' and 'batch' in e]
            if case in (7, 8, 12):
                expected = 384 if case == 12 else 48
                require(len(batches) == 1 and batches[0]['batch']['words'] == record['after']['nw']['selected_words'] and len(batches[0]['batch']['words']) == expected, 'actual complete PB batch/triangle backing')
                require(record['after']['texture']['width'] == record['after']['texture']['height'] == 256 and len(bytes.fromhex(record['after']['texture']['transfer_hex'])) == 262144, 'real atlas upload extent/transfer')
            if case in (0, 10):
                enqueue = [e for e in selected if signature(e, 'entry', 'dn', 'a', '(Lxw;)V')]
                captured = [e for e in selected if e['event'] == 'field-read' and e['field_class'] == 'net.minecraft.client.Minecraft' and e['field'] == 'j' and e['location']['class'] == 'n' and e['location']['pc'] == 683]
                require(len(enqueue) == len(captured) == (8 if case == 10 else 1) and all(a['receiver'] == b['value'] for a, b in zip(enqueue, captured)), 'actual captured DN receiver/enqueue order')
                require(len(record['after']['layer_particles']) == (8 if case == 10 else 1), 'actual PB List publication count')
            if case == 9:
                require(not any(e['event'] == 'entry' and e['location']['class'] == 'pb' for e in selected), 'initial DN NULL returns27 before PB construction')
            case_records.append({'case': case, 'mode': mode, 'entry_bridge': first, 'after_bridge': after, 'events': selected,
                                 'exceptions': exceptions, 'batch_records': batches,
                                 'image_io_entropy': [e for e in selected if image_cache_entropy(e)]})
    for ordinary, debugger in zip(operation_records, debugger_records):
        case = ordinary['case']
        for key in ('input', 'outcome', 'failure_class', 'gl_error'):
            require(ordinary[key] == debugger[key], 'ordinary/JDI exact operation outcome/input')
        for phase in ('before', 'after'):
            for key in ('renderer', 'renderer_world', 'app_dn', 'world_accesses', 'world_access_count', 'origins', 'box_cursor', 'vector_cursor', 'nw', 'gl', 'texture', 'texture_cache_size'):
                require(ordinary[phase][key] == debugger[phase][key], 'ordinary/JDI exact selected state ' + str(case) + '/' + phase + '/' + key)
            require({k: v for k, v in ordinary[phase]['effects'].items() if k != 'random_seed'} ==
                    {k: v for k, v in debugger[phase]['effects'].items() if k != 'random_seed'}, 'ordinary/JDI actual DN/List identities/state; independent DN constructor seed retained separately')
            if case in (3, 4, 5, 6, 7, 8, 9, 11, 12, 15, 16):
                require(ordinary[phase]['particle'] == debugger[phase]['particle'], 'ordinary/JDI exact declared-particle fields')
            if case in (6, 7, 8, 9, 11, 12, 16):
                require(ordinary[phase]['layer_particles'] == debugger[phase]['layer_particles'], 'ordinary/JDI exact declared List particles')
        if case in (7, 8, 12):
            require(ordinary['pixels'] == debugger['pixels'], 'ordinary/JDI exact real raster')
    by_case = {r['case']: r for r in operation_records}
    require(by_case[4]['after']['particle']['fields']['sn.aX'] is True, 'real ground collision flag')
    for case in (5, 6):
        r = by_case[case]
        require(r['after']['particle']['fields']['sn.be'] is True and r['after']['particle']['fields']['sn.aM'] != r['before']['particle']['fields']['sn.aM'], 'expiry continues real movement after K')
    require(by_case[6]['after']['effects']['lists'][2]['size'] == 0, 'DN removes occurrence after completed expired update')
    result = {'scope': 'Finite unchanged-original PB construction/enqueue/update/layer2 draw; raw constructor entropy independent; no native parity.',
              'operation_records': operation_records, 'debugger_operation_records': debugger_records, 'source_events': event_data,
              'case_records': sorted(case_records, key=lambda r: r['case']), 'constructor_records': all_constructors,
              'image_io_entropy': {mode: [e for e in events if image_cache_entropy(e)] for mode, events in event_data.items()},
              'runtime': runtime, 'raw_records': data,
              'counts': {m: dict(collections.Counter(e['event'] for e in events)) for m, events in event_data.items()}}
    (out / 'observations.json').write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    return {'sha256': sha(out / 'observations.json'), 'operations': 17, 'source_constructor_records': {m: len(r) for m, r in all_constructors.items()},
            'live_Code': len(live), 'counts': result['counts'], 'initialized_classes': {m: len(v['initialized_original_classes']) for m, v in runtime.items()}}
