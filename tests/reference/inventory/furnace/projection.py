"""Copy declared Furnace inputs and observed results into portable native data."""
import json
import re


def tokens(source):
    return [value for value in re.findall(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*|-?\d+|[^\s]', source)
            if not value.startswith(('/*', '//'))]


def number(value):
    return 'INT32_MIN' if value == -2147483648 else str(value)


def string(value):
    return 'NULL' if value is None else json.dumps(value)


def vector(values):
    return '{' + ', '.join(map(number, values)) + '}'


def cell(label, state):
    if label is None:
        return '{0}'
    value = state['stacks'][label]
    return '{' + string(label) + ', ' + ', '.join(number(value[key]) for key in
        ('id', 'count', 'damage', 'animation')) + '}'


def cells(values, state):
    entries = [f'[{index}] = {cell(label, state)}' for index, label in enumerate(values or []) if label is not None]
    return '{' + ', '.join(entries) + '}' if entries else '{{0}}'


def header(cases, rows):
    out = ['/* Generated from declared arguments and unchanged original SK/BM observations.',
           ' * All 207 outcomes remain visible, including explicit null-owner failure prefixes.',
           ' * Stack identity labels are per case; NULL means no Stack. NBT input integers',
           ' * are declared operands narrowed by the helper to Byte/Short as documented. */',
           '#ifndef MC_REFERENCE_FURNACE_NATIVE_OBSERVATIONS_H',
           '#define MC_REFERENCE_FURNACE_NATIVE_OBSERVATIONS_H',
           '#include <stdbool.h>', '#include <stddef.h>', '#include <stdint.h>', '',
           'typedef struct mc_furnace_stack_reference {',
           '    const char *identity;', '    int32_t id, count, damage, animation;',
           '} mc_furnace_stack_reference;', '',
           'typedef struct mc_furnace_reference {',
           '    const char *profile, *operation;', '    int32_t index, amount;',
           '    bool array_null, player_null, tile_null;', '    const char *argument;',
           '    int32_t fields[3], xyz[3]; /* burn, initial fuel duration, cook */',
           '    size_t input_stack_count, tracked_stack_count;',
           '    mc_furnace_stack_reference input_stacks[8], tracked_stacks[8];',
           '    mc_furnace_stack_reference before_tile[3], after_tile[3];',
           '    mc_furnace_stack_reference before_main[36], after_main[36];',
           '    int32_t after_fields[3], after_xyz[3];',
           '    bool returned, tile_same, array_same, player_changed;',
           '    int32_t result_kind, result_int; /* 0=null/void,1=int,2=bool,3=text/identity */',
           '    const char *result_text, *exception;',
           '    size_t layout_count;',
           '    int32_t layout[39][6]; /* container index, owner1=SK/2=IX,index,x,y,1=GP/2=VQ */',
           '    bool has_bm_caches;', '    int32_t bm_caches[3]; /* cached cook,burn,duration */',
           '    int32_t nbt_burn, nbt_cook, nbt_xyz[3];',
           '    const char *nbt_mode;', '    size_t nbt_item_count;', '    int32_t nbt_items[8][4];',
           '} mc_furnace_reference;', '',
           'static const mc_furnace_reference mc_furnace_references[] = {']
    for case, row in zip(cases, rows):
        if case['profile'] != row['profile']:
            raise ValueError('literal declared observation alignment')
        before, after = row['before'], row['after']
        if len(before['stacks']) > 8 or len(after['stacks']) > 8:
            raise ValueError('bounded observed Stack table')
        out += ['    {', '        ' + string(case['profile']) + ', ' + string(case['operation']) + ',',
                '        ' + number(case['index']) + ', ' + number(case['amount']) + ',',
                '        ' + ', '.join(str(case[key]).lower() for key in ('array_null', 'player_null', 'tile_null')) + ',',
                '        ' + string(case['argument']) + ', ' + vector(case['fields']) + ', ' + vector(case['xyz']) + ',',
                f"        {len(before['stacks'])}, {len(after['stacks'])},"]
        for state in (before, after):
            entries = [cell(label, state) for label in state['stacks']]
            out.append('        {' + (', '.join(entries) or '{0}') + '},')
        for state, key in ((before, 'slots'), (after, 'slots'), (before, 'player'), (after, 'player')):
            out.append('        ' + cells(state[key], state) + ',')
        out.append('        ' + vector(after['fields'] or [0, 0, 0]) + ', ' + vector(after['xyz'] or [0, 0, 0]) + ',')
        out.append('        ' + ', '.join(str(value).lower() for value in (row['returned'], row['tile_same'], row['array_same'], after['player_changed'] is True)) + ',')
        result = row['result']
        kind = 0 if result is None else 2 if type(result) is bool else 1 if type(result) is int else 3
        out.append(f'        {kind}, ' + number(int(result) if kind in (1, 2) else 0) + ', ' +
                   string(result if kind == 3 else None) + ', ' + string(row['exception']) + ',')
        layout = row['layout'] or []
        out.append(f'        {len(layout)}, {{')
        for index, owner, inventory_index, x, y, name in layout:
            if owner not in ('SK', 'IX') or name not in ('gp', 'vq'):
                raise ValueError('closed original Slot graph')
            out.append('            ' + vector([index, 1 if owner == 'SK' else 2, inventory_index, x, y, 2 if name == 'vq' else 1]) + ',')
        if not layout:
            out.append('            {0}')
        out.append('        },')
        out.append('        ' + str(row['bm_caches'] is not None).lower() + ', ' + vector(row['bm_caches'] or [0, 0, 0]) + ',')
        nbt = case['nbt']
        out.append('        ' + number(nbt['burn']) + ', ' + number(nbt['cook']) + ', ' + vector(nbt['xyz']) + ',')
        out.append('        ' + string(nbt['mode']) + ', ' + str(len(nbt['items'])) + ', {')
        for item in nbt['items']:
            out.append('            ' + vector(item) + ',')
        if not nbt['items']:
            out.append('            {0}')
        out += ['        }', '    },']
    out += ['};', '', 'static const mc_furnace_reference *const mc_furnace_bm_references[] = {']
    out += [f'    &mc_furnace_references[{index}],' for index, case in enumerate(cases) if case['operation'].startswith('bm-')]
    out += ['};', '', '#endif', '']
    return '\n'.join(out)
