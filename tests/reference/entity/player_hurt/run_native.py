"""Replay actual Player hurt against retained unchanged-original observations."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import JAR, SOURCES, digest, ensure, equal, profiles, read_json, rows
from capture import capture, require_success

# These objects/fields originate in independent real constructors. Their raw
# values are retained, and each call must preserve its own incoming value.
# Entity IDs identify separately allocated actors, not cross-process identity.
INDEPENDENT_PLAYER = ('sn.aD', 'ls.F', 'ls.G', 'ls.aj')
INDEPENDENT_WORLD = ('fd.I', 'fd.Q', 'fd.g')


def validate_provenance(provenance, finite_digest):
    ensure(type(provenance) is dict and set(provenance) == {'schema', 'status', 'archive_sha256',
           'finite_sha256', 'source_sha256', 'tool_sha256', 'helper_sha256', 'original',
           'input_unchanged', 'helper_unchanged', 'rows_per_vm', 'profiles_per_vm',
           'independent_player_fields', 'independent_world_fields'}, 'Player provenance schema')
    ensure(provenance['schema'] == 'PLAYER_HURT_CAPTURE_V1' and provenance['status'] == 'PASS' and
           provenance['archive_sha256'] == JAR and provenance['finite_sha256'] == finite_digest and
           provenance['input_unchanged'] is True and provenance['helper_unchanged'] is True and
           type(provenance['rows_per_vm']) is int and provenance['rows_per_vm'] == 28 and
           type(provenance['profiles_per_vm']) is int and provenance['profiles_per_vm'] == 22 and
           provenance['independent_player_fields'] == list(INDEPENDENT_PLAYER) and
           provenance['independent_world_fields'] == list(INDEPENDENT_WORLD), 'Player provenance declarations')
    ensure(provenance['source_sha256'] == {name: digest(HERE / name) for name in SOURCES}, 'Player provenance source pins')
    def fingerprints(value, names):
        return type(value) is dict and set(value) == set(names) and all(
            type(x) is str and re.fullmatch('[0-9a-f]{64}', x) is not None for x in value.values())
    ensure(fingerprints(provenance['tool_sha256'], ('java.exe', 'javac.exe')) and
           fingerprints(provenance['helper_sha256'], ('ConstructorJson.class', 'PlayerHurtObservation.class',
           'PlayerHurtObservation$Guard.class', 'PlayerHurtObservation$Observer.class')), 'Player helper/tool fingerprints')
    ensure(type(provenance['original']) is list and len(provenance['original']) == 2, 'Player original VM receipt count')
    keys = {'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete', 'reader_errors', 'output_overflow',
            'stdout_file', 'stderr_file', 'stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256',
            'stdout_limit', 'stderr_limit'}
    for index, receipt in enumerate(provenance['original']):
        ensure(type(receipt) is dict and set(receipt) == keys and type(receipt['exit']) is int and receipt['exit'] == 0 and
               receipt['timed_out'] is False and receipt['pipe_capture_complete'] is True and
               receipt['reader_errors'] == {} and receipt['output_overflow'] is False and
               receipt['timeout_seconds'] == 90 and receipt['stdout_limit'] == 4194304 and
               receipt['stderr_limit'] == 1048576, 'Player complete bounded original capture')
        for stream in ('stdout', 'stderr'):
            ensure(receipt[stream + '_file'] == 'original-' + str(index) + '.' + stream and
                   type(receipt[stream + '_bytes']) is int and
                   0 < receipt[stream + '_bytes'] <= receipt[stream + '_limit'] and
                   type(receipt[stream + '_sha256']) is str and
                   re.fullmatch('[0-9a-f]{64}', receipt[stream + '_sha256']) is not None, 'Player original capture extent/hash')


def finite():
    value = read_json(HERE / 'finite.json')
    provenance = read_json(HERE / 'provenance.json')
    validate_provenance(provenance, digest(HERE / 'finite.json'))
    ensure(type(value) is dict and set(value) == {'rows', 'source_sha256'} and
           value['source_sha256'] == {name: digest(HERE / name) for name in SOURCES}, 'Player capture input pins')
    return rows('\n'.join('PLAYER_HURT ' + json.dumps(x) for x in value['rows']))


def stable_fields(row):
    for key in INDEPENDENT_PLAYER:
        ensure(equal(row['before']['fields'][key], row['state']['fields'][key]), 'Player independent field changed: ' + key)
    for key in INDEPENDENT_WORLD:
        ensure(equal(row['before']['world']['fields'][key], row['state']['world']['fields'][key]),
               'Player independent World field changed: ' + key)


def compare(actual, expected):
    ensure(len(actual) == len(expected) == 28, 'Player compared call count')
    comparisons = 0
    def visit(a, b, path):
        nonlocal comparisons
        ensure(type(a) is type(b), 'Player type differs: ' + path)
        if isinstance(a, dict):
            ensure(a.keys() == b.keys(), 'Player keys differ: ' + path)
            for key in a:
                if (path in ('/before/fields', '/state/fields') and key in INDEPENDENT_PLAYER or
                    path in ('/before/world/fields', '/state/world/fields') and key in INDEPENDENT_WORLD):
                    continue
                visit(a[key], b[key], path + '/' + key)
        elif isinstance(a, list):
            ensure(len(a) == len(b), 'Player length differs: ' + path)
            for i, (x, y) in enumerate(zip(a, b)):
                visit(x, y, path + '/' + str(i))
        else:
            comparisons += 1
            ensure(a == b, 'Player value differs: ' + path + ': native=' + repr(a) + '; original=' + repr(b))
    for a, b in zip(actual, expected):
        stable_fields(a); stable_fields(b)
        if b['profile'] != 21:
            ensure(a['completion'] is None and b['completion'] is None, 'Player ordinary completion scope')
        # Actual original callback throws IllegalStateException. Native World
        # observer reports IO. Both exact reached snapshots are compared.
        if b['profile'] == 18:
            ensure(a['exception'] == 'IO' and b['exception'] == 'CALLBACK', 'Player callback failure categories')
        # Original complete death is retained separately. The native comparison
        # is the fatal sound checkpoint before unimplemented GS death entry.
        if b['profile'] == 21:
            ensure(a['exception'] == 'DEPENDENCY' and a['result'] is None and a['completion'] is None and
                   b['exception'] == 'NONE' and b['result'] is True and b['completion'] is not None,
                   'Player fatal prefix/completion scopes')
        for key in a:
            if b['profile'] == 18 and key == 'exception' or b['profile'] == 21 and key in ('exception', 'result', 'completion'):
                continue
            visit(a[key], b[key], '/' + key)
    return comparisons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='player-hurt-', dir=args.work_dir.resolve()))
    spec = importlib.util.spec_from_file_location('player_hurt_resources', HERE.parents[1] / 'client/player_statistics/support.py')
    support = importlib.util.module_from_spec(spec); spec.loader.exec_module(support)
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources); game.mkdir()
    native = output / 'native.jsonl'
    inputs = [executable, *[HERE / name for name in SOURCES], HERE / 'run_native.py', HERE / 'finite.json']
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': '22 profiles/28 original calls; full pre/post scalar state, inventory, statistics, sounds, aliases and Random. '
                       'Independent constructor fields are individually unchanged. Fatal sound prefix and callback failure categories remain separate.'}
    try:
        receipt = capture([str(executable), str(resources), str(game), str((HERE / 'profiles.tsv').resolve()), str(native)],
                          output, 'native', 120, os.environ.copy())
        report['native'] = receipt; require_success(receipt, empty_stderr=True)
        text = (output / receipt['stdout_file']).read_text()
        ensure(re.search(r'Player hurt: 28 rows; ([1-9][0-9]*) checks\n?$', text) is not None, 'Player native completion')
        lines = text.splitlines()
        ensure(lines[:2] == ['16 achievements', '151 recipes'] and len(lines) == 53 and
               sum(x.startswith('PLAYER_HURT_CONSTRUCTOR ') for x in lines) == 22 and
               sum(x.startswith('PLAYER_HURT_NATIVE ') for x in lines) == 28, 'Player native receipt inventory')
        receipts = [json.loads(x[len('PLAYER_HURT_NATIVE '):]) for x in lines if x.startswith('PLAYER_HURT_NATIVE ')]
        ensure([x['case'] for x in receipts] == list(range(28)) and all(
               x['holds_unchanged'] is True and x['world_unchanged'] is True and x['calls'] == 0 and x['leases'] == 0
               for x in receipts), 'Player native lifetime receipts')
        actual = rows('\n'.join('PLAYER_HURT ' + line for line in native.read_text().splitlines()))
        comparisons = compare(actual, finite())
        report.update(status='PASS', rows=28, scalar_comparisons=comparisons)
    finally:
        report['input_after_sha256'] = {str(p.resolve()): digest(p) for p in inputs}
        if report['input_after_sha256'] != before:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        ensure(before == report['input_after_sha256'], 'Player native inputs changed')
    print('Player hurt: 28 rows; ' + str(comparisons) + ' scalar comparisons; ' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
