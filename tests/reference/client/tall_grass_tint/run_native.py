"""Replay the exact original RU/QM tint rows on real native saved-World owners."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
from packet import read_packet
from inputs import module, world
from capture_reference import inventory
from capture import capture, digest, ensure, require_success


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    expected = read_packet()['rows']
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='tall-grass-tint-', dir=args.work_dir.resolve()))
    authored = module('tint_native_resources', ROOT / 'tests/tools/client/test_picking.py')
    for name, data in authored.RESOURCES.items():
        path = out / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    world(out / 'world')
    resources = inventory(out / 'resources')
    inputs = [executable, *(p for p in HERE.iterdir() if p.is_file()),
              ROOT / 'tests/tools/client/test_picking.py', HERE.parent / 'controller/capture.py']
    before = {str(p): digest(p) for p in inputs}
    report = {'status': 'FAIL', 'inputs_before': before, 'resources_before': resources}
    try:
        receipt = capture([str(executable), str(out / 'resources'), str(out / 'world'),
                           str(HERE / 'profiles.tsv')], out, 'native', 100, os.environ.copy(), out)
        report['native'] = receipt
        require_success(receipt, empty_stderr=True)
        lines = (out / receipt['stdout_file']).read_text().splitlines()
        actual = [line for line in lines if line.startswith('TINT\t')]
        report['mismatches'] = [{'index': i, 'native': a, 'original': b}
                                for i, (a, b) in enumerate(zip(actual, expected)) if a != b]
        ensure(actual == expected, '28 native/original tint rows differ; retained ' + str(out))
        ensure([x for x in lines if x.startswith('TINT_CONTROL\t')] ==
               ['TINT_CONTROL\tFOUR_CHUNK_BUDGET_LIMIT\t1',
                'TINT_CONTROL\tDN64_AND_RETAINED_IA_FAILURE\t1'], 'actual native fixture/DN ownership controls')
        ensure(re.fullmatch('[1-9][0-9]* source Block color checks passed', lines[-1]), 'normal native completion')
        report.update(status='PASS', rows=28, actual_rows=actual, dn_controls=True)
    finally:
        report['inputs_after'] = {str(p): digest(p) for p in inputs}
        report['resources_after'] = inventory(out / 'resources')
        same = report['inputs_after'] == before and report['resources_after'] == resources
        if not same:
            report['status'] = 'FAIL'
        (out / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'native executable/packet/resources changed')
    print(json.dumps({'status': report['status'], 'rows': 28, 'dn_controls': True, 'report': str(out / 'run.json')}))


if __name__ == '__main__':
    main()
