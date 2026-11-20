"""Java-free audit of finite nested original Stair side-getter observations."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SIDES = (0, 1, 2, 3, 4, 5, -1, 6, -2147483648, 2147483647)
SOURCES = ('InventorySideChainsTarget.java', 'InventorySideChainsJdi.java', 'ConstructorJson.java',
           'capture_reference.py', 'packet.py', 'test_packet.py', 'inputs.json')
CODE = {'ss': (9, '0b86e0da7082456a60ce27f879636b60789c8d2f984090b61ed586aaf786bbd6'),
        'uu': (5, '5e5d14f709d860f1cdea2e6c150d802146d44c7760ea4cc48128430ca05d07ba')}
CLASS = {'ss': (3343, '36150aa40e71dae4e63327956e54875728134fefb8039375ce52249808115364'),
         'uu': (17378, 'caeaf354357d66fc168c7f03bf62bb6c6a1fe459e9be6b537d4d5d1efabd0845')}


def require(value, message):
    if not value:
        raise ValueError('Original nested Stair side packet: ' + message)


def unique(pairs):
    result = dict(pairs)
    require(len(result) == len(pairs), 'duplicate JSON key')
    return result


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def same(actual, expected, message):
    # JSON preserves the distinction between booleans and integer0/1.
    require(json.dumps(actual, sort_keys=True, separators=(',', ':')) ==
            json.dumps(expected, sort_keys=True, separators=(',', ':')), message)


def tagged(text, name):
    prefix = 'SIDE_CHAIN_' + name + ' '
    return [json.loads(line[len(prefix):], object_pairs_hook=unique)
            for line in text.splitlines() if line.startswith(prefix)]


def validate_target(text):
    owner = tagged(text, 'OWNER')
    same(owner, [{'blocks': 96, 'outer_id': 53, 'inner_id': 67, 'wood_id': 5,
                 'outer_class': 'ss', 'inner_class': 'ss', 'wood_class': 'uu', 'field_owner': 'ss',
                 'field_name': 'a', 'field_type': 'uu', 'field_modifiers': 2, 'field_final': False,
                 'raw_allocations': 0, 'game_substitution': False}], 'genuine Registry and mutable original field')
    inputs, returns = tagged(text, 'INPUT'), tagged(text, 'RETURN')
    require(len(inputs) == len(returns) == 30, 'all thirty actual direct getter calls')
    for call, (before, after) in enumerate(zip(inputs, returns)):
        profile, index = divmod(call, 10)
        same(before, {'call': call, 'profile': profile, 'side': SIDES[index], 'outer_source': 67,
                     'inner_source': None if profile == 2 else 5, 'wood_texture': 4 if profile == 0 else 83},
             'exact declared source aliases, side and late texture')
        same(after, {'call': call, 'profile': profile, 'side': SIDES[index], 'returned': profile != 2,
                     'texture': None if profile == 2 else 4 if profile == 0 else 83,
                     'exception': 'java.lang.NullPointerException' if profile == 2 else None,
                     'source_stack': ['ss.a', 'ss.a', 'InventorySideChainsTarget.main'] if profile == 2 else [],
                     'outer_source': 67, 'inner_source': None if profile == 2 else 5, 'bounds_unchanged': True},
             'exact actual return or nested failure with source aliases/bounds retained')
    complete = tagged(text, 'COMPLETE')
    same(complete, [{'calls': 30, 'returned': 20, 'failed': 10, 'network': 0, 'native_links': 0,
                    'cycle_executed': False, 'raw_allocations': 0, 'debugger_method_invocations': 0}],
         'bounded source and external-effect guards')
    return {'owners': owner[0], 'inputs': inputs, 'returns': returns, 'complete': complete[0]}


def selected_pins(pins):
    require(pins['client_jar_sha256'] == JAR, 'unchanged supplied original archive')
    result = {'client_jar_sha256': JAR, 'selected_methods_only': True, 'classes': {}}
    for name in ('ss', 'uu'):
        source = pins['classes'][name]
        method = next(m for m in source['methods'] if m['name'] == 'a' and m['descriptor'] == '(I)I')
        result['classes'][name] = {'class_bytes': source['class_bytes'], 'class_sha256': source['class_sha256'],
                                   'method': {k: method[k] for k in ('name', 'descriptor', 'code_bytes', 'code_sha256')}}
    return result


def audit(observations, pins):
    require(set(observations) == set('owners inputs returns complete events live_code'.split()),
            'selected observation fields')
    wire = json.dumps(observations)
    require(re.search(r'[A-Za-z]:[\\/]|/home/|file://', wire) is None, 'no private absolute paths')
    # Reuse the strict actual stdout projection schema without publishing raw
    # CRLF transport. This is typed JSON, not an original stdout byte claim.
    text = '\n'.join('SIDE_CHAIN_' + name + ' ' + json.dumps(value)
                     for name, values in (('OWNER', [observations['owners']]), ('INPUT', observations['inputs']),
                                          ('RETURN', observations['returns']), ('COMPLETE', [observations['complete']]))
                     for value in values)
    selected = validate_target(text)
    for key in selected:
        same(observations[key], selected[key], 'exact selected actual source observations')
    same(pins, {'client_jar_sha256': JAR, 'selected_methods_only': True, 'classes': {
        name: {'class_bytes': CLASS[name][0], 'class_sha256': CLASS[name][1],
               'method': {'name': 'a', 'descriptor': '(I)I', 'code_bytes': CODE[name][0], 'code_sha256': CODE[name][1]}}
        for name in ('ss', 'uu')}}, 'selected unchanged original class and method Code fingerprints')
    events = observations['events']
    require(type(events) is list and len(events) == 92, 'complete bounded JDI event extent')
    live = [event for event in events if event['event'] == 'live-code']
    same(observations['live_code'], live, 'actual duplicated live Code receipts agree')
    same(live, [{'event': 'live-code', 'class': name, 'method': 'a', 'descriptor': '(I)I',
                 'bytes': CODE[name][0], 'sha256': CODE[name][1]} for name in ('ss', 'uu')],
         'both reached unchanged getter bodies')
    meaningful = [event for event in events if event['event'] != 'live-code']
    require(len(meaningful) == 90, 'eighty actual entries and ten exceptions')
    identities = {}
    cursor = 0
    for call in range(30):
        profile, index = divmod(call, 10)
        count = 3
        group = meaningful[cursor:cursor + count]
        cursor += count
        require([e['event'] for e in group] == (['getter-entry'] * 3 if call < 20 else
                ['getter-entry', 'getter-entry', 'source-exception']), 'exact ordered nested source calls')
        for order, entry in enumerate(group[:3 if call < 20 else 2]):
            block = (53, 67, 5)[order]
            receiver = entry.get('receiver')
            require(type(receiver) is int and receiver > 0, 'actual object identity handle')
            if block in identities:
                require(identities[block] == receiver, 'retained registered singleton identity')
            identities[block] = receiver
            expected = {'event': 'getter-entry', 'call': call, 'class': 'uu' if block == 5 else 'ss',
                        'receiver': receiver, 'id': block, 'side': SIDES[index],
                        'source_id': 67 if block == 53 else None if block == 5 or profile == 2 else 5,
                        'texture': 4 if block == 53 else 16 if block == 67 else 4 if profile == 0 else 83}
            same(entry, expected, 'actual receiver, side, source alias and live texture fields')
        if call >= 20:
            site = {'class': 'ss', 'method': 'a', 'descriptor': '(I)I', 'pc': 5}
            same(group[2], {'event': 'source-exception', 'call': call, 'exception': 'java.lang.NullPointerException',
                             'site': site, 'frames': [{'site': site, 'receiver_id': 67},
                                                     {'site': site, 'receiver_id': 53}]},
                 'exact inner SS5 and outer SS5 source NPE frames')
    require(len(set(identities.values())) == 3, 'three distinct genuine Block singleton identities')
    return {'rows': 30, 'returned': 20, 'source_failures': 10, 'getter_entries': 80,
            'live_code_bodies': 2, 'exact_nested_pcs': [5, 5], 'network': 0, 'native_links': 0}


def audit_public():
    result = audit(read(HERE / 'observations.json'), read(HERE / 'source-pins.json'))
    provenance = read(HERE / 'provenance.json')
    require(provenance['status'] == 'CAPTURED_ORIGINAL' and provenance['client_jar_sha256'] == JAR and
            provenance['ordinary_fresh_vms'] == 2 and provenance['read_only_jdi_fresh_vms'] == 1 and
            provenance['public_sources_pinned_before_capture'] is True and
            provenance['inputs_unchanged'] is True and provenance['helpers_unchanged'] is True,
            'qualified fresh actual processes and unchanged input/helper bindings')
    require(set(provenance['source_files']) == set(SOURCES) and
            set(provenance['fixtures']) == {'observations.json', 'source-pins.json'}, 'exact published binding roles')
    for section in ('source_files', 'fixtures'):
        for name, wanted in provenance[section].items():
            require(type(wanted) is str and re.fullmatch('[0-9a-f]{64}', wanted) is not None and
                    digest(HERE / name) == wanted, 'actual current source/fixture byte binding ' + name)
    return result


if __name__ == '__main__':
    print(audit_public())
