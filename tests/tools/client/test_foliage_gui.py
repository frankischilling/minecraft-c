"""Sixteen genuine native BB GUI calls with imported assets; no original pixels."""
import argparse
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise RuntimeError(f'Duplicate native field: {key}')
        result[key] = value
    return result


def parse(stdout):
    records = []
    for line in stdout.splitlines():
        if line.startswith('FOLIAGE_GUI'):
            prefix, separator, value = line.partition(' ')
            if not separator or prefix not in ('FOLIAGE_GUI', 'FOLIAGE_GUI_FINISH'):
                raise RuntimeError('Closed Foliage GUI native record prefixes')
            records.append((prefix, json.loads(value, object_pairs_hook=unique)))
    if ([prefix for prefix, _ in records] != ['FOLIAGE_GUI'] * 16 + ['FOLIAGE_GUI_FINISH'] or
            not stdout.splitlines()[-1].startswith('FOLIAGE_GUI_FINISH ')):
        raise RuntimeError('Exactly sixteen ordered profiles and final cleanup receipt')
    rows = []
    for index, (_, row) in enumerate(records[:-1]):
        id = 6 if index < 8 else 18
        damage, tint = (index % 8) // 2, bool(index % 2)
        leaf = id == 18
        color = [4764952, 6396257, 8431445, 6396257][damage] if leaf else 0xffffff
        expected = {'row': index, 'id': id, 'damage': damage, 'tint': tint,
                    'icon': ([52, 132, 52, 52] if leaf else [15, 63, 79, 15])[damage],
                    'color': color, 'owner': 'uy' if leaf else 'gm', 'jh': leaf,
                    'faces': 6 if leaf else 0, 'quad_vertices': 0 if leaf else 4,
                    'returned': True, 'matrices': True, 'retired': True, 'aliases_refused': True}
        if set(row) != set(expected) | {'width', 'height', 'rgba'}:
            raise RuntimeError('Exact GUI profile fields')
        if any(type(row[key]) is not type(value) or row[key] != value
               for key, value in expected.items()):
            raise RuntimeError(f'GUI profile identity/source outcome differs at {index}')
        if any(type(row[key]) is not int or not 0 < row[key] <= 1024
               for key in ('width', 'height')):
            raise RuntimeError('Bounded actual framebuffer dimensions')
        components = [((color >> shift) & 255) / 255 for shift in (16, 8, 0)] + [1.0]
        rgba = [struct.pack('>f', value).hex() for value in components]
        if row['rgba'] != rgba:
            raise RuntimeError('Exact binary32 Item color division')
        rows.append(row)
    finish = records[-1][1]
    if (set(finish) != {'profiles', 'checks', 'cleanup', 'windows_absent',
                       'join_attempts', 'join_busy'} or
            any(type(finish[key]) is not int for key in
                ('profiles', 'checks', 'join_attempts', 'join_busy')) or
            finish['profiles'] != 16 or finish['checks'] <= 0 or
            finish['cleanup'] is not True or finish['windows_absent'] is not True or
            not 1 <= finish['join_attempts'] <= 35000 or
            finish['join_busy'] != finish['join_attempts'] - 1):
        raise RuntimeError('Checked renderer/native join/window cleanup')
    return rows, finish


