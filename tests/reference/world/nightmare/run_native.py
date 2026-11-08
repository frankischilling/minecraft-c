"""Replay the actual full CQ native API and compare its selected observations to the packet."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import packet as P

ROOT = P.HERE.parents[3]


def stream_digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    P.read()
    helper_path = ROOT / 'tests/tools/world/test_nightmare.py'
    sys.path.insert(0, str(helper_path.parent))
    spec = importlib.util.spec_from_file_location('nightmare_native_fixture', helper_path)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='nightmare-comparison-', dir=args.work_dir.resolve()))
    resources, worlds, output, profiles = helper.author_inputs(folder)
    executable = args.executable.resolve(strict=True)
    inputs = {executable, helper_path, Path(__file__).resolve(),
              ROOT / 'tests/world/runtime/test_nightmare.c', ROOT / 'tests/world/runtime/test_world_random_tick.c',
              ROOT / 'tests/support/java_random.h', helper_path.with_name('test_random_tick.py'),
              *resources.rglob('*.*'), *worlds.rglob('*.mcr')}
    inputs.update(P.HERE / name for name in (*P.SOURCES, 'expected.json', 'provenance.json'))
    for directory in ('src', 'include'):
        inputs.update(path for path in (ROOT / directory).rglob('*') if path.suffix in ('.c', '.h', '.inc'))
    before = {str(path): stream_digest(path) for path in sorted(inputs)}
    report = {'schema': 'NIGHTMARE_NATIVE_COMPARISON_V1', 'status': 'FAIL', 'input_before_sha256': before,
              'scope': P.SCOPE, 'source_print_frontier_cases': [6, 8]}
    first_failure = None
    try:
        command = [str(executable), str(resources), str(worlds), str(P.HERE / 'profiles.tsv'), str(output)]
        report['command'] = command
        with (folder / 'stdout.txt').open('wb') as stdout, (folder / 'stderr.txt').open('wb') as stderr:
            child = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr, timeout=1200)
        report['returncode'] = child.returncode
        P.need(child.returncode == 0 and (folder / 'stderr.txt').stat().st_size == 0, 'native fixture completion: ' + str(folder))
        actual = helper.native_rows(output, profiles)
        (folder / 'actual.json').write_text(json.dumps(actual, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
        count = P.compare(actual)
        report.update(status='PASS', profiles=28, compared_scalars=count,
                      full_native_returns=sum(row['returned'] for row in actual),
                      successful_nightmares=sum(row['result'] is True for row in actual),
                      actual_sha256=stream_digest(folder / 'actual.json'))
    except BaseException as failure:
        first_failure = failure
        report['exception'] = {'type': type(failure).__name__, 'message': str(failure)}
        report['timed_out'] = isinstance(failure, subprocess.TimeoutExpired)
        raise
    finally:
        unchanged = False
        try:
            report['input_after_sha256'] = {str(path): stream_digest(path) for path in sorted(inputs)}
            unchanged = before == report['input_after_sha256']
        except BaseException as recheck_failure:
            report['input_recheck_exception'] = {'type': type(recheck_failure).__name__, 'message': str(recheck_failure)}
        if not unchanged:
            report['status'] = 'FAIL'
        for name in ('stdout.txt', 'stderr.txt'):
            path = folder / name
            if path.exists():
                report[name] = {'bytes': path.stat().st_size, 'sha256': stream_digest(path)}
        (folder / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        if first_failure is None:
            P.need(unchanged, 'native product, source or input mutated during comparison')
    print('Nightmare original comparison: ' + str(count) + ' shared scalars; ' + str(folder / 'run.json'))


if __name__ == '__main__':
    main()
