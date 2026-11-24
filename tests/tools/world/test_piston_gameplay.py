"""Run saved stable Piston gameplay and failure controls through real World calls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

CASES = ('normal-push', 'sticky-pull', 'twelve-push', 'thirteen-cap', 'obsidian',
         'bedrock', 'missing-code', 'sound-failure', 'factory-class-failure')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='piston-gameplay-', dir=args.work_dir.resolve()))
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    command = [str(executable), 'world-piston-gameplay', str(output).encode('utf-8').hex()]
    result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=240, check=False)
    (output / 'stdout.bin').write_bytes(result.stdout)
    (output / 'stderr.bin').write_bytes(result.stderr)
    report = {'status': 'FAIL', 'scope': __doc__, 'command': command,
              'returncode': result.returncode, 'executable_sha256_before': before,
              'executable_sha256_after': hashlib.sha256(executable.read_bytes()).hexdigest(),
              'stdout_sha256': hashlib.sha256(result.stdout).hexdigest(),
              'stderr_sha256': hashlib.sha256(result.stderr).hexdigest()}
    error = None
    try:
        if result.returncode != 0 or result.stderr:
            raise ValueError('native process failed: ' + result.stderr.decode(errors='replace'))
        lines = result.stdout.decode('ascii').splitlines()
        rows = [line for line in lines if line.startswith('PISTON_GAMEPLAY ')]
        if rows != ['PISTON_GAMEPLAY case=' + name + ' cleanup=1 source-field-repair=0' for name in CASES]:
            raise ValueError('complete ordered nine-case native receipt')
        footer = re.fullmatch(r'PASS: Piston gameplay; cases=9; checks=([1-9][0-9]*); cleanup=1; source-field-repair=0',
                              lines[-1] if lines else '')
        if footer is None or sum(line.startswith('PASS:') for line in lines) != 1:
            raise ValueError('one complete terminal receipt')
        if report['executable_sha256_after'] != before:
            raise ValueError('native executable changed')
        report.update(status='PASS', cases=list(CASES), checks=int(footer.group(1)))
    except (ValueError, UnicodeError) as failure:
        error = failure
        report['error'] = str(failure)
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if error is not None:
        raise ValueError(str(error) + '; retained ' + str(output)) from error
    print(json.dumps(report))


if __name__ == '__main__':
    main()
