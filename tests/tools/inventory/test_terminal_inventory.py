"""Native terminal owner contracts with authored Registry resources."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--scope', choices=('inventory', 'player'), required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    resources = runpy.run_path(str(Path(__file__).with_name('test_crafting.py')))['RESOURCES']
    output = args.work_dir.resolve() / ('run-' + uuid.uuid4().hex)
    directory, native = output / 'resources', output / 'native'
    native.mkdir(parents=True)
    for name, data in resources.items():
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    pins = {name: hashlib.sha256(data).hexdigest() for name, data in resources.items()}
    command = [str(args.executable.resolve(strict=True)), str(directory)]
    if args.scope == 'player':
        command.append(str(native))
    result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True,
                            timeout=60, check=False)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    pattern = (rb'Inventory terminal checks passed: ([1-9][0-9]*)' if args.scope == 'inventory'
               else rb'Player terminal Inventory: ([1-9][0-9]*) checks, pass')
    receipt = re.search(pattern + rb'\r?\n?$', result.stdout)
    unchanged = pins == {name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
                         for name in resources}
    passed = result.returncode == 0 and not result.stderr and receipt is not None and unchanged
    report = {'status': 'PASS' if passed else 'FAIL', 'scope': args.scope,
              'command': command, 'exit_code': result.returncode, 'authored_resources': pins,
              'resources_unchanged': unchanged, 'checks': int(receipt.group(1)) if receipt else 0}
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))
    if not passed:
        raise RuntimeError(f'Native {args.scope} terminal contracts failed; captures: {output}')


if __name__ == '__main__':
    main()
