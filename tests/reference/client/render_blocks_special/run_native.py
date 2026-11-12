"""Replay actual crossed-plant geometry against unchanged-original field/word rows."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import SOURCES, digest, ensure, read_json, rows
from capture import capture, require_success


def expected_rows(packet):
    ensure(type(packet) is dict and set(packet) == {'rows', 'source_sha256'} and
           type(packet['rows']) is list and all(type(row) is str for row in packet['rows']) and
           type(packet['source_sha256']) is dict and set(packet['source_sha256']) == set(SOURCES),
           'original plant finite packet schema')
    ensure(packet['source_sha256'] == {name: digest(HERE / name) for name in SOURCES},
           'original plant capture source pins changed; recapture required')
    return rows('\n'.join(packet['rows']))


def compare(actual, expected):
    ensure(len(actual) == len(expected), 'native/original plant row count differs')
    for index, (native, original) in enumerate(zip(actual, expected)):
        if native == original:
            continue
        a, b = native.split(), original.split()
        cell = next((offset for offset, pair in enumerate(zip(a, b)) if pair[0] != pair[1]), min(len(a), len(b)))
        ensure(False, f'native/original plant row{index} cell{cell} differs: native '
               f'{a[cell] if cell < len(a) else "<missing>"}; original '
               f'{b[cell] if cell < len(b) else "<missing>"}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='crossed-plants-', dir=args.work_dir.resolve()))
    expected = expected_rows(read_json(HERE / 'finite.json'))
    root = HERE.parents[3]
    inputs = [executable, HERE / 'run_native.py', HERE / 'finite.json', *[HERE / name for name in SOURCES],
              root / 'tests/client/render/test_crossed_plant.c', root / 'tests/client/render/test_render_blocks_faces.c',
              root / 'src/client/render/blocks/crossed_plant.c']
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': '125 exact original cv.a(Block,int,DDD)/NW CPU field and backing-word rows. '
                       'The native test has two additional controls outside original comparison; '
                       'cv.i/jitter/anaglyph/World/DK/GL/pixels remain excluded.'}
    try:
        receipt = capture([str(executable)], output, 'native', 90, os.environ.copy(), output)
        report['native'] = receipt
        require_success(receipt, empty_stderr=True)
        text = (output / receipt['stdout_file']).read_text(encoding='utf-8')
        ensure(re.fullmatch(r'Crossed plants: [1-9][0-9]* checks; 117 exact NW atlas/position rows and 10 failure prefixes',
                            text.splitlines()[-1]), 'native plant normal completion')
        actual = rows(text)
        report['mismatches'] = [{'index': index, 'native': a, 'original': b}
                                for index, (a, b) in enumerate(zip(actual, expected)) if a != b]
        compare(actual, expected)
        report.update(status='PASS', rows=125)
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report['input_after_sha256']
        if not same:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'native plant executable/source/reference inputs changed')
    print('Crossed plants: 125 exact unchanged-original CPU rows passed; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
