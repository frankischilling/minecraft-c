"""Copy actual World lifecycle snapshots and phase records into native data."""
import json
import re


def tokens(source):
    return [v for v in re.findall(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*|-?\d+|[^\s]', source)
            if not v.startswith(('/*', '//'))]


def string(value):
    return 'NULL' if value is None else json.dumps(value)


def vector(values):
    return '{' + ', '.join('INT32_MIN' if value == -2147483648 else str(value) for value in values) + '}'


def labels(values):
    return '{' + (', '.join(map(string, values)) or 'NULL') + '}'


def state(value):
    maps = [label for chunk in value['maps'] for label in chunk['values']]
    out = ['{', f"    {value['block']}, {value['metadata']}, " +
           str(value['iterating']).lower() + ', ' + str(value['suppression']).lower() + ',',
           f"    {len(value['active'])}, {len(value['pending'])}, {len(maps)}, " +
           f"{len(value['tiles'])}, {len(value['stacks'])},",
           '    ' + labels(value['active']) + ', ' + labels(value['pending']) + ', ' + labels(maps) + ',',
           '    {']
    for name, tile in value['tiles'].items():
        out.append('        {' + string(name) + ', ' + vector(tile['fields']) + ', ' +
                   str(tile['invalid']).lower() + ', ' + str(tile['world_same']).lower() + ', ' +
                   vector(tile['xyz']) + ', ' + labels(tile['slots']) + '},')
    out += ['    },', '    {']
    for name, item in value['stacks'].items():
        out.append('        {' + string(name) + ', ' + ', '.join(map(str, item)) + '},')
    if not value['stacks']:
        out.append('        {0}')
    out += ['    }', '}']
    return '\n'.join(out)


def header(declaration, rows, setup):
    out = ['/* Generated from actual unchanged-client World.g lifecycle observations.',
           ' * Native replay compares this finite post-step projection only; original',
           ' * phase records remain separate, with no implicit native trace parity. */',
           '#ifndef MC_REFERENCE_FURNACE_LIFECYCLE_NATIVE_OBSERVATIONS_H',
           '#define MC_REFERENCE_FURNACE_LIFECYCLE_NATIVE_OBSERVATIONS_H',
           '#include <stdbool.h>', '#include <stddef.h>', '#include <stdint.h>', '',
           'typedef struct mc_furnace_lifecycle_tile_reference {',
           '    const char *identity;', '    int32_t fields[3]; /* burn,duration,cook */',
           '    bool invalid, world_same;', '    int32_t xyz[3];', '    const char *slots[3];',
           '} mc_furnace_lifecycle_tile_reference;', '',
           'typedef struct mc_furnace_lifecycle_stack_reference {',
           '    const char *identity;', '    int32_t id, count, damage, animation;',
           '} mc_furnace_lifecycle_stack_reference;', '',
           'typedef struct mc_furnace_lifecycle_state_reference {',
           '    int32_t block, metadata;', '    bool iterating, suppression;',
           '    size_t active_count, pending_count, map_count, tile_count, stack_count;',
           '    const char *active[8], *pending[8], *map[8];',
           '    mc_furnace_lifecycle_tile_reference tiles[8];',
           '    mc_furnace_lifecycle_stack_reference stacks[8];',
           '} mc_furnace_lifecycle_state_reference;', '',
           'typedef struct mc_furnace_lifecycle_reference {',
           '    const char *name, *operation;', '    bool author_cook, author_burn, author_fuel_null;',
           '    int32_t cook, burn;', '    bool returned;', '    const char *exception;',
           '    mc_furnace_lifecycle_state_reference before, after;',
           '} mc_furnace_lifecycle_reference;', '',
           'static const mc_furnace_lifecycle_reference mc_furnace_lifecycle_references[] = {']
    for step, row in zip(declaration['steps'], rows):
        if step['name'] != row['name']:
            raise ValueError('literal declared World step alignment')
        author = step['author']
        out += ['    {', '        ' + string(step['name']) + ', ' + string(step['operation']) + ',',
                '        ' + ', '.join(str(v).lower() for v in ('cook' in author, 'burn' in author, author.get('fuel_null', False))) + ',',
                f"        {author.get('cook', 0)}, {author.get('burn', 0)}, " + str(row['returned']).lower() + ', ' + string(row['exception']) + ',',
                state(row['before']) + ',', state(row['after']), '    },']
    out += ['};', '', '/* Actual prepared input metadata; see setup.json for every recorded field. */',
            'static const int32_t mc_furnace_lifecycle_chunk_open_order[4][2] = {{0,0},{0,1},{1,0},{1,1}};',
            'static const int64_t mc_furnace_lifecycle_seed = INT64_C(23);',
            'static const int64_t mc_furnace_lifecycle_time = INT64_C(' + str(setup['info_fields']['e']['value']) + ');',
            'static const bool mc_furnace_lifecycle_no_sky = ' + str(setup['provider_fields']['e']['value']).lower() + ';',
            '', '#endif', '']
    return '\n'.join(out)
