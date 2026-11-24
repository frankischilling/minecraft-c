"""Admit actual loaded game/SDK/JDK origins from pinned authority, not filenames."""
import json
from pathlib import Path
import re
from urllib.parse import urlparse
from urllib.request import url2pathname
import zipfile
from protocol import fingerprint, need
from capture import load
from bootstrap_origin import admit_with_metadata

HERE = Path(__file__).resolve().parent

_LIBRARY_TAG = r'\[library(?: {3})?\]'
_UNIFIED_LOG_ROW = re.compile(r'^\[[0-9.]+s\]\[[^]]+\]\[(?:class,(?:load|init)|library(?: {3})?)\]')
_BOUNDARY_MARKER = re.compile(r'^LOOKUP_BOUNDARY (setup-before|setup-after|device-before|device-after|profile-before|profile-after|call-before .+|call-after .+|(?:static|setter)-(?:before|after) (?:hw|ia|jh))$')


def library_load_names(text):
    """Return raw logged names; a VM-only basename supplies no file origin."""
    return re.findall(_LIBRARY_TAG + r' Loaded library (.+?), handle ', text)


def source_diagnostics(stderr, profile):
    """Retain all non-log bytes and apply the same finite-profile diagnostic gate."""
    diagnostics = []
    for line in stderr.splitlines(keepends=True):
        decoded = line.decode('utf-8').rstrip('\r\n')
        if _BOUNDARY_MARKER.fullmatch(decoded) or _UNIFIED_LOG_ROW.match(decoded):
            continue
        diagnostics.append(line)
    need(not diagnostics or profile == 'frontiers', 'unexpected real source/JVM/native diagnostics retained as refusal')
    return b''.join(diagnostics)


def file_origin(value):
    parsed = urlparse(value)
    need(parsed.scheme == 'file' and parsed.netloc in ('', 'localhost'), 'actual file origin')
    return Path(url2pathname(parsed.path)).resolve(strict=True)


def authority(paths):
    pins = json.loads((HERE / 'authority-pins.json').read_text())
    result = {}
    for role, path in paths.items():
        need(fingerprint(path) == pins['files'][role], 'preexisting complete authority ' + role)
        result[role] = json.loads(path.read_text())
    need(set(result) == set(pins['files']), 'all source/SDK/JDK/runtime authorities')
    return result


def runtime_inputs(proof):
    runtime = proof['runtime']
    result = {}
    for name, row in {'client_jar': runtime['client_jar'], **runtime['libraries'], **runtime['JDK_backing']}.items():
        path = Path(row['path']).resolve(strict=True)
        need(fingerprint(path) == row['pin'], 'actual declared original runtime input ' + name)
        result[name] = {'path': str(path), 'pin': row['pin']}
    need('17.' in Path(runtime['JDK_home'], 'release').read_text(), 'actual JDK17 release')
    native = json.loads((HERE / 'jdk-native-backing.json').read_text())
    for name, expected in native['files'].items():
        path = Path(runtime['JDK_home']) / name
        need(fingerprint(path) == expected, 'actual current-host JDK native backing ' + name)
        result['native:' + name] = {'path':str(path.resolve()),'pin':expected}
    return runtime, result


def native_origin(native_dir, archive):
    policy = json.loads((HERE / 'native-origin-policy.json').read_text())
    # Byte identity alone cannot supply a missing authenticated archive origin.
    need(policy['admitted_origin'] is not None, 'unresolved retained official lwjgl native archive/entry provenance')
    entry = policy['admitted_origin']
    archive = archive.resolve(strict=True)
    need(fingerprint(archive) == entry['archive_pin'], 'actual admitted native archive')
    with zipfile.ZipFile(archive) as bundle:
        raw = bundle.read(entry['entry'])
    file = native_dir / entry['filename']
    need(file.read_bytes() == raw and fingerprint(file) == entry['file_pin'], 'actual native file equals exact authenticated archive entry')
    need({path.name for path in native_dir.iterdir() if path.is_file()} == {entry['filename']}, 'exclusive real native directory')
    return {'archive': str(archive), **entry, 'actual_native': str(file.resolve())}


def colorizer_activation(text, profile):
    if profile not in ('statics','first-setter'):
        return None
    result = {}
    for owner in ('hw','ia','jh'):
        role = 'static' if profile == 'statics' else 'setter'
        before = 'LOOKUP_BOUNDARY ' + role + '-before ' + owner
        after = 'LOOKUP_BOUNDARY ' + role + '-after ' + owner
        need(text.count(before) == 1 and text.count(after) == 1, 'one exact first-use interval ' + owner)
        low, high = text.index(before), text.index(after)
        need(low < high, 'ordered same-stderr first-use markers ' + owner)
        matches = list(re.finditer(r"\[class,init\][^\r\n]*Initializing '" + owner + r"'", text))
        need(len(matches) == 1, 'one actual class initialization event ' + owner)
        point = matches[0].start()
        need(point < high, 'class initialization precedes completion of actual use ' + owner)
        state = 'first_use_inside_observed_interval' if low < point < high else 'already_initialized_before_observed_use'
        result[owner] = {'state':state,'kind':role,'initialization_character_offset':point,
                         'before_character_offset':low,'after_character_offset':high}
    return {'classes':result,'all_observed_uses_are_first':all(row['state'] == 'first_use_inside_observed_interval' for row in result.values()),
            'order_basis':'Same captured raw stderr; explicit PrintStream flush, synchronous unified class logging, no -Xlog:async/JVM option injection'}


