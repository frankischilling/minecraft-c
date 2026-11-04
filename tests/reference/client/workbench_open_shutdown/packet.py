"""Java-free audit of the bounded live-open Workbench native receipt."""
import argparse
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
PROFILES = ('clean', 'container', 'player', 'world')
CHECKS = dict(zip(PROFILES, (1722, 1741, 2045, 2066)))
SECONDS = dict(zip(PROFILES, (7.55, 7.56, 7.45, 7.7)))
ENTITY_COUNTS = dict(zip(PROFILES, (13, 14, 16, 19)))
SAVE_COUNTS = dict(zip(PROFILES, (12, 12, 12, 12)))
FOCUSED_CASES = [
    {'name': 'client.world_save_title', 'status': 'PASS', 'seconds': 16.60},
    {'name': 'client.world_reload_cycle', 'status': 'PASS', 'seconds': 42.22},
    {'name': 'client.world_selection_zombie', 'status': 'PASS', 'seconds': 11.22},
    {'name': 'client.workbench_open_shutdown_clean', 'status': 'FAIL', 'seconds': 16.55},
    {'name': 'client.workbench_open_shutdown_container', 'status': 'PASS', 'seconds': 15.39},
    {'name': 'client.workbench_open_shutdown_player', 'status': 'FAIL', 'seconds': 11.66},
    {'name': 'client.workbench_open_shutdown_world', 'status': 'PASS', 'seconds': 17.90},
]
PRECEDING_CASES = [
    {'name': 'client.workbench_open_shutdown_clean', 'status': 'FAIL', 'seconds': 21.61},
    {'name': 'client.workbench_open_shutdown_container', 'status': 'PASS', 'seconds': 13.92},
    {'name': 'client.workbench_open_shutdown_player', 'status': 'PASS', 'seconds': 15.99},
    {'name': 'client.workbench_open_shutdown_world', 'status': 'PASS', 'seconds': 17.76},
]
PRECEDING_OBSERVATION = (
    'No right click was recorded in three 50-event input batches despite Source focus and '
    'the process owning foreground. Setup then failed wb_hit before terminal preparation. '
    'The test wait was corrected to consume actual serial input events; unrestricted input '
    'timing remains unqualified.')
SUFFIXES = ('.c', '.h', '.inc', '.cmake', '.py')


def require(value, message):
    if not value:
        raise ValueError('workbench native receipt: ' + message)


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key ' + key)
        result[key] = value
    return result


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def canonical(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def hash_string(value):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value) is not None


def relative_name(value):
    return (isinstance(value, str) and bool(value) and '\\' not in value and
            ':' not in value and not value.startswith('/') and
            all(part not in ('', '.', '..') for part in value.split('/')))


def keys(value, names, label):
    require(isinstance(value, dict) and set(value) == set(names.split()), label + ' exact keys')


def manifest(value, count, label):
    require(isinstance(value, dict) and len(value) == count, label + ' count')
    for name, entry in value.items():
        require(relative_name(name), label + ' repository-relative path')
        keys(entry, 'bytes sha256', label)
        require(type(entry['bytes']) is int and entry['bytes'] > 0 and
                hash_string(entry['sha256']), label + ' exact bytes/hash')


def expected_receipts(profile, checks):
    busy = []
    if profile == 'container':
        busy = [{'profile': profile, 'phase': 'pre-transfer', 'source-unchanged': '1',
                 'native-owner-unchanged': '1'}]
    elif profile in ('player', 'world'):
        busy = [{'profile': profile, 'phase': 'world-audit', 'source-unchanged': '1',
                 'same-it': '1', 'status': 'busy'}]
    return {
        'setup': [{'sdk-open': '1', 'sdk-grid': '1', 'sdk-cursor': '1',
                   'result-preview': '1', 'actual-aliases': '1'}],
        'prepare': [{'profile': profile, 'returned': '1', 'source-unchanged': '1',
                     'world-ticks': '0', 'stacks': '97', 'entities': str(ENTITY_COUNTS[profile]),
                     'randoms': str(ENTITY_COUNTS[profile] + 4),
                     'active-container-is-screen-container': True}],
        'transfer': [{'profile': profile, 'same-it': '1', 'source-aliases': '1',
                      'sealed': '1', 'owner-moved': '1', 'idempotent': '1'}],
        'denials': [{'profile': profile, 'getters-revoked': '1', 'player-retain': 'busy',
                     'container-retain': 'busy', 'ix-set': 'busy', 'source-set': 'busy',
                     'router': 'refused', 'frame': 'refused'}],
        'busy': busy,
        'release': [{'profile': profile, 'existing-hold-only': '1',
                     'released': 'none' if profile == 'clean' else profile, 'same-it': '1'}],
        'terminal': [{'profile': profile, 'native-prepare': '1', 'source-close': '0',
                      'source-save': '0', 'unchanged': '1'}],
        'finish': [{'profile': profile, 'native-cleanup': '1', 'windows-absent': '1',
                    'source-close': '0', 'source-save': '0', 'checks': str(checks)}],
    }


