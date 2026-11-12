"""Finite actual Arrow hurt failure, owner handoff, and alternate victim controls."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=(
        'source-sound', 'source-unknown', 'source-exact-budget', 'native-close',
        'scope-drift', 'workspace-limit', 'fatal-publication', 'reject', 'wolf',
        'impact-null-list', 'retire-provenance'), required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    spec = importlib.util.spec_from_file_location(
        'arrow_handoff_capture', repo / 'tests/reference/client/statistics/support.py')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='arrow-handoff-', dir=args.work_dir)).resolve()
    root = out / 'worlds'
    root.mkdir()
    child = support.run_bounded(
        [str(executable), args.mode, str(root).encode('utf-8').hex()], out, 'native',
        timeout=240, limit=1024**2, env=os.environ.copy())
    lines = child.stdout.decode('utf-8', errors='strict').splitlines()
    footer = (re.fullmatch(r'PASS: ([1-9][0-9]*) actual Arrow hurt handoff checks;mode=' +
                           re.escape(args.mode), lines[-1]) if lines else None)
    report = {'mode': args.mode, 'exit': child.returncode, 'capture': child.capture_report,
              'stderr_empty': child.stderr == b'',
              'executable_unchanged': hashlib.sha256(executable.read_bytes()).hexdigest() == before,
              'scope': 'Finite owned Arrow741 controls; historical captures unchanged; GS35 open'}
    report['passed'] = bool(child.returncode == 0 and footer and not child.stderr and
                            report['executable_unchanged'])
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(child.stdout.decode('utf-8', errors='strict'), end='')
    if child.stderr:
        print(child.stderr.decode('utf-8', errors='strict'), end='')
    if not report['passed']:
        raise RuntimeError('Arrow hurt handoff control failed: ' + str(out))


if __name__ == '__main__':
    main()
