"""Disjoint raw-leaf policy for future FR captures; preparation has no observations."""


def leaves(value, path=()):
    if isinstance(value, dict) and value:
        for key, word in value.items():
            yield from leaves(word, path + (key,))
    elif isinstance(value, list) and value:
        for index, word in enumerate(value):
            yield from leaves(word, path + (index,))
    else:
        yield path


def group(case, path):
    """Assign each actual raw row leaf once, without subtracting unlike totals.

    This is a declared future grouping policy, not a claim that a native
    projection, count, capture or comparison has already been qualified.
    """
    if path[0] == 'source_raw':
        return 'unselected-raw-instance-field-inventory'
    if path[0] in ('exception_message', 'exception_frames'):
        return 'source-exception-text-and-stack-labels'
    if path[0] == 'schema':
        return 'raw-transport-schema'
    if path[-1] == 'counter':
        return 'absolute-counter-operands-relative-increment-is-derived'
    if 'random' in path or 'math_random' in path:
        if case == 23 and path[:1] == ('arrow_after',):
            return 'literal-callback-reseed-final-Random-candidate'
        return 'independent-Random-raw-state-and-within-run-relations'
    if path[-1] == 'id' and 'records' not in path:
        return 'absolute-Entity-ID'
    if path[0] == 'arrow_after' and case >= 7 and case not in {7, 8, 9, 10, 11, 17, 18, 19, 20, 21, 23}:
        return 'ordinary-unobservable-local-Arrow-no-filled-state'
    if 'pitch' in path and path[0] == 'callbacks':
        return 'independent-Skeleton-float-derived-sound-pitch'
    if ('motion' in path or 'angles' in path) and (path[0] == 'arrow_after' or path[0] == 'callbacks' and 'arrow' in path):
        if case == 23 and path[0] == 'arrow_after':
            return 'literal-callback-reseed-final-heading-candidate'
        return 'independent-Arrow-Gaussian-derived-motion-and-angles'
    if case == 21:
        return 'cross-World-publication-native-policy-unresolved'
    return 'ordinary-common-parent-state-and-input-candidate'


def partition(rows):
    groups = {}
    seen = set()
    for row in rows:
        for path in leaves(row):
            location = (row['case'], *path)
            if location in seen:
                raise ValueError('duplicate raw leaf location')
            seen.add(location)
            groups.setdefault(group(row['case'], path), []).append(list(location))
    return {'locations': len(seen), 'groups': groups,
            'note': 'Empty containers count as raw locations. Source journals, helper Code and failed/unpublished local-Arrow witnesses form an additional passive-only domain. No selected scalar count is inferred by subtraction.'}
