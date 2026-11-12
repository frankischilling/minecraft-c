"""Run fifteen real native quiet metadata calls and retain their typed observations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/tools/entity'))
from test_mob_tick import authored_world
from test_mob_construction import RESOURCES

REFERENCE = ROOT / 'tests/reference/world/metadata_set/capture_reference.py'
FOOTER = r'PASS: ([1-9][0-9]*) Metadata set source-order/ownership checks'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parser():
    spec = importlib.util.spec_from_file_location('metadata_set_typed_rows', REFERENCE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def observations(data):
    text = data.decode('utf-8', errors='strict')
    lines = text.splitlines()
    footer = re.fullmatch(FOOTER, lines[-1]) if lines else None
    if footer is None or any(not line.startswith('METADATA_SET ') and
                             line not in ('16 achievements', '151 recipes')
                             for line in lines[:-1]):
        raise ValueError('Exact finite metadata row protocol and PASS footer required')
    typed = parser()
    rows = typed.rows(text)
    return rows, int(footer[1]), typed.controls(rows)


def main():
    args_parser = argparse.ArgumentParser(description=__doc__)
    args_parser.add_argument('--executable', type=Path, required=True)
    args_parser.add_argument('--work-dir', type=Path, required=True)
    args = args_parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='metadata-set-', dir=args.work_dir.resolve()))
    resources, world = output / 'resources', output / 'world'
    for name, value in RESOURCES.items():
        target = resources / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(value)
    authored_world(world)
    executable = args.executable.resolve(strict=True)
    inputs = {executable, Path(__file__).resolve(), REFERENCE,
              ROOT / 'cmake/tests/metadata_set.cmake',
              ROOT / 'tests/world/runtime/test_metadata_set.c',
              ROOT / 'tests/world/runtime/test_path.c',
              ROOT / 'tests/world/runtime/test_world_random_tick.c',
              ROOT / 'tests/tools/entity/test_mob_tick.py',
              ROOT / 'tests/tools/entity/test_mob_construction.py',
              ROOT / 'tests/reference/client/controller/capture.py',
              ROOT / 'tests/reference/client/controller/classfile.py',
              *resources.rglob('*.*'), *world.rglob('*.mcr')}
    for directory in ('src', 'include'):
        inputs.update(path for path in (ROOT / directory).rglob('*')
                      if path.suffix in ('.c', '.h', '.inc'))
    before = {str(path): digest(path) for path in sorted(inputs)}
    child = subprocess.run([str(executable), str(resources), str(world)],
                           stdin=subprocess.DEVNULL, capture_output=True, timeout=120)
    (output / 'native.stdout').write_bytes(child.stdout)
    (output / 'native.stderr').write_bytes(child.stderr)
    report = {'status': 'FAIL', 'exit_code': child.returncode,
              'input_before_sha256': before,
              'scope': 'Fifteen authored native scalar profiles over genuine World, cached normal '
                       'and provider-created Blank owners. Actual metadata writes, Source failures, '
                       'dirty ordering, unaffected state and checked fixture retirement are asserted. '
                       'This wrapper does not execute the original or establish build-input identity.'}
    try:
        if child.returncode != 0 or child.stderr:
            raise ValueError('Native metadata fixture failed or emitted stderr')
        rows, checks, controls = observations(child.stdout)
        report.update(rows=len(rows), checks=checks, comparator_refusals=controls)
        (output / 'rows.json').write_text(json.dumps(rows, indent=2) + '\n', encoding='utf-8')
        report['status'] = 'PASS'
    except Exception as error:
        report['error'] = f'{type(error).__name__}: {error}'
    after = {str(path): digest(path) for path in sorted(inputs)}
    report['input_after_sha256'] = after
    if after != before:
        report.update(status='FAIL', input_error='Source/product/helper/fixture inputs changed')
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if report['status'] != 'PASS':
        raise RuntimeError('Metadata set controls failed; inspect ' + str(output))
    print(f"PASS: {report['checks']} Metadata set source-order/ownership checks; capture={output}")


if __name__ == '__main__':
    main()
