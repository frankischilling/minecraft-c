"""Retain genuine native Arrow calls and compare to complete original methods."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import tempfile

import packet as P
from observation import native_receipt

ROOT = P.HERE.parents[3]
TIMEOUT = 240
STREAM_LIMIT = 2 * 1024 * 1024


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def scalars(value):
    return sum(map(scalars, value.values())) if isinstance(value, dict) else sum(map(scalars, value)) if isinstance(value, list) else 1


def completed_native(child):
    c = child.capture_report
    P.need(type(child.returncode) is int and child.returncode == 0 and type(c.get('exit')) is int and c['exit'] == 0 and
           c.get('timed_out') is False and c.get('output_overflow') is False and c.get('pipe_capture_complete') is True and
           c.get('reaped') is True and c.get('reader_errors') == {} and c.get('timeout_seconds') == TIMEOUT and
           c.get('limit') == STREAM_LIMIT and c.get('stdout_bytes') == len(child.stdout) and c.get('stderr_bytes') == len(child.stderr),
           'complete bounded native process and stream receipts')
    P.need(child.stderr == b'' and child.stdout.endswith(b'\n'), 'quiet complete native transport')
    text = child.stdout.decode('utf-8'); lines = text.splitlines()
    P.need(len(lines) == 43 and lines[1:3] == ['16 achievements', '151 recipes'] and
           all(line.startswith('ARROW_CONSTRUCTION ') for line in [lines[0], *lines[3:42]]), 'exact native row/bootstrap ordering')
    footer = re.fullmatch(r'Arrow construction reference: ([1-9][0-9]*) checks;40 genuine profiles', lines[-1])
    P.need(footer is not None, 'actual checked cleanup footer')
    return P.parse_rows(text), int(footer.group(1))


def compare_native(finite, actual):
    rows = []
    for row, trace in zip(actual, finite['traces'][0]):
        native_receipt(row, trace)
        seeded_gaussians(finite, row)
        rows.append(P.project(row))
    P.compare(finite['rows'], rows)
    return scalars(finite['rows'])


def seeded_gaussians(finite, row):
    if row['input'][1] == 3 and row['returned']:
        observed = finite['receipts'][0][row['case']]
        P.need(observed['gaussian_calls'] == 3 and
               P.exact(row['native']['gaussian_values'], observed['gaussian_values']),
               'all three actual seeded Gaussian return words agree, including zero speed/spread')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args(); finite, _ = P.read(); executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='arrow-construction-comparison-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'; game.mkdir()
    support_path = ROOT / 'tests/reference/client/player_statistics/support.py'
    support = module('arrow_statistics_support', support_path); support.resources_at(resources)
    inputs = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format',
              ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/entity/test_arrow_construction.c',
              ROOT / 'tests/entity/test_arrow_construction_reference.c', ROOT / 'tests/client/statistics/test_player_dispatch.c',
              support_path, ROOT / 'tests/reference/client/statistics/support.py', *P.HERE.glob('*.*'),
              *resources.rglob('*.*'), *executable.parent.glob('*.dll')}
    inputs = {path for path in inputs if path.is_file()}
    for directory in ('src', 'include', 'cmake'):
        inputs.update(path for path in (ROOT / directory).rglob('*') if path.is_file())
    before = {str(path): digest(path) for path in sorted(inputs)}
    report = {'schema': 'ARROW_CONSTRUCTION_NATIVE_COMPARISON_V1', 'status': 'FAIL', 'scope': P.SCOPE,
              'input_before_sha256': before, 'timeout_seconds': TIMEOUT, 'stream_limit_each': STREAM_LIMIT}
    first_failure = None
    try:
        command = [str(executable), str(resources), str(game), str(P.HERE / 'profiles.tsv')]
        report['command'] = command
        child = support.run(command, output, 'native', timeout=TIMEOUT)
        report['returncode'] = child.returncode; report['capture'] = child.capture_report
        rows, checks = completed_native(child); report['fixture_checks'] = checks
        (output / 'actual.json').write_text(json.dumps(rows, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
        count = compare_native(finite, rows)
        report.update(status='PASS', calls=40, normal_returns=sum(row['returned'] for row in rows), exceptions=sum(not row['returned'] for row in rows),
                      compared_scalars=count, actual_sha256=digest(output / 'actual.json'))
    except BaseException as failure:
        first_failure = failure; report['exception'] = {'type': type(failure).__name__, 'message': str(failure)}
        if getattr(failure, 'capture_report', None) is not None:
            report['capture'] = failure.capture_report
        raise
    finally:
        unchanged = False
        try:
            report['input_after_sha256'] = {str(path): digest(path) for path in sorted(inputs)}
            unchanged = before == report['input_after_sha256']
        except BaseException as failure:
            report['recheck_exception'] = {'type': type(failure).__name__, 'message': str(failure)}
        if not unchanged:
            report['status'] = 'FAIL'
        for name in ('stdout', 'stderr'):
            path = output / ('native.' + name)
            if path.exists():
                report[name] = {'bytes': path.stat().st_size, 'sha256': digest(path)}
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        if first_failure is None:
            P.need(unchanged, 'native source/product/resource/packet inputs changed during comparison')
    print(f'Arrow construction original comparison: {count} paired scalars; {output / "run.json"}')


if __name__ == '__main__':
    main()
