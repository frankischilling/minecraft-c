"""Bounded genuine native chat components and closed HUD; original replay has prepared scope."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--cpu', action='store_true')
    args = parser.parse_args()
    packet_dir = ROOT / 'tests/reference/client/gui_ingame_chat'
    packet = module('native_chat_packet', packet_dir / 'packet.py')
    expected = packet.load()
    process = module('native_chat_capture', ROOT / 'tests/reference/client/controller/capture.py')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='gui-ingame-chat-cpu-' if args.cpu else 'gui-ingame-chat-',
                                   dir=args.work_dir.resolve()))
    resources = output / 'resources'
    shutil.copytree(ROOT / 'tests/client/startup/fixtures', resources)
    for relative in ('lang/en_US.lang', 'lang/stats_US.lang'):
        (resources / relative).write_bytes((packet_dir / 'inputs' / relative).read_bytes())
    if args.cpu:
        (resources / 'missing-language').mkdir()
        statistics = resources / 'missing-statistics/lang'
        statistics.mkdir(parents=True)
        (statistics / 'en_US.lang').write_bytes((packet_dir / 'inputs/lang/en_US.lang').read_bytes())
    command = [str(executable), str(resources)]
    if not args.cpu:
        hud = module('native_chat_hud_author', Path(__file__).with_name('test_gui_ingame_draw.py'))
        assets = output / 'assets'
        hud.authored_assets(assets)
        (assets / 'jar/font.txt').write_bytes((packet_dir / 'inputs/font.txt').read_bytes())
        world = output / 'world'
        module('native_chat_world_author', Path(__file__).with_name('test_picking.py')).authored_world(world)
        command.extend((str(assets), str(world), str(output / 'chat.png')))
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format'}
    for directory in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client',
                       'tests/reference/client/gui_ingame_chat', 'tests/reference/client/controller'):
        paths.update(p for p in (ROOT / directory).rglob('*') if p.is_file() and
                     p.suffix in ('.c', '.h', '.inc', '.cmake', '.py', '.json', '.java', '.txt', '.lang'))
    paths.update(executable.parent.glob('*.dll'))
    before = {str(p): digest(p) for p in sorted(paths)}
    immutable = [resources] if args.cpu else [resources, assets]
    inputs = {p.relative_to(output).as_posix(): digest(p) for directory in immutable
              for p in directory.rglob('*') if p.is_file()}
    report = {'status': 'FAIL', 'scope': __doc__, 'mode': 'cpu' if args.cpu else 'gl-component',
              'inputs_before': before, 'authored_inputs_before': inputs,
              'original_scope': packet.SCOPE, 'original_replay_rows': 0}
    environment = os.environ.copy()
    environment['ALSOFT_DRIVERS'] = 'null'
    for name in ('APPDATA', 'LOCALAPPDATA', 'USERPROFILE', 'HOME', 'XDG_DATA_HOME',
                 'XDG_CONFIG_HOME', 'XDG_CACHE_HOME'):
        directory = output / name.lower()
        directory.mkdir(exist_ok=True)
        environment[name] = str(directory)
    try:
        # Capture may spend five seconds reaping and two seconds joining readers.
        # Leave the existing180-second CTest bound room for hashes and receipts.
        receipt = process.capture(command, output, 'native', 120, environment, output)
        report['process'] = receipt
        process.require_success(receipt, empty_stderr=True)
        stdout = (output / receipt['stdout_file']).read_text(encoding='utf-8')
        lines = stdout.splitlines()
        footer = r'GuiIngame chat CPU: [1-9][0-9]* checks; real failure prefixes, no Video/GL' if args.cpu else r'GuiIngame chat: [1-9][0-9]* checks; genuine Font, text/List/age components and closed HUD'
        packet.require(lines and re.fullmatch(footer, lines[-1]), 'completed native chat fixture')
        if not args.cpu:
            rows = [line for line in lines if line.startswith(('CHAT_CASE ', 'CHAT_LINE '))]
            packet.validate_rows(rows)
            packet.require(rows == expected, '81 exact original component rows')
            packet.require(lines.count('CHAT_DRAW closed=10 backgrounds=8 text=8 real-font=1') == 1,
                           'genuine native closed HUD draw')
            screenshot = output / 'chat.png'
            packet.require(screenshot.is_file() and screenshot.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),
                           'retained native chat screenshot')
            report.update(original_replay_rows=81, screenshot_sha256=digest(screenshot))
        report['status'] = 'PASS'
        print(stdout, end='')
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(p): digest(p) for p in sorted(paths)}
        report['authored_inputs_after'] = {name: digest(output / name) for name in inputs}
        unchanged = before == report['inputs_after'] and inputs == report['authored_inputs_after']
        if not unchanged:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        packet.require(unchanged, 'chat source/product/reference/authored input continuity')
    print('Native chat evidence: ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
