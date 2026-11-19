r"""Build or compare native Windows client frame observers in one authored sky scene.

Build from an existing Release Ninja/MSVC SDK configuration:
  python scripts/benchmarks/client_frames.py --build build-windows --out frame-builds

Compare already-built observers from two revisions with generated resources:
  python scripts/benchmarks/client_frames.py --executable before=PATH --executable after=PATH \
      --authored-assets --artifacts frame-runs --rounds 3

Use an existing local resource directory when comparing that presentation:
  python scripts/benchmarks/client_frames.py --executable before=PATH --executable after=PATH \
      --assets IMPORTED_ASSETS --artifacts frame-runs --rounds 3

The fixed scene uses seed 123, Tiny distance, 854x480 and saved Rotation 180,-30.
Each fresh process warms up naturally for 2s and measures 10s of ordinary Source
frames on DISPLAY2 with fixed input. All attempts remain in artifacts.
There is no universal FPS or original-game parity claim and no FPS threshold.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from client_frames_build import build_observer
from client_frames_run import module, qualify_cohort

ROOT = Path(__file__).resolve().parents[2]


def author_frame_assets(assets):
    hud = module('frame_authored_hud', 'tests/tools/client/test_gui_ingame_draw.py')
    startup = module('frame_authored_startup', 'tests/tools/client/test_startup.py')
    hud.authored_assets(assets)
    # The saved statistics fixture needs two distinct hexadecimal identifiers.
    (assets / 'jar/achievement/map.txt').write_bytes(b'5242880,ae017\r\n5242881,be018\r\n')

    def checker(x, y):
        return 40 + (x // 16) % 2 * 100, 70 + (y // 16) % 2 * 110, 150, 255

    texture = startup.authored_png(256, 256, checker)
    for name in ('item/arrows.png', 'mob/char.png', 'misc/shadow.png', 'misc/particlefield.png',
                 'particles.png', 'misc/water.png', 'misc/pumpkinblur.png'):
        path = assets / 'jar' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists():
            path.write_bytes(texture)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build', type=Path, help='Existing built Ninja/MSVC Release SDK directory')
    parser.add_argument('--out', type=Path, help='Parent directory for the optional observer build')
    parser.add_argument('--executable', action='append', default=[], metavar='LABEL=PATH')
    fixtures = parser.add_mutually_exclusive_group()
    fixtures.add_argument('--assets', type=Path, help='Local resource directory')
    fixtures.add_argument('--authored-assets', action='store_true',
                          help='Generate test textures and resources for this cohort')
    parser.add_argument('--artifacts', type=Path, help='Parent directory for retained run evidence')
    parser.add_argument('--rounds', type=int, default=3)
    options = parser.parse_args()
    if os.name != 'nt':
        parser.error('This observer qualifies Windows native SDK frames only')
    if not 1 <= options.rounds <= 10:
        parser.error('--rounds must be between 1 and 10')
    executables = {}
    for value in options.executable:
        label, separator, path = value.partition('=')
        if not separator or not label or label in executables or not all(
                c.isascii() and (c.isalnum() or c in '-_') for c in label):
            parser.error('Use unique LABEL=PATH values with simple filename labels')
        executables[label] = Path(path).resolve(strict=True)
    build_record = None
    if options.build is not None:
        if options.out is None:
            parser.error('--out is required with --build')
        if 'current' in executables:
            parser.error('The generated observer uses the reserved label current')
        executable, build_record = build_observer(options.build, options.out)
        executables['current'] = executable
        print('Observer built: ' + str(executable), flush=True)
        if options.assets is None and not options.authored_assets and options.artifacts is None:
            print('Build receipt: ' + str(build_record))
            return 0
    if not executables or options.artifacts is None or (options.assets is None and
                                                        not options.authored_assets):
        parser.error('Running needs --executable (or --build), --assets or --authored-assets, '
                     'and --artifacts')
    assets = options.assets.resolve(strict=True) if options.assets is not None else None
    options.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='client-frames-', dir=options.artifacts.resolve()))
    if assets is None:
        assets = output / 'assets'
        author_frame_assets(assets)
    children = output / 'runs'
    children.mkdir()
    report = {'status': 'FAIL', 'scope': __doc__, 'profiles': [], 'rounds': options.rounds,
              'build_record': None if build_record is None else str(build_record),
              'asset_fixture': 'authored' if options.authored_assets else 'provided',
              'qualification': 'Only complete focused screen-free intervals support this finite scene comparison. No failed attempt is discarded.'}
    for round_index in range(options.rounds):
        order = list(executables)
        if round_index & 1:
            order.reverse()
        for label in order:
            before = set(children.iterdir())
            command = [sys.executable, '-B', str(Path(__file__).with_name('client_frames_run.py')),
                       '--executable', str(executables[label]), '--assets', str(assets),
                       '--artifacts', str(children)]
            result = subprocess.run(command, capture_output=True, timeout=300)
            (output / f'{round_index}-{label}.stdout').write_bytes(result.stdout)
            (output / f'{round_index}-{label}.stderr').write_bytes(result.stderr)
            created = set(children.iterdir()) - before
            if len(created) != 1 or not (next(iter(created)) / 'run.json').is_file():
                raise RuntimeError('Observer driver did not retain one complete process report; see ' + str(output))
            path = created.pop() / 'run.json'
            observation = json.loads(path.read_text())
            report['profiles'].append({'round': round_index, 'label': label,
                                       'driver_exit': result.returncode,
                                       'retained_report': str(path), 'observation': observation})
            (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
            print(label, round_index, observation['status'], flush=True)
    report.update(qualify_cohort(report['profiles']))
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    summary = [{'label': p['label'], 'round': p['round'], 'status': p['observation']['status'],
                'measurement': p['observation'].get('summary', {}).get('measurement'),
                'fps': p['observation'].get('summary', {}).get('measured_fps'),
                'host_frame': p['observation'].get('summary', {}).get('host_frame')}
               for p in report['profiles']]
    print(json.dumps({'status': report['status'], 'report': str(output / 'run.json'),
                      'matched': report['authored_scene_and_camera_matched'], 'profiles': summary}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
