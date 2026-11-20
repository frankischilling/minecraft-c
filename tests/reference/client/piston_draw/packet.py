"""Strict offline admission of the finite original direct-draw projection."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
# Roots for the independently audited finite publication. Repairing a manifest
# cannot change the original observation or the exact executed Java helpers.
PINNED = {
    'authored-worlds.json': '55dfbe570968fd26e8f54b324a59e0bd4721005510d3b4f237b900ff3e1e856b',
    'cases.json': '377f0ef8a761992f782cda98b957cf0f445a76b152e5fe7cc47568964f8dcc6d',
    'ConstructorJson.java': '9ed002149ed9782c5deb4b833be9ed97ad2319ed6291af526b02baf58f62ceb3',
    'geometry.json': '74379eb57db124b7473fbf01b4e270234214c6e7600a88e0fe96464b5e99670a',
    'native-observations.json': '547985bc35a676268a0084ac75d6b37f2c49707dcac5cecb79108a0572889ac9',
    'observations.json': 'dbbc3a2b94942ed38a345d88c9826f8717e48f3f988e7777cfdb2ffb89364fd0',
    'PistonDrawJdi.java': '945ccfaa48985d1ddacda1f32f056e779e2578f16d30ff64a0089ff864ef527a',
    'PistonDrawObservation.java': '70db3ea83e4ce1c7b0b5ee5218368bd7cb7b02be30c22f7d58bd48159a32600d',
    'profiles.tsv': '66ce291d90343fc1a7904f8fa4592d35c52ab9c74c28f40d1158402300f517a2',
    'provenance.json': 'd0dd7635ce542ea6425ea11cdce46cbe9092fad65504a799e6eb812ff4470e1c',
    'source-pins.json': 'eff43c09c95c685bc2531ac5316aacb43bb5591a078a07b74203edb315d51e06',
    'write_world.py': '15adc51f91c27f2a0fe1e04fbcd46655d65d1397b79ba565dfee4978866bd834',
}
FILES = set(PINNED) | {'packet.py', 'test_packet.py', 'README.md'}
MODES = ['ordinary-interpreter', 'ordinary-default', 'jdi-interpreter', 'jdi-default']
EXCLUDED = ['ordinary World tick', 'visible-window proof', 'pixels', 'whole renderer/game parity',
            'native parity', 'original save', 'viewer/World entropy equality',
            'raw debugger object-ID equality', 'stale retained NW suffix interpreted as active geometry',
            'archive hash treated as live Code']
SUMMARY = {'processes': 4, 'rows_per_process': 27, 'live_methods_per_jdi': 25,
           'journal_records_per_jdi': 1963, 'source_calls_per_jdi': 1914,
           'batches_per_jdi': 24, 'classes': 35, 'declared_methods': 853,
           'Code_methods': 837, 'source_files': 81, 'owned_locks_per_process': 27,
           'compiled_products': 4}


def require(value, message):
    if not value:
        raise ValueError(message)


def unique(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, 'duplicate JSON key: ' + key)
        out[key] = value
    return out


def read(path):
    return json.loads(Path(path).read_text(encoding='utf8'), object_pairs_hook=unique)


def pin(path):
    raw = Path(path).read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def _retained(after):
    out = {k: after[k] for k in ('tile', 'world_time', 'block', 'world_metadata', 'cv',
                               'carried_bounds', 'head_bounds', 'moving_bounds',
                               'head_texture', 'nw', 'gl')}
    out['gl'] = {k: v for k, v in out['gl'].items() if k != 'bound_texture'}
    return out


def _scope(provenance):
    scope = provenance['scope']
    require(scope == {
        'boundary': 'direct HY.a(UK,DDDF)V after declared direct UK.n_()V preparation',
        'processes': MODES,
        'geometry': 'actual active NW buffer observed read-only at NW.a() entry, HY callerPC357',
        'environment': 'hidden32x32Pbuffer, genuineLWJGL2/originalterrain resource',
        'excluded': EXCLUDED}, 'exact finite direct-call scope')
    require(provenance['independent_physical_audit'] == SUMMARY,
            'exact finite capture/physical audit counts')
    require(provenance['ordinary_and_jdi_finite_equality'] is True,
            'declared ordinary/passive finite projection')
    require(provenance['runtime']['target_options'] == [
        '-Xint only interpreter modes', '-Xverify:all', '-Xmx256m', '-XX:-UsePerfData',
        '-Djava.awt.headless=true', '-Djava.security.manager=allow',
        '-Dorg.lwjgl.librarypath=<owned-pinned-natives>',
        '-Djava.io.tmpdir=<owned-imageio-temp>', '-Duser.home=<owned-home>'],
        'exact executed target options, role projected')
    require(provenance['runtime']['cleared_environment'] ==
            ['JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'],
            'historical VM option environment cleared')
    require(provenance['publication_excludes'] == [
        'game JAR/classes/Code bytes', 'raw assets/textures', 'saved World/NBT/region bytes',
        'private absolute paths', 'raw private stdout/stderr/journals/receipts',
        'unobserved gameplay or pixel results'], 'exact public material exclusions')
    require(list(provenance['streams']) == MODES, 'all four original processes')
    for mode, stream in provenance['streams'].items():
        require(stream['exit'] == 0 and type(stream['exit']) is int and
                stream['timed_out'] is False and stream['pipe_capture_complete'] is True and
                stream['reader_errors'] == {} and stream['output_overflow'] is False,
                'complete unsuppressed outer transport: ' + mode)
        require(stream['stdout_limit'] == 4194304 and stream['stderr_limit'] == 1048576,
                'declared finite pipe limits')
        if mode.startswith('jdi'):
            require(stream['target_status'] == {
                'target_exit': 0, 'timed_out': False, 'target_complete': True,
                'pipe_capture_complete': True, 'reader_errors': [], 'events': 1914,
                'draws': 24, 'entries': 27, 'returns': 27, 'failure': None},
                'complete independently captured passive child')


def _content(root):
    declaration = read(root / 'cases.json')
    require(declaration['schema'] == 'ORIGINAL_PISTON_DIRECT_DRAW_INPUTS_V1' and
            declaration['processes'] == MODES and declaration['chunks'] ==
            [[0, 0], [0, 1], [1, 0], [1, 1]] and declaration['tile_xyz'] == [8, 64, 8] and
            declaration['draw_xyz_bits'] == ['3fd8000000000000', '3fe4000000000000',
                                             'bfc0000000000000'] and
            declaration['seed'] == 123 and declaration['time'] == 8 and
            declaration['moving_block'] == 36, 'exact predeclared source setup')
    cases = declaration['cases']
    require(len(cases) == 27 and [c['index'] for c in cases] == list(range(27)),
            'all 27 ordered authored inputs')
    profiles = ''.join('%d\t%s\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t123\n' % (
        c['index'], c['folder'], c['block'], c['metadata'], c['facing'], c['ticks'],
        c['partial_bits'], str(c['extending']).lower(), str(c['render_head']).lower(),
        str(c['ambient_occlusion']).lower(), 'Original Piston draw %02d' % c['index'])
        for c in cases)
    require((root / 'profiles.tsv').read_bytes() == profiles.replace('\n', '\r\n').encode('utf8'),
            'exact authored runtime TSV arguments')
    provenance = read(root / 'provenance.json')
    require(provenance['schema'] == 'ORIGINAL_PISTON_DRAW_PROVENANCE_V1', 'provenance schema')
    _scope(provenance)
    source = read(root / 'source-pins.json')
    require(source['schema'] == 'ORIGINAL_PISTON_CLASS_CODE_PINS_V1' and
            source['client_jar_sha256'] == JAR and len(source['classes']) == 35 and
            sum(len(c['methods']) for c in source['classes'].values()) == 853 and
            sum('code_bytes' in m for c in source['classes'].values()
                for m in c['methods']) == 837, 'actual archive Class/Code census')
    ordinary = read(root / 'observations.json')
    require(ordinary['schema'] == 'ORIGINAL_PISTON_DRAW_OBSERVATIONS_V1', 'observation schema')
    observation = ordinary['observation']
    require(observation['origin'] == {
        'jar_sha256': JAR, 'classes': {n.replace('/', '.'): p['class_sha256']
                                     for n, p in source['classes'].items()},
        'terrain_sha256': '9e32336c83732193a20d0f942ae8aa1186ae01231245b5e83659ce6e9d3511cf',
        'terrain_jar': True}, 'actual loaded original Class/resource origins')
    rows = observation['rows']
    require(len(rows) == 27 and [r['index'] for r in rows] == list(range(27)) and
            all(r['returned'] is True and r['failure'] is None for r in rows),
            '27 actual direct source returns')
    for case, row in zip(cases, rows):
        require(row['after']['tile'] == row['before']['tile'] and
                row['after']['tile']['block'] == case['block'] and
                row['after']['tile']['facing'] == case['facing'] and
                row['after']['tile']['render_head'] is case['render_head'] and
                row['after']['world_time'] == 8 and row['after']['block'] == 36 and
                row['after']['world_metadata'] == case['metadata'] and
                row['after']['cv_world_same'] is True and row['after']['gl']['error'] == 0,
                'actual owner/direct preparation/GL state')
    geometry = read(root / 'geometry.json')
    require(geometry['schema'] == 'ORIGINAL_PISTON_PASSIVE_GEOMETRY_V1' and
            geometry['identity_projection'] == provenance['jdi_identity_projection'],
            'explicit passive identity projection')
    events = geometry['events']
    require(len(events) == 1963 and sum(e['event'] in ('entry', 'exit') for e in events) == 1914,
            'complete selected passive call journal')
    live = [e for e in events if e['event'] == 'live-code']
    require(len(live) == len({(e['class'], e['method'], e['descriptor']) for e in live}) == 25,
            '25 unique actual live method Code observations')
    for entry in live:
        methods = source['classes'][entry['class'].replace('.', '/')]['methods']
        method = next(m for m in methods if m['name'] == entry['method'] and
                      m['descriptor'] == entry['descriptor'])
        require(entry['bytes'] == method['code_bytes'] and
                entry['sha256'] == method['code_sha256'], 'actual selected live Code identity')
    derived = []
    for i in range(27):
        group = [e for e in events if e.get('profile') == i]
        require(group and group[0]['event'] == 'entry' and group[0]['site'] ==
                {'class': 'hy', 'method': 'a', 'descriptor': '(Luk;DDDF)V', 'pc': 0} and
                group[-1] == {'event': 'exit', 'profile': i, 'class': 'hy', 'method': 'a',
                              'descriptor': '(Luk;DDDF)V', 'return': {'void': True}},
                'complete original HY source boundary')
        stack = []
        for e in group:
            if e['event'] == 'entry':
                stack.append((e['site']['class'], e['site']['method'], e['site']['descriptor']))
            elif e['event'] == 'exit':
                require(stack and stack.pop() == (e['class'], e['method'], e['descriptor']),
                        'actual passive source-call nesting')
        require(not stack, 'all selected source calls returned')
        batches = [e for e in group if e['event'] == 'nw-draw']
        require(len(batches) == (0 if i in (19, 20, 23) else 1),
                'observed actual batch/cull cardinality')
        if batches:
            b = batches[0]
            require(b['source_frames'][:2] == [
                {'class': 'nw', 'method': 'a', 'descriptor': '()V', 'pc': 0},
                {'class': 'hy', 'method': 'a', 'descriptor': '(Luk;DDDF)V', 'pc': 357}],
                'actual NW entry and HY357 caller')
            require(type(b['state']['o']) is int and type(b['state']['h']) is int and
                    b['state']['o'] == 8 * b['state']['h'] == len(b['words']) and
                    all(type(w) is str and re.fullmatch('[0-9a-f]{8}', w) for w in b['words']),
                    'actual active vertex count and complete raw words')
            require(rows[i]['after']['retained_word_window'][:len(b['words'])] == b['words'],
                    'ordinary retained prefix equals actual passive active words')
            vertices, words = b['state']['h'], b['words']
        else:
            vertices, words = 0, []
        derived.append({'index': i, 'vertices': vertices, 'word_count': len(words),
                        'words': words, 'retained_state': _retained(rows[i]['after'])})
    native = read(root / 'native-observations.json')
    require(native['schema'] == 'ORIGINAL_PISTON_ACTIVE_NW_PROJECTION_V1' and
            native['rows'] == derived, 'native comparison data derives exactly from original reads')
    authored = read(root / 'authored-worlds.json')
    require(authored['schema'] == 'PISTON_AUTHORED_WORLD_BYTES_V1' and
            len(authored['files']) == 81 and len(authored['directories']) == 81 and
            len(authored['payloads']) == 135,
            'complete authored finite input census')
    return {'declaration': declaration, 'cases': cases, 'expected': derived}


def read_packet(root=HERE):
    """Admit the public packet; return inputs and separate comparison-only data."""
    root = Path(root)
    names = {p.name for p in root.iterdir() if p.is_file()}
    require(names == FILES | {'publication.json'} and
            not any(p.is_dir() for p in root.iterdir()), 'exact public file census')
    manifest = read(root / 'publication.json')
    require(manifest['schema'] == 'PISTON_DRAW_PUBLICATION_V1' and
            manifest['kind'] == 'postexecution-finite-projection' and
            set(manifest['files']) == FILES, 'exact publication manifest scope')
    require(manifest['files'] == {n: pin(root / n) for n in sorted(FILES)},
            'current complete public file hashes/lengths')
    result = _content(root)
    for name, digest in PINNED.items():
        require(pin(root / name)['sha256'] == digest,
                'independently audited immutable original projection: ' + name)
    return result


if __name__ == '__main__':
    result = read_packet()
    print('PASS original Piston packet: 4 processes, 27 cases, 24 active batches, 25 live Code pins')
