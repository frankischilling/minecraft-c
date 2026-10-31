"""Use the existing bounded pipe owner; require separate real child receipts."""
import hashlib
import importlib.util
import json
import re
from pathlib import Path
from protocol import LIMIT, completed, fingerprint, need, pairs


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    need(spec is not None and spec.loader is not None, 'actual source module')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def write(path, value):
    with Path(path).open('x', encoding='utf-8', newline='\n') as stream:
        text = json.dumps(value, indent=2) + '\n'
        need(stream.write(text) == len(text), 'complete receipt write')


def source_inventory(root, metadata):
    root = root.resolve(strict=True)
    result = {}
    for relative, expected in metadata['source_files'].items():
        path = (root / relative).resolve(strict=True)
        need(path.is_relative_to(root) and fingerprint(path) == expected, 'actual source pin ' + relative)
        result[relative] = expected
    need(result, 'nonempty source closure')
    return result


def run(root, command, work, stem, env):
    owner = load('lookup_bounded_pipe_owner', root / 'tests/reference/client/statistics/support.py')
    result = owner.run_bounded([str(x) for x in command], work, stem, timeout=180, limit=LIMIT, env=env)
    completed(result.capture_report, result.stdout, result.stderr, command[0])
    return result


def product_map(path, executable, expected_sha256):
    need(type(expected_sha256) is str and re.fullmatch('[0-9a-f]{64}',expected_sha256) is not None and
         fingerprint(path)['sha256'] == expected_sha256, 'independently supplied exact qualified product-map identity')
    record = json.loads(path.read_text(encoding='utf-8'))
    need(type(record) is dict and set(record) == {'executable', 'artifacts', 'qualification_receipt'}, 'explicit supplied cohort-artifact map')
    need(record['executable'] == {'path': str(executable.resolve(strict=True)), 'pin': fingerprint(executable)}, 'direct native artifact identity')
    files = record['artifacts']
    need(type(files) is dict and str(executable.resolve()) in files and len(files) > 1, 'actual executable and dependency/config artifact set')
    for name, expected in files.items():
        need(Path(name).is_absolute() and fingerprint(Path(name)) == expected, 'coherent actual product ' + name)
    receipt = record['qualification_receipt']
    need(type(receipt) is dict and set(receipt) == {'path','pin'} and fingerprint(Path(receipt['path'])) == receipt['pin'], 'separate immutable qualification receipt binding')
    return record


ACCEPTED = {'NATIVE_OBSERVATION_ADMITTED','MATCH','SUPPORTED_ROWS_MATCH_WITH_SEPARATE_FRONTIERS'}


def bounded_bytes(path, limit):
    with path.open('rb') as stream: data = stream.read(limit + 1)
    need(len(data) <= limit, 'bounded closed receipt/stream')
    return data


def bytes_pin(data):
    return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}


