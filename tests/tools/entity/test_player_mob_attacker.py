"""Authored incoming Mob damage over actual Player/World/App/statistics owners.

Native component and checked cleanup controls only: no original runtime,
Minecraft frame, loaded gameplay, pixels or audio comparison.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile


ROOT = Path(__file__).resolve().parents[3]
TIMEOUT_SECONDS = 120
FOOTER = b'Incoming Skeleton: actual World/App/statistics; health20-to16; checked cleanup'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None,
            'Unavailable actual statistics support: ' + str(path))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def protected_inputs(executable, resources):
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
             ROOT / '.clang-format', ROOT / 'tests/entity/player/test_mob_attacker.c',
             ROOT / 'tests/client/statistics/test_player_dispatch.c',
             ROOT / 'tests/reference/client/player_statistics/support.py',
             ROOT / 'tests/reference/client/statistics/support.py'}
    for folder in ('src', 'include', 'cmake'):
        paths.update(path for path in (ROOT / folder).rglob('*') if path.is_file())
    paths.update(path for path in resources.rglob('*') if path.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return {str(path): digest(path) for path in sorted(paths)}


def completed_native(child):
    """Require the real process and exact fixture completion independently."""
    capture = child.capture_report
    require(type(child.returncode) is int and child.returncode == 0,
            'Incoming Mob damage fixture did not exit successfully')
    require(capture.get('exit') == child.returncode and
            capture.get('timed_out') is False and capture.get('output_overflow') is False and
            capture.get('pipe_capture_complete') is True and capture.get('reaped') is True and
            capture.get('reader_errors') == {}, 'Incomplete bounded native capture')
    require(child.stderr == b'', 'Incoming Mob damage fixture wrote stderr; retain this failure')
    require(child.stdout.endswith(b'\n') and
            child.stdout.splitlines() == [b'16 achievements', b'151 recipes',
                                          b'16 achievements', b'151 recipes', FOOTER],
            'Missing both exact fixture constructor banner pairs and checked-cleanup footer')
    return child.stdout.splitlines()[-1].decode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    require(executable.is_file(), 'Native attacker executable must be a file')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='mob-attacker-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    report = {'status': 'FAIL', 'scope': __doc__, 'original_rows': 0,
              'minecraft_frame_execution': False, 'loaded_gameplay': False,
              'timeout_seconds': TIMEOUT_SECONDS,
              'capture_cleanup_budgets_seconds': {'reap': 5, 'reader_join_each': 1,
                                                'incomplete_capture_reap': 5},
              'stream_limit_bytes_each': 2097152}
    before = None
    footer = None
    try:
        support = module('player_mob_attacker_statistics',
                         ROOT / 'tests/reference/client/player_statistics/support.py')
        support.resources_at(resources)
        game.mkdir()
        before = protected_inputs(executable, resources)
        report['inputs_before_sha256'] = before
        environment = os.environ.copy()
        environment['ALSOFT_DRIVERS'] = 'null'
        # The genuine fixture changes cwd to game, then creates mob-attacker
        # itself. Only resources are authored; its source World/stats files
        # remain retained outputs. Reviewed statistics capture streams both
        # pipes from process start and retains bounded timeout/cleanup evidence.
        child = support.run([str(executable), str(resources), str(game)], output, 'native',
                            timeout=TIMEOUT_SECONDS, env=environment)
        report['native'] = child.capture_report
        footer = completed_native(child)
        report['returned_fixture_footer'] = footer
        report['status'] = 'PASS'
    except BaseException as error:
        capture = getattr(error, 'capture_report', None)
        if capture is not None:
            report['native'] = capture
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        if before is not None:
            try:
                after = protected_inputs(executable, resources)
                report['inputs_after_sha256'] = after
                report['unchanged_protected_inputs'] = after == before
                if after != before:
                    report['status'] = 'FAIL'
            except BaseException as error:
                report['status'] = 'FAIL'
                report['guard_error'] = {'type': type(error).__name__, 'message': str(error)}
        report['stream_sha256'] = {
            name: digest(output / ('native.' + name))
            for name in ('stdout', 'stderr') if (output / ('native.' + name)).is_file()}
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Protected native attacker inputs changed: ' + str(output))
    print(footer + '; capture=' + str(output))


if __name__ == '__main__':
    main()
