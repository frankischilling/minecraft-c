"""Exercise imported-directory loading with synthetic byte resources only."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def require(ok, message):
    if not ok:
        raise ValueError(message)


def encoded(text):
    return text.encode('utf-16-be', errors='surrogatepass').hex() or '-'


def inspect(raw, expected):
    require(raw.endswith(b'\n') and b'\r' not in raw and len(raw) <= 1024 * 1024, 'bounded LF output')
    rows = [line.split('\t') for line in raw[:-1].decode('ascii').split('\n')]
    require(rows[:3] == [['TRANSLATION_PROPERTIES_V1'], ['MAP', str(len(expected))], ['SINGLETON', '1']],
            'map and singleton publication')
    cursor = 3
    entries = rows[cursor:cursor + len(expected)]
    cursor += len(entries)
    queries = rows[cursor:cursor + 5]
    cursor += 5
    require(rows[cursor][0] == 'STRINGS' and len(rows[cursor]) == 2, 'String count row')
    count = int(rows[cursor][1])
    cursor += 1
    strings = {}
    for index in range(1, count + 1):
        row = rows[cursor]
        cursor += 1
        require(len(row) == 4 and row[:2] == ['STRING', str(index)], 'complete String rows')
        length = int(row[2])
        require((length == 0 and row[3] == '-') or
                (len(row[3]) == 4 * length and re.fullmatch('[0-9a-f]+', row[3])), 'UTF16 content')
        strings[index] = row[3]
    require(rows[cursor:] == [['END', str(len(expected)), str(count), str(cursor)]], 'complete footer')
    actual = {}
    map_refs = {}
    references = []
    for row in entries:
        require(len(row) == 3 and row[0] == 'ENTRY', 'entry row')
        key, value = int(row[1]), int(row[2])
        require(key in strings and value in strings and key != value, 'map String identities')
        require(strings[key] not in actual, 'unique actual keys')
        actual[strings[key]] = strings[value]
        map_refs[strings[key]] = value
        references += [key, value]
    require(len(set(references)) == len(references), 'distinct loaded key/value String owners')
    require(actual == {encoded(key): encoded(value) for key, value in expected.items()}, 'complete decoded map')
    require(list(actual) == sorted(actual, key=lambda key: b'' if key == '-' else bytes.fromhex(key)),
            'unsigned UTF16 diagnostic order')
    for row, key in zip(queries, ['gui.done', 'tile.stone.name', 'stat.startGame',
                                  'minecraft-c.absent.translation.key', '']):
        require(len(row) == 5 and row[:3] == ['QUERY', str(len(key)), encoded(key)], 'independent query')
        reference = int(row[3])
        present = key in expected
        require(reference in strings and strings[reference] == encoded(expected[key] if present else key),
                'actual wrapper result')
        require(row[4] == ('0' if present else '1'), 'same-input fallback identity')
        require((present and reference == map_refs[encoded(key)]) or
                (not present and reference not in references), 'lookup preserves exact result identity')


def run(probe, side, root):
    command = [str(probe), side, '--root-hex', str(root).encode('utf-8').hex()]
    return subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=20, check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='mc-resources-') as directory:
        root = Path(directory) / 'assets-\u00e9-\u6f22-\U0001f33f'
        (root / 'lang').mkdir(parents=True)
        first = b'gui.done=First\r\ntile.stone.name=Stone\\u0021\rstat.startGame=Old\n=blank\nraw=\xe9\xff\x00\n'
        second = b'gui.done=Second\nstat.startGame=Started\nempty=\nutf=\\ud800\\u0000\\ud83d\\ude00\n'
        (root / 'lang/en_US.lang').write_bytes(first)
        (root / 'lang/stats_US.lang').write_bytes(second)
        expected = {'gui.done': 'Second', 'tile.stone.name': 'Stone!', 'stat.startGame': 'Started',
                    '': 'blank', 'raw': '\u00e9\u00ff\x00', 'empty': '', 'utf': '\ud800\x00\ud83d\ude00'}
        for side in ('client', 'server'):
            for path in (str(root), str(root) + '/'):
                result = run(probe, side, path)
                require(result.returncode == 0 and not result.stderr, 'native imported-directory execution')
                inspect(result.stdout, expected)
        (root / 'lang/stats_US.lang').unlink()
        for side in ('client', 'server'):
            result = run(probe, side, root)
            require(result.returncode != 0 and not result.stdout and b'pc=40' in result.stderr,
                    'missing second file reaches actual load40')
        (root / 'lang/en_US.lang').unlink()
        for side in ('client', 'server'):
            result = run(probe, side, root)
            require(result.returncode != 0 and not result.stdout and b'pc=26' in result.stderr,
                    'missing first file reaches actual load26')
    print('PASS: UTF8 imported-directory paths, binary bytes, translation identity and missing-file load boundaries')


if __name__ == '__main__':
    main()
