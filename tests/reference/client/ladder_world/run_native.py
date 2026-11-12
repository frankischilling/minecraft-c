"""Compare entered native Ladder World rows with the guarded original packet."""
import argparse
import importlib.util
import json
import os
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


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    packet = read_packet()
    executable = args.executable.resolve(strict=True)
    capture = module('ladder_native_capture', HERE.parent / 'controller/capture.py')
    picking = module('ladder_native_inputs', ROOT / 'tests/tools/client/test_picking.py')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ladder-world-', dir=args.work_dir.resolve()))
    for name, data in picking.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    picking.authored_world(output / 'world')
    resources = inventory(output / 'resources')
    # Bind this native replay to current runtime/fixture inputs separately from
    # unchanged-original provenance. Source files are never an original oracle.
    inputs = [executable, *(HERE / name for name in FILES), HERE / 'provenance.json',
              HERE / 'packet.py', HERE / 'test_packet.py', Path(__file__).resolve(),
              HERE.parent / 'controller/capture.py', ROOT / 'tests/tools/client/test_picking.py',
              ROOT / 'tests/client/render/test_ladder_world.c',
              ROOT / 'tests/client/world/test_picking.c', ROOT / 'cmake/client_ladder_world.cmake',
              ROOT / 'tests/entity/player/test_construction.c',
              *sorted((ROOT / 'include').rglob('*.h')),
              *(p for p in sorted((ROOT / 'src').rglob('*'))
                if p.is_file() and p.suffix in ('.c', '.h', '.inc', '.glsl'))]
    before = {str(path): digest(path) for path in sorted(set(inputs))}
    report = {'status': 'FAIL', 'inputs_before': before, 'resource_before': resources,
              'world_before': inventory(output / 'world'),
              'scope': 'Native original-row replay: actual CV Ladder h, retained CV/Block fields, NW vertices/prefixes and four actual CV.b dispatches and explicit native FP/ownership controls. Original EW/JDI trace is retained source evidence; matching outer rows alone does not claim a native per-query trace comparison, GL, full terrain frames, gameplay or performance.'}
    try:
        receipt = capture.capture([str(executable), str(output / 'resources'), str(output / 'world'),
                                   str(HERE / 'profiles.txt')], output, 'native', 100,
                                  os.environ.copy(), output)
        report['capture'] = receipt
        capture.require_success(receipt, empty_stderr=True)
        text = (output / receipt['stdout_file']).read_text(encoding='ascii')
        lines = text.splitlines()
        marker = re.fullmatch(r'Source Ladder world: ([1-9][0-9]*) checks; 35 original calls, fields, bounds, NW words and cleanup',
                              lines[-1] if lines else '')
        capture.ensure(marker is not None, 'complete native Ladder/control/cleanup calls')
        actual = [line for line in lines if line.startswith(('LADDER\t', 'LADDER_INPUT\t'))]
        expected = [line for pair in zip(packet['inputs'], packet['rows']) for line in pair]
        report['actual_rows'] = actual
        report['mismatches'] = [{'index': i, 'native': a, 'original': b}
                                for i, (a, b) in enumerate(zip(actual, expected)) if a != b]
        if actual != expected:
            first = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                         min(len(actual), len(expected)))
            report['first_mismatch'] = {'index': first, 'native': actual[first:first+1], 'original': expected[first:first+1]}
            raise AssertionError('native/original Ladder World rows differ: ' + str(report['first_mismatch']))
        native = [line for line in lines if line.startswith('LADDER_NATIVE ')]
        capture.ensure(len(native) == 1 and re.fullmatch(r'LADDER_NATIVE own-unchanged-words=73399360 dispatch=4 fp-admission=1 cleanup=1', native[0]),
                       'separate native backing/FP/cleanup control receipt')
        report.update(status='PASS', original_calls=35, original_selected_rows=70,
                      native_checks=int(marker[1]), native_controls=native[0])
    finally:
        report['inputs_after'] = {path: digest(Path(path)) for path in before}
        report['resource_after'] = inventory(output / 'resources')
        report['world_after'] = inventory(output / 'world')
        same = before == report['inputs_after'] and resources == report['resource_after']
        if not same:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        capture.ensure(same, 'native replay source/executable/packet/resources changed')
    print('PASS: 35 exact original Ladder World calls; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
