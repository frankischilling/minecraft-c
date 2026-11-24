"""Authored real World failure ownership through client native cleanup."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError('Unavailable cleanup control helper: ' + str(path))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def sources(executable, resources):
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
             ROOT / '.clang-format', ROOT / 'tests/client/gameplay/test_player_sleep_tick_cleanup.c'}
    for folder in ('src', 'include', 'cmake', 'tests/entity', 'tests/world/runtime',
                   'tests/support', 'tests/reference/client/controller',
                   'tests/reference/client/player_statistics', 'tests/reference/client/statistics'):
        paths.update(p for p in (ROOT / folder).rglob('*') if p.is_file() and
                     p.suffix in ('.c', '.h', '.inc', '.py', '.cmake', '.json', '.txt'))
    paths.update(p for p in resources.rglob('*') if p.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return sorted(paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    support = module('player_sleep_cleanup_resources',
                     ROOT / 'tests/reference/client/player_statistics/support.py')
    capture = module('player_sleep_cleanup_capture',
                     ROOT / 'tests/reference/client/controller/capture.py')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='player-sleep-cleanup-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    before = {str(p): capture.digest(p) for p in sources(executable, resources)}
    report = {'status': 'FAIL', 'scope': __doc__, 'original_rows': 0,
              'minecraft_frame_execution': False, 'loaded_gameplay': False,
              'inputs_before_sha256': before}
    environment = os.environ.copy()
    environment['ALSOFT_DRIVERS'] = 'null'
    try:
        child = capture.capture([str(executable), 'player-sleep-cleanup',
            str(game).encode('utf-8').hex(), str(resources).encode('utf-8').hex()],
            output, 'native', 180, environment, output)
        report['native'] = child
        capture.require_success(child, empty_stderr=True)
        lines = (output / child['stdout_file']).read_bytes().splitlines()
        support.require(len(lines) == 3 and lines[:2] == [b'16 achievements', b'151 recipes'],
                        'Unexpected client cleanup constructor output')
        match = re.fullmatch(rb'Client Player wake cleanup: ([1-9][0-9]*) checks', lines[-1])
        support.require(match is not None, 'Missing actual client ownership completion')
        report['checks'] = int(match[1])
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        after = {str(p): capture.digest(p) for p in sources(executable, resources)}
        report['inputs_after_sha256'] = after
        if after != before:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    support.require(report['status'] == 'PASS', 'Client cleanup protected inputs changed')
    print('Client Player wake cleanup: ' + str(report['checks']) + ' checks; capture=' + str(output))


if __name__ == '__main__':
    main()
