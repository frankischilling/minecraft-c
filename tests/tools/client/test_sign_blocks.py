"""Run native Sign bounds and draw gates with authored saved-World inputs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('sign_inputs', Path(__file__).with_name('test_picking.py'))
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='sign-blocks-', dir=args.work_dir.resolve()))
    for name, data in inputs.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs.authored_world(output / 'world')
    executable = args.executable.resolve(strict=True)
    before = digest(executable)
    command = [str(executable), str(output / 'resources'), str(output / 'world')]
    start = time.perf_counter()
    result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=180, check=False)
    (output / 'stdout.bin').write_bytes(result.stdout)
    (output / 'stderr.bin').write_bytes(result.stderr)
    report = {'status': 'FAIL', 'returncode': result.returncode, 'command': command,
              'executable_sha256_before': before, 'executable_sha256_after': digest(executable),
              'elapsed_seconds': time.perf_counter() - start,
              'stdout_sha256': hashlib.sha256(result.stdout).hexdigest(),
              'stderr_sha256': hashlib.sha256(result.stderr).hexdigest(),
              'scope': 'Authored native CPU controls; no original JVM or GL frame qualification'}
    error = None
    try:
        if result.returncode != 0 or result.stderr:
            raise ValueError('native process failed: ' + result.stderr.decode(errors='replace'))
        lines = result.stdout.decode('ascii').splitlines()
        footer = re.fullmatch(r'PASS: ([1-9][0-9]*) Source Sign bounds/draw gate checks \(CPU only\)',
                              lines[-1] if lines else '')
        if footer is None or sum(line.startswith('PASS:') for line in lines) != 1:
            raise ValueError('one complete native terminal receipt')
        if before != report['executable_sha256_after']:
            raise ValueError('native executable changed during controls')
        report.update(status='PASS', checks=int(footer.group(1)))
    except (ValueError, UnicodeError) as failure:
        error = failure
        report['error'] = str(failure)
    (output / 'run.json').write_bytes((json.dumps(report, indent=2) + '\n').encode('utf-8'))
    if error is not None:
        raise ValueError(str(error) + '; retained ' + str(output)) from error
    print(json.dumps(report))


if __name__ == '__main__':
    main()
