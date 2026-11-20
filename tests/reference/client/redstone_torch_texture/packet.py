"""Asset-independent admission of the original DB/SM scalar texture packet."""
import argparse
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
MANIFEST_SHA256 = '1a4efab8fa56ccbe7d7375582bbea67ef67dcf117c4d8ae5c107b6c6e7e82a26'
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
AUTHORED = ('.gitattributes', 'TorchTextureObservation.java', 'capture_reference.py',
            'declared-inputs.json', 'input-freeze.json', 'profiles.tsv', 'source-pins.json')
METHODS = {('db', 'a', '(II)I'): (21, 'f952911f4e75864213cae04c5e47e926c71f726bff4652eae297a36db7d6cbd7'),
           ('sm', 'a', '(II)I'): (5, 'a646e90b4a6c63b21bd632d705c86d4c461fd01da9b823ec6470917531af1574'),
           ('uu', 'a', '(II)I'): (6, 'f20c3e09b28f823cc21d11124611ce6c00ee8c6ff2401080810b345ed3650fdf')}
HELPERS = {'TorchTextureObservation.class', 'TorchTextureObservation$1.class'}
SUPPORT = ('tests/reference/client/controller/capture.py', 'tests/reference/client/controller/classfile.py',
           'tests/reference/client/picking/observe.py', 'tests/tools/client/test_picking.py')
LIBRARIES = ('c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
             '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
             '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
             'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5')
RECEIPT_KEYS = {'command', 'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete', 'reader_errors',
                'output_overflow'} | {c + '_' + f for c in ('stdout', 'stderr')
                                     for f in ('file', 'bytes', 'sha256', 'limit')}


def need(value, message):
    if not value:
        raise ValueError(message)


def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def invalid_constant(value):
    raise ValueError('non-JSON constant: ' + value)


def loads(text):
    return json.loads(text, object_pairs_hook=unique, parse_constant=invalid_constant)


