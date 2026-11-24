"""Project genuine original Chest predicate outcomes into portable C tables."""
import json
import re


def tokens(source):
    return [word for word in re.findall(
        r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*|-?\d+|[^\s]', source)
        if not word.startswith(('/*', '//'))]


def number(value):
    return 'INT32_MIN' if value == -2147483648 else str(value)


def header(cases, rows):
    observed = {row['name']: row for row in rows}
    out = ['/* Generated from predeclared inputs and genuine unchanged-original outcomes.',
           ' * Public E.a and private h observations are deliberately separate arrays. */',
           '#ifndef MC_REFERENCE_WORLD_CHEST_PLACEMENT_NATIVE_OBSERVATIONS_H',
           '#define MC_REFERENCE_WORLD_CHEST_PLACEMENT_NATIVE_OBSERVATIONS_H',
           '#include <stdbool.h>', '#include <stddef.h>', '#include <stdint.h>', '',
           'typedef struct mc_chest_placement_reference {',
           '    const char *profile;', '    int32_t operation, x, y, z, target_id;',
           '    bool null_world;', '    size_t chest_count;', '    int32_t chests[9][3];',
           '    bool returned, result;', '    const char *exception;',
           '} mc_chest_placement_reference;', '']
    for operation, name in ((0, 'mc_chest_placement_references'), (1, 'mc_chest_double_references')):
        out.append('static const mc_chest_placement_reference ' + name + '[] = {')
        for case in cases:
            if case['operation'] != operation:
                continue
            row = observed[case['name']]
            if len(case['chests']) > 9 or row['result'] not in ('true', 'false', 'UNRETURNED'):
                raise ValueError('bounded original projection')
            out.append('    {' + json.dumps(case['name']) + ', ' + ', '.join(number(case[key]) for key in
                       ('operation', 'x', 'y', 'z', 'target_id')) + ', ' +
                       str(case['null_world']).lower() + ', ' + str(len(case['chests'])) + ',')
            cells = ', '.join('{' + ', '.join(map(number, cell)) + '}' for cell in case['chests'])
            out.append('     {' + (cells or '{0}') + '}, ' + str(row['result'] != 'UNRETURNED').lower() + ', ' +
                       str(row['result'] == 'true').lower() + ', ' +
                       ('NULL' if row['exception'] == 'NONE' else json.dumps(row['exception'])) + '},')
        out += ['};', '']
    out += ['#endif', '']
    return '\n'.join(out)


def query_header(cases, traces):
    out = ['/* Generated only from actual JDI method entries/returns, not bytecode predictions. */',
           '#ifndef MC_REFERENCE_WORLD_CHEST_PLACEMENT_QUERY_OBSERVATIONS_H',
           '#define MC_REFERENCE_WORLD_CHEST_PLACEMENT_QUERY_OBSERVATIONS_H',
           '#include <stddef.h>', '#include <stdint.h>', '',
           'typedef struct mc_chest_query_reference {',
           '    int32_t x, y, z, value;', '    const char *method; /* Original E caller a or h. */',
           '    int32_t pc, parent_pc; /* Parent E.a invocation PC for h; zero for outer a. */',
           '} mc_chest_query_reference;', '',
           'typedef struct mc_chest_placement_trace_reference {',
           '    const char *profile;', '    size_t query_count, helper_count;',
           '    mc_chest_query_reference queries[24];',
           '} mc_chest_placement_trace_reference;', '',
           'static const mc_chest_placement_trace_reference mc_chest_placement_trace_references[] = {']
    for case, trace in zip(cases, traces):
        if case['operation'] != 0:
            continue
        if len(trace['queries']) > 24:
            raise ValueError('bounded actual query table')
        helpers = sum(entry['method'] == 'e.h(Lfd;III)Z' for entry in trace['entered_E'])
        out.append('    {' + json.dumps(case['name']) + ', ' + str(len(trace['queries'])) + ', ' + str(helpers) + ', {')
        for query in trace['queries']:
            frames = query['source_frames']
            method = 'h' if frames[0]['method'] == 'e.h(Lfd;III)Z' else 'a'
            parent_pc = frames[1]['pc'] if len(frames) == 2 else 0
            out.append('        {' + ', '.join(map(number, (*query['xyz'], query['result']))) + ', ' +
                       json.dumps(method) + ', ' + number(frames[0]['pc']) + ', ' + number(parent_pc) + '},')
        if not trace['queries']:
            out.append('        {0},')
        out.append('    }},')
    out += ['};', '', '#endif', '']
    return '\n'.join(out)
