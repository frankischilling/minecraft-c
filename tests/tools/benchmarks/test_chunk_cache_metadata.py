"""Audit the complete retained Windows EW CPU capture without native execution."""
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import statistics
import sys
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
RECEIPT = ROOT / 'tests/benchmarks/chunk_cache_metadata_windows.json'
SPEC = importlib.util.spec_from_file_location(
    'retained_metadata_parser', ROOT / 'tests/tools/world/benchmark_chunk_cache_metadata.py')
CPU = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CPU)
EMPTY = hashlib.sha256(b'').hexdigest()
PLAN = '7ab455edfbd6090fdce117919286099a038135185b751745ec41e26621bcb40d'
BUILD = 'a2f8638125f3c45c70f88631b3771204b0ca612c1ef6211df2f34c312d38ecf8'
OBSERVER = 'f74ef10ac3ad1cf3ddcb2f181d4871e9bb4c7470f818884b1a7c19c87f3e6b51'
ORDER = [(1, 1, 'public'), (1, 2, 'failure'), (2, 1, 'failure'), (2, 2, 'public'),
         (3, 1, 'public'), (3, 2, 'failure'), (4, 1, 'failure'), (4, 2, 'public')]
STDOUT = (
    '22b512e3f832a26ffd5fdda8cbb1b31b36e3d5bea93b688e5734b15a6d966185',
    'a953078560985e871aeb6c7357973419a1abdbf070553f398b535fafcb1ebae2',
    '62b2dbec47aaf4bd3b71f12270018b0a36cab4df6f836f8fa406d6baed045223',
    '7b6f78bea37dc2cdc9fdf03ce53e6004285e8a77f06d017303aa57911855f439',
    '70ffe53d17843995812e2cd18b03d7373a2ddba7121ac7dc89a600c6b9ff6412',
    'ba74caa75349ba1c0ab4adf4a9eddae535ba9d14a6e1426250183a126da6389b',
    'ddea572f263d83dbaa485c979d505b7d40db42ef60c53443503befe26e4b39d1',
    '27810584c824342298dca539dd7fd03d6f95658dc3367b03a2403eecb388f243',
)
CONTROL_STDOUT = (
    'f932ab3264219419ac4fb615d6ff46a09320ebd89a887147870b881e3e2b5435',
    'f6c489ba60e4708876d853a10f10b5719cf784f85064d2fd44dbe7bf12531c41',
)
CONTROL_PATTERN = re.compile(
    r'EW_METADATA_CPU_CONTROL name=(\S+) status=(\d+) value=(-?\d+) '
    r'method=(\S+) descriptor=(\S+) pc=(\d+) child=(\d+) native=(\d+) '
    r'nested=(\d+) operation=(.*?) exact-public-failure=(\d+)')
# Ordered actual outcomes, including the eight distinct signed-coordinate cases.
CONTROL_CONTRACT = [
    ('null-cache', 1, 777, 0, 0, 1, 'ew native accessor arguments'),
    ('null-out', 1, 777, 0, 0, 1, 'ew native accessor arguments'),
    ('null-out-before-y', 1, 777, 0, 0, 1, 'ew native accessor arguments'),
    ('active', 2, 777, 0, 0, 1, 'ew native active/retiring accessor'),
    ('active-before-y', 2, 777, 0, 0, 1, 'ew native active/retiring accessor'),
    ('retiring', 2, 777, 0, 0, 1, 'ew native active/retiring accessor'),
    ('data-unread', 0, 4, 54, 0, 0, 'NULL'),
    ('null-nibble', 5, 777, 54, 10, 0, 'actual Chunk null Nibble backing'),
    ('blank-before-null-nibble', 0, 0, 54, 0, 0, 'NULL'),
    ('null-nibble-backing', 5, 777, 54, 10, 0, 'actual Chunk null Nibble backing'),
    ('short-nibble-backing', 6, 777, 54, 10, 0, 'actual Chunk Nibble index'),
    ('last-nibble-even', 0, 4, 54, 0, 0, 'NULL'),
    ('last-nibble-odd', 0, 13, 54, 0, 0, 'NULL'),
    ('null-outer', 5, 777, 41, 0, 0, 'actual ew null outer array'),
    ('y-before-null', 0, 0, 0, 0, 0, 'NULL'),
] + [
    ('signed-array-index', 6, 777, pc, 0, 0,
     'actual ew ' + ('outer' if pc == 41 else 'inner') + ' array index')
    for pc in (41, 41, 44, 44, 41, 41, 44, 44)
] + [
    ('y-low', 0, 0, 0, 0, 0, 'NULL'),
    ('y-high', 0, 0, 0, 0, 0, 'NULL'),
    ('failed-constructor-null-cell', 5, 777, 54, 0, 0, 'actual ew null Chunk receiver'),
]


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return digest(json.dumps(value, sort_keys=True, separators=(',', ':')).encode())


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key')
        result[key] = value
    return result


