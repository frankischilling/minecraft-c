"""Replay every actual original JQ getter result and the independent native controls."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import tempfile

from packet import FILES, HERE, digest, read_packet

ROOT = HERE.parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    expected = read_packet()['rows']
    executable = args.executable.resolve(strict=True)
    spec = importlib.util.spec_from_file_location('jq_bounded_capture', HERE.parent / 'controller/capture.py')
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='piston-texture-', dir=args.work_dir.resolve()))
    inputs = [executable, *(HERE / name for name in FILES), HERE / 'provenance.json',
              HERE / 'packet.py', Path(__file__).resolve(), HERE.parent / 'controller/capture.py',
              ROOT / 'src/block/registry/rendering.c', ROOT / 'src/block/registry/directions.c',
              ROOT / 'src/block/registry/runtime_internal.h',
              ROOT / 'tests/client/render/test_piston_inventory_texture.c']
    before = {str(path): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'inputs_before': before,
              'scope': 'Direct genuine JQ metadata getter and existing Direction owner; no original full inventory draw or propagated CV failure trace qualification.'}
    try:
        child = capture.capture([str(executable)], output, 'native', 30, working_directory=output)
        report['capture'] = child
        capture.require_success(child, empty_stderr=True)
        lines = (output / child['stdout_file']).read_text().splitlines()
        marker = re.fullmatch(r'PASS: 34320 original PistonBase textures, ([1-9][0-9]*) native checks',
                              lines[-1] if lines else '')
        capture.ensure(marker is not None, 'complete native source and admission controls')
        actual = [line for line in lines if line.startswith('JQ_')]
        if actual != expected:
            first = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                         min(len(actual), len(expected)))
            report['first_mismatch'] = {'index': first, 'native': actual[first:first + 1],
                                        'original': expected[first:first + 1]}
            raise AssertionError('Native/original JQ differs: ' + str(report['first_mismatch']))
        capture.ensure(all(line.startswith('JQ_') or line in ('16 achievements', '151 recipes', marker[0])
                           for line in lines), 'no undeclared native output')
        report.update(status='PASS', rows=34612, textures=34320, native_checks=int(marker[1]))
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        capture.ensure(report['inputs_unchanged'], 'native replay inputs changed')
    print('PASS: 34320 exact original JQ metadata textures, 34612 total rows; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
