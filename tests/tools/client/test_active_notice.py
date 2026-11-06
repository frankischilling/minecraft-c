"""Bounded actual Notice components; no original rendering or injected clock."""
import argparse
import hashlib
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


def read(path, maximum=4 * 1024 * 1024):
    if not path.is_file() or path.stat().st_size > maximum:
        raise RuntimeError(f"Bounded required file: {path}")
    return path.read_bytes()


def digest(path, maximum=64 * 1024 * 1024):
    if not path.is_file() or path.stat().st_size > maximum:
        raise RuntimeError(f'Bounded required hash input: {path}')
    value, total = hashlib.sha256(), 0
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            total += len(block)
            if total > maximum:
                raise RuntimeError(f'Hash input grew beyond bound: {path}')
            value.update(block)
    return value.hexdigest()


def input_digests(paths, executable):
    # Sanitized native fixtures include their complete debug information.
    # This separate input bound does not admit larger captures or resources.
    return {str(path): digest(path, 256 * 1024 * 1024 if path == executable else
                             64 * 1024 * 1024) for path in paths}


def inventory(root):
    if not root.is_dir():
        return {}
    paths = sorted(path for path in root.rglob('*') if path.is_file())
    if len(paths) > 8192 or sum(path.stat().st_size for path in paths) > 256 * 1024 * 1024:
        raise RuntimeError(f"Bounded inventory: {root}")
    return {path.relative_to(root).as_posix(): digest(path) for path in paths}


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def fields(line, prefix):
    if not line.startswith(prefix + ' '):
        raise RuntimeError(f"Wrong row prefix: {prefix}")
    pairs = [word.split('=', 1) for word in line.split()[1:]]
    if any(len(pair) != 2 for pair in pairs) or len({key for key, _ in pairs}) != len(pairs):
        raise RuntimeError(f"Unique fields: {prefix}")
    return dict(pairs)


def floating(bits):
    if re.fullmatch('[0-9a-f]{16}', bits) is None:
        raise RuntimeError('Exact binary64 receipt')
    value = struct.unpack('>d', bytes.fromhex(bits))[0]
    if not math.isfinite(value):
        raise RuntimeError('Finite real-clock scalar')
    return value


def rows(stdout, prefix):
    return [fields(line, prefix) for line in stdout.splitlines() if line.startswith(prefix + ' ')]


def scalars(draw):
    if draw['clock'] != '1':
        return
    now, g = int(draw['now']), int(draw['g'])
    delta = ((now - g + (1 << 63)) % (1 << 64)) - (1 << 63)
    elapsed = floating(draw['elapsed'])
    if elapsed != float(delta) / 3000.0:
        raise RuntimeError('Actual clock/g Java-long elapsed')
    if draw['pc'] == '156':
        if draw['expired'] != '1' or 0 <= elapsed <= 1 or draw['tutorial'] != '0':
            raise RuntimeError('Actual completion expiry interval')
        return
    # Independent closed-form curve, evaluated against the actual selected
    # wall clock. This is native scalar checking, not equal-clock original proof.
    expected = max(0.0, 4.0 * abs(2.0 * elapsed - 1.0) - 3.0) ** 4
    actual = floating(draw['easing'])
    tolerance = 16 * math.ulp(expected if expected else 1.0)
    if abs(actual - expected) > tolerance:
        raise RuntimeError('Actual quartic differs from independent closed form')
    if int(draw['x']) != int(draw['width']) - 160:
        raise RuntimeError('Actual rounded QQ width/source x')
    offset = min(2147483647, max(-2147483648, int(actual * 36.0)))
    y = ((-offset + (1 << 31)) % (1 << 32)) - (1 << 31)
    if int(draw['y']) != y:
        raise RuntimeError('Actual d2i/wrapped y')


