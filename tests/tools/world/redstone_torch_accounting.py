"""Exact native constructor/stream accounting with an explicit Entity frontier."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
MULT = 1181783497276652981
MASK = (1 << 64) - 1
WITNESSES = (58, 59, 62, 63)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


N = load('db_native_current_decoder', HERE / 'redstone_torch_current.py')
require = N.need
unchanged = N.exact


def advance_counter(value, count):
    return (value * pow(MULT, count, 1 << 64)) & MASK


def native_bounded(row, own, declared):
    """Native-only gate: no Source Entity/List/Set graphs are manufactured."""
    if row['cohort'] != 'lifecycle' or row['index'] not in WITNESSES:
        return None
    unchanged({'name': row['name'], 'values': row['values']}, declared, 'exact frozen native witness input')
    v = row['values']
    require(type(row['index']) is int and len(v) == 15 and all(N.integer(x, 32) for x in v) and
            v[1] in (0, 2) and v[3] in (-2147483648, 2147483647), 'declared far-X added/neighbor only')
    unchanged(v[:1] + v[2:3] + v[4:], [0, 76, 65, 8, 5, 76, 0, 0, 0, 0, 0, 1, 0], 'precise far-X operands')
    actual = row['native']
    require(actual['invoke_ok'] is True and actual['entered'] is True and actual['returned'] is True and
            actual['native_failure'] is False and actual['observer_refused'] is False and
            actual['source_null'] is False and actual['error']['status'] == 0 and
            actual['error']['db_source_null'] is False and actual['error']['vm_source_null'] is False,
            'native witness completed without refusal or source-null')
    require(row['events'] == [] and row['changes'] == [] and
            row['center_before'] is None and row['center_after'] is None, 'no hidden publication/query/array effect')
    before, after = row['before'], row['after']
    for key in ('arrays', 'history', 'position_set'):
        unchanged(before[key], after[key], 'native witness unchanged ' + key)
    require(len(before['worlds']) == len(after['worlds']) == 1, 'native witness single World')
    wb, wa = before['worlds'][0], after['worlds'][0]
    require(wb['role'] == wa['role'] == 0 and wb['time'] == wa['time'] == 0, 'actual saved witness World')
    # The actual neighbor scheduler E/F/counter changes remain untouched and
    # are independently checked by the complete literal Source comparison.
    unchanged({k: v for k, v in wb.items() if k not in ('ordered', 'membership')},
              {k: v for k, v in wa.items() if k not in ('ordered', 'membership')},
              'all native nonscheduler World fields including M/Entities/C')
    require(own['mode'] == 0 and len(own['owners_before']['worlds']) == len(own['owners_after']['worlds']) == 1,
            'native witness actual World RNG argument')
    rb, ra = own['owners_before']['worlds'][0], own['owners_after']['worlds'][0]
    require(rb['role'] == ra['role'] == 0 and rb['alias'] == ra['alias'] == own['supplied_alias'],
            'native witness retained World RNG identity')
    unchanged(own['owners_before']['independent'], own['owners_after']['independent'], 'unreached independent RNG')
    E = normal_algorithm().E
    seed, unused = E.floats(E.state(rb['state']), 4)
    require(seed == E.state(ra['state']) and E.same_cache(rb['state'], ra['state']), 'exact native four World float steps')
    mb, ma = own['math_before'], own['math_after']
    require(type(mb) is dict and type(ma) is dict, 'native initialized Math predecessor')
    seed, unused = E.doubles(E.state(mb), 4)
    require(seed == E.state(ma) and E.same_cache(mb, ma), 'exact native eight Math LCG steps')
    require(advance_counter(int(own['seed_uniquifier_before'], 16), 1) == int(own['seed_uniquifier_after'], 16),
            'exact native single unpublished constructor')
    return {'index': row['index'], 'name': row['name'], 'inferred_unpublished_HL_constructors': 1,
            'world_float_steps': 4, 'math_lcg_steps': 8, 'seed_uniquifier_multiplications': 1,
            'scheduler_changed': not N.same(wb['ordered'], wa['ordered']) or not N.same(wb['membership'], wa['membership']),
            'entity_identity_observed': False, 'entity_fields_observed': False, 'entity_bs_observed': False}


_NORMAL = None


def normal_algorithm():
    global _NORMAL
    if _NORMAL is None:
        _NORMAL = load('db_native_accounted_shared_normal', N.REFERENCE / 'entropy.py')
    return _NORMAL


def exact_counts(rows, own, inferred):
    previous = None
    totals = {'published_HL': 0, 'inferred_unpublished_HL': 0, 'first_Math_holder': 0, 'default_constructor_steps': 0}
    per_row = []
    for row, raw in zip(rows, own):
        require(all(type(raw[k]) is str and re.fullmatch('[0-9a-f]{16}', raw[k])
                    for k in ('seed_uniquifier_before', 'seed_uniquifier_after')), 'typed exact native counters')
        before, after = [int(raw[k], 16) for k in ('seed_uniquifier_before', 'seed_uniquifier_after')]
        if previous is not None:
            require(before == previous, 'native constructor counter cross-row join')
        published = sum(e['kind'] == 'item' for e in row['events'])
        hidden = int(row['index'] in inferred)
        cold = raw['math_before'] is None and raw['math_after'] is not None
        if cold:
            require(published > 0 and raw['math_after']['gaussian'] == '0000000000000000' and
                    raw['math_after']['have'] is False, 'native first cold Math post cache')
            first = next(e for e in row['events'] if e['kind'] == 'item')
            require(first['math_post']['gaussian'] == '0000000000000000' and first['math_post']['have'] is False,
                    'native first Item Math constructor cache')
        count = published + hidden + int(cold)
        require(advance_counter(before, count) == after, 'exact native published+inferred+first-holder constructor count')
        cursor, holder_pending = before, cold
        for event in row['events']:
            if event['kind'] == 'item':
                cursor = advance_counter(cursor, 1 + int(holder_pending))
                holder_pending = False
            require(type(event['uniquifier']) is str and event['uniquifier'] == f'{cursor:016x}',
                    'actual native callback-boundary constructor counter')
        require(not holder_pending, 'first Math holder reached an observed Item')
        previous = after
        totals['published_HL'] += published
        totals['inferred_unpublished_HL'] += hidden
        totals['first_Math_holder'] += int(cold)
        totals['default_constructor_steps'] += count
        per_row.append({'index': row['index'], 'published_HL': published, 'inferred_unpublished_HL': hidden,
                        'first_Math_holder': int(cold), 'default_constructor_steps': count})
    return dict(totals, rows=per_row)


def audit_native(cohort, rows, own, original):
    unchanged(len(rows), N.COHORTS[cohort], 'complete native cohort')
    require(len(own) == len(rows), 'complete native own stream cohort')
    for index, (row, raw, source_row) in enumerate(zip(rows, own, original['rows'])):
        unchanged({k: row[k] for k in ('cohort', 'index', 'name', 'values')},
                  {k: source_row[k] for k in ('cohort', 'index', 'name', 'values')}, 'exact admitted native row identity')
        require(type(row['index']) is int and row['index'] == index and type(raw['index']) is int and
                raw['index'] == index and raw['cohort'] == cohort and raw['name'] == row['name'], 'typed native entropy pairing')
        require(type(raw['mode']) is int and raw['mode'] == row['values'][11], 'typed exact native supplied mode')
        require(all(type(row['native'][k]) is bool for k in N.NATIVE_KEYS - {'error'}), 'typed native outcome')
        require(row['native']['error'] is not None, 'reached native diagnostic')
        N.validate_error(row['native']['error'])
        for snap in (row['before'], row['after']):
            N.validate_snapshot(snap, 6 if cohort == 'aging' else 1)
        for event in row['events']:
            N.validate_event(event)
    # This retains the full existing deterministic effects/aliases/scheduler/M
    # comparison. Its old entropy diagnostic remains raw historical context.
    deterministic = N.compare_cohort(cohort, rows, own, original)
    declaration = N.inputs()['cohorts'][cohort]
    minimal, adapted, inferred = [], [], {}
    for row, raw in zip(rows, own):
        logical, entropy_events, drops = N.project_events(row)
        minimal.append({'cohort': cohort, 'index': row['index'], 'name': row['name'], 'values': row['values'],
                        'world_role': row['values'][0], 'drops': drops})
        adapted.append(dict(raw, supplied_alias=raw['supplied_alias'] if raw['supplied_alias'] is not None else 0,
                            events=entropy_events))
        witness = native_bounded(row, raw, declaration[row['index']])
        if witness is not None:
            inferred[row['index']] = witness
    unchanged(sorted(inferred), list(WITNESSES) if cohort == 'lifecycle' else [], 'only four frozen native unpublished witnesses')
    counts = exact_counts(rows, own, inferred)
    unchanged({k: counts[k] for k in ('published_HL', 'inferred_unpublished_HL', 'first_Math_holder', 'default_constructor_steps')},
              {'published_HL': 12, 'inferred_unpublished_HL': 4, 'first_Math_holder': 1, 'default_constructor_steps': 17}
              if cohort == 'lifecycle' else
              {'published_HL': 0, 'inferred_unpublished_HL': 0, 'first_Math_holder': 0, 'default_constructor_steps': 0},
              'precise frozen native cohort constructor totals')
    algorithm = normal_algorithm()
    # Replace only the separate accounting eligibility hook on this fresh
    # module instance. The pinned normal stream algorithm executes unchanged.
    algorithm.bounded_only = lambda row, raw: inferred.get(row['index'])
    before = hashlib.sha256(json.dumps([rows, own], sort_keys=True).encode()).hexdigest()
    streams = algorithm.audit_cohort(minimal, adapted)
    require(before == hashlib.sha256(json.dumps([rows, own], sort_keys=True).encode()).hexdigest(),
            'no positive native observation mutation')
    return {'status': 'PASS_BOUNDED_OBSERVED_STREAMS_WITH_UNOBSERVED_ENTITY_FRONTIER', 'rows': len(rows),
            'full_entropy_qualification': False, 'deterministic_effects': deterministic['deterministic_effects'],
            'own_streams': streams, 'exact_constructor_accounting': counts,
            'native_inferred_unpublished': list(inferred.values()), 'native_JDK_topology_fabricated': False,
            'Source_Java_ISE_equals_native_LIMIT': False}


def corruption_controls(rows, own, original):
    """Reject changed copies of the actual completed lifecycle observations."""
    import copy
    before = hashlib.sha256(json.dumps([rows, own], sort_keys=True).encode()).hexdigest()

    def missing_constructor(r, o):
        o[58]['seed_uniquifier_after'] = o[58]['seed_uniquifier_before']

    def unused_random(r, o):
        o[59]['owners_before']['independent']['state']['seed'] = '000000000001'

    def deadline(r, o):
        r[62]['after']['worlds'][0]['membership']['records'][0]['time'] += 1

    def counter(r, o):
        r[62]['after']['worlds'][0]['M'] += 1

    def return_value(r, o):
        r[58]['native']['returned'] = False

    def operand(r, o):
        r[58]['values'][3] = 8

    def array_bytes(r, o):
        r[58]['after']['arrays'][0][5] = '0' * 64

    def history(r, o):
        r[62]['after']['history']['modCount'] += 1

    def boolean_type(r, o):
        r[0]['native']['entered'] = 1

    def callback_counter(r, o):
        next(e for row in r for e in row['events'] if e['kind'] == 'item')['uniquifier'] = '0000000000000000'

    rejected = []
    for mutate in (missing_constructor, unused_random, deadline, counter, return_value,
                   operand, array_bytes, history, boolean_type, callback_counter):
        r, o = copy.deepcopy((rows, own))
        mutate(r, o)
        try:
            audit_native('lifecycle', r, o, original)
        except ValueError:
            rejected.append(mutate.__name__)
        else:
            raise ValueError('changed native observation admitted: ' + mutate.__name__)
    require(before == hashlib.sha256(json.dumps([rows, own], sort_keys=True).encode()).hexdigest(),
            'positive completed native observations unchanged by controls')
    return {'rejected': rejected, 'count': len(rejected), 'actual_positive_objects_unchanged': True}


