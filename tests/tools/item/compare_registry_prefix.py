#!/usr/bin/env python3
"""Compare the finite native Item base slice with pinned original observations.

This does not equate original INITIALIZING with native FAILED, or compare
constructor/subtype metadata, JDK internals, clocks or Random/profile state.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

# Pinned after the actual public-tool original capture and original-only export.
# Provenance and complete fixture fingerprints are checked before native launch.
PROVENANCE_SHA256 = '58e352394906f87f9ef892221a0f191c94f0fee967302adc9017580e8eb07a7b'
FIXTURES = {'client': '16498dc46ebbdd1f81d1d3b1fa8d73fbd7a64a59a29524cada1bc2f5b68344b1',
            'server': '86969fb411475363fb14362ac396fd26b3a9da4f714de62eb7d995c6b24fec27'}
JARS = {'client': 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c',
        'server': '033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def fingerprint(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def valid_fingerprint(value):
    return (isinstance(value, dict) and type(value.get('bytes')) is int and value['bytes'] >= 0
            and isinstance(value.get('sha256'), str)
            and re.fullmatch('[0-9a-f]{64}', value['sha256']) is not None)


def load_fixtures(directory):
    require(all(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value)
                for value in [PROVENANCE_SHA256, *FIXTURES.values()]),
            'original fixture/provenance pins have not been filled')
    provenance_path = directory / 'provenance.json'
    require(provenance_path.stat().st_size <= 512 * 1024, 'bounded original provenance')
    raw = provenance_path.read_bytes()
    require(fingerprint(raw)['sha256'] == PROVENANCE_SHA256, 'pinned original provenance required')
    provenance = json.loads(raw)
    require(provenance['schema'] == 'ITEM_BASE_PREFIX_PROVENANCE_V1'
            and valid_fingerprint(provenance['capture_manifest'])
            and valid_fingerprint(provenance['projector']), 'original projection provenance schema')
    inputs, compiled, artifacts = (provenance['inputs'], provenance['compiled_classes'], provenance['artifacts'])
    require(isinstance(inputs, dict) and inputs and all(valid_fingerprint(v) for v in inputs.values())
            and any(k.startswith('runtime/') for k in inputs), 'full original source/JDK/DLL input ledger')
    for name in ('java', 'javac', 'modules', 'jvm', 'java_native', 'release', 'source_archive',
                 'bounded_runner', 'capture_driver', 'ItemRegistryPrefixObservation.java',
                 'ItemRegistryPrefixController.java', 'client_jar', 'server_jar'):
        require(name in inputs, 'complete original input labels')
    require(isinstance(compiled, dict) and compiled and isinstance(artifacts, dict) and artifacts
            and all(valid_fingerprint(v) for v in artifacts.values())
            and all(valid_fingerprint(v) and artifacts.get('classes/' + k) == v for k, v in compiled.items()),
            'original compiled/artifact fingerprint joins')
    expected = {}
    require(set(provenance['sides']) == set(FIXTURES), 'both original sides required')
    for side, checksum in FIXTURES.items():
        source = directory / (side + '.expected.tsv')
        require(source.stat().st_size <= 2 * 1024 * 1024, 'bounded original fixture')
        data = source.read_bytes()
        record = provenance['sides'][side]
        require(fingerprint(data)['sha256'] == checksum and record['fixture'] == fingerprint(data)
                and record['rows'] == 33064, 'complete pinned original fixture required')
        require(data.startswith(('ITEM_BASE_PREFIX_V1\t' + side + '\n').encode('ascii'))
                and data.endswith(b'END\t33063\n') and data.count(b'\n') == 33064
                and b'\r' not in data, 'literal original projection framing')
        require(valid_fingerprint(record['raw']) and valid_fingerprint(record['receipt'])
                and record['raw'] == artifacts[side + '/snapshot.raw.tsv']
                and record['receipt'] == artifacts[side + '/receipt.tsv']
                and inputs[side + '_jar']['sha256'] == JARS[side], 'original capture/receipt/JAR joins')
        expected[side] = data
    return expected


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
                    failure = 'native Item prefix deadline exceeded'
                if os.fstat(stdout.fileno()).st_size > len(expected) or os.fstat(stderr.fileno()).st_size > 65536:
                    failure = 'native Item prefix output bound exceeded'
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
        require(child is not None and child.returncode == 0, 'native Item prefix failed')
        require(os.fstat(stderr.fileno()).st_size == 0, 'unexpected native Item prefix stderr')
        require(os.fstat(stdout.fileno()).st_size == len(expected), 'native Item prefix byte count differs')
        stdout.seek(0)
        actual = stdout.read(len(expected) + 1)
        if actual != expected:
            for index, (left, right) in enumerate(zip(actual.split(b'\n'), expected.split(b'\n')), 1):
                if left != right:
                    raise ValueError(f'{side} row {index} differs: native={left[:256]!r}, original={right[:256]!r}')
            raise ValueError('native Item prefix bytes differ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    options = parser.parse_args()
    fixtures = load_fixtures(options.fixtures.resolve(strict=True))
    executable = options.executable.resolve(strict=True)
    for side in ('client', 'server'):
        compare(executable, side, fixtures[side])
        print(f'PASS: {side}: 33064 original Item base-prefix rows')


if __name__ == '__main__':
    main()
