"""Retain real execution-owned Bed exit controls and their authored source inputs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def pin(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='bed-exit-', dir=args.work_dir.resolve()))
    helper = ROOT / 'tests/tools/world/test_block_scalar_execution.py'
    spec = importlib.util.spec_from_file_location('bed_exit_source_inputs', helper)
    scalar = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(scalar)
    native = output / 'native'
    for mode in ('admission', 'open-descendant', 'capture', 'support-pending', 'raw-ok'):
        for relative, data in scalar.prepared('admission' if mode == 'admission' else 'open-descendant').items():
            path = native / mode / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    executable = args.executable.resolve(strict=True)
    inputs = {executable, Path(__file__).resolve(), helper,
              ROOT / 'tests/world/runtime/world_access/test_bed_exit_execution.c',
              ROOT / 'tests/world/runtime/world_access/test_block_scalar_execution.c',
              ROOT / 'tests/world/runtime/world_access/test_collision_query_execution.c',
              ROOT / 'tests/world/runtime/tile_entities/test_tile_entities.c',
              ROOT / 'cmake/tests/world_bed_exit_execution.cmake',
              ROOT / 'tests/tools/world/test_source_saving.py',
              ROOT / 'tests/tools/world/test_source_world_construction.py',
              ROOT / 'tests/tools/world/collision_query_inputs.py',
              *native.rglob('*.*')}
    for directory in ('src', 'include'):
        inputs.update(p for p in (ROOT / directory).rglob('*') if p.suffix in ('.c', '.h', '.inc'))
    before = {str(p): pin(p) for p in sorted(inputs)}
    child = subprocess.run([str(executable), str(native / 'admission'), str(native / 'open-descendant'),
                            str(native / 'capture'), str(native / 'support-pending'), str(native / 'raw-ok')], capture_output=True,
                           stdin=subprocess.DEVNULL, timeout=180)
    (output / 'native.stdout').write_bytes(child.stdout)
    (output / 'native.stderr').write_bytes(child.stderr)
    after = {str(p): pin(p) for p in sorted(inputs)}
    lines = child.stdout.splitlines()
    footer = re.fullmatch(rb'PASS: ([1-9][0-9]*) actual owned bed exit checks', lines[-1]) if lines else None
    passed = child.returncode == 0 and not child.stderr and footer is not None and before == after
    report = {'status': 'PASS' if passed else 'FAIL', 'exit_code': child.returncode,
              'input_before_sha256': before, 'input_after_sha256': after,
              'checks': int(footer[1]) if footer else 0,
              'scope': 'Actual SourceSaving bootstrap and evaluated World scopes: ARMED refusal, '
                       'quiet search journals, Source faults, nullable static World, captured '
                       'Block.m reentry, descendant refusal/drain and raw-OK native completion refusal. '
                       'No original, full wake or Arrow completion claim.'}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not passed:
        raise RuntimeError('Owned bed exit controls failed; inspect ' + str(output))
    print(lines[-1].decode('ascii') + '; capture=' + str(output))


if __name__ == '__main__':
    main()
