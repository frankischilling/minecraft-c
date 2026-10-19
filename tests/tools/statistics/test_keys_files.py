"""Exercise native statistic-key file loading with synthetic resource bytes."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def require(ok, message):
    if not ok:
        raise ValueError(message)


def inspect(raw, values):
    require(raw.endswith(b'\n') and b'\r' not in raw and len(raw) < 1024 * 1024, 'bounded LF output')
    rows = [line.split('\t') for line in raw[:-1].decode('ascii').split('\n')]
    require(rows[:5] == [['KEY_MAP_V1'], ['INITIALIZATION', 'COMPLETE'],
            ['ORDER', 'SIGNED_INT_ASCENDING'], ['MAP', str(len(values))], ['SINGLETON', '1']],
            'actual singleton/map completion')
    position = 5
    entries = rows[position:position + len(values)]
    position += len(entries)
    queries = rows[position:position + len(values) + 7]
    position += len(queries)
    require(rows[position][0] == 'STRINGS' and len(rows[position]) == 2, 'String table')
    string_count = int(rows[position][1])
    position += 1
    strings = {}
    for index in range(1, string_count + 1):
        row = rows[position]
        position += 1
        require(len(row) == 4 and row[:2] == ['STRING', str(index)], 'String identity')
        length = int(row[2])
        require(row[3] == '-' if length == 0 else len(row[3]) == length * 4, 'UTF16 length')
        strings['R:' + str(index)] = row[3]
    require(rows[position:] == [['SUMMARY', 'PASS'], ['END', str(len(values)),
            str(len(queries)), str(string_count), str(position + 1)]], 'complete footer')
    map_refs = {}
    for row, key in zip(entries, sorted(values)):
        require(len(row) == 3 and row[:2] == ['ENTRY', str(key)] and row[2] in strings,
                'complete sorted actual entries')
        expected = values[key].encode('utf-16-be').hex() or '-'
        require(strings[row[2]] == expected, 'actual key value')
        map_refs[key] = row[2]
    query_keys = sorted(values) + [-2147483648, 2147483647, 0, -1, 1, 32767, -32768]
    for row, key in zip(queries, query_keys):
        require(row == ['QUERY', str(key), map_refs.get(key, 'NULL')], 'lookup exact String identity')
    if 2 in values and 3 in values:
        require(map_refs[2] != map_refs[3], 'equal nonempty values retain distinct identities')
    if 4 in values and 5 in values:
        require(map_refs[4] == map_refs[5], 'empty substrings share literal identity')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='mc-statistic-keys-') as directory:
        root = Path(directory) / 'assets-\u00e9-\u6f22-\U0001f33f'
        (root / 'achievement').mkdir(parents=True)
        path = root / 'achievement/map.txt'
        path.write_bytes(b'1,old\r\n2,same\n3,same\n4,,x\n5,,x\n-2,negative,ignored\n'
                         b'1,new\n+2147483647,max\n-2147483648,min\n-0,zero')
        expected = {-2147483648: 'min', -2: 'negative', 0: 'zero', 1: 'new', 2: 'same',
                    3: 'same', 4: '', 5: '', 2147483647: 'max'}

        def run(side):
            return subprocess.run([str(probe), side, '--root-hex', str(root).encode('utf-8').hex()],
                                  stdin=subprocess.DEVNULL, capture_output=True, timeout=20)

        for side in ('client', 'server'):
            result = run(side)
            require(result.returncode == 0 and not result.stderr, 'native key loading')
            inspect(result.stdout, expected)
        path.write_bytes(b'')
        for side in ('client', 'server'):
            result = run(side)
            require(result.returncode == 0 and not result.stderr, 'empty resource still closes normally')
            inspect(result.stdout, {})
        for malformed in (b'1,retained\n2147483648,overflow\n', b'1,retained\n2,\n'):
            path.write_bytes(malformed)
            for side in ('client', 'server'):
                result = run(side)
                require(result.returncode != 0 and not result.stdout and b'pc=91' in result.stderr,
                        'caught source exception reaches unfinished print91')
        path.unlink()
        for side in ('client', 'server'):
            result = run(side)
            require(result.returncode != 0 and not result.stdout and b'pc=91' in result.stderr,
                    'null InputStream is caught before unfinished print91')
    print('PASS: imported key files, native Unicode paths, exact values/identities and failure boundaries')


if __name__ == '__main__':
    main()
