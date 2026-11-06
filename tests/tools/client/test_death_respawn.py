"""Exercise two ordinary Death-screen respawns and a retained native failure."""
import argparse
import gzip
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import time

sys.dont_write_bytecode = True


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'This APPDATA integration profile requires Windows')
    spec = importlib.util.spec_from_file_location('death_world_fixture',
                                                Path(__file__).with_name('test_world_selection_loop.py'))
    fixture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fixture)
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='death-respawn-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'
    game = appdata / '.minecraft'
    fixture.repeat_statistics(game, assets)
    saves = game / 'saves'
    saves.mkdir()
    (saves / 'preserve-me.bin').write_bytes(b'unrelated save-directory file\x00\xff')
    for folder, display, played in (('older', 'Old world', 1303000000000),
                                    ('bravo', '', 1304000000000),
                                    ('alpha', 'Alpha world', 1304000000000)):
        directory = saves / folder
        directory.mkdir()
        (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(
            gzip.compress(fixture.metadata(display, played, 19132), mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored untouched chunk')
    regions = saves / 'alpha/region'
    regions.mkdir()
    fixture.saved_regions(regions, generate=False)
    before_resources = fixture.snapshot(assets)
    before_files = fixture.save_title_files(saves, evidence / 'before')
    environment = os.environ.copy()
    environment['APPDATA'] = str(appdata)
    capture = evidence / 'last-returned-frame.png'
    result = fixture.retained_native_run(
        [str(executable), str(assets), 'repeat', str(capture)], environment, evidence,
        saves, before_files, 'death-respawn', time.time_ns() // 1000000, 180)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    require(result.returncode == 0 and result.stderr == '', 'Actual loaded Death/Respawn native failure')
    rows = [line for line in result.stdout.splitlines() if line.startswith('DEATH_RESPAWN ')]
    require(len(rows) == 4, 'Two complete cycles, native failure prefix and checked display retirement')
    for cycle in range(2):
        require(rows[cycle] == (
            f'DEATH_RESPAWN cycle={cycle} direct-fatal-body=1 actual-health-tick=1 screen=ch '
            'respawn-button=1 new-player=1 new-inventory=1 same-world=1 same-id=1 health=20 '
            'input=1 terrain=289 source-return=372 wrapper-return=1 gui-return=30 child-retired=1'),
            'Actual fatal-body, screen, click, replacement, terrain and returned-frame receipt')
    require(rows[2] == 'DEATH_RESPAWN partial-publication=1 source-pc=303 new-player-input-null=1 '
            'rejected-output-preserved=1 callback-cause=native-io pending-call-retired=1 native-cleanup-next=1',
            'Typed partial-replacement/refused-output receipt')
    require(rows[3] == 'DEATH_RESPAWN borrowed-display-before=1 terminal-prefix=1 '
            'borrowed-display-cleared=1 common-cleanup-next=1',
            'Borrowed display view ends before terminal dispatcher retirement')
    footer = [line for line in result.stdout.splitlines() if line.startswith('WORLD_SELECTION mode=repeat cleanup=')]
    require(len(footer) == 1 and re.fullmatch(
        r'WORLD_SELECTION mode=repeat cleanup=1 checks=[1-9][0-9]*', footer[0]) is not None,
        'Checked loop, partial graph and bootstrap retirement')
    require(fixture.snapshot(assets) == before_resources, 'Imported resource bytes unchanged')
    require(capture.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), 'Retained preceding returned-frame PNG')
    report = dict(status='PASS', cycles=2, native_partial_failure=1,
                  scope='Loaded native direct fatal-body inputs; ordinary CH button events; no original whole-frame, death-input or FPS qualification',
                  footer=footer[0], resources_unchanged=True)
    (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Death/Respawn scene: PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
