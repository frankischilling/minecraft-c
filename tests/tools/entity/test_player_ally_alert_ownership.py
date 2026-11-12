"""Native ally-alert classification and ownership component controls.

The World/App and WorldAccess receipt producers are genuine native owners.
Returned query/loader child frames are explicitly authored envelopes; no full
loader, original-JAR, Minecraft frame or loaded-gameplay execution is claimed.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile


ROOT = Path(__file__).resolve().parents[3]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def protected_inputs(executable, resources):
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
             ROOT / '.clang-format', ROOT / 'tests/entity/player/test_ally_alert_ownership.c',
             ROOT / 'tests/client/statistics/test_player_dispatch.c',
             ROOT / 'tests/reference/client/player_statistics/support.py',
             ROOT / 'tests/reference/client/statistics/support.py'}
    for folder in ('src', 'include', 'cmake'):
        paths.update(path for path in (ROOT / folder).rglob('*') if path.is_file())
    paths.update(path for path in resources.rglob('*') if path.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return {str(path): digest(path) for path in sorted(paths)}


def completed_native(child):
    capture = child.capture_report
    require(type(child.returncode) is int and child.returncode == 0,
            'Ally ownership fixture did not exit successfully')
    require(capture.get('exit') == 0 and capture.get('timed_out') is False and
            capture.get('output_overflow') is False and capture.get('reaped') is True and
            capture.get('pipe_capture_complete') is True and capture.get('reader_errors') == {},
            'Incomplete bounded native capture')
    require(child.stderr == b'', 'Ally ownership fixture wrote stderr')
    lines = child.stdout.splitlines()
    require(child.stdout.endswith(b'\n') and len(lines) == 3 and
            lines[:2] == [b'16 achievements', b'151 recipes'],
            'Actual World/App constructors and one completion footer required')
    footer = re.fullmatch(rb'Ally alert ownership: ([1-9][0-9]*) checks; '
                          rb'19 authored child envelopes', lines[-1])
    require(footer is not None, 'Missing exact component completion footer')
    return int(footer[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ally-ownership-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    report = {'status': 'FAIL', 'scope': __doc__, 'original_rows': 0,
              'authored_child_envelopes': 19, 'actual_source_receipt_invocations': 16,
              'full_source_loader_execution': False, 'minecraft_frame_execution': False,
              'loaded_gameplay': False}
    before = None
    try:
        helper = ROOT / 'tests/reference/client/player_statistics/support.py'
        spec = importlib.util.spec_from_file_location('ally_ownership_statistics', helper)
        require(spec is not None and spec.loader is not None, 'Statistics support unavailable')
        support = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(support)
        support.resources_at(resources)
        game.mkdir()
        before = protected_inputs(executable, resources)
        report['inputs_before_sha256'] = before
        environment = os.environ.copy()
        environment['ALSOFT_DRIVERS'] = 'null'
        child = support.run([str(executable), str(resources), str(game)], output, 'native',
                            timeout=120, env=environment)
        report['native'] = child.capture_report
        report['checks'] = completed_native(child)
        report['status'] = 'PASS'
    except BaseException as error:
        capture = getattr(error, 'capture_report', None)
        if capture is not None:
            report['native'] = capture
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        if before is not None:
            after = protected_inputs(executable, resources)
            report['inputs_after_sha256'] = after
            report['unchanged_protected_inputs'] = after == before
            if after != before:
                report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Ally ownership inputs changed during capture')
    print('Ally alert ownership: ' + str(report['checks']) + ' checks; capture=' + str(output))


if __name__ == '__main__':
    main()
