"""Typed observed-stream accounting, with full unpublished Entity frontier explicit."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

R = load("redstone_torch_observation", Path(__file__).with_name("observation.py"))
S = load("redstone_torch_entropy", Path(__file__).with_name("entropy.py"))
UNIQUE_MULT = 1181783497276652981
require = R.require

def state(value):
    return S.E.state(value)

def typed_entropy(row, raw):
    require(type(raw) is dict and set(raw) == R.ENTROPY_KEYS, 'exact entropy schema')
    require(type(raw['index']) is int and type(raw['mode']) is int and (raw['mode'] in (0, 1, 2)) and (type(raw['supplied_alias']) is int) and (raw['supplied_alias'] >= 0), 'typed entropy integer fields')
    for key in ('seed_uniquifier_before', 'seed_uniquifier_after'):
        require(type(raw[key]) is str and re.fullmatch('[0-9a-f]{16}', raw[key]), 'canonical typed allocation counter')
    for side in ('owners_before', 'owners_after'):
        value = raw[side]
        require(type(value) is dict and set(value) == {'worlds', 'independent'} and (type(value['worlds']) is list), 'closed owner inventory')
        for ordinal, owner in enumerate(value['worlds']):
            require(type(owner) is dict and set(owner) == {'role', 'alias', 'state'} and (type(owner['role']) is int) and (owner['role'] == ordinal) and (type(owner['alias']) is int) and (owner['alias'] > 0), 'typed actual World owner')
            state(owner['state'])
        owner = value['independent']
        require(type(owner) is dict and set(owner) == {'alias', 'state'} and (type(owner['alias']) is int) and (owner['alias'] > 0), 'typed independent owner')
        state(owner['state'])
    for key in ('math_before', 'math_after'):
        if raw[key] is not None:
            state(raw[key])
    schemas = {'item': set('event ordinal world alias position motion yaw bob entity_random math_post states'.split()), 'sound': set('event ordinal world name position volume pitch states'.split()), 'particle': set('event ordinal world name position motion states'.split())}
    require(type(raw['events']) is list, 'raw event List')
    for ordinal, event in enumerate(raw['events']):
        require(type(event) is dict and event.get('event') in schemas and (set(event) == schemas[event['event']]) and (type(event['ordinal']) is int) and (event['ordinal'] == ordinal) and (type(event['world']) is int) and (event['world'] == row['world_role']), 'closed typed raw event')
        require(set(event['states']) == {'world', 'independent'}, 'exact event states')
        state(event['states']['world'])
        state(event['states']['independent'])
        if event['event'] == 'item':
            state(event['math_post'])
            state(event['entity_random'])
    if raw['math_before'] is None and raw['math_after'] is not None:
        require(raw['math_after']['gaussian'] == '0000000000000000' and raw['math_after']['have'] is False, 'first cold Math post cache is actual constructor default')
        items = [event for event in raw['events'] if event['event'] == 'item']
        require(items and items[0]['math_post']['gaussian'] == '0000000000000000' and (items[0]['math_post']['have'] is False), 'first observed Item Math cache default')

def audit(rows, own):
    require(len(rows) == len(own), 'complete successor stream cohort')
    for row, raw in zip(rows, own):
        typed_entropy(row, raw)
    observation_digest = lambda: hashlib.sha256(json.dumps([rows, own], sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    observation_before = observation_digest()
    result = S.audit_cohort(rows, own)
    require(observation_before == observation_digest(), 'positive Source observations unchanged by successor algorithm')
    previous = None
    constructors = 0
    holder_initializations = 0
    for row, raw in zip(rows, own):
        before = int(raw['seed_uniquifier_before'], 16)
        after = int(raw['seed_uniquifier_after'], 16)
        if previous is not None:
            require(before == previous, 'default-allocation counter cross-row join')
        published = sum((event['event'] == 'item' for event in raw['events']))
        inferred = 1 if S.bounded_only(row, raw) is not None else 0
        cold = raw['math_before'] is None and raw['math_after'] is not None
        count = published + inferred + int(cold)
        require(before * pow(UNIQUE_MULT, count, 1 << 64) & (1 << 64) - 1 == after, 'precise published/unpublished/first-holder default-constructor count')
        constructors += count
        holder_initializations += int(cold)
        previous = after
    return dict(result, default_constructor_steps=constructors, first_math_holder_initializations=holder_initializations, full_entropy_qualification=False, entity_state_frontier='unpublished identity/fields/Entity.bs/System.nanoTime unobserved')

audit_cohort = audit