def load(path):
    return loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def same(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return a.keys() == b.keys() and all(same(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    return a == b


def integer(text):
    need(type(text) is str and re.fullmatch('0|-?[1-9][0-9]*', text), 'canonical signed scalar')
    value = int(text)
    need(-2**31 <= value < 2**31, 'Java signed scalar range')
    return value


def join(root, relative):
    need(type(root) is str and root and '\n' not in root and '\r' not in root and
         (re.match(r'^[A-Za-z]:\\', root) or root.startswith('/')), 'absolute captured path')
    separator = '\\' if re.match(r'^[A-Za-z]:\\', root) else '/'
    return root.rstrip('/\\') + separator + relative.replace('/', separator)


def frozen_files():
    return set(AUTHORED) | {'capture.json', 'freeze.json', 'provenance.json'} | {
        n + '.' + suffix for n in ('java', 'javac', 'compile')
        for suffix in ('stdout', 'stderr', 'capture.json')} | {
        'ordinary-' + str(i) + '/' + n for i in range(2)
        for n in ('original.stdout', 'original.stderr', 'original.capture.json', 'selected.tsv')}


def authored(directory=HERE):
    directory = Path(directory)
    need(sha(directory / 'input-freeze.json') == '930829e66aa17e3a59f8cdd05985e049d05b36bd09a49d215568974f6f83f098',
         'original pre-helper input freeze')
    freeze = load(directory / 'input-freeze.json')
    need(all(sha(directory / n) == h for n, h in freeze['files'].items()), 'unchanged pre-helper operands')
    need(sha(directory / 'TorchTextureObservation.java') == 'f5b29b8846db6f5a1b82b79cb08d41f62dc14c836762fd6d995c5952b549beb1',
         'unchanged corrected authored helper')
    attributes = (directory / '.gitattributes').read_bytes()
    need(b'*.capture.json -text whitespace=trailing-space,space-before-tab,cr-at-eol\n' in attributes,
         'binary capture-receipt rule present before execution')
    pins = load(directory / 'source-pins.json')
    need(pins['client_jar_sha256'] == JAR, 'unchanged original archive')
    for (owner, name, descriptor), expected in METHODS.items():
        methods = [m for m in pins['classes'][owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        need(len(methods) == 1 and (methods[0]['code_bytes'], methods[0]['code_sha256']) == expected,
             'actual DB/SM/UU Code authority')
    need(not any(m['name'] == 'a' and m['descriptor'] == '(II)I' for m in pins['classes']['vm']['methods']),
         'VM metadata method is inherited, not a guessed override')
    aw = [f for f in pins['fields']['uu'] if f['name'] == 'aw']
    need(len(aw) == 1 and aw[0]['access'] == 25 and aw[0]['descriptor'] == 'Luu;', 'actual final named field contract')
    raw = (directory / 'profiles.tsv').read_bytes()
    need(raw.endswith(b'\n') and b'\r' not in raw, 'canonical LF profiles')
    profiles = []
    for line in raw.decode('ascii').splitlines():
        fields = line.split('\t')
        need(len(fields) == 7 and re.fullmatch('[a-z0-9-]+', fields[0]), 'closed named input width')
        profiles.append({'name': fields[0], 'values': [integer(v) for v in fields[1:]]})
    need(len(profiles) == 160 and len({p['name'] for p in profiles}) == 160 and
         same(profiles, load(directory / 'declared-inputs.json')['rows']), 'complete ordered frozen roster')
    return profiles


def validate_rows(rows, profiles):
    need(len(rows) == len(profiles) == 160, 'complete scalar row roster')
    saved = {}
    for index, (row, profile) in enumerate(zip(rows, profiles)):
        need(type(row) is list and len(row) == 21 and all(type(v) is str for v in row) and
             row[:3] == ['DB_TEXTURE', str(index), profile['name']] and row[15:] == ['db', 'sm', 'sm', '55', '1', '1'],
             'actual metadata declaring classes, owners, restoration and row identity')
        values = [integer(v) for v in row[3:14]]
        need(same(values[:6], profile['values']) and row[14] == format(values[-1] & 0xffffffff, '08x'),
             'literal operands and returned signed/hex join')
        block, side, metadata, mutate, self_bm, dust_bm = values[:6]
        before_self, before_dust, entered_self, entered_dust, returned = values[6:]
        need(block in (75, 76) and mutate in (0, 1), 'finite genuine receiver/setup modes')
        saved.setdefault(block, before_self)
        saved.setdefault('dust', before_dust)
        need(before_self == saved[block] and before_dust == saved['dust'] and
             [entered_self, entered_dust] == ([self_bm, dust_bm] if mutate else [before_self, before_dust]),
             'declared live instance input/restoration conditioning')
        # This is the finite pinned scalar branch admission. Native comparisons
        # still consume the literal original result, not a reconstructed oracle.
        need(returned == (entered_dust if side == 1 else entered_self), 'source-reachable DB/SM scalar result')
    return rows


def allowed_stderr(capture):
    path = str(capture).replace(chr(92), '/').rstrip('/') + '/classes/'
    uri = 'file:' + ('/' if re.match('[A-Za-z]:/', path) else '') + path
    return ('WARNING: A terminally deprecated method in java.lang.System has been called\n'
            'WARNING: System::setSecurityManager has been called by TorchTextureObservation (' + uri + ')\n'
            'WARNING: Please consider reporting this to the maintainers of TorchTextureObservation\n'
            'WARNING: System::setSecurityManager will be removed in a future release\n').encode('ascii')


def validate_streams(payload, profiles, capture):
    need(payload['stderr'] == allowed_stderr(capture), 'exact SecurityManager-only stderr; Throwable/setup/daemon failures refused')
    raw = payload['stdout']
    need(raw.endswith(b'\r\n') and raw == raw.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'), 'exact original CRLF transport')
    lines = raw.decode('ascii').splitlines()
    need(lines[:2] == ['16 achievements', '151 recipes'] and len(lines) == 165, 'closed bootstrap and scalar line count')
    need(lines[-3:] == ['DB_GUARD\tsource\tnetwork=0\tnative=0\texecs=0\twrites=0',
         'DB_GUARD\tcontrols\tnetwork=2\tnative=1\texecs=1\twrites=1', 'DB_COMPLETE\t160'], 'exact source/denial guards and completion')
    need(payload['selected'] == ('\n'.join(lines[2:]) + '\n').encode('ascii'), 'complete raw/selected LF join')
    return validate_rows([line.split('\t') for line in lines[2:-3]], profiles)


def validate_receipt(receipt, payload, stem, timeout):
    need(type(receipt) is dict and set(receipt) == RECEIPT_KEYS and type(receipt['exit']) is int and receipt['exit'] == 0 and
         type(receipt['timeout_seconds']) is int and receipt['timeout_seconds'] == timeout and receipt['timed_out'] is False and
         receipt['pipe_capture_complete'] is True and receipt['reader_errors'] == {} and receipt['output_overflow'] is False and
         type(receipt['command']) is list and all(type(v) is str for v in receipt['command']), 'actual bounded complete process receipt')
    for channel, limit in (('stdout', 4194304), ('stderr', 1048576)):
        need(receipt[channel + '_file'] == stem + '.' + channel and type(receipt[channel + '_bytes']) is int and
             receipt[channel + '_bytes'] == len(payload[channel]) and receipt[channel + '_limit'] == limit and
             type(receipt[channel + '_limit']) is int and len(payload[channel]) < limit and
             receipt[channel + '_sha256'] == hashlib.sha256(payload[channel]).hexdigest(), 'actual raw pipe byte receipt')


def validate(directory=HERE):
    directory = Path(directory)
    need(re.fullmatch('[0-9a-f]{64}', MANIFEST_SHA256), 'packet is UNFROZEN; genuine public captures required')
    need(sha(directory / 'manifest.json') == MANIFEST_SHA256, 'sealed manifest authority')
    manifest = load(directory / 'manifest.json')
    need(set(manifest) == {'format', 'client_jar_sha256', 'files_sha256'} and type(manifest['format']) is int and
         manifest['format'] == 1 and manifest['client_jar_sha256'] == JAR and set(manifest['files_sha256']) == frozen_files(),
         'closed public packet roster')
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()}
    need(actual == frozen_files() | {'manifest.json', 'packet.py', 'test_packet.py', 'README.md'}, 'closed asset-free on-disk packet')
    need(all(re.fullmatch('[0-9a-f]{64}', h) and sha(directory / n) == h for n, h in manifest['files_sha256'].items()), 'unchanged sealed packet bytes')
    return validate_evidence(directory)


def validate_evidence(directory):
    """Semantic layer for corruption controls; ordinary callers use read_packet."""
    directory = Path(directory)
    profiles = authored(directory)
    report, freeze, provenance = [load(directory / n) for n in ('capture.json', 'freeze.json', 'provenance.json')]
    need(set(provenance) == {'format', 'capture_root', 'prepared_root', 'repository_root', 'paths',
         'authored_files_sha256', 'capture_sha256', 'freeze_sha256'} and type(provenance['format']) is int and provenance['format'] == 1 and
         provenance['capture_sha256'] == sha(directory / 'capture.json') and provenance['freeze_sha256'] == sha(directory / 'freeze.json'), 'actual capture/provenance joins')
    capture, prepared, repository = [provenance[k] for k in ('capture_root', 'prepared_root', 'repository_root')]
    need(join(repository, 'tests/reference/client/redstone_torch_texture') == prepared, 'actual public helper destination')
    paths = provenance['paths']
    need(set(paths) == {'java', 'javac', 'jvm', 'jmod', 'modules', 'libraries', 'java_dlls'} and
         all(type(paths[k]) is list and all(type(v) is str for v in paths[k]) and len(set(paths[k])) == len(paths[k])
             for k in ('libraries', 'java_dlls')) and len(paths['libraries']) == 4, 'closed actual dependency roles')
    jar = join(repository, 'b1.7.3.jar')
    expected_inputs = {jar, *paths['libraries'], *(paths[k] for k in ('java', 'javac', 'jvm', 'jmod', 'modules')),
                       *paths['java_dlls'], *(join(prepared, n) for n in AUTHORED), *(join(repository, n) for n in SUPPORT)}
    before = report['inputs_before']
    need(report['status'] == 'PASS' and report['inputs_unchanged'] is True and report['helpers_unchanged'] is True and
         same(before, report['inputs_after']) and same(before, freeze['inputs_before']) and set(before) == expected_inputs and
         all(re.fullmatch('[0-9a-f]{64}', h) for h in before.values()), 'complete actual execution closure and continuity')
    need(before[jar] == JAR and [before[n] for n in paths['libraries']] == list(LIBRARIES), 'actual original/library identities')
    need(set(provenance['authored_files_sha256']) == set(AUTHORED) and all(sha(directory / n) ==
         provenance['authored_files_sha256'][n] == before[join(prepared, n)] for n in AUTHORED), 'actual public authored capture inputs')
    helpers = report['helpers_before']
    need(set(helpers) == HELPERS and same(helpers, report['helpers_after']) and all(re.fullmatch('[0-9a-f]{64}', h) for h in helpers.values()),
         'unchanged two-class compiled authored helper closure')
    cp = (';' if re.match(r'^[A-Za-z]:\\', capture) else ':').join([join(capture, 'classes'), jar, *paths['libraries']])
    for stem, timeout in (('java', 10), ('javac', 10), ('compile', 30)):
        receipt = load(directory / (stem + '.capture.json'))
        need(same(receipt, report[stem]), 'tool report/receipt join')
        payload = {c: (directory / (stem + '.' + c)).read_bytes() for c in ('stdout', 'stderr')}
        validate_receipt(receipt, payload, stem, timeout)
        if stem == 'compile':
            need(receipt['command'] == [paths['javac'], '-encoding', 'UTF-8', '-implicit:none', '-cp', cp,
                 '-d', join(capture, 'classes'), join(prepared, 'TorchTextureObservation.java')] and payload == {'stdout': b'', 'stderr': b''}, 'exact sole helper compilation')
        else:
            need(receipt['command'] == [paths[stem], '-version'] and b'17.0.17' in payload['stdout'] + payload['stderr'], 'actual qualified tool version')
    need([r['repeat'] for r in report['runs']] == [0, 1] and all(type(r['repeat']) is int for r in report['runs']), 'two independent ordered VM outcomes')
    results, selected = [], []
    for run in report['runs']:
        folder = directory / ('ordinary-' + str(run['repeat']))
        receipt = load(folder / 'original.capture.json')
        need(same(receipt, run['capture']) and run['passed'] is True and type(run['rows']) is int and run['rows'] == 160, 'actual original run/report join')
        payload = {c: (folder / n).read_bytes() for c, n in (('stdout', 'original.stdout'), ('stderr', 'original.stderr'), ('selected', 'selected.tsv'))}
        validate_receipt(receipt, payload, 'original', 90)
        need(receipt['command'] == [paths['java'], '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
             '-Djava.security.manager=allow', '-cp', cp, 'TorchTextureObservation', jar, join(prepared, 'profiles.tsv')], 'ordinary unmodified helper command; no agent/repair')
        rows = validate_streams(payload, profiles, capture)
        need(run['selected_sha256'] == hashlib.sha256(payload['selected']).hexdigest(), 'actual selected receipt join')
        results.append(rows)
        selected.append(payload['selected'])
    need(selected[0] == selected[1], 'independent original VM scalar agreement')
    return {'status': 'PASS', 'rows': results[0], 'ordinary_vms': 2,
            'scope': '160 finite original scalar calls per VM; no native child trace, World/GUI, FPS or global entropy qualification'}


def read_packet(directory=HERE):
    """Return 160 literal TSV field lists only after complete sealed admission."""
    return validate(directory)['rows']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, nargs='?', default=HERE)
    parser.add_argument('--authored-only', action='store_true')
    args = parser.parse_args()
    if args.authored_only:
        print(json.dumps({'status': 'AUTHORED_ONLY_NOT_CAPTURED', 'rows': len(authored(args.directory))}))
    else:
        result = validate(args.directory)
        print(json.dumps({**result, 'rows': len(result['rows'])}, indent=2))
