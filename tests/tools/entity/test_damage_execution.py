"""Run the actual World/Living damage ownership control in a fresh directory."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=(
        'sound-receipt', 'unrelated-death', 'unknown-receipt', 'victims', 'early',
        'math-missing', 'native-stop', 'older-math-no-in', 'sound-null-list', 'sound-scope-drift',
        'sound-scope-drift-bare',
        'wolf-query', 'wolf-query-math', 'wolf-owner',
        'player-live', 'player-wake', 'player-fatal', 'player-fatal-math', 'player-statistic-null',
        'mob-fatal',
        'pending', 'admission'), required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='damage-owned-', dir=args.work_dir)).resolve()
    world = output / 'world'
    world.mkdir()
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    result = subprocess.run([str(executable), args.mode, str(world).encode().hex()],
                            capture_output=True, timeout=180)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    stdout = result.stdout.decode('utf-8', errors='strict')
    lines = stdout.splitlines()
    footer = (re.fullmatch(r'PASS: ([1-9][0-9]*) execution-owned damage checks;mode=' +
                           re.escape(args.mode), lines[-1]) if lines else None)
    report = {'mode': args.mode, 'returncode': result.returncode,
              'stderr_empty': not result.stderr,
              'executable_unchanged': hashlib.sha256(executable.read_bytes()).hexdigest() == before,
              'scope': 'Authored actual-owner control; no original-runtime comparison'}
    report['passed'] = bool(result.returncode == 0 and not result.stderr and footer and
                            report['executable_unchanged'])
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(stdout, end='')
    if result.stderr:
        print(result.stderr.decode('utf-8', errors='strict'), end='')
    if not report['passed']:
        raise RuntimeError('Native owned damage control failed: ' + str(output))


if __name__ == '__main__':
    main()
