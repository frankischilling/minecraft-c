"""Nine Monster component controls and two actual World entity update controls.

Natural/fresh damage, actual true/false results, native versus source sound
failures, own NULL-error failed wake, prior-work rejection and checked cleanup.
Actual World success retains passive GZ61/65 health/statistic observations;
actual failed Mob damage preserves source failure through refused cleanup/retry.
No complete original runtime, Minecraft frame, loaded gameplay or pixels claim.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
from pathlib import Path
import tempfile


ROOT = Path(__file__).resolve().parents[3]
TIMEOUT_SECONDS = 120
FOOTER = rb'Monster hurt: ([1-9][0-9]*) checks;9 caller/producer and2 actual World controls'


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
             ROOT / '.clang-format', ROOT / 'tests/entity/test_monster_hurt.c',
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
            'Monster hurt fixture did not exit successfully')
    require(capture.get('exit') == child.returncode and
            capture.get('timed_out') is False and capture.get('output_overflow') is False and
            capture.get('pipe_capture_complete') is True and capture.get('reaped') is True and
            capture.get('reader_errors') == {}, 'Incomplete bounded native capture')
    require(child.stderr == b'', 'Monster hurt fixture wrote stderr; retain this failure')
    require(child.stdout.endswith(b'\n') and
            len(child.stdout.splitlines()) == 23 and
            child.stdout.splitlines()[:-1] == [b'16 achievements', b'151 recipes'] * 11 and
            re.fullmatch(FOOTER, child.stdout.splitlines()[-1]) is not None,
            'Missing eleven exact constructor banner pairs or control completion footer')
    return child.stdout.splitlines()[-1].decode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    require(executable.is_file(), 'Native attacker executable must be a file')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='monster-hurt-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    report = {'status': 'FAIL', 'scope': __doc__, 'original_rows': 0, 'native_component_cases': 9,
              'native_world_cases': 2,
              'minecraft_frame_execution': False, 'loaded_gameplay': False,
              'timeout_seconds': TIMEOUT_SECONDS,
              'capture_cleanup_budgets_seconds': {'reap': 5, 'reader_join_each': 1,
                                                'incomplete_capture_reap': 5},
              'stream_limit_bytes_each': 2097152}
    before = None
    footer = None
    try:
        support = module('monster_hurt_statistics',
                         ROOT / 'tests/reference/client/player_statistics/support.py')
        support.resources_at(resources)
        game.mkdir()
        before = protected_inputs(executable, resources)
        report['inputs_before_sha256'] = before
        environment = os.environ.copy()
        environment['ALSOFT_DRIVERS'] = 'null'
        # The genuine fixture changes cwd to game, then creates each monster-hurt directory
        # itself. It authors ordinary Chunk NBT for the World controls via the
        # actual region writer; source World/stats files remain retained outputs.
        # Reviewed statistics capture streams both
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
