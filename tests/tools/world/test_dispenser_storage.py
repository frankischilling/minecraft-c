"""Run authored CPU AZ storage controls with genuine Registry resource streams."""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def admit(stdout, stderr, code):
    if type(stdout) is not bytes or type(stderr) is not bytes or type(code) is not int:
        raise ValueError('Incomplete native process transport')
    if code != 0 or stderr:
        raise ValueError('Failed native process or nonempty stderr')
    separator = b'\r\n' if b'\r' in stdout else b'\n'
    if not stdout.endswith(separator):
        raise ValueError('Incomplete native stdout line')
    lines = stdout.split(separator)[:-1]
    if not lines or any(b'\r' in line or b'\n' in line for line in lines):
        raise ValueError('Bare or mixed native stdout separators')
    # This executable creates one fresh Registry and initializes statistics:
    # crafting registration prints before achievement initialization. Preserve
    # both actual Source rows and their observed order, without filtering raw.
    if lines[:-1] != [b'151 recipes', b'16 achievements']:
        raise ValueError('Unexpected native Registry bootstrap output')
    match = re.fullmatch(rb'Dispenser storage CPU controls passed: ([1-9][0-9]*)', lines[-1])
    if match is None or int(match[1]) > 0xffffffff:
        raise ValueError('Missing complete native storage footer')
    return int(match[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='dispenser-storage-', dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('dispenser_storage_resources', support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / 'resources'
    support.resources_at(resources)
    try:
        child = subprocess.run([str(args.executable.resolve(strict=True)), str(resources)],
                               capture_output=True, timeout=120, check=False)
    except subprocess.TimeoutExpired as error:
        (output / 'stdout.bin').write_bytes(error.stdout or b'')
        (output / 'stderr.bin').write_bytes(error.stderr or b'')
        raise RuntimeError('Dispenser storage control timed out: ' + str(output)) from error
    (output / 'stdout.bin').write_bytes(child.stdout)
    (output / 'stderr.bin').write_bytes(child.stderr)
    print(child.stdout.decode('utf-8'), end='')
    print(child.stderr.decode('utf-8'), end='', file=sys.stderr)
    try:
        admit(child.stdout, child.stderr, child.returncode)
    except ValueError as error:
        raise RuntimeError('Dispenser storage controls failed: ' + str(output)) from error


if __name__ == '__main__':
    main()
