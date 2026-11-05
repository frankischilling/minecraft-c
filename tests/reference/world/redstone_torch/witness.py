"""Four frozen input witnesses; inferred construction is not an Entity snapshot."""
MASK = (1 << 48) - 1
MULT = 25214903917
UNIQUE_MULT = 1181783497276652981

def require(value, message):
    if not value:
        raise ValueError(message)

def same(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return set(a) == set(b) and all((same(a[k], b[k]) for k in a))
    if type(a) is list:
        return len(a) == len(b) and all((same(x, y) for x, y in zip(a, b)))
    return a == b

def advance(seed, count):
    for _ in range(count):
        seed = seed * MULT + 11 & MASK
    return seed

def cache_unchanged(a, b):
    return same(a['gaussian'], b['gaussian']) and same(a['have'], b['have'])

def bounded_witness(row, own, declared):
    """Eligibility comes from frozen inputs/static path, not unexplained drift."""
    require(same(row['values'], declared['values']) and row['name'] == declared['name'], 'frozen row')
    v = row['values']
    require(row['cohort'] == 'lifecycle' and len(v) == 15 and (v[1] in (0, 2)) and (v[3] in (-2147483648, 2147483647)) and same(v[:1] + v[2:3] + v[4:], [0, 76, 65, 8, 5, 76, 0, 0, 0, 0, 0, 1, 0]), 'only frozen far-X added/neighbor support-loss path')
    require(row['world_role'] == 0 and row['time'] == 0 and (row['result'] == 'VOID') and (row['exception'] == 'NONE') and (row['observer_refused'] is False), 'completed ordinary Source path')
    require(row['events'] == [] and row['drops'] == [] and (own['events'] == []) and (row['changes'] == []) and same(row['arrays_before'], row['arrays_after']), 'no invented publication or array effect')
    require(len(row['worlds_before']) == len(row['worlds_after']) == 1 and same(row['worlds_before'][0]['entities'], row['worlds_after'][0]['entities']), 'actual Entity container unchanged')
    require(same(row['history_before'], row['history_after']) and same(row['position_set_before'], row['position_set_after']), 'actual unrelated list/Set unchanged')
    before, after = (own['owners_before'], own['owners_after'])
    require(own['cohort'] == row['cohort'] and own['index'] == row['index'] and (own['name'] == row['name']) and (own['mode'] == 0), 'raw row pairing')
    require(len(before['worlds']) == len(after['worlds']) == 1, 'actual single World owner')
    wb, wa = (before['worlds'][0], after['worlds'][0])
    require(wb['role'] == wa['role'] == 0 and wb['alias'] == wa['alias'] == own['supplied_alias'], 'actual same World/argument owner')
    require(same(before['independent'], after['independent']), 'unreached independent stream unchanged')
    require(advance(int(wb['state']['seed'], 16), 4) == int(wa['state']['seed'], 16) and cache_unchanged(wb['state'], wa['state']), 'exact four World nextFloat steps')
    mb, ma = (own['math_before'], own['math_after'])
    require(type(mb) is dict and type(ma) is dict, 'observed initialized Math predecessor required')
    require(advance(int(mb['seed'], 16), 8) == int(ma['seed'], 16) and cache_unchanged(mb, ma), 'exact four Math nextDouble calls/eight LCG steps')
    ub, ua = (int(own['seed_uniquifier_before'], 16), int(own['seed_uniquifier_after'], 16))
    require(ub * UNIQUE_MULT & (1 << 64) - 1 == ua, 'exact one default Random constructor')
    return dict(index=row['index'], name=row['name'], inferred_unpublished_HL_constructors=1, world_float_steps=4, math_double_calls=4, math_lcg_steps=8, seed_uniquifier_multiplications=1, scheduler_graph_changed=not same(row['worlds_before'], row['worlds_after']), entity_alias=None, entity_fields_observed=False, entity_bs_observed=False, status='BOUNDED_CONTINUATION_ACCOUNTED_ENTITY_STATE_UNQUALIFIED')