def project_profile(directory, seconds):
    """Select actual receipts; never publish process addresses or private paths."""
    directory = Path(directory)
    source = load(directory / 'run.json')
    profile = source['profile']
    require(profile in PROFILES and source['status'] == 'PASS' and
            source['native_returncode'] == 0, 'actual native PASS ' + profile)
    require(source['inputs_unchanged'] is True and source['assets_unchanged'] is True and
            source['inputs_before'] == source['inputs_after'] and
            source['assets_before'] == source['assets_after'], 'private continuity')
    stdout = (directory / 'stdout.txt').read_text(encoding='utf-8')
    for name, records in source['receipts'].items():
        marker = 'WORKBENCH_OPEN_' + name.upper() + ' '
        actual = [unique(token.split('=', 1) for token in line[len(marker):].split())
                  for line in stdout.splitlines() if line.startswith(marker)]
        require(actual == records, 'actual native stdout ' + name)
    execution = load(directory / 'execution.json')
    require(execution['mode'] == 'workbench-open' and execution['event'] == 'result' and
            execution['exit_code'] == 0, 'retained native process exit')
    receipts = json.loads(json.dumps(source['receipts']))
    prepare = receipts['prepare'][0]
    require(set(prepare) == {'profile', 'returned', 'source-unchanged', 'world-ticks',
                            'stacks', 'entities', 'randoms', 'r', 'e', 'j'} and
            bool(prepare['r']) and prepare['e'] == prepare['j'], 'actual retained aliases')
    prepare['active-container-is-screen-container'] = prepare['e'] == prepare['j']
    for name in ('r', 'e', 'j'):
        del prepare[name]
    require(source['setup_receipts'] == source['receipts']['setup'] and
            source['prepare_receipts'] == source['receipts']['prepare'] and
            source['busy_receipts'] == source['receipts']['busy'], 'private duplicate receipts agree')
    require(source['native_prepare_disk_unchanged'] is True and
            source['stopped_disk'] == source['prepared_disk'] and
            all(v == source['stopped_disk'] for v in source['phase_disks'].values()),
            'actual stopped selected save tree at every native phase')
    stopped = {name.replace('\\', '/'): value for name, value in source['stopped_disk'].items()}
    return {
        'profile': profile, 'status': 'PASS', 'ctest_seconds': seconds,
        'checks': int(receipts['finish'][0]['checks']), 'capture_label': directory.name,
        'private_artifacts': {name: {'bytes': (directory / name).stat().st_size,
                                   'sha256': digest(directory / name)}
                              for name in ('run.json', 'execution.json', 'stdout.txt', 'stderr.txt',
                                           'workbench-open-shutdown.png')},
        'receipts': receipts, 'receipts_digest': canonical(receipts),
        'selected_inputs_unchanged': True, 'resources_unchanged': True,
        'live_source_window_capture': source['live_ui_capture'],
        'stopped_save_files': stopped, 'stopped_save_digest': canonical(stopped),
        'unchanged_save_phases': sorted(source['phase_disks']),
    }