def vertices(stdout, draw):
    found = [line.split() for line in stdout.splitlines()
             if line.startswith('NOTICE_VERTEX label=' + draw['label'] + ' ')]
    if len(found) != 4:
        raise RuntimeError('Four retained real source vertices')
    x, y = int(draw['x']), int(draw['y'])
    for index, cells in enumerate(found):
        if len(cells) != 13 or cells[2] != f'row={index}':
            raise RuntimeError('Exact background/item vertex receipt')
        actual = [floating(value) for value in cells[3:]]
        right, bottom = index in (1, 2), index < 2
        expected = [x + (160 if right else 0), y + (32 if bottom else 0), 0,
                    (96 + (160 if right else 0)) / 256,
                    (202 + (32 if bottom else 0)) / 256,
                    x + 8 + (16 if right else 0), y + 8 + (16 if bottom else 0), 0,
                    (176 + (16 if right else 0)) / 256,
                    (48 + (16 if bottom else 0)) / 256]
        if actual != expected:
            raise RuntimeError('Actual source quad geometry/Book UVs')


def book_pixels(output, label, draw, assets, png):
    width, height = int(draw['physical-width']), int(draw['physical-height'])
    raw = read(output / (label + '.rgba'))
    if len(raw) != width * height * 4:
        raise RuntimeError('Actual raw RGBA dimensions')
    aw, ah, atlas = png(read(assets / 'jar/gui/items.png'))
    if (aw, ah) != (256, 256) or int(draw['y']) != 0:
        raise RuntimeError('Actual visible plateau and original Book atlas')
    gui_width, gui_height = int(draw['width']), int(draw['height'])
    matched, colored = 0, set()
    for v in range(16):
        for u in range(16):
            at = ((48 + v) * 256 + 176 + u) * 4
            expected = atlas[at:at + 4]
            if expected[3] != 255:
                continue
            px = math.floor((int(draw['x']) + 8 + u + .5) * width / gui_width)
            py = math.floor((int(draw['y']) + 8 + v + .5) * height / gui_height)
            if not 0 <= px < width or not 0 <= py < height:
                raise RuntimeError('Book interior projects inside actual framebuffer')
            pixel = raw[(py * width + px) * 4:(py * width + px) * 4 + 3]
            if any(abs(a - b) > 1 for a, b in zip(pixel, expected[:3])):
                raise RuntimeError(f'Book opaque texel differs at {u},{v}: {list(pixel)} / {list(expected)}')
            matched += 1
            colored.add(bytes(expected[:3]))
    if matched < 16 or len(colored) < 3:
        raise RuntimeError('Actual non-placeholder Book opaque texels')
    return {'opaque_texels': matched, 'source_colors': len(colored),
            'raw_rgb8_absolute_tolerance': 1,
            'scope': 'native texture/geometry readback; no original pixel equality'}