def capture_checks(row, directory, atlas, read):
    raw = read(directory / f"row-{row['row']:02}.rgba")
    width, height = row['width'], row['height']
    if len(raw) != width * height * 4:
        raise RuntimeError('Complete actual RGBA capture')
    background = raw[:3]
    if any(abs(a - b) > 1 for a, b in zip(background, (16, 32, 48))):
        raise RuntimeError('Declared native clear color')
    changed = sum(raw[index:index + 3] != background for index in range(0, len(raw), 4))
    if changed == 0:
        raise RuntimeError('Returned GUI draw produced no visible framebuffer samples')
    selected = 0
    if row['id'] == 6:
        aw, ah, pixels = atlas
        if (aw, ah) != (256, 256):
            raise RuntimeError('Actual imported terrain atlas dimensions')
        left, top = row['icon'] % 16 * 16, row['icon'] // 16 * 16
        factor = (1.0, 1.0, 1.0) if row['tint'] else (.25, .5, .75)
        for y in range(16):
            for x in range(16):
                at = ((top + y) * aw + left + x) * 4
                texel = pixels[at:at + 4]
                if texel[3] != 255:
                    continue
                px = math.floor((112 + x + .5) * width / 256)
                py = math.floor((112 + y + .5) * height / 256)
                actual = raw[(py * width + px) * 4:(py * width + px) * 4 + 3]
                if any(abs(a - t * f) > 2 for a, t, f in zip(actual, texel, factor)):
                    raise RuntimeError(f"Native Sapling opaque texel/color differs: row{row['row']} {x},{y}")
                selected += 1
        if selected < 8:
            raise RuntimeError('Sufficient actual opaque Sapling atlas samples')
    return {'foreground_pixels': changed, 'opaque_sapling_samples': selected,
            'rgb8_absolute_tolerance': 2, 'scope': 'native imported texture/color readback'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--mode', choices=('components',), default='components')
    parser.add_argument('--linux-game-dir', type=Path)
    args = parser.parse_args()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='foliage-gui-', dir=args.artifacts.resolve()))
    helper = Path(__file__).with_name('test_active_notice.py')
    notice = module('foliage_gui_host_protocol', helper)
    png_helper = Path(__file__).with_name('test_startup.py')
    paths = [Path(__file__), helper, png_helper, executable,
             ROOT / 'tests/client/render/test_foliage_gui.c']
    paths += sorted(path for base in ('src', 'include') for path in (ROOT / base).rglob('*')
                    if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    paths += sorted(executable.parent.glob('*.dll'))
    before = notice.input_digests(paths, executable)
    assets_before = notice.inventory(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before,
              'assets_before': assets_before, 'command': None}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    game = None
    try:
        environment = os.environ.copy()
        environment['ALSOFT_DRIVERS'] = 'null'
        home = output / 'home'
        home.mkdir()
        for key in ('APPDATA', 'LOCALAPPDATA', 'USERPROFILE', 'HOME'):
            environment[key] = str(home)
        if os.name == 'nt':
            game = home / '.minecraft'
            game.mkdir()
            (game / 'options.txt').write_bytes(b'viewDistance:3\nanaglyph3d:false\n')
        else:
            import pwd
            actual = Path(pwd.getpwuid(os.getuid()).pw_dir) / '.minecraft'
            if args.linux_game_dir is None or args.linux_game_dir.resolve(strict=True) != actual.resolve(strict=True):
                raise RuntimeError('Linux requires the existing actual private getpwuid game directory')
            game = actual
            report['linux_namespace'] = notice.linux_namespace_receipt(game)
        report['game_before'] = notice.inventory(game)
        command = [str(executable), str(assets), args.mode, str(output)]
        report['command'] = command
        try:
            result = subprocess.run(command, cwd=output, env=environment, capture_output=True,
                                    timeout=150)
        except subprocess.TimeoutExpired as error:
            (output / 'native.stdout').write_bytes(error.stdout or b'')
            (output / 'native.stderr').write_bytes(error.stderr or b'')
            raise
        (output / 'native.stdout').write_bytes(result.stdout)
        (output / 'native.stderr').write_bytes(result.stderr)
        report['exit'] = result.returncode
        print(result.stdout.decode('utf-8', 'replace'), end='')
        print(result.stderr.decode('utf-8', 'replace'), end='', file=sys.stderr)
        diagnostic = b'[ALSOFT] (WW) No capture backend available!'
        if result.returncode or result.stderr.replace(b'\r\n', b'\n') not in (b'', diagnostic + b'\n'):
            raise RuntimeError(f'Native GUI parent failed: exit{result.returncode}; {output}')
        rows, finish = parse(result.stdout.decode('utf-8', 'strict'))
        png = module('foliage_gui_png', png_helper).decoded_png
        atlas = png(notice.read(assets / 'jar/terrain.png'))
        report['profiles'] = rows
        report['finish'] = finish
        report['captures'] = [capture_checks(row, output, atlas, notice.read) for row in rows]
        report['status'] = 'PASS'
    except Exception as error:
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        report['inputs_after'] = notice.input_digests(paths, executable)
        report['assets_after'] = notice.inventory(assets)
        if game is not None:
            report['game_after'] = notice.inventory(game)
        if report['inputs_after'] != before or report['assets_after'] != assets_before:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Source/product/assets changed'
        report['outputs'] = {name: digest for name, digest in notice.inventory(output).items()
                             if name != 'run.json'}
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if 'continuity_error' in report:
            raise RuntimeError(report['continuity_error'])
    print(f'Foliage GUI components PASS; {output / "run.json"}')


if __name__ == '__main__':
    main()
