"""Compare real entered native billboard prefixes with the retained original draws."""
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
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='item-billboard-', dir=args.work_dir.resolve()))
    packet = module('native_billboard_packet', HERE / 'packet.py')
    compare = module('native_billboard_compare', HERE / 'compare.py')
    evidence = packet.audit_public()
    expected = json.loads((HERE / 'finite.json').read_text(encoding='utf-8'))['rows']
    inputs = [executable, ROOT / 'tests/client/render/test_item_billboard.c']
    inputs += sorted(path for folder in ('src', 'include') for path in (ROOT / folder).rglob('*')
                     if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs += sorted(path for path in HERE.iterdir() if path.is_file())
    before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    child = subprocess.run([str(executable)], capture_output=True, timeout=60)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    report = {'status': 'FAIL', 'packet': evidence, 'returncode': child.returncode,
              'inputs_before': before,
              'scope': 'Exact selected UV/state/active words; each own inactive backing; native completed draws0 versus original10 per process remain distinct.'}
    try:
        if child.returncode:
            raise RuntimeError('Native billboard probe failed: ' + child.stderr.decode('utf-8'))
        actual = compare.rows(child.stdout.decode('utf-8'))
        compare.compare(actual, expected)
        report.update(status='PASS', rows=len(actual), active_words=340,
                      own_inactive_words=140, native_completed_draws=0,
                      original_completed_draws_per_process=10)
    finally:
        report['inputs_after'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if report['status'] != 'PASS':
        raise RuntimeError('Native billboard input continuity failed: ' + str(output))
    print(child.stdout.decode('utf-8').splitlines()[-1])
    print('Item billboard comparison PASS: 10 real native prefixes, 340 active words, own inactive backing; native0/source10 completed draws per original process')


if __name__ == '__main__':
    main()
