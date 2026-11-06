"""Compare native Reader replay with complete recorded JDK observations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def fingerprint(path):
    raw = path.read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def rebound(rows):
    rows[-1][-1] = str(len(rows) - 1)
    return ('\n'.join('\t'.join(row) for row in rows) + '\n').encode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    directory = ROOT / 'tests/reference/java'
    fixture = directory / 'reader_observation.tsv'
    provenance = json.loads((directory / 'reader_provenance.json').read_text(encoding='utf-8'))
    require(provenance['schema'] == 'READER_OBSERVATION_PROVENANCE_V1', 'provenance schema')
    require(provenance['inputs_unchanged'] is True and fingerprint(fixture) == provenance['fixture'],
            'recorded observation binding')
    for path, expected in provenance['sources'].items():
        require(fingerprint(ROOT / path) == expected, 'executed source binding: ' + path)
    for mode in provenance['modes']:
        require(mode['trace'] == provenance['fixture'] and mode['exit_code'] == 0 and mode['reaped'],
                'actual mode completion')
    spec = importlib.util.spec_from_file_location('reader_protocol',
                                                ROOT / 'scripts/reference/java/reader_protocol.py')
    protocol = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(protocol)
    raw = fixture.read_bytes()
    projected = protocol.parse(raw)
    require(projected['counts'] == provenance['counts'], 'complete captured corpus')
    source_rows = protocol.rows(raw)
    malformed = [raw[:-1]]
    duplicated = [list(row) for row in source_rows]
    index = next(i for i, row in enumerate(duplicated) if row[0] == 'STRING')
    duplicated.insert(index, list(duplicated[index]))
    malformed.append(rebound(duplicated))
    wrong_exception = [list(row) for row in source_rows]
    index = next(i for i, row in enumerate(wrong_exception)
                 if row[0] == 'RESULT' and row[4] == 'THREW')
    wrong_exception[index][7] = '0000' + wrong_exception[index][7][4:]
    malformed.append(rebound(wrong_exception))
    for mutation in malformed:
        try:
            protocol.parse(mutation)
        except ValueError:
            pass
        else:
            raise ValueError('malformed observation accepted')
    # Changing only a valid observed String must not change declared input.
    changed = [list(row) for row in source_rows]
    index = next(i for i, row in enumerate(changed) if row[0] == 'STRING' and row[3] != '0')
    old = changed[index][4]
    changed[index][4] = ('0001' if old[:4] == '0000' else '0000') + old[4:]
    altered = protocol.parse(rebound(changed))
    require(altered['input'] == projected['input'] and altered['expected'] != projected['expected'],
            'observed content must not supply replay input')
    with tempfile.TemporaryDirectory(prefix='mc-reader-') as temporary:
        input_path = Path(temporary) / 'input.tsv'
        input_path.write_bytes(projected['input'])
        result = subprocess.run([str(probe), str(input_path)], stdin=subprocess.DEVNULL,
                                capture_output=True, timeout=30, check=False)
        require(result.returncode == 0 and not result.stderr, 'native Reader replay failed: '
                + result.stderr.decode('utf-8', errors='replace')[:500])
        if result.stdout != projected['expected']:
            actual = result.stdout.splitlines()
            expected = projected['expected'].splitlines()
            first = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                         min(len(actual), len(expected)))
            detail = f'actual={actual[first:first + 1]!r} expected={expected[first:first + 1]!r}'
            raise ValueError(f'native Reader mismatch at projected row {first + 1}: {detail}')
        require(result.stdout != altered['expected'], 'observed content mismatch was hidden')
        mutations = [projected['input'][:-1], projected['input'] + b'RESULT\t0\n']
        native_rows = projected['input'].splitlines(keepends=True)
        query = next(i for i, row in enumerate(native_rows) if row.startswith(b'QUERY\t'))
        mutations.append(b''.join(native_rows[:query] + [native_rows[query]] + native_rows[query:]))
        for mutation in mutations:
            input_path.write_bytes(mutation)
            result = subprocess.run([str(probe), str(input_path)], stdin=subprocess.DEVNULL,
                                    capture_output=True, timeout=20, check=False)
            diagnostic = b'reader probe: invalid input-only replay transport/declarations/footer'
            require(result.returncode == 1 and not result.stdout
                    and result.stderr in (diagnostic + b'\n', diagnostic + b'\r\n'),
                    'invalid transport did not exit with its deliberate rejection diagnostic')
    print(f"PASS: {projected['counts']['owners']} recorded JDK Reader owners, "
          f"{projected['counts']['actions']} actions, full String identities and stream effects; "
          '3 malformed observations, 1 content separation and 3 native transport guards')


if __name__ == '__main__':
    main()