def closed_replay(replay, receipt_path, receipt_sha256, executable, arguments, products,
                  artifact_manifest, context):
    # The caller supplies the closed receipt digest independently. Nothing in a
    # newly supplied stream/capture is allowed to establish this trust anchor.
    need(receipt_path is not None and type(receipt_sha256) is str and
         re.fullmatch('[0-9a-f]{64}', receipt_sha256) is not None,
         'replay requires an independently supplied closed run-receipt SHA-256')
    receipt_path = receipt_path.resolve(strict=True)
    receipt_bytes = bounded_bytes(receipt_path, 2 * LIMIT)
    receipt_pin = bytes_pin(receipt_bytes)
    need(receipt_pin['sha256'] == receipt_sha256, 'trusted closed run-receipt identity')
    report = json.loads(receipt_bytes.decode('utf-8'), object_pairs_hook=pairs)
    need(type(report) is dict and report.get('status') in ACCEPTED, 'accepted closed observation report')
    need(type(context) is dict and set(context) == {'mode','profile','source_root','source_before','resources_before','directory_before'},
         'explicit current replay source/input/profile context')
    for key, value in context.items():
        need(report.get(key) == value, 'trusted replay context ' + key)
    need(report.get('source_after') == context['source_before'], 'closed unchanged source binding')
    need(report.get('numeric_ID_mapping_performed') is False and
         report.get('constructor_startup_or_playable_claim') is False and
         type(report.get('input_profiles_executed')) is int and report['input_profiles_executed'] == 1,
         'closed bounded observation scope')
    directory = receipt_path.parent
    need(replay.resolve(strict=True) == (directory/'native.stdout').resolve(strict=True), 'receipt-owned original native stdout path')
    native = report.get('native')
    need(type(native) is dict, 'complete trusted native receipt')
    need(native.get('artifact_manifest') == {'path':str(artifact_manifest.resolve(strict=True)), 'pin':fingerprint(artifact_manifest)} and
         native.get('actual_product_map') == products, 'same trusted executable/products/qualification map')
    def streams(stem, recorded):
        need(type(recorded) is dict, 'complete closed stream receipt')
        paths = [directory/(stem+'.stdout'), directory/(stem+'.stderr')]
        data = [bounded_bytes(path,LIMIT) for path in paths]
        for raw, name in zip(data, ('stdout','stderr')):
            need(bytes_pin(raw) == recorded.get(name), 'trusted original ' + stem + ' ' + name + ' bytes')
        capture_path = directory/(stem+'.capture.json')
        capture = json.loads(bounded_bytes(capture_path,LIMIT).decode('utf-8'), object_pairs_hook=pairs)
        # JSON text equality preserves integer/Boolean/float distinctions, unlike
        # Python numeric equality. All original capture fields stay bound.
        need(json.dumps(capture,sort_keys=True) == json.dumps(recorded.get('capture'),sort_keys=True),
             'same trusted original complete capture, no retrospective retie')
        need(type(capture.get('command')) is list and capture['command'], 'recorded direct command')
        completed(capture,*data,Path(capture['command'][0]))
        return *data, capture
    stdout, stderr, capture = streams('native', native)
    need(capture['command'] == [str(executable.resolve(strict=True)),*map(str,arguments)], 'same trusted direct native command')
    banners = directory/'native-source-stdout.raw'
    need(bytes_pin(bounded_bytes(banners,LIMIT)) == report.get('native_source_banners'), 'trusted raw native source banners')
    if 'original' in report:
        original = report['original']; streams('original',original)
        banners = directory/'original-source-stdout.raw'
        need(bytes_pin(bounded_bytes(banners,LIMIT)) == original.get('source_banners'), 'trusted raw original source banners')
    need(fingerprint(receipt_path) == receipt_pin, 'trusted closed receipt unchanged through replay admission')
    return stdout, stderr, capture, {'path':str(receipt_path),'pin':receipt_pin,
                                   'trust':'SHA-256 supplied independently before stream admission'}


def captured_native(root, executable, arguments, work, env, artifact_manifest, expected_sha256,
                    replay=None, replay_receipt=None, replay_sha256=None, replay_context=None):
    products = product_map(artifact_manifest, executable, expected_sha256)
    trusted = None
    if replay is None:
        need(replay_receipt is None and replay_sha256 is None, 'run-receipt options require operational replay')
        result = run(root, [executable, *arguments], work, 'native', env)
        stdout, stderr, capture = result.stdout, result.stderr, result.capture_report
    else:
        stdout, stderr, capture, trusted = closed_replay(replay, replay_receipt, replay_sha256,
            executable, arguments, products, artifact_manifest, replay_context)
    need(product_map(artifact_manifest, executable, expected_sha256) == products, 'all provided products unchanged through actual child/replay')
    need(stderr == b'', 'preserve native diagnostics/refusal as a failure')
    if trusted is not None:
        # Copy only already authenticated original bytes; never re-anchor changed
        # data by manufacturing a fresh stream hash from an untrusted capture.
        for name, data in (('native.stdout',stdout),('native.stderr',stderr)):
            with (work/name).open('xb') as stream: need(stream.write(data) == len(data), 'complete authenticated replay copy')
        write(work/'native.capture.json', capture)
    receipt = {'capture':capture, 'stdout':{'bytes':len(stdout),'sha256':hashlib.sha256(stdout).hexdigest()},
               'stderr':{'bytes':len(stderr),'sha256':hashlib.sha256(stderr).hexdigest()},
               'artifact_manifest':{'path':str(artifact_manifest.resolve()),'pin':fingerprint(artifact_manifest)},
               'actual_product_map':products,
               'artifact_enumeration_scope':'The separately bound qualification receipt owns complete configured artifact enumeration; this observer rehashes its supplied exact map.'}
    if trusted is not None: receipt['authenticated_replay_of'] = trusted
    return stdout, stderr, receipt
