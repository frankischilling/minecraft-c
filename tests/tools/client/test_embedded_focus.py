"""Witness native Frame/Canvas focus and pointer transitions on physical monitor 2.

No original assets, World, game ticks, SDK coordinate stores or focus repair.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def pin(path):
    raw = path.read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        raise RuntimeError('Native Windows focus control requires Windows')
    monitor_path = ROOT / 'tests/tools/client/test_bed_sleep.py'
    capture_path = ROOT / 'tests/reference/client/notice/capture.py'
    monitor = module('embedded_monitor', monitor_path).second_monitor()
    capture = module('embedded_capture', capture_path)
    executable = args.executable.resolve(strict=True)
    inputs = [executable, Path(__file__).resolve(), monitor_path, capture_path]
    inputs += [ROOT / name for name in (
        'tests/client/window/test_embedded_focus.c', 'src/client/window/frame.c',
        'src/client/window/frame_platform.c', 'src/client/display/events.c',
        'cmake/tests/client_world_selection_loop.cmake', 'cmake/sdl_native_fixes.cmake')]
    inputs += list(executable.parent.glob('*.dll'))
    before = {str(path): pin(path) for path in inputs}
    args.artifacts.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='embedded-focus-', dir=args.artifacts.resolve()))
    environment = dict(os.environ, SDL_DISPLAY_USABLE_BOUNDS=','.join(map(str, monitor['selected']['work'])))
    report = {'status': 'FAIL', 'monitor': monitor, 'inputs_before': before,
              'scope': 'Native Frame/Canvas and GL context; no World, original parity, sustained play or FPS claim.'}
    try:
        result = capture.capture([str(executable)], work, 'native', 90, environment)
        report['native'] = result
        capture.require_success(result, empty_stderr=True)
        raw = (work / 'native.stdout').read_bytes()
        expected = b'Embedded focus: 200 native focus/pointer transitions; cleanup complete'
        if raw not in (expected + b'\n', expected + b'\r\n'):
            raise RuntimeError('Missing complete native focus/pointer/cleanup receipt')
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {str(path): pin(path) for path in inputs}
        if report['inputs_after'] != before:
            report['status'] = 'FAIL'
        (work / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Embedded focus artifacts: ' + str(work))
    if report['status'] != 'PASS':
        raise RuntimeError('Focus inputs changed during observation')
    print('PASS: 200 native focus/pointer transitions and checked cleanup')


if __name__ == '__main__':
    main()