def validate(document, root=None):
    keys(document, 'schema status scope runtime_source_files runtime_source_count runtime_source_digest '
         'harness_source_files harness_source_count harness_source_digest selected_execution_inputs '
         'imported_resources cohort authored_scene assertions profiles failed_cohorts_preserved '
         'intervening_focused_cohort preceding_sdk_cohort limits', 'document')
    require(type(document['schema']) is int and document['schema'] == 1 and
            document['status'] == 'PASS', 'schema and status')
    public_wire = json.dumps(document)
    require(re.search(r'[A-Za-z]:[\\/]|\\\\Users\\\\|/home/|file://', public_wire) is None,
            'no private absolute paths')
    for group, count in (('runtime', 1450), ('harness', 526)):
        files = document[group + '_source_files']
        manifest(files, count, group)
        require(document[group + '_source_count'] == count and
                document[group + '_source_digest'] == canonical(files), group + ' manifest digest')
    runtime = document['runtime_source_files']
    harness = document['harness_source_files']
    require(not set(runtime).intersection(harness) and
            all(p.startswith(('src/', 'include/')) for p in runtime) and
            all(p == 'CMakeLists.txt' or p.startswith(('cmake/', 'tests/')) for p in harness),
            'binding roles')
    execution = document['selected_execution_inputs']
    keys(execution, 'count source_count binaries digest', 'execution inputs')
    manifest(execution['binaries'], 2, 'native binaries')
    require(set(execution['binaries']) == {'mc_test_workbench_open_shutdown.exe', 'OpenAL32.dll'} and
            execution['count'] == 1978 and execution['source_count'] == 1976 and
            execution['digest'] == canonical({**runtime, **harness, **{
                'build-windows/' + name: value for name, value in execution['binaries'].items()}}),
            'complete selected input manifest')
    resources = document['imported_resources']
    keys(resources, 'count digest', 'resources')
    require(resources['count'] == 413 and hash_string(resources['digest']), 'resource count/digest')
    cohort = document['cohort']
    keys(cohort, 'passed total_seconds private_log_sha256', 'cohort')
    require(cohort['passed'] == 4 and cohort['total_seconds'] == 30.28 and
            hash_string(cohort['private_log_sha256']), 'four-profile measured cohort')
    require(document['authored_scene'] == {
        'inventory': [[0, 270, 1, 0], [9, 17, 2, 0], [10, 5, 7, 0]],
        'table': [0, 64, 2, 58, 0], 'grid0': [17, 2, 0], 'cursor': [5, 7, 0],
        'result_preview': [5, 4, 0], 'source_inventory_repairs': False}, 'declared authored scene')
    require(len(document['profiles']) == 4, 'exact four profiles')
    for profile, record in zip(PROFILES, document['profiles']):
        keys(record, 'profile status ctest_seconds checks capture_label private_artifacts receipts '
             'receipts_digest selected_inputs_unchanged resources_unchanged live_source_window_capture '
             'stopped_save_files stopped_save_digest unchanged_save_phases', 'profile')
        require(record['profile'] == profile and record['status'] == 'PASS' and
                record['checks'] == CHECKS[profile] and record['ctest_seconds'] == SECONDS[profile],
                'exact profile order, checks and measured time')
        require(re.fullmatch('workbench-open-' + profile + '-[a-z0-9_]+', record['capture_label'])
                is not None, 'sanitized private capture label')
        artifacts = record['private_artifacts']
        require(set(artifacts) == {'run.json', 'execution.json', 'stdout.txt', 'stderr.txt',
                                   'workbench-open-shutdown.png'}, 'private artifact roles')
        for name, value in artifacts.items():
            keys(value, 'bytes sha256', 'private artifact')
            require(type(value['bytes']) is int and value['bytes'] >= 0 and
                    hash_string(value['sha256']), 'private artifact bytes/hash')
        require(artifacts['workbench-open-shutdown.png']['bytes'] > 1000 and
                record['live_source_window_capture'] is True and
                record['selected_inputs_unchanged'] is True and record['resources_unchanged'] is True,
                'actual capture and unchanged inputs')
        require(record['receipts'] == expected_receipts(profile, record['checks']) and
                record['receipts_digest'] == canonical(record['receipts']), 'strict sanitized source receipts')
        saved = record['stopped_save_files']
        require(len(saved) == SAVE_COUNTS[profile] and all(relative_name(n) and hash_string(h) for n, h in saved.items()) and
                'alpha/region/r.9.9.mcr' in saved and record['stopped_save_digest'] == canonical(saved),
                'stopped selected saves manifest')
        phases = ['open-after-native-close', 'open-after-prepare', 'open-after-release']
        if profile != 'clean':
            phases.append('open-container-busy' if profile == 'container' else 'open-audit-busy')
        require(record['unchanged_save_phases'] == sorted(phases), 'each actual native phase')
    failures = document['failed_cohorts_preserved']
    require(len(failures) == 3 and [v['classification'] for v in failures] ==
            ['harness-setup', 'harness-close-pump', 'native-live-owner-guard'], 'all retained failed cohorts')
    for failure in failures:
        keys(failure, 'classification status capture_label private_artifacts production_red reason '
             'source_unchanged stopped_save_tree_unchanged', 'historical failure')
        require(failure['status'] == 'FAIL' and
                failure['production_red'] is (failure['classification'] == 'native-live-owner-guard') and
                re.fullmatch('workbench-open-clean-[a-z0-9_]+', failure['capture_label']) is not None,
                'honest failure qualification')
        require(set(failure['private_artifacts']) == {'run.json', 'stdout.txt', 'stderr.txt'} and
                all(hash_string(h) for h in failure['private_artifacts'].values()), 'retained failure hashes')
    require(failures[0]['source_unchanged'] is None and failures[1]['source_unchanged'] is None and
            failures[2]['source_unchanged'] is True and
            [f['stopped_save_tree_unchanged'] for f in failures] == [None, None, True], 'failure evidence scope')
    focused = document['intervening_focused_cohort']
    keys(focused, 'status passed selected total_seconds private_log_sha256 cases failed_sdk_profiles',
         'intervening focused cohort')
    require(focused['status'] == 'FAIL' and focused['passed'] == 5 and focused['selected'] == 7 and
            focused['total_seconds'] == 131.58 and hash_string(focused['private_log_sha256']) and
            focused['cases'] == FOCUSED_CASES, 'separate actual 5/7 focused result')
    require(len(focused['failed_sdk_profiles']) == 2, 'both intervening SDK failures retained')
    for profile, failure in zip(('clean', 'player'), focused['failed_sdk_profiles']):
        keys(failure, 'profile status capture_label private_artifacts setup_complete '
             'terminal_prepare_reached observation', 'intervening SDK failure')
        require(failure['profile'] == profile and failure['status'] == 'FAIL' and
                failure['setup_complete'] is False and failure['terminal_prepare_reached'] is False and
                re.fullmatch('workbench-open-' + profile + '-[a-z0-9_]+', failure['capture_label']) is not None,
                'SDK failure during setup, before terminal preparation')
        require(set(failure['private_artifacts']) == {'run.json', 'stdout.txt', 'stderr.txt'} and
                all(hash_string(h) for h in failure['private_artifacts'].values()),
                'intervening private failed artifacts')
    require(focused['failed_sdk_profiles'][0]['observation'] ==
            'Source-call-count assertion during SDK setup, before terminal preparation; root cause unconfirmed.' and
            focused['failed_sdk_profiles'][1]['observation'] ==
            'Pause focus-recovery limit during SDK setup, before terminal preparation.', 'bounded observed failure causes')
    preceding = document['preceding_sdk_cohort']
    keys(preceding, 'status passed selected total_seconds private_log_sha256 cases failed_sdk_profiles',
         'preceding SDK cohort')
    require(preceding['status'] == 'FAIL' and preceding['passed'] == 3 and preceding['selected'] == 4 and
            preceding['total_seconds'] == 69.32 and hash_string(preceding['private_log_sha256']) and
            preceding['cases'] == PRECEDING_CASES, 'separate actual 3/4 SDK result')
    require(len(preceding['failed_sdk_profiles']) == 1, 'preceding failed clean profile retained')
    failure = preceding['failed_sdk_profiles'][0]
    keys(failure, 'profile status capture_label private_artifacts setup_complete terminal_prepare_reached '
         'selected_inputs_unchanged resources_unchanged observation', 'preceding SDK failure')
    require(failure['profile'] == 'clean' and failure['status'] == 'FAIL' and
            failure['setup_complete'] is False and failure['terminal_prepare_reached'] is False and
            failure['selected_inputs_unchanged'] is True and failure['resources_unchanged'] is True and
            failure['observation'] == PRECEDING_OBSERVATION and
            re.fullmatch('workbench-open-clean-[a-z0-9_]+', failure['capture_label']) is not None,
            'bounded preceding setup failure observation')
    require(set(failure['private_artifacts']) == {'run.json', 'stdout.txt', 'stderr.txt', 'execution.json'} and
            all(hash_string(h) for h in failure['private_artifacts'].values()),
            'preceding actual private failure artifact hashes')
    require(isinstance(document['assertions'], list) and len(document['assertions']) == 8 and
            isinstance(document['limits'], list) and len(document['limits']) == 8 and
            all(isinstance(v, str) and v for v in document['assertions'] + document['limits']), 'bounded prose')
    # The schema exposes only a typed projection; no address or private path fields are accepted.
    if root is not None:
        root = Path(root)
        actual = {p.relative_to(root).as_posix() for folder in ('src', 'include')
                  for p in (root / folder).rglob('*') if p.is_file() and p.suffix in SUFFIXES}
        require(actual == set(runtime), 'all current runtime sources bound')
        for name, expected in {**runtime, **harness}.items():
            path = root / name
            require(path.is_file() and path.stat().st_size == expected['bytes'] and
                    digest(path) == expected['sha256'], 'unchanged selected source ' + name)
    return {'profiles': 4, 'runtime_sources': 1450, 'harness_sources': 526,
            'selected_inputs': 1978, 'resources': 413, 'native_checks': sum(CHECKS.values()),
            'historical_failures': 3, 'intervening_failed_sdk_profiles': 2,
            'preceding_failed_sdk_profiles': 1,
            'source_shutdown_or_pixel_comparisons': 0}


