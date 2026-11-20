"""Require native completion and compare the retained original CH component rows."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
from capture_reference import JAR, SOURCES, SCOPE, rows
from capture import capture, digest, ensure, require_success


def unique(pairs):
    value = {}
    for key, item in pairs:
        ensure(key not in value, 'Duplicate Death screen JSON key')
        value[key] = item
    return value


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def fingerprints(value, names):
    return type(value) is dict and set(value) == set(names) and all(
        type(item) is str and re.fullmatch('[0-9a-f]{64}', item) is not None for item in value.values())


def validate_provenance(value, finite_hash):
    keys = {'schema', 'archive_sha256', 'scope', 'rows', 'source_sha256', 'finite_sha256',
            'tool_sha256', 'helper_sha256', 'original', 'input_unchanged', 'helper_unchanged'}
    ensure(type(value) is dict and set(value) == keys, 'Death screen provenance schema')
    ensure(value['schema'] == 'DEATH_SCREEN_CAPTURE_V1' and value['archive_sha256'] == JAR and
           value['scope'] == SCOPE and type(value['rows']) is int and value['rows'] == 72 and
           value['finite_sha256'] == finite_hash and value['input_unchanged'] is True and
           value['helper_unchanged'] is True, 'Death screen provenance declarations')
    ensure(value['source_sha256'] == {name: digest(HERE / name) for name in SOURCES},
           'Death screen original observation source pins')
    ensure(fingerprints(value['tool_sha256'], ('java.exe', 'javac.exe')) and
           fingerprints(value['helper_sha256'], ('DeathScreenObservation.class', 'DeathScreenObservation$1.class')),
           'Death screen helper/tool fingerprints')
    ensure(type(value['original']) is list and len(value['original']) == 2, 'Death screen VM extent')
    receipt_keys = {'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete', 'reader_errors',
                    'output_overflow', 'stdout_file', 'stderr_file', 'stdout_bytes', 'stderr_bytes',
                    'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit'}
    for index, receipt in enumerate(value['original']):
        ensure(type(receipt) is dict and set(receipt) == receipt_keys and type(receipt['exit']) is int and
               receipt['exit'] == 0 and receipt['timeout_seconds'] == 45 and receipt['timed_out'] is False and
               receipt['pipe_capture_complete'] is True and receipt['reader_errors'] == {} and
               receipt['output_overflow'] is False and receipt['stdout_limit'] == 4194304 and
               receipt['stderr_limit'] == 1048576, 'Death screen bounded complete VM receipt')
        for stream in ('stdout', 'stderr'):
            ensure(receipt[stream + '_file'] == 'original-' + str(index) + '.' + stream and
                   type(receipt[stream + '_bytes']) is int and
                   0 <= receipt[stream + '_bytes'] <= receipt[stream + '_limit'] and
                   (stream != 'stdout' or receipt[stream + '_bytes'] > 0) and
                   type(receipt[stream + '_sha256']) is str and
                   re.fullmatch('[0-9a-f]{64}', receipt[stream + '_sha256']) is not None,
                   'Death screen original stream extent/hash')


def finite():
    value = read_json(HERE / 'finite.json')
    validate_provenance(read_json(HERE / 'provenance.json'), digest(HERE / 'finite.json'))
    ensure(type(value) is dict and set(value) == {'rows'} and type(value['rows']) is list and
           all(type(line) is str for line in value['rows']), 'Death screen finite schema')
    expected = rows('\n'.join(value['rows']))
    ensure(expected == value['rows'], 'Death screen exact finite ordering')
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    expected = finite()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='death-screen-', dir=args.work_dir.resolve()))
    executable = args.executable.resolve(strict=True)
    helper = ROOT / 'tests/reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('death_screen_resources', helper)
    ensure(spec is not None and spec.loader is not None, 'Death screen resource fixture helper')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources, game = out / 'resources', out / 'game'
    support.resources_at(resources)
    game.mkdir()
    native_sources = [ROOT / name for name in (
        'tests/client/gui/test_death_screen.c', 'tests/client/statistics/test_player_dispatch.c',
        'src/client/gui/source_death_screen.c', 'src/client/gui/source_death_screen.h',
        'src/client/gui/source_death_screen_internal.h', 'src/client/gui/source_screen.c',
        'src/client/gui/source_screen_initialize.c', 'src/client/gui/source_screen_events.c')]
    inputs = [executable, HERE / 'finite.json', HERE / 'provenance.json', HERE / 'run_native.py',
              *[HERE / name for name in SOURCES], helper, *native_sources]
    before = {str(path.resolve()): digest(path) for path in inputs}
    resource_before = {path.relative_to(resources).as_posix(): digest(path) for path in resources.rglob('*') if path.is_file()}
    report = {'status': 'FAIL', 'input_before_sha256': before, 'resource_before_sha256': resource_before,
              'scope': SCOPE + ' Native fixture uses registered production App storage and genuine Session/World/Player owners, without complete App construction; native ownership controls remain separate.'}
    try:
        receipt = capture([str(executable), str(resources.resolve()), str(game.resolve())], out, 'native', 120)
        require_success(receipt, empty_stderr=True)
        stdout = (out / receipt['stdout_file']).read_text(encoding='utf-8')
        actual = rows(stdout)
        lines = stdout.splitlines()
        ensure(lines[:2] == ['16 achievements', '151 recipes'] and len(lines) == 75,
               'Death screen exact native output extent')
        checks = re.fullmatch(r'Death screen source: ([1-9][0-9]*) checks', lines[-1])
        ensure(checks is not None and actual == expected, 'Death screen native completion/original rows')
        report.update(status='PASS', native=receipt, checks=int(checks[1]), rows=72)
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        report['resource_after_sha256'] = {path.relative_to(resources).as_posix(): digest(path)
                                            for path in resources.rglob('*') if path.is_file()}
        unchanged = before == report['input_after_sha256'] and resource_before == report['resource_after_sha256']
        if not unchanged:
            report['status'] = 'FAIL'
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(unchanged, 'Death screen native inputs/resources changed')
    print('Death screen: 72 exact original rows and ' + str(report['checks']) + ' native controls; ' + str(out / 'report.json'))


if __name__ == '__main__':
    main()
