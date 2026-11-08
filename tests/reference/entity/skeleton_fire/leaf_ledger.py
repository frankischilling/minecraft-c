"""Disjoint raw source-row leaf locations; empty retained containers count once."""
import hashlib
import json


def leaves(value, path=()):
    if isinstance(value, dict) and value:
        for key, word in value.items():
            yield from leaves(word, path + (key,))
    elif isinstance(value, list) and value:
        for index, word in enumerate(value):
            yield from leaves(word, path + (index,))
    else:
        yield path


def snapshot_roots(row):
    result = []
    for side in ('before', 'after'):
        result += [((side, 'actor', 'entity'), False, False), ((side, 'target'), False, False),
                   ((side, 'wolf'), False, row['input'][3] != 2)]
    result.append((('arrow_after',), True, False))
    for i, event in enumerate(row['callbacks']):
        if event['kind'] == 'sound':
            result.append((('callbacks', i, 'actor', 'entity'), False, False))
        elif event['kind'] in ('added', 'added_return'):
            result.append((('callbacks', i, 'arrow'), True, False))
    return result


def group(row, path):
    if path[0] == 'source_raw':
        return 'unselected_raw_instance_inventory'
    if path[0] in ('exception_message', 'exception_frames'):
        return 'source_exception_text_and_stack_labels'
    if path[0] == 'schema':
        return 'raw_transport_schema'
    if path in (('before', 'counter'), ('after', 'counter')):
        return 'absolute_counter_operands_relative_increment_is_derived'
    if 'math_random' in path:
        return 'independent_shared_Math_stream_checked_within_run'
    for prefix, arrow, unposed_wolf in snapshot_roots(row):
        if path[:len(prefix)] != prefix:
            continue
        rest = path[len(prefix):]
        if rest[:1] == ('id',):
            return 'absolute_Entity_ID'
        if rest[:1] == ('random',):
            if row['case'] == 23 and prefix == ('arrow_after',):
                return 'ordinary_common_field_storage'
            return 'independent_Random_storage_presence_and_transitions_separate'
        if unposed_wolf and rest == ('angles', 0):
            return 'unposed_auxiliary_Wolf_Math_random_constructor_yaw'
        if arrow and rest[:1] in (('motion',), ('angles',)):
            if row['case'] == 23 and prefix == ('arrow_after',):
                return 'ordinary_common_field_storage'
            return 'independent_Arrow_Gaussian_derived_motion_and_angles'
    if 'random' in path:
        return 'independent_World_Random_checked_within_run'
    if path[0] == 'callbacks' and path[-1] == 'pitch':
        return 'independent_Skeleton_float_derived_bow_pitch'
    return 'ordinary_common_field_storage'


def partition(rows):
    groups = {}; seen = set()
    for row in rows:
        for path in leaves(row):
            location = (row['case'], *path)
            if location in seen:
                raise ValueError('duplicate raw leaf location')
            seen.add(location)
            groups.setdefault(group(row, path), []).append(list(location))
    return groups


def summary(rows):
    groups = partition(rows)
    return {'raw_locations': sum(map(len, groups.values())),
            'groups': {name: {'locations': len(paths), 'location_inventory_sha256': hashlib.sha256(
                json.dumps(paths, separators=(',', ':'), ensure_ascii=True).encode('ascii')).hexdigest()}
                for name, paths in sorted(groups.items())},
            'note': 'Each raw row leaf is assigned once; empty containers count once. Derived selected scalars and additional passive witnesses use different domains; no exclusion count is inferred by subtraction.'}
