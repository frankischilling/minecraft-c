"""Compare the shared native placement metadata bodies with unchanged original Items."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
REFERENCE = Path(__file__).resolve().parents[2] / 'reference/item/placement_metadata'
PREFIXES = ('CLASS\t', 'METADATA\t', 'COMPLETE\t')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compare(actual, expected):
    if actual != expected:
        for index, (left, right) in enumerate(zip(actual, expected)):
            if left != right:
                raise ValueError(f'Placement metadata row {index}: {left!r} != {right!r}')
        raise ValueError(f'Placement metadata row count {len(actual)} != {len(expected)}')


def controls(expected):
    changes = []
    changes.append(expected[1:])
    changes.append(expected + [expected[-1]])
    changes.append([expected[1], expected[0], *expected[2:]])
    for prefix, column in (('CLASS\t', 2), ('CLASS\t', 3), ('METADATA\t', 2), ('METADATA\t', 3)):
        changed = expected.copy()
        index = next(i for i, row in enumerate(changed) if row.startswith(prefix))
        fields = changed[index].split('\t')
        fields[column] = fields[column] + '1'
        changed[index] = '\t'.join(fields)
        changes.append(changed)
    for changed in changes:
        try:
            compare(changed, expected)
        except ValueError:
            continue
        raise ValueError('Placement metadata comparator accepted changed transcript')
    return len(changes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('placement_metadata_packet', REFERENCE / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    packet.audit()
    expected = (REFERENCE / 'original.stdout').read_text(encoding='utf-8').splitlines()
    rejected = controls(expected)
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='metadata-', dir=args.work_dir)).resolve()
    before = digest(executable)
    report = {'status': 'FAIL', 'executable_before': before, 'comparator_controls': rejected}
    try:
        result = subprocess.run([str(executable)], capture_output=True, timeout=45)
        (output / 'stdout.txt').write_bytes(result.stdout)
        (output / 'stderr.txt').write_bytes(result.stderr)
        report['returncode'] = result.returncode
        if result.returncode != 0 or result.stderr:
            raise ValueError('Native placement metadata process failed: ' + result.stderr.decode('utf-8'))
        lines = result.stdout.decode('utf-8').splitlines()
        if any(not row.startswith(PREFIXES) and row not in ('151 recipes', '16 achievements')
               for row in lines):
            raise ValueError('Unexpected native placement metadata output')
        compare([row for row in lines if row.startswith(PREFIXES)], expected)
        report['executable_after'] = digest(executable)
        if report['executable_after'] != before:
            raise ValueError('Native placement metadata executable changed during comparison')
        report.update(status='PASS', items=202, classes=38, comparisons=4040)
    except BaseException as error:
        report['error'] = repr(error)
        if isinstance(error, subprocess.TimeoutExpired):
            (output / 'stdout.txt').write_bytes(error.stdout or b'')
            (output / 'stderr.txt').write_bytes(error.stderr or b'')
        raise
    finally:
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: 4040 original placement metadata comparisons, {rejected} corruption controls; {output}')


if __name__ == '__main__':
    main()
