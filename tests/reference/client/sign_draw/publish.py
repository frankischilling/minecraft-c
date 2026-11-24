"""Publish a portable, sanitized finite packet from qualified private captures."""
import sys
sys.dont_write_bytecode = True
import argparse
import copy
import json
from pathlib import Path
import shutil

import project as P

HERE = Path(__file__).resolve().parent
SCRIPTS = ('project.py', 'packet.py', 'publish.py', 'test_packet.py', 'verify_projection.py', 'README.md')

def healthy(value):
    P.need(type(value['exit']) is int and value['exit'] == 0 and value['timed_out'] is False
           and value['pipe_capture_complete'] is True and not value['reader_errors']
           and value['output_overflow'] is False, 'complete outer process transport')
    return {k: value[k] for k in ('exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete',
                                 'reader_errors', 'output_overflow', 'stdout_bytes', 'stderr_bytes',
                                 'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit')}

def guards(raw, expected_name):
    lines = raw.decode('utf8').splitlines()
    P.need(len(lines) == 4 and lines[0] == 'WARNING: A terminally deprecated method in java.lang.System has been called'
           and lines[1].startswith('WARNING: System::setSecurityManager has been called by ' + expected_name + ' (file:')
           and lines[2] == 'WARNING: Please consider reporting this to the maintainers of ' + expected_name
           and lines[3] == 'WARNING: System::setSecurityManager will be removed in a future release',
           'complete four-warning stderr, no discarded extra stderr')
    lines[1] = 'WARNING: System::setSecurityManager has been called by ' + expected_name + ' (<owned-helper-directory>)'
    return lines

def save_bracket(before, after):
    P.need(set(before) == set(after) and len(before) == 108, 'all declared owned saved files retained')
    locks = []
    for name in before:
        if before[name] != after[name]:
            P.need(name.replace('\\', '/').endswith('/session.lock')
                   and before[name]['bytes'] == after[name]['bytes'] == 8, 'only owned session locks change')
            locks.append(name.replace('\\', '/'))
    P.need(len(locks) == 36, 'all owned session locks observed')
    return {'files': 108, 'changed_eight_byte_session_locks': 36,
            'before_inventory_sha256': P.sha(json.dumps(before, sort_keys=True).encode()),
            'after_inventory_sha256': P.sha(json.dumps(after, sort_keys=True).encode())}

