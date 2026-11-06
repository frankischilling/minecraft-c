"""Run actual Player damage/wake controls without display or original assets."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='damage-wake-', dir=args.work_dir.resolve()))
    child = subprocess.run([args.executable.resolve(strict=True), 'damage-wake',
                            str(output).encode('utf-8').hex()], capture_output=True, timeout=120)
    (output / 'native.stdout').write_bytes(child.stdout)
    (output / 'native.stderr').write_bytes(child.stderr)
    if child.returncode != 0 or child.stderr:
        raise RuntimeError('Player damage wake failed: ' + child.stderr.decode('utf-8', 'replace'))
    lines = child.stdout.splitlines()
    if (not lines or re.fullmatch(rb'PASS: [1-9][0-9]* Player damage wake checks', lines[-1]) is None
            or any(line not in (b'16 achievements', b'151 recipes') for line in lines[:-1])):
        raise RuntimeError('Unexpected Player damage wake output')
    print(lines[-1].decode('ascii') + '; capture=' + str(output))


if __name__ == '__main__':
    main()