def inspect_logs(stderr, classes, jars, proof, reader, mode, profile):
    text = stderr.decode('utf-8', errors='strict')
    marker = _BOUNDARY_MARKER
    markers = [line for line in text.splitlines() if marker.fullmatch(line)]
    need(markers[:2] == ['LOOKUP_BOUNDARY setup-before', 'LOOKUP_BOUNDARY setup-after'], 'real actual-constructor/resource setup bounds')
    required = ['LOOKUP_BOUNDARY profile-before', 'LOOKUP_BOUNDARY profile-after']
    if mode == 'texture': required = ['LOOKUP_BOUNDARY device-before', 'LOOKUP_BOUNDARY device-after', *required]
    filtered = [line for line in markers[2:] if not line.startswith(('LOOKUP_BOUNDARY call-before ', 'LOOKUP_BOUNDARY call-after ', 'LOOKUP_BOUNDARY static-', 'LOOKUP_BOUNDARY setter-'))]
    need(filtered == required, 'actual independent CPU/device/profile boundaries')
    active = None
    for line in markers[2:]:
        if line.startswith('LOOKUP_BOUNDARY call-before '):
            need(active is None, 'no overlapping original observation call'); active = line.split(' ', 2)[2]
        elif line.startswith('LOOKUP_BOUNDARY call-after '):
            need(active == line.split(' ', 2)[2], 'actual complete original call marker'); active = None
    need(active is None, 'complete original call boundary')
    game = proof['game']['classes']; sdk = proof['sdk']['classes']; nests = proof['nests']['classes']
    allowed_helpers = {'ColorLookupObservation', 'LookupObservation', 'TextureLookupObservation'}
    game_jar = Path(proof['runtime']['client_jar']['path']).resolve()
    sdk_jars = {name: Path(row['path']).resolve() for name, row in proof['runtime']['libraries'].items()}
    loaded, bootstrap, lambdas = {}, {}, []
    for name, origin in re.findall(r'\[class,load\] ([^ ]+) source: ([^\r\n]+)', text):
        internal = name.replace('.','/')
        if name in allowed_helpers:
            need(file_origin(origin) == classes.resolve(), 'actual authored helper directory')
            loaded[name] = origin
        elif internal in game or name in sdk:
            archive = game_jar if internal in game else sdk_jars[sdk[name]['archive']]
            need(file_origin(origin) == archive, 'actual expected original archive ' + name)
            with zipfile.ZipFile(archive) as bundle: raw = bundle.read(internal + '.class')
            actual = reader.read_class(raw)
            compact = {key:value for key,value in actual.items() if key != 'methods'}
            compact['methods'] = [{key:value for key,value in method.items() if key != 'instructions'} for method in actual['methods']]
            wanted = game[internal] if internal in game else sdk[name]['complete_class_Code']
            need(compact == wanted, 'actual complete class/Code identity ' + name)
            loaded[name] = {'origin': origin, 'class_sha256': actual['class_sha256'], 'methods': compact['methods']}
        elif origin.startswith('file:'):
            raise ValueError('unlisted actual file class ' + name)
        elif origin.startswith(('jrt:/','shared objects file')):
            bootstrap[name] = origin
        elif origin.startswith(('__JVM_LookupDefineClass__','__ClassDefiner__')):
            # This is a VM origin, never proof of a game/SDK/helper definition.
            need(name not in allowed_helpers and internal not in game and name not in sdk, 'no VM-defined game substitution')
        else:
            forbidden = {key.replace('/','.') for key in game} | set(sdk) | allowed_helpers
            lambdas.append(admit_with_metadata(name,origin,bootstrap,forbidden,nests))
    primary = {'LookupObservation', 'kq', 'net.minecraft.client.Minecraft', 'kv', 'ik', 'ji', 'ge',
               'ColorLookupObservation' if mode == 'color' else 'TextureLookupObservation'}
    need(primary <= set(loaded), 'actual setup/observer families loaded')
    if mode == 'texture': need('TextureLookupObservation' in loaded and 'org.lwjgl.opengl.Display' in loaded, 'real LWJGL Display class origin')
    initializers = re.findall(r"\[class,init\].*Initializing '([^']+)'", text)
    if mode == 'color':
        need(not any(name.replace('/','.') in {'org.lwjgl.opengl.Display', 'org.lwjgl.opengl.GL11', 'org.lwjgl.opengl.GLContext'} for name in initializers), 'no original GL active use in CPU color')
    diagnostics = source_diagnostics(stderr, profile)
    native_loads = library_load_names(text)
    native_pins = json.loads((HERE / 'jdk-native-backing.json').read_text())['files']
    allowed_natives = {(Path(proof['runtime']['JDK_home']) / name).resolve():expected for name,expected in native_pins.items()}
    actual_native = []
    for name in native_loads:
        path = Path(name)
        if not path.is_absolute():
            # Built-in/VM library names do not supply a file-origin claim.
            actual_native.append({'vm_library_name':name}); continue
        path = path.resolve(strict=True)
        if path in allowed_natives:
            need(fingerprint(path) == allowed_natives[path], 'actual loaded JDK native file')
        else:
            # The sole game SDK native is bound separately by exact archive
            # entry plus actual load path in the controller, never by basename.
            need(mode == 'texture', 'unexpected CPU original native library')
        actual_native.append({'path':str(path),'pin':fingerprint(path)})
    return {'actual_loaded_class_Code': loaded, 'actual_initializers': initializers,
            'actual_JDK_hosted_lambdas': lambdas, 'markers': markers,
            'colorizer_first_use':colorizer_activation(text,profile),
            'actual_JVM_native_loads':actual_native,
            'source_diagnostics_hex': diagnostics.hex()}
