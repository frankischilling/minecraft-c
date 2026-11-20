"""Run real execution-owned Entity drops against literal SourceSaving inputs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('math-missing', 'math-limit', 'success', 'absent',
                                          'nullable', 'callback-failure', 'callback-capture',
                                          'native-close', 'preflight', 'capture-capacity'), required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    saving = load('drop_source_saving', repo / 'tests/tools/world/test_source_saving.py')
    construction = load('drop_source_construction', repo / 'tests/tools/world/test_source_world_construction.py')
    recipe = load('drop_source_inputs', repo / 'tests/tools/world/entity_insert_inputs.py')
    expected = recipe.prepared('success', saving, construction)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='drop-owned-', dir=args.work_dir)).resolve()
    for relative, data in expected.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    run = subprocess.run([str(executable), args.mode, str(root)], capture_output=True, timeout=240)
    (root / 'stdout.txt').write_bytes(run.stdout)
    (root / 'stderr.txt').write_bytes(run.stderr)
    stdout = run.stdout.decode('utf-8', errors='strict')
    lines = stdout.splitlines()
    footer = (re.fullmatch(r'PASS: ([1-9][0-9]*) actual execution-owned Entity drop checks;mode=' +
                           re.escape(args.mode), lines[-1]) if lines else None)
    unchanged = all((root / name).read_bytes() == data for name, data in expected.items())
    report = {'mode': args.mode, 'returncode': run.returncode, 'inputs_unchanged': unchanged,
              'executable_unchanged': hashlib.sha256(executable.read_bytes()).hexdigest() == before,
              'stderr_empty': not run.stderr,
              'scope': 'Authored actual-owner regression; no original runtime comparison'}
    report['passed'] = bool(run.returncode == 0 and footer and unchanged and
                            report['executable_unchanged'] and report['stderr_empty'])
    (root / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(stdout, end='')
    if run.stderr:
        print(run.stderr.decode('utf-8', errors='strict'), end='')
    if not report['passed']:
        raise RuntimeError('Owned Entity drop control failed: ' + str(root))
    print('Owned Entity drop PASS: ' + str(root))


if __name__ == '__main__':
    main()
