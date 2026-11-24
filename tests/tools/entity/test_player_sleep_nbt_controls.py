"""Bounded authored Player NBT wake and detailed World ownership controls."""
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
        raise RuntimeError('Unavailable native control helper: ' + str(path))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def sources(executable, resources):
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format'}
    for folder in ('src', 'include', 'cmake', 'tests/entity', 'tests/world/runtime',
                   'tests/support', 'tests/reference/client/controller',
                   'tests/reference/client/player_statistics', 'tests/reference/client/statistics'):
        paths.update(p for p in (ROOT / folder).rglob('*') if p.is_file() and
                     p.suffix in ('.c', '.h', '.inc', '.cmake', '.py', '.json', '.txt'))
    paths.update(p for p in resources.rglob('*') if p.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return sorted(paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    capture = module('sleep_nbt_control_capture',
                     ROOT / 'tests/reference/client/controller/capture.py')
    support = module('sleep_nbt_control_resources',
                     ROOT / 'tests/reference/client/player_statistics/support.py')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='player-sleep-nbt-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'worlds'
    support.resources_at(resources)
    before = {str(p): capture.digest(p) for p in sources(executable, resources)}
    report = {'status': 'FAIL', 'scope': __doc__, 'original_rows': 0,
              'minecraft_frame_execution': False, 'client_load_execution': False,
              'loaded_gameplay': False, 'inputs_before_sha256': before}
    environment = os.environ.copy()
    environment['ALSOFT_DRIVERS'] = 'null'
    try:
        child = capture.capture([str(executable), 'player-nbt-sleep-controls',
            str(game).encode('utf-8').hex(), str(resources).encode('utf-8').hex()],
            output, 'native', 180, environment, output)
        report['native'] = child
        capture.require_success(child, empty_stderr=True)
        lines = (output / child['stdout_file']).read_bytes().splitlines()
        support.require(len(lines) == 7 and lines[:5] == [b'16 achievements', b'151 recipes',
            b'16 achievements', b'151 recipes',
            b'Player sleeping NBT World loader: 4 authored scenarios'],
            'Unexpected actual NBT control constructor/scenario output')
        count = re.fullmatch(rb'PLAYER_NBT_SLEEP_CONTROLS_COMPLETE ([1-9][0-9]*) checks', lines[-1])
        support.require(count is not None, 'Missing actual NBT control completion')
        checks = int(count[1])
        support.require(lines[-2] == ('Player active tick: ' + str(checks) + ' checks').encode('ascii'),
                        'Actual fixture footer and completion count differ')
        report.update(status='PASS', checks=checks, world_attachment_native_scenarios=4)
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        after = {str(p): capture.digest(p) for p in sources(executable, resources)}
        report['inputs_after_sha256'] = after
        if after != before:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    support.require(report['status'] == 'PASS', 'NBT control protected inputs changed')
    print('Player sleeping NBT native controls: ' + str(report['checks']) + ' checks; report=' +
          str(output / 'report.json'))


if __name__ == '__main__':
    main()
