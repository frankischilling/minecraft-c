"""Replay genuine native NW additive calls against every retained original row."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import tempfile

from packet import FILES, HERE, digest, read_packet

ROOT = HERE.parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    expected = read_packet()['rows']
    executable = args.executable.resolve(strict=True)
    capture = module('translation_bounded_capture', HERE.parent / 'controller/capture.py')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='nw-translation-', dir=args.work_dir.resolve()))
    inputs = [executable, *(HERE / name for name in FILES), HERE / 'provenance.json',
              HERE / 'packet.py', Path(__file__).resolve(), HERE.parent / 'controller/capture.py',
              ROOT / 'src/client/render_engine/nw.c', ROOT / 'src/client/render_engine/nw.h',
              ROOT / 'src/entity/player/tick_math_internal.h',
              ROOT / 'tests/client/render_engine/test_nw_translation.c']
    before = {str(path): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'inputs_before': before,
              'scope': 'Whole native additive NW method, retained original CPU rows and separate native admission/FP/ownership controls. No genuine Display cleanup-failure or Cactus geometry qualification.'}
    try:
        child = capture.capture([str(executable), str(HERE / 'profiles.tsv')],
                                output, 'native', 30, working_directory=output)
        report['capture'] = child
        capture.require_success(child, empty_stderr=True)
        text = (output / child['stdout_file']).read_text(encoding='ascii')
        rows = text.splitlines()
        marker = re.fullmatch(r'PASS: 106 original NW translation rows, 36 profiles, ([1-9][0-9]*) native checks',
                              rows[-1] if rows else '')
        capture.ensure(marker is not None and len(rows) == 107, 'complete native source/control calls')
        actual = rows[:-1]
        if actual != expected:
            first = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                         min(len(actual), len(expected)))
            report['first_mismatch'] = {'index': first, 'native': actual[first:first + 1],
                                        'original': expected[first:first + 1]}
            raise AssertionError('Native/original NW translation differs: ' + str(report['first_mismatch']))
        report.update(status='PASS', rows=106, profiles=36, native_checks=int(marker[1]))
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        capture.ensure(report['inputs_unchanged'], 'native replay inputs changed')
    print('PASS: 106 exact original NW translation rows, 36 profiles; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
