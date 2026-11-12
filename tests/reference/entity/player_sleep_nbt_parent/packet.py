"""Portable strict reader; positive admission needs only this flat packet."""
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import sys
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('player_sleep_nbt_parent_validate', HERE / 'validate.py')
V = importlib.util.module_from_spec(spec)
spec.loader.exec_module(V)
require, loads, exact = V.require, V.loads, V.exact
ORIGINAL = {
    'finite.json': '455e101c9feb56b5bbe9a91a8aa04cd8444df2db3ac44130985fee98ee543637',
    'profiles.json': '1f17ce23dc082dbfbd3f27ccd330f3015c20d1d00073d905cac0cc9f95d13171',
    'profiles.tsv': '69a47c2e9b83b44cf7c0e8a47fcc99e58d4b9a27f394ae382259197f85967d79',
    'class-names.txt': '87fe781b7a2eaa13e3eece1a62f3181ab88edd8a87ea639a4c81f62324fc549b',
    'helper-pins.json': '42c31965b508c5b7d9b693f435863b3bb2c6cd31f9b1acec7138979cdb1ddc64',
    'executed-source-inputs.json': 'f622d1540218b01b3e0ad102b76b0f33336fd156614412574c1b03cd6650177b',
    'source-pins.json': '2cec8ce76f536b3d2f093a14c4ab2985dc84cdc6e00dbbc31aace9a44278063b',
    'witnesses.json': '209321a60a63fd65f678c7b0457d62afd7b82c2da2697f7fc4363bba48f6fe62'}
FILES = frozenset((*ORIGINAL, '.gitattributes', 'README.md', 'packet.py', 'validate.py',
                   'test_packet.py', 'capture_reference.py', 'capture_reader.py', 'export_reference.py',
                   'PlayerNbtSleepMatrixObservation.java', 'PlayerNbtSleepMatrixJdi.java'))