def retained_native_run(command, environment, output, game, world, report, timeout):
    """Retain native evidence before snapshots; a snapshot cannot replace its cause."""
    result = None
    try:
        try:
            result = subprocess.run(command, env=environment, cwd=output, capture_output=True,
                                    timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (output / 'native.stdout').write_bytes(error.stdout or b'')
            (output / 'native.stderr').write_bytes(error.stderr or b'')
            raise
        (output / 'native.stdout').write_bytes(result.stdout)
        (output / 'native.stderr').write_bytes(result.stderr)
        report['exit'] = result.returncode
        return result
    finally:
        original_error = sys.exc_info()[0] is not None
        failures = []
        try:
            report['game_after'] = inventory(game)
        except Exception as error:
            failures.append({'operation': 'game inventory', 'type': type(error).__name__,
                             'message': str(error)})
        if os.name == 'nt':
            try:
                report['game_copies'] = world.save_title_files(game, output / 'game-after')
            except Exception as error:
                failures.append({'operation': 'game copies', 'type': type(error).__name__,
                                 'message': str(error)})
        if failures:
            report['snapshot_errors'] = failures
            # A nonzero result is diagnosed by the caller using the actual exit;
            # an active launch/timeout/logging exception keeps its own identity.
            if not original_error and (result is None or result.returncode == 0):
                raise RuntimeError('Native game snapshot failed; see snapshot_errors')


def mount_field(text):
    return re.sub(r'\\(040|011|012|134)', lambda match: chr(int(match[1], 8)), text)


def process_identity(pid):
    text = read(Path(f'/proc/{pid}/stat'), 16384).decode('utf-8', 'strict')
    end = text.rfind(')')
    tail = text[end + 2:].split() if end >= 0 else []
    if len(tail) < 20 or text[:text.find(' ')] != str(pid):
        raise RuntimeError('Bounded actual Linux process identity')
    return int(tail[1]), int(tail[19])


def linux_namespace_receipt(game):
    """Admit only the real launcher-owned private bind in a distinct mount namespace."""
    import pwd
    receipt_path = os.environ.get('MC_CLIENT_ACTIVE_NOTICE_MOUNT_RECEIPT')
    if not receipt_path:
        raise RuntimeError('Linux native launch requires a disposable mount-namespace READY receipt')
    receipt_file = Path(receipt_path).resolve(strict=True)
    raw = read(receipt_file, 32768)
    receipt = json.loads(raw)
    uid, gid = os.getuid(), os.getgid()
    account = pwd.getpwuid(uid)
    if (receipt.get('format') != 1 or type(receipt.get('format')) is not int or
            receipt.get('status') != 'READY_PRIVATE_MOUNT_NAMESPACE' or
            receipt.get('controller_scope') != 'active-notice-linux-fs-isolation'):
        raise RuntimeError('Exact Linux namespace readiness format/scope')
    integers = ('uid', 'gid', 'namespace_inode', 'host_namespace_inode', 'host_game_dev',
                'host_game_ino', 'backing_dev', 'backing_ino', 'game_dev', 'game_ino',
                'mount_id', 'parent_id', 'ready_pid', 'ready_start_ticks')
    if any(type(receipt.get(key)) is not int or receipt[key] < 0 for key in integers):
        raise RuntimeError('Typed nonnegative Linux namespace identities')
    if (receipt['uid'] != uid or receipt['gid'] != gid or
            receipt.get('user') != account.pw_name or receipt.get('home') != account.pw_dir or
            receipt.get('game') != str(game) or receipt_file.stat().st_uid != uid):
        raise RuntimeError('Actual Linux account/owned receipt identity')
    if game != Path(account.pw_dir) / '.minecraft':
        raise RuntimeError('Actual getpwuid source game directory')
    backing = Path(receipt['backing'])
    if (not game.is_absolute() or not backing.is_absolute() or not game.is_dir() or
            not backing.is_dir() or game != game.resolve(strict=True) or
            backing != backing.resolve(strict=True) or not os.path.samefile(game, backing)):
        raise RuntimeError('Actual nonsymlink game/private-backing bind identity')
    namespace = os.stat('/proc/self/ns/mnt').st_ino
    game_stat, backing_stat = game.stat(), backing.stat()
    pair = (game_stat.st_dev, game_stat.st_ino)
    if (namespace != receipt['namespace_inode'] or namespace == receipt['host_namespace_inode'] or
            backing_stat.st_uid != uid or pair != (receipt['game_dev'], receipt['game_ino']) or
            pair != (receipt['backing_dev'], receipt['backing_ino']) or
            pair != (backing_stat.st_dev, backing_stat.st_ino) or
            pair == (receipt['host_game_dev'], receipt['host_game_ino'])):
        raise RuntimeError('Distinct actual mount namespace/private filesystem identities')
    current = os.getpid()
    ready_start = None
    for _ in range(128):
        parent, started = process_identity(current)
        if current == receipt['ready_pid']:
            ready_start = started
            break
        if parent <= 0 or parent == current:
            break
        current = parent
    if (receipt['ready_pid'] <= 0 or ready_start != receipt['ready_start_ticks'] or
            os.stat(f"/proc/{receipt['ready_pid']}/ns/mnt").st_ino != namespace):
        raise RuntimeError('Live same-namespace READY writer ancestor/start identity')
    mounts = read(Path('/proc/self/mountinfo')).decode('utf-8', 'strict').splitlines()
    if len(mounts) > 8192:
        raise RuntimeError('Bounded Linux mount inventory')
    selected = []
    for line in mounts:
        before, separator, after = line.partition(' - ')
        left, right = before.split(), after.split()
        if not separator or len(left) < 6 or len(right) < 3:
            raise RuntimeError('Actual Linux mountinfo framing')
        if mount_field(left[4]) == str(game):
            selected.append({'mount_id': int(left[0]), 'parent_id': int(left[1]),
                             'mount_root': mount_field(left[3]), 'mount_point': mount_field(left[4]),
                             'mount_fs': mount_field(right[0]), 'mount_source': mount_field(right[1])})
    if len(selected) != 1 or any(receipt.get(key) != value for key, value in selected[0].items()):
        raise RuntimeError('Exact actual private game mountinfo entry')
    if any(re.fullmatch('[0-9a-f]{64}', receipt.get(key, '')) is None
           for key in ('host_before_manifest_sha256', 'tool_config_freeze_sha256')):
        raise RuntimeError('Frozen launcher/host-input receipt hashes')
    return {'ready_receipt': str(receipt_file), 'ready_receipt_sha256': hashlib.sha256(raw).hexdigest(),
            'readiness': receipt, 'scope': 'namespace-only admission; no native test PASS'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--mode', choices=('components', 'rounded', 'tutorial'), required=True)
    parser.add_argument('--linux-game-dir', type=Path)
    args = parser.parse_args()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    output_root = args.artifacts.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=args.mode + '-', dir=output_root))
    report = {'status': 'FAIL', 'mode': args.mode, 'scope': __doc__}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    helpers = [Path(__file__), ROOT / 'tests/tools/client/test_startup.py',
               ROOT / 'tests/tools/client/test_world_selection_loop.py',
               ROOT / 'tests/client/gui/test_active_notice.c',
               ROOT / 'tests/client/startup/test_world_selection_loop.c',
               ROOT / 'src/client/gui/achievement_notice_draw.c',
               ROOT / 'src/client/gui/achievement_notice_draw.h',
               ROOT / 'src/client/render/item_gui.c', ROOT / 'src/client/render/item_gui.h',
               ROOT / 'src/client/gui/bitmap_font_wrap.c',
               ROOT / 'include/mc/client/gui/achievement_notice.h', executable]
    before = input_digests(helpers, executable)
    assets_before = inventory(assets)
    external_before = None
    try:
        with tempfile.TemporaryDirectory(prefix='notice-home-', dir=output) as temporary:
            environment = os.environ.copy()
            environment['ALSOFT_DRIVERS'] = 'null'
            home = Path(temporary)
            environment['APPDATA'] = str(home)
            environment['LOCALAPPDATA'] = str(home)
            environment['USERPROFILE'] = str(home)
            environment['HOME'] = str(home)
            if os.name == 'nt':
                game = home / '.minecraft'
                game.mkdir()
                (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            else:
                if args.mode == 'tutorial':
                    raise RuntimeError('Tutorial needs genuine disposable account/filesystem isolation on Linux; HOME is not user.home')
                import pwd
                actual = Path(pwd.getpwuid(os.getuid()).pw_dir) / '.minecraft'
                if args.linux_game_dir is None or args.linux_game_dir.resolve(strict=True) != actual.resolve(strict=True):
                    raise RuntimeError('Explicit existing actual getpwuid game directory required for Linux components')
                game = actual
                report['linux_namespace'] = linux_namespace_receipt(game)
                external_before = inventory(game)
            world = module('notice_world_inputs', ROOT / 'tests/tools/client/test_world_selection_loop.py')
            if args.mode == 'tutorial':
                (game / 'stats').mkdir()
                saves = game / 'saves'
                saves.mkdir()
                for folder, display, played in [('older', 'Old world', 1303000000000),
                                                 ('bravo', '', 1304000000000),
                                                 ('alpha', 'Alpha world', 1304000000000)]:
                    directory = saves / folder
                    directory.mkdir()
                    import gzip
                    (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(
                        gzip.compress(world.metadata(display, played, 19132), mtime=0))
                regions = saves / 'alpha/region'
                regions.mkdir()
                world.saved_regions(regions, generate=False)
                if list((game / 'stats').iterdir()):
                    raise RuntimeError('Absent-stat input must be genuinely empty before bootstrap')
            report['game_before'] = inventory(game)
            command = [str(executable), str(assets), args.mode, str(output)]
            report['command'] = command
            result = retained_native_run(command, environment, output, game, world, report,
                                         360 if args.mode == 'tutorial' else 120)
            stdout = result.stdout.decode('utf-8', 'replace')
            print(stdout, end='')
            print(result.stderr.decode('utf-8', 'replace'), end='', file=sys.stderr)
            if result.returncode:
                raise RuntimeError(f'Native {args.mode} exited {result.returncode}')
            finish = rows(stdout, 'NOTICE_FINISH')
            if len(finish) != 1 or finish[0].get('mode') != args.mode or any(
                    finish[0].get(key) != value for key, value in
                    {'cleanup': '1', 'compact-nbt-error': '544', 'owned-nbt-error': '568', 'windows-absent': '1'}.items()):
                raise RuntimeError('Actual checked cleanup/compact error receipt')
            draw_rows = rows(stdout, 'NOTICE_DRAW')
            if not draw_rows or len({draw['label'] for draw in draw_rows}) != len(draw_rows):
                raise RuntimeError('Unique actual Notice draw receipts')
            for draw in draw_rows:
                scalars(draw)
                if draw['item'] == '1':
                    if (draw['pc'], draw['id'], draw['icon'], draw['vertices'], draw['item-vertices']) != (
                            '477', '340', '59', '4', '4'):
                        raise RuntimeError('Actual returned Book notice')
                    vertices(stdout, draw)
            png = module('notice_independent_png', ROOT / 'tests/tools/client/test_startup.py').decoded_png
            if args.mode == 'tutorial':
                summary = rows(stdout, 'NOTICE_WORLD')
                capture = rows(stdout, 'NOTICE_WORLD_CAPTURE')
                if (len(summary) != 1 or int(summary[0].get('active', '0')) < 3 or
                        int(summary[0].get('refreshes', '0')) < 2 or len(capture) != 1 or
                        summary[0].get('absent-openInventory') != '1'):
                    raise RuntimeError('Actual absent-stat DC refresh/World/SDK receipt')
                selected = next(draw for draw in draw_rows if draw['label'] == capture[0]['label'])
                report['pixels'] = book_pixels(output, 'world-tutorial', selected, assets, png)
            else:
                labels = {draw['label']: draw for draw in draw_rows}
                if set(labels) != {'inactive', 'completion', 'cache', 'expired',
                                   'inactive-after-expiry', 'tutorial-direct'}:
                    raise RuntimeError('Exact finite component cohort')
                if labels['cache']['bg-hit'] != '1' or labels['cache']['item-hit'] != '1':
                    raise RuntimeError('Actual repeated texture lookups')
                if args.mode == 'rounded' and (labels['completion']['width'], labels['completion']['height']) != ('321', '241'):
                    raise RuntimeError('Actual641x481 rounded VU projection')
                wrapped = rows(stdout, 'NOTICE_WRAP')
                if len([row for row in wrapped if 'case' in row]) != 12:
                    raise RuntimeError('12 actual READY Font wrap cases')
                report['pixels'] = {
                    label: book_pixels(output, label, labels[label], assets, png)
                    for label in ('completion', 'tutorial-direct')}
            report['draws'] = draw_rows
            if external_before is not None:
                # Bootstrap legitimately saves options and rotates statistics.
                # These are outputs in the proven disposable bind mount.
                # The outer launcher checks the untouched host tree separately.
                linux_namespace_receipt(game)
                report['private_namespace_outputs_changed'] = inventory(game) != external_before
                report['host_tree_continuity'] = 'requires successful outer namespace receipt'
        report['status'] = 'PASS'
    except subprocess.TimeoutExpired as error:
        for name, value in [('native.stdout', error.stdout), ('native.stderr', error.stderr)]:
            (output / name).write_bytes(value or b'')
        report['error'] = 'native timeout'
        report['status'] = 'FAIL'
        raise
    except Exception as error:
        report['error'] = str(error)
        report['status'] = 'FAIL'
        raise
    finally:
        after = input_digests(helpers, executable)
        assets_after = inventory(assets)
        report['inputs_before'], report['inputs_after'] = before, after
        report['assets_before'], report['assets_after'] = assets_before, assets_after
        if before != after or assets_before != assets_after:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'runtime inputs/products/assets changed'
        report['outputs'] = {name: value for name, value in inventory(output).items()
                             if name != 'run.json'}
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if report['status'] != 'PASS' and 'continuity_error' in report:
            raise RuntimeError(report['continuity_error'])
    print(f'Active Notice {args.mode} PASS; {output / "run.json"}')


if __name__ == '__main__':
    main()
