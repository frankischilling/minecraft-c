"""Compare genuine native Player/Workbench calls with unchanged-original observations."""
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
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='workbench-original-', dir=args.work_dir.resolve()))
    audit = module('native_workbench_packet_audit', HERE / 'audit.py')
    packet = audit.audit()
    capture = module('native_workbench_declarations', HERE / 'capture.py')
    support = module('native_workbench_resources', ROOT / 'tests/reference/client/player_statistics/support.py')
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    cases = json.loads((HERE / 'cases.json').read_text(encoding='utf-8'))['cases']
    declarations = output / 'cases.tsv'
    declarations.write_text(capture.declarations(cases), encoding='utf-8', newline='\n')
    executable = args.executable.resolve(strict=True)
    inputs = [executable, Path(__file__).resolve(), ROOT / 'tests/inventory/workbench_interaction_original.c']
    inputs += sorted(path for folder in ('src', 'include') for path in (ROOT / folder).rglob('*')
                     if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs += sorted(path for path in HERE.iterdir() if path.is_file())
    before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    child = subprocess.run([str(executable), str(resources), str(game), str(declarations)],
                           capture_output=True, timeout=180)
    (output / 'stdout.txt').write_bytes(child.stdout)
    (output / 'stderr.txt').write_bytes(child.stderr)
    report = {'status': 'FAIL', 'packet': packet, 'inputs_before': before,
              'returncode': child.returncode}
    try:
        if child.returncode:
            raise RuntimeError('Native Workbench probe failed: ' + child.stderr.decode('utf-8'))
        actual = audit.P.rows(child.stdout.decode('utf-8'))
        expected = audit.load('finite.json')['rows']
        audit.P.compare(actual, expected)
        report['status'] = 'PASS'
        report['rows'] = len(actual)
    finally:
        report['inputs_after'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(child.stdout.decode('utf-8').splitlines()[-1])
    if report['status'] != 'PASS':
        raise RuntimeError('Native Workbench source continuity failed: ' + str(output))
    print('Workbench original comparison PASS: 116 exact selected calls; independent own-stream audits')


if __name__ == '__main__':
    main()
