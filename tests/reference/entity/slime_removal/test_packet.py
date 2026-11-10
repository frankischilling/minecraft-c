"""Python-only negative comparator controls over genuinely captured observation VALUES."""
import copy
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from capture_reference import DIRECT_PROFILES, HERE, ensure, exact, read_json, rows, validate_row
from compare import compare_subset


def main():
    original = read_json(HERE / 'finite.json')
    selected = [row for name in DIRECT_PROFILES for row in original if row['profile'] == name]
    ensure(compare_subset(original, selected, DIRECT_PROFILES) == 11, 'all captured direct-call records match themselves')
    refused = 0
    mutations = (
        ('health', lambda r: r[0].__setitem__('health', -1)),
        ('Boolean-as-integer', lambda r: r[0].__setitem__('dead', 1)),
        ('wrong-parent-stream', lambda r: r[0].__setitem__('random', '0' * 16)),
        ('wrong-yaw', lambda r: r[0]['children'][0].__setitem__('yaw', '00000000')),
        ('wrong-offset', lambda r: r[0]['children'][0]['position'].__setitem__(0, '0000000000000000')),
        ('wrong-child-health', lambda r: r[0]['children'][0].__setitem__('health', 2)),
        ('early-parent-removal', lambda r: r[0]['children'][0].__setitem__('parent_dead', True)),
        ('lost-child', lambda r: r[0]['children'].pop()),
        ('reordered-children', lambda r: r[0]['children'].reverse()),
        ('lost-world-membership', lambda r: r[0]['children'][0].__setitem__('world_member', False)),
        ('wrong-callback-World', lambda r: r[0]['children'][0].__setitem__('world', 'SECONDARY')),
        ('wrong-exception', lambda r: r[5].__setitem__('exception', 'NONE')),
        ('lost-row', lambda r: r.pop()),
        ('extra-row', lambda r: r.append(copy.deepcopy(r[0]))),
        ('unknown-field', lambda r: r[0].__setitem__('made_up', 1)),
    )
    for name, change in mutations:
        altered = copy.deepcopy(selected); change(altered)
        try:
            compare_subset(original, altered, DIRECT_PROFILES)
        except AssertionError:
            refused += 1
        else:
            raise AssertionError('corrupt actual observation accepted: ' + name)
    row = json.dumps(selected[0])
    duplicate = row[:-1] + ',"health":0}'
    try:
        rows('SLIME_REMOVAL ' + duplicate)
    except AssertionError:
        refused += 1
    else:
        raise AssertionError('duplicate JSON key accepted')
    print(f'PASS: 11 captured Slime direct profiles; {refused} corrupt typed observations refused; no native process')


if __name__ == '__main__':
    main()