def stream(record, expected):
    raw = record['text'].encode('utf-8')
    require(len(raw) == record['bytes'], 'Raw stream byte count')
    require(digest(raw) == record['sha256'] == expected, 'Historical raw stream identity')
    return record['text']


def audit_control_rows(rows):
    require(len(rows) == len(CONTROL_CONTRACT) == 26, 'All 26 ordered controls')
    for line, expected in zip(rows, CONTROL_CONTRACT):
        match = CONTROL_PATTERN.fullmatch(line)
        require(match is not None, 'Complete control observation')
        name, status, value, method, descriptor, pc, child, native, nested, operation, exact = match.groups()
        actual = (name, int(status), int(value), int(pc), int(child), int(native), operation)
        require(actual == expected, 'Control outcome, order and actual failure site')
        require((method, descriptor, nested, int(exact)) ==
                ('e', '(III)I', '0', int(int(status) != 0)), 'Canonical error identity')


def replay_cpu(text, measure, label):
    """Semantic replay deliberately independent of retained raw hash checks."""
    parsed = CPU.parse_cpu_run(text, measure)
    context = parsed['context']
    require(context['adapter'] == label, 'Adapter selection')
    require(context['success-error'] == ('full' if label == 'public' else 'untouched'),
            'Distinct public/private success error contracts')
    for key, expected in {
        'error-bytes': '7352', 'calls': '65536', 'points': '1024', 'chunks': '9',
        'samples': '7' if measure else '0', 'planes': '15454881346718783258',
        'coordinates': '9375872228490941588', 'public-init-per-call': '1',
        'public-copy-per-call': '1', 'public-nominal-bytes-per-sample': '963641344',
    }.items():
        require(context[key] == expected, 'Fixed workload context: ' + key)
    audit_control_rows(parsed['control_rows'])
    require(parsed['finish'] == 'EW_METADATA_CPU_FINISH checks=' +
            ('465115' if measure else '6342') + ' cleanup=1 genuine-world=1 genuine-ew=1 gl=0',
            'Complete native controls and cleanup')
    require(len(parsed['samples']) == (7 if measure else 0), 'Complete sample inventory')
    for index, sample in enumerate(parsed['samples']):
        for key, expected in {
            'sample': str(index), 'frequency': '10000000', 'calls': '65536',
            'result': '17338508285966561155', 'planes': context['planes'],
            'owner-same': '1', 'world-same': '1', 'random-same': '1', 'holds-same': '1',
            'acquisition-gets': '0',
        }.items():
            require(sample[key] == expected, 'Actual sample work/identity: ' + key)
        require(int(sample['ticks']) > 0, 'Positive actual QPC interval')
    return parsed


def audit_cpu_process(process, measure, expected_stdout):
    require(process['exit'] == 0 and process['timeout'] is False, 'Successful complete process')
    require(math.isfinite(process['seconds']) and process['seconds'] > 0, 'Process duration')
    require(process['stdout_sha256'] == expected_stdout, 'Raw report stdout binding')
    require(process['stderr_sha256'] == EMPTY, 'Raw report stderr binding')
    text = stream(process['stdout'], expected_stdout)
    require(stream(process['stderr'], EMPTY) == '', 'Empty native stderr')
    parsed = replay_cpu(text, measure, process['label'])
    require(parsed == {key: process[key] for key in ('context', 'samples', 'control_rows', 'finish')},
            'Raw/parser/report agreement')
    before, after = process['authored_inputs'], process['files_after']
    require(canonical(before) == 'b79bf3628bcf14682a38f40591cc86918dc59ca7da4afe846c68654cb4aaa28f',
            'Actual authored input identities')
    require(len(before) == 5 and set(after) == set(before) | {'session.lock'}, 'Authored save inventory')
    require(all(after[key] == value for key, value in before.items()), 'Authored save bytes unchanged')
    require(after['session.lock']['bytes'] == 8, 'Actual lock retained separately')
    return parsed


