"""Compare entered native UB quad prefixes with completed unchanged-original draws."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='inventory-quads-', dir=args.work_dir.resolve()))
    packet = module('inventory_drawing_packet', HERE / 'packet.py')
    proof = packet.audit_public()
    compare = module('inventory_drawing_compare', HERE / 'compare.py')
    executable = args.executable.resolve(strict=True)
    inputs = [executable, ROOT / 'tests/client/gui/test_inventory_quads.c']
    inputs += sorted(path for folder in ('src', 'include') for path in (ROOT / folder).rglob('*')
                     if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs += sorted(path for path in HERE.iterdir() if path.is_file())
    before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    child = subprocess.run([str(executable)], capture_output=True, timeout=180)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    report = {'status': 'FAIL', 'packet': proof, 'inputs_before': before,
              'returncode': child.returncode, 'native_completed_draws': 0, 'original_completed_draws': 4}
    try:
        if child.returncode:
            raise RuntimeError('Native UB prefixes failed: ' + child.stderr.decode('utf-8'))
        actual = compare.rows(child.stdout.decode('utf-8'))
        expected = json.loads((HERE / 'finite.json').read_text(encoding='utf-8'),
                              object_pairs_hook=compare.unique)['rows']
        compare.compare(actual, expected)
        report['active_words'] = sum(len(compare.active_indices(row)) for row in actual)
        report['inactive_words_per_process'] = 192 - report['active_words']
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(child.stdout.decode('utf-8').splitlines()[-1])
    if report['status'] != 'PASS':
        raise RuntimeError('Native UB source continuity failed: ' + str(output))
    print('Inventory quads PASS: 108 exact active words; 84 own inactive words per process; '
          'four original completed draws versus four native zero-draw prefixes')


if __name__ == '__main__':
    main()
