#!/usr/bin/env python3
"""Compare a finite native Block prefix with recorded original field reads."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile
import time

FIXTURES = {
    'client': '6835e4b019721e2a2c2c00af44efaa23d50aa963c3100923401011712fb938d3',
    'server': '0426f8fe7cf7c93904dfd0e1194dc6f021b069d9297cf50a2f21c6b91cc486ae',
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def compare(executable, side, expected):
    child = None
    failure = None
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        started = time.monotonic()
        try:
            child = subprocess.Popen([str(executable), side], stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr)
            while child.poll() is None:
                if time.monotonic() - started >= 15:
                    failure = 'native prefix deadline exceeded'
                if stdout.tell() > len(expected) or stderr.tell() > 65536:
                    failure = 'native prefix output bound exceeded'
                if failure:
                    break
                try:
                    child.wait(timeout=0.05)
                except subprocess.TimeoutExpired:
                    pass
        finally:
            if child is not None:
                try:
                    if child.poll() is None:
                        child.kill()
                finally:
                    child.wait(timeout=5)
        require(failure is None, failure)
        require(child is not None and child.returncode == 0, 'native prefix failed')
        require(stderr.tell() == 0, 'unexpected native prefix stderr')
        require(stdout.tell() == len(expected), 'native prefix byte count differs')
        stdout.seek(0)
        actual = stdout.read(len(expected) + 1)
        if actual != expected:
            for index, (left, right) in enumerate(zip(actual.splitlines(), expected.splitlines()), 1):
                if left != right:
                    raise ValueError(f'{side} row {index} differs: native={left!r}, original={right!r}')
            raise ValueError('native prefix bytes differ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    options = parser.parse_args()
    executable = options.executable.resolve(strict=True)
    for side, checksum in FIXTURES.items():
        expected = (options.fixtures / (side + '.expected.tsv')).read_bytes()
        require(hashlib.sha256(expected).hexdigest() == checksum, 'complete original fixture required')
        require(expected.startswith(('REGISTRY_BASE_PREFIX_V1\t' + side + '\n').encode('ascii')),
                'original prefix header')
        require(expected.endswith(b'END\t113\t96\t2048\t100\t241\t3949\n')
                and expected.count(b'\n') == 3950 and b'\r' not in expected, 'original prefix completeness')
        compare(executable, side, expected)
        print(f'PASS: {side}: 3950 original Block base-prefix rows')


if __name__ == '__main__':
    main()
