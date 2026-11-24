"""Independent integer/Boolean audit of the declared finite held attack scene."""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent


def require(value, text):
    if not value:
        raise AssertionError(text)


def i32(value):
    return ((value + (1 << 31)) % (1 << 32)) - (1 << 31)


def audit(rows, inventory, cases=None):
    declared = json.loads((HERE / 'cases.json').read_text()) if cases is None else cases
    cases = declared['cases']
    require(len(rows) == len(cases) == 422 and
            [r['profile'] for r in rows] == [r['profile'] for r in cases], 'all422 declared original calls')
    lookup = {r['id']: r for r in inventory}
    require(len(lookup) == len(inventory) == 202 and
            len({r['class'] for r in inventory}) == 38, 'actual202 Item/38-class namespace')
    base_ids = {r['id'] for r in inventory if r['class'] == 'gm'}
    require({r['id-before'] for r in rows if r['profile'].startswith('base-id-')} == base_ids and
            len(base_ids) == 22, 'every direct base Item selected')
    require({r['item-class'] for r in rows} == {r['class'] for r in inventory},
            'all38 actual runtime Item classes represented')
    durability, breaks, saddle = 0, 0, 0
    for row, case in zip(rows, cases):
        item = lookup[case['id']]
        require(all(row[k] == case[v] for k, v in [('profile', 'profile'), ('mode', 'mode'),
            ('id-before', 'id'), ('count-before', 'count'), ('damage-before', 'damage'),
            ('target-kind', 'target-kind')]), 'exact declared pre-invocation operands')
        require(all(row[k] == item[v] for k, v in [('item-class', 'class'),
            ('attack-owner', 'attack-owner'), ('hit-owner', 'hit-owner'), ('use-owner', 'use-owner'),
            ('max-damage', 'max-damage')]), 'actual selected Item and virtual method owners')
        owner = item['hit-owner']
        max_damage = item['max-damage']
        material = {59: 0, 131: 1, 250: 2, 1561: 3, 32: 0}
        if owner == 'dq':
            power = {'wc': 1, 'au': 2, 'ta': 3}[item['class']] + material[max_damage]
        elif owner == 'qd':
            power = 4 + 2 * material[max_damage]
        else:
            require(owner in ('gm', 'sa'), 'only actual GM/DQ/QD/SA hit bodies')
            power = 1
        require(row['attack-power'] == power, 'independent BU material and DQ/QD damage formula')
        damage = power + int(case['falling'])
        resistant = case['target-hurt'] > 10
        accepted = not resistant or damage > case['target-last-damage']
        actual_damage = max(0, damage - case['target-last-damage']) if resistant else damage
        health = case['target-health'] - actual_damage
        fatal = health <= 0 and accepted
        expected = {'id-after': case['id'], 'animation-before': 5, 'animation-after': 5,
            'inventory-changed': False, 'health': health,
            'previous-health': 10 if resistant else case['target-health'], 'hurt': 20,
            'hurt-time': 0 if resistant else 10, 'max-hurt': 0 if resistant else 10,
            'last-damage': damage if accepted else case['target-last-damage'], 'idle': 0,
            'dying': fatal, 'score': 3 + (case['target-reward'] if fatal else 0),
            'mob-kills': int(fatal), 'damage-dealt': damage, 'drops': int(fatal),
            'used-before': 11, 'used-after': 11 + int(owner != 'gm'),
            'sound-events': 0 if resistant else 1, 'particle-events': 0, 'break-sounds': 0}
        count, stored_damage = case['count'], case['damage']
        broken = False
        if owner in ('dq', 'qd'):
            durability += 1
            stored_damage = i32(stored_damage + (2 if owner == 'dq' else 1))
            if stored_damage > max_damage:
                broken = True
                breaks += 1
                count = max(0, i32(count - 1))
                stored_damage = 0
        if owner == 'sa':
            saddle += 1
            if case['target-kind'] == 'Pig' and not case['pig-saddled']:
                count = i32(count - 1)
        expected.update({'count-after': count, 'damage-after': stored_damage,
            'selected-same': count > 0, 'selected-cleared': count <= 0,
            'broken-before': 17 if max_damage > 0 else -1,
            'broken-after': 17 + int(broken) if max_damage > 0 else -1,
            'saddled-before': int(case['pig-saddled']) if case['target-kind'] == 'Pig' else -1,
            'saddled-after': 1 if case['target-kind'] == 'Pig' else -1})
        require(all(type(row[key]) is type(value) and row[key] == value
                    for key, value in expected.items()), 'independent source integer/Boolean formula: ' + row['profile'])
    research = json.loads((HERE / 'research-rows.json').read_text())['rows']
    require([{k: row[k] for k in original} for row, original in zip(rows, research)] == research,
            'exact32 retained research projection')
    return {'rows': len(rows), 'inventory_ids': len(inventory), 'runtime_classes': 38,
            'direct_base_ids': 22, 'durability_rows': durability, 'break_rows': breaks,
            'saddle_rows': saddle, 'research_rows_preserved': 32,
            'random_rebinding': False, 'deterministic_fields_only': True}
