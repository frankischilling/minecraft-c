"""Real SDK live Workbench, Source window stop and native-only terminal join."""
import argparse
import gzip
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


A = module('open_shutdown_fixture', ROOT / 'tests/tools/client/test_workbench_auto_close_loop.py')
B = A.B
require = A.require


def metadata(mining, selected, world, name):
    wire = mining.mining_metadata(selected, world, name)
    empty = world.tag(9, 'Inventory', b'\x0a\x00\x00\x00\x00')
    records = []
    for slot, item, count in ((0, 270, 1), (9, 17, 2), (10, 5, 7)):
        records.append(world.tag(1, 'Slot', bytes([slot])) +
                       world.tag(2, 'id', struct.pack('>h', item)) +
                       world.tag(1, 'Count', bytes([count])) +
                       world.tag(2, 'Damage', b'\x00\x00') + b'\x00')
    require(wire.count(empty) == 1, 'One exact declared pre-load Inventory')
    return wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' +
                        struct.pack('>i', len(records)) + b''.join(records)))


def receipts(mining, stdout, profile):
    values = {name: mining.rows(stdout, 'WORKBENCH_OPEN_' + name.upper()) for name in
              ('setup', 'prepare', 'transfer', 'denials', 'busy', 'release', 'terminal', 'join', 'finish')}
    require(values['setup'] == [{'sdk-open': '1', 'sdk-grid': '1', 'sdk-cursor': '1',
            'result-preview': '1', 'actual-aliases': '1'}], 'Genuine SDK live Workbench scene')
    prepare = values['prepare']
    require(len(prepare) == 1 and set(prepare[0]) == {'profile', 'returned', 'source-unchanged',
            'world-ticks', 'stacks', 'entities', 'randoms', 'r', 'e', 'j'} and
            prepare[0]['profile'] == profile and prepare[0]['returned'] == '1' and
            prepare[0]['source-unchanged'] == '1' and prepare[0]['world-ticks'] == '0' and
            int(prepare[0]['stacks']) >= 51 and int(prepare[0]['entities']) > 0 and
            int(prepare[0]['randoms']) > 0 and prepare[0]['e'] == prepare[0]['j'],
            'Actual prepared same-IT Source graph, with zero additional World ticks')
    require(values['transfer'] == [{'profile': profile, 'same-it': '1', 'source-aliases': '1',
            'sealed': '1', 'owner-moved': '1', 'idempotent': '1'}],
            'Authoritative same-owner handoff and repeated prepare without double consumption')
    require(values['denials'] == [{'profile': profile, 'getters-revoked': '1',
            'player-retain': 'busy', 'container-retain': 'busy', 'ix-set': 'busy',
            'source-set': 'busy', 'router': 'refused', 'frame': 'refused'}],
            'Closed native new-right and Source-producer admission')
    busy = []
    if profile == 'container':
        busy = [{'profile': profile, 'phase': 'pre-transfer', 'source-unchanged': '1',
                 'native-owner-unchanged': '1'}]
    elif profile in ('player', 'world'):
        busy = [{'profile': profile, 'phase': 'world-audit', 'source-unchanged': '1',
                 'same-it': '1', 'status': 'busy'}]
    require(values['busy'] == busy, 'Exact declared external-hold refusal phase')
    require(values['release'] == [{'profile': profile, 'existing-hold-only': '1',
            'released': 'none' if profile == 'clean' else profile, 'same-it': '1'}],
            'Only the existing declared external hold is released')
    require(values['terminal'] == [{'profile': profile, 'native-prepare': '1',
            'source-close': '0', 'source-save': '0', 'unchanged': '1'}],
            'Native-only terminal join without Source close or forced World save')
    join = values['join']
    require(len(join) == 1 and set(join[0]) == {'profile', 'attempts', 'busy', 'limit', 'status',
            'slot-null'} and join[0]['profile'] == profile and join[0]['limit'] == '35000' and
            join[0]['status'] == '0' and join[0]['slot-null'] == '1',
            'Completed native bootstrap join with its consumed owner')
    attempts, busy_count = int(join[0]['attempts']), int(join[0]['busy'])
    require(1 <= attempts <= 35000 and busy_count == attempts - 1 and
            join[0]['attempts'] == str(attempts) and join[0]['busy'] == str(busy_count),
            'Only retained-owner BUSY responses precede successful bootstrap consumption')
    finish = values['finish']
    require(len(finish) == 1 and set(finish[0]) == {'profile', 'native-cleanup', 'windows-absent',
            'source-close', 'source-save', 'checks'} and finish[0]['profile'] == profile and
            finish[0]['native-cleanup'] == finish[0]['windows-absent'] == '1' and
            finish[0]['source-close'] == finish[0]['source-save'] == '0' and
            int(finish[0]['checks']) > 0, 'Checked complete graph cleanup and absent source windows')
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--profile', choices=('clean', 'container', 'player', 'world'), default='clean')
    args = parser.parse_args()
    require(os.name == 'nt', 'Windows source-window SDK qualification')
    mining = module('open_shutdown_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    selected = mining.module('open_shutdown_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('open_shutdown_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='workbench-open-' + args.profile + '-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
              ROOT / 'tests/tools/world/test_source_saving.py'}
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*')
                      if path.is_file() and path.suffix in ('.c', '.h', '.inc', '.cmake', '.py'))
    inputs.update(executable.parent.glob('*.dll'))
    before_inputs = {str(path): mining.digest(path) for path in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'profile': args.profile,
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'authored_inventory': [[0, 270, 1, 0], [9, 17, 2, 0], [10, 5, 7, 0]],
              'authored_table': [0, 64, 2, 58, 0],
              'terminal_scope': 'Native-only graph join after actual windowClosing/source f; Minecraft.d, Source close and forced save are not supplied.',
              'input_scope': 'Selected source/harness/CMake/executable/siblingDLLs and imported resources; excludes toolchain and Desktop state'}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            game = Path(temporary) / '.minecraft'
            game.mkdir()
            report['statistics_input'] = B.statistics_input(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Open Workbench shutdown', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                wire = metadata(mining, selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
            region = saves / 'alpha/region'
            region.mkdir()
            A.saved_regions(selected, world, region)
            unrelated = region / 'r.9.9.mcr'
            unrelated.write_bytes(bytes(8192))
            unrelated_before = unrelated.read_bytes()
            before = world.save_title_files(saves, evidence / 'before')
            home = game.parent
            environment = os.environ.copy()
            environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            result = world.retained_native_run([str(executable), str(assets), str(evidence), args.profile],
                environment, evidence, saves, before, 'workbench-open', time.time_ns() // 1000000, 120)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            report['setup_receipts'] = mining.rows(result.stdout, 'WORKBENCH_OPEN_SETUP')
            report['prepare_receipts'] = mining.rows(result.stdout, 'WORKBENCH_OPEN_PREPARE')
            report['busy_receipts'] = mining.rows(result.stdout, 'WORKBENCH_OPEN_BUSY')
            report['native_returncode'] = result.returncode
            image = evidence / 'workbench-open-shutdown.png'
            report['live_ui_capture'] = image.is_file() and image.stat().st_size > 1000
            stopped = evidence / 'snapshots/open-stopped'
            prepared = evidence / 'snapshots/open-after-prepare'
            if stopped.is_dir() and prepared.is_dir():
                report['stopped_disk'] = world.snapshot(stopped)
                report['prepared_disk'] = world.snapshot(prepared)
                report['native_prepare_disk_unchanged'] = report['stopped_disk'] == report['prepared_disk']
            require(result.returncode == 0, 'Native live-OO terminal preparation and retirement must return')
            report['receipts'] = receipts(mining, result.stdout, args.profile)
            require(report['live_ui_capture'], 'Actual returned source-window capture before quit')
            phases = ['open-after-prepare', 'open-after-release', 'open-after-native-close']
            if args.profile == 'container':
                phases.append('open-container-busy')
            elif args.profile in ('player', 'world'):
                phases.append('open-audit-busy')
            report['phase_disks'] = {}
            for phase in phases:
                directory = evidence / 'snapshots' / phase
                require(directory.is_dir(), 'Actual stopped-phase disk snapshot ' + phase)
                report['phase_disks'][phase] = world.snapshot(directory)
                require(report['phase_disks'][phase] == report['stopped_disk'],
                        'Selected save tree unchanged through native phase ' + phase)
            require(world.snapshot(saves) == report['stopped_disk'],
                    'Selected save tree unchanged through native cleanup; no forced World save')
            require(unrelated.read_bytes() == unrelated_before, 'Unrelated region bytes preserved')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'error': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): mining.digest(path) for path in sorted(inputs)}
        report['assets_after'] = world.snapshot(assets)
        report['inputs_unchanged'] = report['inputs_after'] == before_inputs
        report['assets_unchanged'] = report['assets_after'] == before_assets
        if not report['inputs_unchanged'] or not report['assets_unchanged']:
            report['status'] = 'FAIL'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Complete native join and selected input continuity')
    print('Live Workbench native shutdown PASS: ' + str(evidence))


if __name__ == '__main__':
    main()
