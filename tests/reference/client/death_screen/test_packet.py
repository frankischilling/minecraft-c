"""Exercise original Death screen packet admission against authored corruptions."""
import copy
import hashlib
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from run_native import finite, read_json, rows, validate_provenance, digest

checks = 0


def reject(call):
    global checks
    try:
        call()
    except (AssertionError, ValueError, TypeError, KeyError, IndexError):
        checks += 1
        return
    raise AssertionError('Corrupted Death screen packet accepted')


def main():
    global checks
    expected = finite()
    checks += 1
    for damaged in (expected[:-1], expected + expected[:1], list(reversed(expected)),
                    [*expected[:1], expected[2], expected[1], *expected[3:]],
                    [expected[0].replace('DEATH ', 'DEATHX '), *expected[1:]],
                    [*expected[:-1], expected[-1] + ' trailing']):
        reject(lambda value=damaged: rows('\n'.join(value)))
    base = read_json(HERE / 'provenance.json')
    modifications = [
        (('schema',), 'other'), (('archive_sha256',), '0' * 64), (('scope',), 'whole gameplay'),
        (('rows',), True), (('rows',), 71), (('input_unchanged',), False), (('helper_unchanged',), False),
        (('finite_sha256',), 'f' * 64), (('source_sha256', 'DeathScreenObservation.java'), '0' * 64),
        (('tool_sha256', 'java.exe'), 'invalid'), (('helper_sha256', 'DeathScreenObservation.class'), ''),
        (('original', 0, 'exit'), 1), (('original', 0, 'exit'), False),
        (('original', 0, 'timed_out'), True), (('original', 0, 'pipe_capture_complete'), False),
        (('original', 0, 'reader_errors'), {'stdout': ['missing']}),
        (('original', 0, 'output_overflow'), True), (('original', 0, 'timeout_seconds'), 46),
        (('original', 0, 'stdout_file'), '../other'), (('original', 1, 'stdout_bytes'), 4194305),
        (('original', 1, 'stdout_sha256'), 'invalid'), (('original', 1, 'stderr_limit'), 0),
    ]
    for path, replacement in modifications:
        changed = copy.deepcopy(base)
        target = changed
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = replacement
        reject(lambda value=changed: validate_provenance(value, digest(HERE / 'finite.json')))
    changed = copy.deepcopy(base)
    changed['original'] = changed['original'][:1]
    reject(lambda: validate_provenance(changed, digest(HERE / 'finite.json')))
    changed = copy.deepcopy(base)
    changed['extra'] = True
    reject(lambda: validate_provenance(changed, digest(HERE / 'finite.json')))
    changed_finite = read_json(HERE / 'finite.json')
    changed_finite['rows'][0] = changed_finite['rows'][0].replace(' 0 2 0', ' 1 2 0')
    encoded = (json.dumps(changed_finite, indent=2) + '\n').encode('utf-8')
    reject(lambda: validate_provenance(base, hashlib.sha256(encoded).hexdigest()))
    print('Death screen packet: ' + str(checks) + ' admission/corruption controls; 72 original rows')


if __name__ == '__main__':
    main()
