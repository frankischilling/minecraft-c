"""Authored real filesystem checks, distinct from callback stream controls."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def require(value, message):
    if not value:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='mc-java-writer-') as directory:
        root = Path(directory)
        first = root / 'options.txt'
        unicode_path = root / 'options-\u00e9-\U0001f680.txt'
        abandoned = root / 'abandoned.txt'
        missing = root / 'missing-parent' / 'options.txt'
        first.write_bytes(b'AUTHORED OLD CONTENT MUST BE TRUNCATED\n')
        abandoned.write_bytes(b'AUTHORED OLD CONTENT\n')
        result = subprocess.run([str(executable), *(str(path) for path in
                                                  (first, unicode_path, abandoned, missing))],
                                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=15, check=False)
        require(len(result.stdout) <= 65536 and len(result.stderr) <= 65536, 'bounded actual native streams')
        require(result.returncode == 0, 'actual native file test failed: ' + repr(result.stderr))
        require(not result.stderr and re.fullmatch(rb'PASS: [1-9][0-9]* real FileWriter interface checks\r?\n',
                                                   result.stdout), 'complete actual native terminal')
        require(first.read_bytes() == b'skin:\xe2\x82\xac\xf0\x9f\x98\x80?\r\n', 'UTF8/CRLF exact file bytes')
        require(unicode_path.read_bytes() == b'key:\xe9\x80??\n', 'Unicode filename and windows1252/LF exact bytes')
        require(abandoned.read_bytes() == b'', 'native retirement did not invent source flush/final replacement')
        require(not missing.exists() and not missing.parent.exists(), 'FileWriter did not create parents')
        require({path.name for path in root.iterdir()} == {first.name, unicode_path.name, abandoned.name},
                'complete authored output membership')
        print(result.stdout.decode('ascii').strip())
        print('PASS: 5 independent real-file byte/ownership checks')


if __name__ == '__main__':
    main()