def class_pins(tools, live):
    source, library, jdk = (P.read(tools / n) for n in
                            ('source-pins-v1.json', 'lwjgl-pins-v2.json', 'jdk-string-pins-v1.json'))
    P.need(source['client_jar_sha256'] == P.JAR, 'original archive authority')
    classes = {**source['classes'], **library['classes']}
    methods = []
    for observed in live:
        original = [m for m in classes[observed['class']]['methods']
                    if m['name'] == observed['method'] and m['descriptor'] == observed['descriptor']]
        P.need(len(original) == 1 and observed['native'] is False
               and original[0]['code_sha256'] == observed['sha256']
               and original[0]['code_bytes'] == observed['bytes'], 'actual live Code matches selected static authority')
        methods.append(observed)
    return {'schema': 1, 'client_jar_sha256': P.JAR,
            'classes': {name: {k: c[k] for k in ('class_bytes', 'class_sha256')}
                        for name, c in classes.items()},
            'live_methods': methods, 'resources': source['resources'],
            'lwjgl_archive_sha256': library['lwjgl_sha256'],
            'jdk_modules': {k: jdk['lib_modules'][k] for k in ('bytes', 'sha256')},
            'jdk_string_classes': {name: {'class_sha256': c['class_sha256'],
                                         'fields': [f for f in c['fields'] if f['name'] in
                                                    ('value', 'coder', 'HI_BYTE_SHIFT', 'LO_BYTE_SHIFT')]}
                                   for name, c in jdk['classes'].items()},
            'static_metadata_raw_pins': {name: P.pin(tools / name) for name in
                                         ('source-pins-v1.json', 'lwjgl-pins-v2.json', 'jdk-string-pins-v1.json')}}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('ordinary', 'joined', 'tools', 'string-control', 'buffer-control', 'string-audit',
                 'buffer-audit', 'ordinary-audit', 'interpreter-audit', 'default-audit', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    ordinary, joined, tools = args.ordinary.resolve(), args.joined.resolve(), args.tools.resolve()
    a, b = P.read(ordinary / 'run.json'), P.read(joined / 'run.json')
    P.need(a['status'] == b['status'] == 'PASS' and a['inputs_before'] == a['inputs_after']
           and b['inputs_before'] == b['inputs_after'], 'immutable qualified capture input brackets')
    P.need(a['prepared_fixtures'] == b['prepared_fixtures'] and a['profiles'] == b['profiles'], 'same saved input schedule')
    for root, products in ((ordinary, a['products']), (joined, b['products'])):
        P.need({p.name: P.pin(p) for p in (root / 'classes').iterdir()} == products, 'physical authored product inventory')
    P.need(b['qualified_shared_helper']['pin'] == b['products']['JdiStringUnits.class']
           and b['products']['JdiStringUnits.class']['sha256'] == '152f31972b883dae14d7a9053c176b46287ce08fa93db9c5a8001bc769b296e7',
           'exact standalone-executed shared helper product used in composite')
    original_cases = P.read(ordinary / 'prepared/cases.json')
    P.need(P.pin(ordinary / 'prepared/profiles.tsv') == a['profiles']
           and (ordinary / 'prepared/profiles.tsv').read_bytes() == (joined / 'prepared/profiles.tsv').read_bytes(),
           'exact frozen profile bytes')
    cases = {'schema': 1, 'time': 8, 'seed': 123, 'tile_xyz': [8, 64, 8],
             'draw_xyz_bits': ['3fd8000000000000', '3fe4000000000000', 'bfc0000000000000'],
             'cases': [{k: c[k] for k in ('index', 'folder', 'kind', 'block', 'metadata', 'input_units',
                                         'loaded_units', 'edit_row', 'null_font', 'partial_bits', 'level_name', 'seed')}
                       for c in original_cases['cases']]}
    ordinary_values, streams = [], {}
    for i, mode in enumerate(('interpreter', 'default')):
        outer = a['processes'][i]
        healthy_outer = healthy(outer)
        for stream in ('stdout', 'stderr'):
            P.need(P.pin(ordinary / outer[stream + '_file']) ==
                   {'bytes': outer[stream + '_bytes'], 'sha256': outer[stream + '_sha256']}, 'physical ordinary pipe')
        value = P.ordinary((ordinary / outer['stdout_file']).read_bytes()); ordinary_values.append(value)
        streams['ordinary-' + mode] = {'transport': healthy_outer,
                                      'warnings': guards((ordinary / outer['stderr_file']).read_bytes(), 'SignDrawObservation'),
                                      'saves': save_bracket(outer['saves_before'], outer['saves_after'])}
    projections, evidence = [], []
    for i, mode in enumerate(('interpreter', 'default')):
        outer = b['processes'][i]
        controller = healthy(outer['controller'])
        for stream in ('stdout', 'stderr'):
            P.need(P.pin(joined / outer['controller'][stream + '_file']) ==
                   {'bytes': controller[stream + '_bytes'], 'sha256': controller[stream + '_sha256']}, 'physical controller pipe')
        directory = joined / (str(i) + '-' + mode + '-observer')
        projection, proof, value = P.journal_projection(directory)
        projections.append(projection); evidence.append(proof); ordinary_values.append(value)
        streams['jdi-' + mode] = {'controller': controller, 'target': proof,
                                 'warnings': guards((directory / 'target.stderr').read_bytes(), 'SignDrawObservation'),
                                 'saves': save_bracket(outer['saves_before'], outer['saves_after'])}
    P.need(projections[0] == projections[1], 'complete finite sanitized JDI projections agree across fresh modes')
    P.need(all(P.comparison_without_yaw(v) == P.comparison_without_yaw(ordinary_values[0]) for v in ordinary_values),
           'all4 Target projections differ only in explicit retained LL.h viewer yaw fields')
    modes = ('ordinary-interpreter', 'ordinary-default', 'jdi-interpreter', 'jdi-default')
    for mode, value in zip(modes, ordinary_values):
        yaw = []
        for row in value['rows']:
            before = row['before']['manager']['primitives']['h']; after = row['after']['manager']['primitives']['h']
            P.need(before == after, 'viewer yaw unchanged by selected Source call')
            yaw.append(before)
        streams[mode]['raw_viewer_yaw_bits'] = yaw
    audit_paths = {'ordinary': args.ordinary_audit, 'jdi-interpreter': args.interpreter_audit,
                   'jdi-default': args.default_audit, 'string_control': args.string_audit, 'buffer_control': args.buffer_audit}
    audits = {name: P.read(path) for name, path in audit_paths.items()}
    P.need(all(v['status'] == 'PASS' for v in audits.values()), 'all independent retained-byte audits passed')
    for index, mode in enumerate(('interpreter', 'default')):
        audit = audits['jdi-' + mode]
        P.need(audit['journal_sha256'] == evidence[index]['streams']['journal.ndjson']['sha256']
               and audit['actual_public_gl_call_lists_payloads'] == 140
               and audit['raw_string_backing_reads'] == 775, 'independent JDI audit joins exact raw journal')
    P.need(P.pin(ordinary / 'run.json')['sha256'] == audits['ordinary']['root_run_sha256'], 'independent ordinary audit joins capture')
    P.need(P.pin(args.string_control / 'run.json')['sha256'] == b['string_qualification_sha256']
           and P.pin(args.buffer_control / 'run.json')['sha256'] == b['bridge_qualification_sha256'], 'exact qualified reader control roots')
    provenance = {'schema': 1, 'scope': P.SCOPE, 'modes': list(modes), 'streams': streams,
                  'raw_capture_receipts': {'ordinary': P.pin(ordinary / 'run.json'), 'joined': P.pin(joined / 'run.json')},
                  'independent_audits': {name: P.pin(path) for name, path in audit_paths.items()},
                  'products': {'ordinary': a['products'], 'joined_executed': b['products'],
                               'joined_separate_build': b['build_products'],
                               'helper_lineage': 'Qualified standalone6859-byte helper copied unchanged; unused separate7112-byte build differs in debug attributes.'},
                  'control_qualification': {
                      'strings': {'raw_receipt': P.pin(args.string_control / 'run.json'), 'counts': audits['string_control']['counts'],
                                  'helper_product': audits['string_control']['helper_product']},
                      'buffer': {'raw_receipt': P.pin(args.buffer_control / 'run.json'), 'counts': audits['buffer_control']['counts'],
                                 'header_scope': 'Actual loaded ImageBase admitted; other selected PE header bytes checked. No loaded-code-integrity claim.'}},
                  'source_input_brackets': {'ordinary_equal': True, 'joined_equal': True,
                                            'ordinary_count': len(a['inputs_before']), 'joined_count': len(b['inputs_before']),
                                            'ordinary_raw_inventory_sha256': P.sha(json.dumps(a['inputs_before'], sort_keys=True).encode()),
                                            'joined_raw_inventory_sha256': P.sha(json.dumps(b['inputs_before'], sort_keys=True).encode())},
                  'runtime': {'platform': 'Windows x64', 'jdk': 'Eclipse Adoptium Temurin17.0.17+10',
                              'target_options': ['-Xint only interpreter modes', '-Xverify:all', '-Xmx256m', '-XX:-UsePerfData',
                                                 '-Djava.awt.headless=true', '-Djava.security.manager=allow',
                                                 '-Dorg.lwjgl.librarypath=<owned-pinned-natives>', '-Djava.io.tmpdir=<owned-temp>',
                                                 '-Duser.home=<owned-home>'],
                              'option_environment': 'Recorded controllers reject nonempty JAVA_TOOL_OPTIONS, _JAVA_OPTIONS and JDK_JAVA_OPTIONS'},
                  'summary': {'processes': 4, 'cases_per_process': 36, 'po_entries_per_jdi': 36, 'po_returns_per_jdi': 35,
                              'font_payloads_per_jdi': 140, 'nw_batches_per_jdi': 268, 'nw_words_per_jdi': 12864,
                              'font_glyph_batches': 256, 'model_face_batches': 12, 'live_code_methods': 44,
                              'string_records_per_jdi': 635, 'string_reads_per_jdi': 775},
                  'projection_rules': {'canonical': 'ordinary interpreter stdout and JDI interpreter structured projection',
                                       'viewer_entropy': 'All four raw36-value LL.h lists retained; each before equals after. No blanket float normalization.',
                                       'debugger_references': 'Output-only class-specific encounter labels in selected projection; raw IDs remain hash-bound privately.',
                                       'trace_table': 'Repeated observed Source frame arrays stored once, with indexed references.',
                                       'private_omissions': ['absolute paths and commands', 'buffer addresses and PIDs', 'raw PE header bytes',
                                                             'original Class/Code/assets/World bytes', 'raw private journals and reader responses']}}
    selected = projections[0]
    source = class_pins(tools, selected['live_code'])
    generated = {'cases.json': cases, 'source-pins.json': source, 'draws.json': {'schema': 1, 'draws': selected['draws']},
                 'batches.json': {'schema': 1, 'batches': selected['batches']}, 'font.json': {'schema': 1, **selected['font']},
                 'traces.json': {'schema': 1, 'traces': selected['traces']}, 'provenance.json': provenance}
    for value in generated.values():
        P.private_free(value)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    for name in SCRIPTS:
        shutil.copyfile(HERE / name, output / name)
    for name, expected in P.HELPERS.items():
        P.need(P.pin(tools / name)['sha256'] == expected, 'exact executed authored Java source')
        shutil.copyfile(tools / name, output / name)
    for name, value in generated.items():
        P.write(output / name, value)
    (output / 'ordinary.stdout').write_bytes((ordinary / a['processes'][0]['stdout_file']).read_bytes())
    (output / '.gitattributes').write_bytes(b'*.stdout -text\n')
    roots = {name: P.pin(output / name) for name in sorted([
        *generated, 'ordinary.stdout', *P.HELPERS, '.gitattributes',
        *(name for name in SCRIPTS if name != 'packet.py')])}
    validator = (output / 'packet.py').read_text(encoding='utf8')
    begin, end = '# DATA_ROOTS_BEGIN', '# DATA_ROOTS_END'
    left, rest = validator.split(begin, 1); _, right = rest.split(end, 1)
    (output / 'packet.py').write_text(left + begin + '\nROOTS = ' + repr(roots) + '\n' + end + right, encoding='utf8', newline='\n')
    P.write(output / 'publication.json', {'schema': 1, 'files': {p.name: P.pin(p) for p in sorted(output.iterdir()) if p.is_file()}})
    print('Published sanitized original Sign projection:', output.name)

if __name__ == '__main__':
    main()