def audit_original(document):
    original = document['existing_original_data']
    # These are existing public finite data and identities, not a newly captured Java cohort.
    fixed = {key: value for key, value in original.items() if key != 'replay'}
    require(canonical(fixed) == 'd65bb3d4471773c12e0b3c47194963a334fa4f7ce90813410bf65f9008e58ce5',
            'Existing original data identity and finite scope')
    report = original['replay']
    require(report['status'] == 'PASS' and report['rows'] == 950 and report['mismatches'] == [],
            'Existing finite compatibility replay')
    require(report['input_before_sha256'] == report['input_after_sha256'] and
            report['resource_before_sha256'] == report['resource_after_sha256'], 'Replay inputs unchanged')
    capture = report['native']
    require(capture['exit'] == 0 and capture['timed_out'] is False and
            capture['pipe_capture_complete'] is True and capture['reader_errors'] == {} and
            capture['output_overflow'] is False, 'Original-row native capture completed')
    text = stream(report['stdout'], '73af3f97f9f56ed9c668bfccdf95845241587e5b8cbc3672fac74e029c6c3e7c')
    require(stream(report['stderr'], EMPTY) == '', 'Original-row native stderr')
    require(capture['stdout_sha256'] == report['stdout']['sha256'] and
            capture['stdout_bytes'] == report['stdout']['bytes'] and
            capture['stderr_sha256'] == EMPTY and capture['stderr_bytes'] == 0, 'Reference capture binding')
    actual = [line for line in text.splitlines() if line.startswith('EW ')]
    require(len(actual) == 950 and len({line.split()[1] for line in actual}) == 950,
            'Complete distinct original row inventory')
    require(actual == report['actual_rows'] == original['expected_rows'], 'All 950 exact original rows')
    require(text.splitlines()[-1] ==
            'Source EW: 3541 checks; 950 original rows and canonical leases', 'Full private controls completed')


def audit_build(document):
    build = document['build']
    require(canonical(build) == '7d2c6c712b9b0020778df326b01219a8b40e5d3e6d99c02358b4924f5273caa8',
            'Frozen build/source/header/library/tool/history bindings')
    first = build['build1']['receipt']
    second = build['build2']['receipt']
    require(first['status'] == second['build1_status_remains'] == 'FAIL', 'Warned build remains failed')
    compiles = [row for row in first['commands'] if row['command']['kind'] == 'compile']
    require(len(compiles) == build['compiled_object_count'] == 6 and
            all(row['exit'] == 0 for row in compiles), 'Six retained successful compiles')
    for row in compiles:
        require(all(flag in row['command']['argv'] for flag in ('/O2', '/fp:strict', '-std:c17', '/WX')),
                'Actual optimized strict C17 compiler options')
    require(second['status'] == 'BUILT_AUDITED_NOT_RUN' and second['compiler_executions'] == 0 and
            len(second['commands']) == 2 and all(row['exit'] == 0 for row in second['commands']),
            'Warning-free two-link recovery without recompilation')
    require(len(second['headers']) == build['actual_header_count'] == 372 and
            first['headers'] == second['headers'], 'Exact actual header reuse')
    require(second['shared_observer_sha256'] == OBSERVER and
            second['products_sha256']['observer.obj'] == OBSERVER, 'Same compiled observer')
    require(second['public_code_layout_imports_equal_retained_warned_output'] is True and
            second['PE_models']['public'] == build['retained_warned_public']['pe'],
            'Recovery preserves public code/layout/imports')
    for label, owners in build['map_ownership'].items():
        if label == 'controls':
            require(owners['mc_cv_metadata'] == 'candidate-cv.obj' and
                    owners['mc_client_chunk_cache_metadata_failure'] == 'candidate-cache.obj',
                    'Integrated semantic control ownership')
        else:
            require(owners['main'] == 'observer.obj' and
                    owners['ew_cpu_adapter_get'] == label + '-adapter.obj', 'Common caller / selected adapter')
            require(owners['mc_client_chunk_cache_metadata'] ==
                    ('baseline-cache.obj' if label == 'public' else 'candidate-cache.obj'),
                    'Actual public/private cache owner')