def audit_public():
    return validate(load(HERE / 'native-validation.json'), ROOT)


def audit_private(private_root):
    """Recheck the published projection against retained private reports, without copying them."""
    document = load(HERE / 'native-validation.json')
    validate(document)
    root = Path(private_root)
    for record in document['profiles']:
        matches = list(root.glob('workbench-open-shutdown*captures/' + record['capture_label'] + '/run.json'))
        require(len(matches) == 1, 'one retained private capture')
        directory = matches[0].parent
        require(project_profile(directory, record['ctest_seconds']) == record,
                'actual private report projection ' + record['profile'])
        source = load(directory / 'run.json')
        files = {**document['runtime_source_files'], **document['harness_source_files'], **{
            'build-windows/' + k: v for k, v in document['selected_execution_inputs']['binaries'].items()}}
        relative = {Path(p).relative_to(ROOT).as_posix(): h for p, h in source['inputs_before'].items()}
        require(relative == {k: v['sha256'] for k, v in files.items()}, 'captured selected binding map')
        assets = {p.replace('\\', '/'): h for p, h in source['assets_before'].items()}
        require(len(assets) == 413 and canonical(assets) == document['imported_resources']['digest'],
                'actual imported-resource digest')
    all_failures = (document['failed_cohorts_preserved'] +
                    document['intervening_focused_cohort']['failed_sdk_profiles'] +
                    document['preceding_sdk_cohort']['failed_sdk_profiles'])
    for failure in all_failures:
        matches = list(root.glob('workbench-open-shutdown*captures/' + failure['capture_label'] + '/run.json'))
        require(len(matches) == 1, 'one retained failed capture')
        directory = matches[0].parent
        require(load(matches[0])['status'] == 'FAIL' and all(
            digest(directory / name) == sha for name, sha in failure['private_artifacts'].items()),
            'preserved actual failed artifacts')
        if 'setup_complete' in failure:
            source = load(matches[0])
            require(not source['setup_receipts'] and not source['prepare_receipts'] and
                    source['native_returncode'] != 0, 'actual SDK setup failure before terminal preparation')
        if 'selected_inputs_unchanged' in failure:
            source = load(matches[0])
            require(source['inputs_unchanged'] is True and source['assets_unchanged'] is True and
                    source['inputs_before'] == source['inputs_after'] and
                    source['assets_before'] == source['assets_after'], 'preceding failed input/resource continuity')
    return {'private_profiles': 4, 'private_failed_cohorts': 3, 'intervening_failed_sdk_profiles': 2,
            'preceding_failed_sdk_profiles': 1}


def audit_cohort_log(path):
    document = load(HERE / 'native-validation.json')
    require(digest(path) == document['cohort']['private_log_sha256'], 'actual retained cohort log')
    raw = Path(path).read_bytes()
    wire = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    for profile, seconds in SECONDS.items():
        require(re.search(r'client\.workbench_open_shutdown_' + profile +
                          r'\s+\.+\s+Passed\s+' + re.escape(f'{seconds:.2f}') + r' sec', wire),
                'actual CTest profile time ' + profile)
    require('100% tests passed, 0 tests failed out of 4' in wire and
            re.search(r'Total Test time \(real\) =\s+30\.28 sec', wire), 'actual four-profile total')
    return {'passed': 4, 'seconds': 30.28}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-root', type=Path)
    parser.add_argument('--cohort-log', type=Path)
    args = parser.parse_args()
    print(audit_public())
    if args.private_root:
        print(audit_private(args.private_root))
    if args.cohort_log:
        print(audit_cohort_log(args.cohort_log))