EXECUTION = '993ac0062481a12278ec27a1159a5fa6716a3f190d54fcbdddd6b20e8ed00043'
PRODUCERS = 'd6b204ce347b6f94959d2e290b2628909686b9e0792920f9109811477c4a23ac'
BINARIES = 'faf98e82592cbf6db8d3dfeb46e4b7d61cffb3209e4085fc69f536a5a6a7f341'
HELPERS = 'dd7286d6c2cceccdb2ffc1e897f676c9543e8ad8ad008e471c9af3ce961a3ab6'
PROTECTED = '1ed082b2cbd9a22e409cc222bbf8d3a50e7f784e4dd5bd9f187050f58a124dcf'
VM_RECORDS = '4430034021548f05d8ee9811a9e7a80a005401ba9e7fd8d95381ed6883d6df96'
PROVENANCE_KEYS = frozenset((
    'schema', 'scope', 'historical_capture_receipt_sha256',
    'historical_failed_protocol_receipt_sha256', 'historical_failed_protocol_status',
    'historical_preflight_receipt_sha256', 'execution_input_sha256',
    'execution_producer_input_sha256', 'execution_binary_input_sha256',
    'execution_helper_sha256', 'executed_source_scope', 'protected_old_and_current_closure',
    'old33_37_raw_products', 'guards', 'original_vm_records', 'bounds',
    'protocol_controls', 'current_tool_scope', 'current_artifact_sha256'))


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def commitment(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def logical(name):
    return (type(name) is str and name and '\\' not in name and ':' not in name and
            not name.startswith('/') and '..' not in PurePosixPath(name).parts)


def hash_map(value, count, label):
    require(type(value) is dict and len(value) == count and
            all(logical(name) and type(sha) is str and V.SHA.fullmatch(sha)
                for name, sha in value.items()), label)


def provenance(value):
    V.keys(value, PROVENANCE_KEYS, 'closed portable provenance')
    require(value['schema'] == 'PLAYER_SLEEP_NBT_PARENT_PROVENANCE_V1', 'packet provenance schema')
    require(exact(value['scope'], {'original_vms': 2, 'rows_per_vm': 40, 'native': False,
                               'loaded_client': False, 'original_draw': False,
                               'whole_restart': False, 'natural_night': False,
                               'initializer_execution_extent': 'UNOBSERVED_UNQUALIFIED'}),
            'bounded original-component qualification')
    require(value['historical_capture_receipt_sha256'] ==
            'cc7ea35f3768b805c3269a5e6481676f6e9ed3f770835ff2217a163690d38d22' and
            value['historical_failed_protocol_receipt_sha256'] ==
            'adddca8d505b69ce8ac8fd7eb4b88d9c722af69be05fdb350325c85419d49e04' and
            value['historical_failed_protocol_status'] == 'FAIL' and
            value['historical_preflight_receipt_sha256'] ==
            'e183c0572939c159491608b7099eabbdaf595d5cb1be649c6db097ca813fcf28',
            'actual historical attempts retain their separate status and identity')
    for key, count, expected in (('execution_input_sha256', 171, EXECUTION),
                                  ('execution_producer_input_sha256', 13, PRODUCERS),
                                  ('execution_binary_input_sha256', 152, BINARIES),
                                  ('execution_helper_sha256', 4, HELPERS)):
        hash_map(value[key], count, 'complete actual historical map ' + key)
        require(commitment(value[key]) == expected, 'immutable complete historical map ' + key)
    inputs = value['execution_input_sha256']
    require(value['execution_producer_input_sha256'] ==
            {k: v for k, v in inputs.items() if k.startswith(('producer/', 'support/'))} and
            value['execution_binary_input_sha256'] ==
            {k: v for k, v in inputs.items() if k.startswith(('jdk/', 'sdk/'))},
            'actual historical producer/tool maps are never current-tool substitutes')
    require(inputs['archive/client.jar'] == 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c' and
            inputs['archive/server.jar'] == '033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d',
            'unchanged supplied archives')
    require(exact(value['executed_source_scope'], {
        'repository_head': '0412cdf76ee294a831b71e0ee3ab04271e74b70b', 'working_tree_dirty': True,
        'inputs': 5529, 'file': 'executed-source-inputs.json', 'sha256': ORIGINAL['executed-source-inputs.json']}),
        'actual executed dirty5529 source declaration, not committed-tree equivalence')
    require(exact(value['protected_old_and_current_closure'],
            {'files': 6045, 'sha256': PROTECTED, 'raw_paths_published': False}),
            'actual old/current closure commitment without private raw paths')
    require(exact(value['old33_37_raw_products'], {'availability': 'NOT_LOCATED_UNAVAILABLE',
                                              'before_after_guard_claimed': False,
                                              'inherited_qualification_claimed': False,
                                              'new_execution_input': False}), 'unavailable old raw evidence remains uncredited')
    require(exact(value['guards'], {'execution_inputs_unchanged': True, 'protected_inputs_unchanged': True,
                                'compiled_helpers_unchanged': True, 'generated_pins_unchanged': True,
                                'actual_processes_terminal': True}), 'actual completed before/after guards')
    require(exact(value['bounds'], {'target_seconds': 180, 'controller_seconds': 210, 'process_seconds': 240,
                                'runner_seconds': 720, 'target_heap_mib': 256, 'controller_heap_mib': 256,
                                'target_pipe_bytes': 16777216, 'journal_bytes': 16777216,
                                'case_calls': 512, 'case_events': 8192}), 'actual finite capture limits')
    require(type(value['original_vm_records']) is list and len(value['original_vm_records']) == 2,
            'two fresh actual VMs')
    for i, record in enumerate(value['original_vm_records']):
        V.keys(record, ('vm', 'rows', 'returns', 'throws', 'calls', 'events', 'Code',
                        'raw_stdout_sha256', 'raw_stderr_sha256', 'raw_journal_sha256'), 'portable actual VM proof')
        require((record['vm'], record['rows'], record['returns'], record['throws'], record['calls'],
                 record['events'], record['Code']) == (i, 40, 29, 11, 5393, 13062, 62) and
                all(type(record[k]) is str and V.SHA.fullmatch(record[k]) for k in
                    ('raw_stdout_sha256', 'raw_stderr_sha256', 'raw_journal_sha256')),
                'actual per-VM full-return/first-throw extent')
    require(commitment(value['original_vm_records']) == VM_RECORDS, 'immutable complete actual original VM records')
    require(type(value['protocol_controls']) is list and len(value['protocol_controls']) == 2,
            'both actual protocol-control modes')
    for mode, control in zip(('normal', 'optimized'), value['protocol_controls']):
        require(exact(control, {'mode': mode, 'checks': 11,
                            'receipt_sha256': inputs['protocol-control/' + mode + '.json'],
                            'scope': 'copied_failed_stream_protocol_only_no_new_VM'}),
                'protocol controls never relabel failed original execution')
    require(value['current_tool_scope'] ==
            'packet/export/reproduction tools; no inherited original or native qualification', 'current tooling scope')
    hash_map(value['current_artifact_sha256'], len(FILES), 'exact current flat artifact pins')
    require(set(value['current_artifact_sha256']) == FILES, 'closed current artifact inventory')
    for name, sha in ORIGINAL.items():
        require(value['current_artifact_sha256'][name] == sha, 'immutable original artifact pin: ' + name)
    for name in ('PlayerNbtSleepMatrixObservation.java', 'PlayerNbtSleepMatrixJdi.java'):
        require(value['current_artifact_sha256'][name] == inputs['producer/' + name],
                'authored Java/JDI bytes equal actual historical producer')


def _read(directory=None):
    directory = HERE if directory is None else Path(directory)
    require(directory.is_dir(), 'flat packet directory')
    require({p.name for p in directory.iterdir()} == FILES | {'provenance.json'} and
            all(p.is_file() and not p.is_symlink() for p in directory.iterdir()), 'closed flat packet files')
    proof = loads((directory / 'provenance.json').read_text(encoding='utf-8'))
    provenance(proof)
    for name, sha in proof['current_artifact_sha256'].items():
        require(digest(directory / name) == sha, 'current artifact changed: ' + name)
    def load(name):
        return loads((directory / name).read_text(encoding='utf-8'))
    source, helper = load('source-pins.json'), load('helper-pins.json')
    require(source['archive_sha256'] == proof['execution_input_sha256']['archive/client.jar'], 'original source archive pins')
    require({name + '.class': value['class_sha256'] for name, value in helper.items()} ==
            proof['execution_helper_sha256'], 'all actual helper Class identities retained as metadata only')
    source_inputs = load('executed-source-inputs.json')
    hash_map(source_inputs, 5529, 'complete actual executed public source closure')
    declarations = V.profiles(load('profiles.json'), (directory / 'profiles.tsv').read_text(encoding='ascii'))
    graph, witness = load('finite.json'), load('witnesses.json')
    V.finite(graph, declarations, source, helper)
    V.witnesses(witness, graph, declarations, source)
    return graph, witness, declarations


def read_reference(directory=None):
    return _read(directory)[0]


def read_witnesses(directory=None):
    return _read(directory)[1]


def profiles(directory=None):
    return _read(directory)[2]
