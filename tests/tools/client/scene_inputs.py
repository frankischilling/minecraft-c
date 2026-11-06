"""Explicit current fixture admission; historical scene plans remain unchanged."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

DECLARATION = 'tests/client/gameplay/current_scene_inputs.json'
DECLARATION_SHA = '39d007b2c1011b48cc9d76c0bacce4eca803bc545468545bba89e274c7d9daf4'
SCOPE = 'CURRENT_FIXTURE_INPUT_ADMISSION_ONLY'
PROFILES = {'redstone_torch', 'lever'}


def require(value, message):
    if not value:
        raise ValueError(message)


def file_bytes(root, name):
    require(type(name) is str and name and '\\' not in name and ':' not in name,
            'relative source input path')
    path = PurePosixPath(name)
    require(not path.is_absolute() and path.as_posix() == name and
            all(part not in ('.', '..') for part in path.parts),
            'canonical source input path')
    try:
        root = Path(root).resolve(strict=True)
        source = (root / name).resolve(strict=True)
        require(source.is_relative_to(root) and source.is_file(), 'contained source input file')
        return source.read_bytes()
    except OSError as error:
        raise ValueError('unavailable source input: ' + name) from error


def unique(pairs):
    value = {}
    for name, item in pairs:
        require(name not in value, 'duplicate declaration key')
        value[name] = item
    return value


def closed(value, keys, message):
    require(type(value) is dict and set(value) == set(keys), message)


def pins(value):
    require(type(value) is dict and value and
            all(type(name) is str and type(pin) is str and
                re.fullmatch('[0-9a-f]{64}', pin) for name, pin in value.items()),
            'exact source input digest map')


def read_current_plan(root, profile, scene, plan_sha):
    require(type(profile) is str and profile in PROFILES, 'declared current input profile')
    raw = file_bytes(root, DECLARATION)
    require(hashlib.sha256(raw).hexdigest() == DECLARATION_SHA, 'pinned current input declaration')
    declaration = json.loads(raw, object_pairs_hook=unique,
                             parse_constant=lambda value: require(False, 'finite declaration JSON'))
    closed(declaration, ('format', 'scope', 'profiles'), 'closed current input declaration')
    require(type(declaration['format']) is int and declaration['format'] == 1 and
            declaration['scope'] == SCOPE, 'current fixture admission scope')
    closed(declaration['profiles'], PROFILES, 'closed current input profiles')
    entry = declaration['profiles'][profile]
    closed(entry, ('predecessor', 'source_input_sha256'), 'closed current input profile')
    predecessor = entry['predecessor']
    closed(predecessor, ('scene', 'sha256', 'source_input_sha256'), 'closed predecessor declaration')
    require(predecessor['scene'] == scene and predecessor['sha256'] == plan_sha,
            'exact predecessor scene identity')
    raw = file_bytes(root, scene)
    require(hashlib.sha256(raw).hexdigest() == plan_sha, 'unchanged frozen predecessor plan')
    plan = json.loads(raw, object_pairs_hook=unique)
    old, current = predecessor['source_input_sha256'], entry['source_input_sha256']
    pins(old)
    pins(current)
    require(old == plan['source_input_sha256'], 'unchanged predecessor input map')
    require(set(current) == set(old), 'complete current inherited input map')
    require(all(hashlib.sha256(file_bytes(root, name)).hexdigest() == pin
                for name, pin in current.items()), 'exact current inherited input bytes')
    return plan


def receipt(root, profile, scene, plan_sha):
    read_current_plan(root, profile, scene, plan_sha)
    return {'scope': SCOPE, 'profile': profile, 'declaration': DECLARATION,
            'declaration_sha256': DECLARATION_SHA, 'predecessor_plan_sha256': plan_sha,
            'historical_native_results': 'NOT_REQUALIFIED'}
