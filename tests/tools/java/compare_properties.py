"""Compare the native decoder with complete recorded JDK byte-stream outcomes."""
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


def rebound(lines):
    fields = lines[-1].split('\t')
    fields[-1] = str(len(lines) - 1)
    return ('\n'.join(lines[:-1] + ['\t'.join(fields)]) + '\n').encode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    directory = ROOT / 'tests/reference/java'
    fixture = directory / 'properties_observation.tsv'
    provenance = json.loads((directory / 'properties_provenance.json').read_text(encoding='utf-8'))
    require(provenance['schema'] == 'PROPERTIES_OBSERVATION_PROVENANCE_V1', 'provenance schema')
    require(provenance['inputs_unchanged'] is True and fingerprint(fixture) == provenance['fixture'],
            'recorded observation binding')
    for path, expected in provenance['sources'].items():
        require(fingerprint(ROOT / path) == expected, 'executed source binding: ' + path)
    for mode in provenance['modes']:
        require(mode['trace'] == provenance['fixture'] and mode['exit_code'] == 0 and mode['reaped'],
                'actual mode completion')
    source = ROOT / 'scripts/reference/java/properties_protocol.py'
    spec = importlib.util.spec_from_file_location('properties_protocol', source)
    protocol = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(protocol)
    raw = fixture.read_bytes()
    projected = protocol.parse(raw)
    require(projected['counts'] == provenance['counts'], 'complete captured corpus')
    malformed = [raw[:-1]]
    lines = raw[:-1].decode('ascii').split('\n')
    duplicate = list(lines)
    index = next(i for i, row in enumerate(duplicate) if row.startswith('ENTRY\t'))
    snapshot = index - 1
    require(duplicate[snapshot].startswith('SNAPSHOT\t'), 'first-entry guard setup')
    columns = duplicate[snapshot].split('\t')
    columns[-1] = str(int(columns[-1]) + 1)
    duplicate[snapshot] = '\t'.join(columns)
    duplicate.insert(index, duplicate[index])
    malformed.append(rebound(duplicate))
    missing = list(lines)
    index = next(i for i, row in enumerate(missing) if row.startswith('STRING\t'))
    del missing[index]
    malformed.append(rebound(missing))
    for mutation in malformed:
        try:
            protocol.parse(mutation)
        except ValueError:
            pass
        else:
            raise ValueError('malformed observation accepted')
    with tempfile.TemporaryDirectory(prefix='mc-properties-') as temporary:
        input_path = Path(temporary) / 'input.tsv'
        input_path.write_bytes(projected['input'])
        # This file contains CASE/INPUT/QUERY data only. Expected map and
        # String rows never become native probe inputs.
        result = subprocess.run([str(probe), str(input_path)], stdin=subprocess.DEVNULL,
                                capture_output=True, timeout=20, check=False)
        require(result.returncode == 0 and not result.stderr, 'native Properties observation failed')
        if result.stdout != projected['expected']:
            actual = result.stdout.splitlines()
            expected = projected['expected'].splitlines()
            first = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                         min(len(actual), len(expected)))
            raise ValueError(f'native Properties mismatch at projected row {first + 1}')
        mutations = [projected['input'][:-1], projected['input'] + b'EXTRA\n']
        native_rows = projected['input'].splitlines(keepends=True)
        query = next(i for i, row in enumerate(native_rows) if row.startswith(b'QUERY\t'))
        mutations.append(b''.join(native_rows[:query] + [native_rows[query]] + native_rows[query:]))
        for mutation in mutations:
            input_path.write_bytes(mutation)
            result = subprocess.run([str(probe), str(input_path)], stdin=subprocess.DEVNULL,
                                    capture_output=True, timeout=20, check=False)
            require(result.returncode != 0 and not result.stdout, 'invalid native transport accepted')
    print(f"PASS: {projected['counts']['loads']} recorded JDK Properties loads, "
          f"{projected['counts']['queries']} queries, full String identities; "
          '3 observation and 3 native transport guards')


if __name__ == '__main__':
    main()