def audit_receipt(document):
    require(type(document['schema']) is int and document['schema'] == 1, 'Receipt schema')
    require(document['qualification'] == 'PASS_FIXED_WORK_CPU_ONLY', 'Finite CPU qualification only')
    require(document['scope']['frame_or_fps_claim'] is False and
            document['scope']['new_original_parity_claim'] is False, 'No frame or new original-parity credit')
    require(not re.search(r'[A-Za-z]:[\\/]', json.dumps(document)), 'No private absolute path')
    require(canonical(document['source']) ==
            '03240ea41402752e91aff8183fd0c31f426d5e4f4367db03dcfc2eff3a1ba085', 'Captured source projections')
    require(canonical(document['workload']) ==
            '96ee8a937c8cc3650ea4d156a4e94570940038019c62bc582bbb2cd7f9c1de1b', 'Actual CPU work shape')
    audit_build(document)
    audit_original(document)
    controls = document['controls']['report']
    measure = document['measurement']['report']
    require(document['controls']['raw_report_identity']['sha256'] ==
            '5cf1707faabafcef36b8fa2cdb010f119b6fd8eb5cded75121e9e14ffdf28f9b' and
            document['measurement']['raw_report_identity']['sha256'] ==
            '8f8cfa5af9c7d029fad2fa7af7673b6d4f34221b548801af26d460b88c89db1b',
            'Actual retained raw report identities')
    products = document['build']['build2']['receipt']['products_sha256']
    for report in (controls, measure):
        require(report['status'] == 'PASS' and report['plan_sha256'] == PLAN and
                report['build_receipt_sha256'] == BUILD and report['products_sha256'] == products and
                report['selected_root_private_products_unchanged'] is True, 'Native run / build continuity')
    require(controls['mode'] == 'controls' and measure['mode'] == 'measure', 'Separate control and CPU releases')
    require(controls['declared_order'] == [['public', 'failure']], 'CPU control order')
    require(len(controls['processes']) == 3, 'Original/private controls plus two CPU control processes')
    driver = controls['processes'][0]
    require(driver['label'] == 'ordinary-private-controls' and driver['exit'] == 0 and
            driver['timeout'] is False and driver['stdout_sha256'] ==
            '1c6281f8ba6fac1201ba7754497876dce2e74cc68cfd04af9e8c8680c12531ba' and
            driver['stderr_sha256'] == EMPTY and driver['stdout_projection']['original_bytes'] == 180 and
            driver['stdout_projection']['original_sha256'] == driver['stdout_sha256'] and
            driver['stdout_projection']['text'] ==
            'Source EW: 950 exact original rows and canonical lease controls; existing-reference-report\r\n' and
            driver['stdout_projection']['projected_sha256'] ==
            digest(driver['stdout_projection']['text'].encode()),
            'Ordinary control driver and explicit path projection')
    require(stream(driver['stderr'], EMPTY) == '', 'Control driver stderr')
    parsed_controls = []
    for position, (process, identity, label) in enumerate(
            zip(controls['processes'][1:], CONTROL_STDOUT, ('public', 'failure')), 1):
        require(process['label'] == label and process['round'] == 1 and
                process['position'] == position, 'Control order')
        parsed_controls.append(audit_cpu_process(process, False, identity))
    require(len(measure['processes']) == len(STDOUT) == 8, 'Retain all eight processes')
    require(measure['declared_order'] == [['public', 'failure'], ['failure', 'public']] * 2,
            'Four balanced AB/BA rounds')
    groups = {'public': [], 'failure': []}
    profiles = []
    authored = controls['processes'][1]['authored_inputs']
    for process, identity, order in zip(measure['processes'], STDOUT, ORDER):
        require((process['round'], process['position'], process['label']) == order, 'Actual process order')
        require(process['authored_inputs'] == authored, 'Same authored data per process')
        parsed = audit_cpu_process(process, True, identity)
        profiles.append(parsed)
        require(parsed['control_rows'] == parsed_controls[0]['control_rows'], 'Paired complete control observations')
        groups[process['label']].extend(1000 * int(row['ticks']) / int(row['frequency'])
                                        for row in parsed['samples'])
    require(parsed_controls[0]['control_rows'] == parsed_controls[1]['control_rows'], 'Both control variants agree')
    # Also exercise the public cohort parser, independently of the receipt summary.
    CPU.compare_cpu_runs(profiles)
    summary = {}
    for label, values in groups.items():
        require(len(values) == 28, 'All 28 samples per variant')
        summary[label] = {'n': len(values), 'min': min(values), 'median': statistics.median(values),
                          'mean': statistics.mean(values), 'max': max(values)}
    require(summary == measure['summary_ms_per_65536_calls'], 'Unfiltered raw QPC statistics')
    require(measure['result_identity'] == ['17338508285966561155', '15454881346718783258'],
            'All actual result/plane identities')
    reduction = 100 * (1 - summary['failure']['median'] / summary['public']['median'])
    require(reduction == document['measurement']['median_reduction_percent'], 'Fixed-work median reduction')
    return summary


class ChunkCacheMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(RECEIPT.read_text(encoding='utf-8'), object_pairs_hook=unique)

    def test_complete_retained_cpu_capture(self):
        result = audit_receipt(self.document)
        self.assertEqual(result['public']['median'], 9.06405)
        self.assertEqual(result['failure']['median'], 1.08805)

    def test_all_processes_and_sample_order_required(self):
        for field in ('process', 'sample', 'order'):
            changed = copy.deepcopy(self.document)
            processes = changed['measurement']['report']['processes']
            if field == 'process':
                processes.pop()
            elif field == 'sample':
                processes[0]['samples'].pop()
            else:
                processes[0], processes[1] = processes[1], processes[0]
            with self.subTest(field=field), self.assertRaises(ValueError):
                audit_receipt(changed)

    def test_rehashed_raw_timing_cannot_rebind_history(self):
        changed = copy.deepcopy(self.document)
        process = changed['measurement']['report']['processes'][0]
        process['stdout']['text'] = process['stdout']['text'].replace('ticks=87864', 'ticks=77864')
        process['stdout']['sha256'] = process['stdout_sha256'] = digest(process['stdout']['text'].encode())
        with self.assertRaises(ValueError):
            audit_receipt(changed)

    def test_semantic_sample_corruptions_without_hash_gate(self):
        text = self.document['measurement']['report']['processes'][0]['stdout']['text']
        replacements = [('calls=65536', 'calls=65535'), ('points=1024', 'points=1'),
                        ('result=17338508285966561155', 'result=17338508285966561154'),
                        ('owner-same=1', 'owner-same=0'), ('world-same=1', 'world-same=0'),
                        ('random-same=1', 'random-same=0'), ('holds-same=1', 'holds-same=0'),
                        ('acquisition-gets=0', 'acquisition-gets=1'), ('sample=1 ', 'sample=0 '),
                        ('ticks=87864', 'ticks=0'), ('cleanup=1', 'cleanup=0'),
                        ('genuine-ew=1', 'genuine-ew=0'), ('gl=0', 'gl=1')]
        for old, new in replacements:
            with self.subTest(old=old), self.assertRaises((ValueError, AssertionError)):
                replay_cpu(text.replace(old, new), True, 'public')

    def test_full_error_and_short_circuit_control_corruptions(self):
        rows = self.document['controls']['report']['processes'][1]['control_rows']
        for index, old, new in [(0, 'status=1', 'status=0'), (2, 'pc=0', 'pc=54'),
                                (7, 'child=10', 'child=0'), (8, 'value=0', 'value=4'),
                                (14, 'pc=0', 'pc=41'), (17, 'pc=44', 'pc=41'),
                                (25, 'exact-public-failure=1', 'exact-public-failure=0')]:
            changed = list(rows)
            changed[index] = changed[index].replace(old, new)
            with self.subTest(index=index), self.assertRaises(ValueError):
                audit_control_rows(changed)

    def test_distinct_success_error_contracts(self):
        process = self.document['measurement']['report']['processes'][1]
        with self.assertRaises((ValueError, AssertionError)):
            replay_cpu(process['stdout']['text'].replace('success-error=untouched', 'success-error=full'),
                       True, 'failure')

    def test_no_frame_or_new_java_parity_credit(self):
        for key in ('frame_or_fps_claim', 'new_original_parity_claim'):
            changed = copy.deepcopy(self.document)
            changed['scope'][key] = True
            with self.subTest(key=key), self.assertRaises(ValueError):
                audit_receipt(changed)

    def test_existing_950_rows_cannot_be_subset_or_relabelled(self):
        for key in ('actual_rows', 'mismatches'):
            changed = copy.deepcopy(self.document)
            replay = changed['existing_original_data']['replay']
            if key == 'actual_rows':
                replay[key].pop()
            else:
                replay[key].append({'index': 0, 'native': 'bad', 'original': 'retained'})
            with self.subTest(key=key), self.assertRaises(ValueError):
                audit_original(changed)

    def test_provenance_and_failed_history_are_immutable(self):
        for change in ('observer', 'header', 'failure'):
            changed = copy.deepcopy(self.document)
            build = changed['build']
            if change == 'observer':
                build['build2']['receipt']['shared_observer_sha256'] = '0' * 64
            elif change == 'header':
                build['build2']['receipt']['headers'].pop(next(iter(build['build2']['receipt']['headers'])))
            else:
                build['build1']['receipt']['status'] = 'PASS'
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_build(changed)

    def test_claimed_summary_and_authored_bytes_must_match(self):
        for change in ('median', 'authored'):
            changed = copy.deepcopy(self.document)
            measure = changed['measurement']['report']
            if change == 'median':
                measure['summary_ms_per_65536_calls']['failure']['median'] /= 2
            else:
                measure['processes'][0]['files_after']['level.dat']['sha256'] = '0' * 64
            with self.subTest(change=change), self.assertRaises(ValueError):
                audit_receipt(changed)


if __name__ == '__main__':
    unittest.main()
