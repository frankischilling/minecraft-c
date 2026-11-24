"""Two genuine SDK/World-tick callback controls; no ordinary save qualification."""
import argparse
import gzip
import importlib.util
import json
import os
from pathlib import Path
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


A = module('workbench_adversarial_scene', ROOT / 'tests/tools/client/test_workbench_auto_close_loop.py')
require = A.require


def receipts(mining, stdout, mode):
    failed = mode == 'failure'
    row = {'mode': mode, 'sdk-open': '1', 'actual-world-tick': '1', 'publications': '2',
           'callback-default-swap': '1', 'inherited-AA-returned': '1',
           'direct-IT-returned': '0' if failed else '1', 'GS151': '0' if failed else '1',
           'smoothing-suffix': '0' if failed else '1',
           'play-time-suffix': '0' if failed else '1', 'typed-receipt': '1' if failed else '0',
           'cleanup-no-replay': '1', 'ordinary-save': '0'}
    require(mining.rows(stdout, 'WORKBENCH_ADVERSARIAL') == [row],
            'Exact actual failed-prefix or fresh-default callback receipt')
    finish = mining.rows(stdout, 'WORKBENCH_ADVERSARIAL_FINISH')
    expected = {'mode': mode, 'cleanup': '1', 'windows-absent': '1', 'publications': '2',
                'diagnostic-after-router': '1', 'diagnostic-after-bootstrap': '1'}
    require(len(finish) == 1 and set(finish[0]) == set(expected) | {'checks'} and
            all(finish[0][key] == value for key, value in expected.items()) and
            finish[0]['checks'].isdigit() and int(finish[0]['checks']) > 0,
            'Checked native graph teardown and surviving value diagnostics')
    return {'control': row, 'finish': finish[0]}


def run(args, mode, mining, selected, world):
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    evidence = Path(tempfile.mkdtemp(prefix='workbench-adversarial-' + mode + '-',
                                    dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt'}
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*')
                      if path.is_file() and path.suffix in ('.c', '.h', '.inc', '.cmake', '.py'))
    inputs.update(executable.parent.glob('*.dll'))
    before_inputs = {str(path): mining.digest(path) for path in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'mode': mode,
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'ordinary_save_claim': False,
              'authored_inventory': [[0, 270, 1, 0], [9, 5, 7, 0], [11, 280, 2, 0]],
              'authored_table': [0, 64, 2, 58, 0],
              'adversarial_scope': 'Actual Entity position setter moves x to 9.0, outside the Table distance but within the separately checked ten-block smoothing delta. Actual second grid-Item entity_added callback changes d to a separately owned real AA; failure mode refuses that native callback. The World tick uses the real DC/display child.',
              'teardown_scope': 'Original d/e restored only after all prefix/suffix observations for independent fixture teardown; no Source invocation completion is credited to that restoration.'}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            game = Path(temporary) / '.minecraft'
            game.mkdir()
            report['statistics_input'] = A.B.statistics_input(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Adversarial close world', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                wire = (A.metadata(mining, selected, world, name) if folder == 'alpha' else
                        world.metadata(name, played, 19132))
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(
                    gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
            region = saves / 'alpha/region'
            region.mkdir()
            A.saved_regions(selected, world, region)
            unrelated = region / 'r.9.9.mcr'
            unrelated.write_bytes(bytes(8192))
            before = world.save_title_files(saves, evidence / 'before')
            environment = os.environ.copy()
            home = game.parent
            environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            result = world.retained_native_run([str(executable), str(assets), str(evidence), mode],
                environment, evidence, saves, before, 'workbench-adversarial-' + mode,
                time.time_ns() // 1000000, 600)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native callback control and checked teardown returned')
            report['receipt'] = receipts(mining, result.stdout, mode)
            after = json.loads((evidence / 'execution.json').read_text(encoding='utf-8'))['after']
            preserved = [path for path in before if path.startswith(('older/', 'bravo/')) or
                         path.endswith('/preserve-chunk.dat') or path == 'alpha/region/r.9.9.mcr']
            require(all(after.get(path) == before[path] for path in preserved),
                    'Every unrelated fixture file and region retains its exact bytes')
            report['unrelated_files_unchanged'] = preserved
            report['changed_selected_files'] = sorted(path for path in set(before) | set(after)
                if path.startswith('alpha/') and before.get(path) != after.get(path))
            image = evidence / 'workbench-adversarial-before.png'
            require(image.is_file() and image.stat().st_size > 1000,
                    'Actual SDK-opened pre-close source window capture')
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
    require(report['status'] == 'PASS', 'Complete native callback control and input continuity')
    print('Workbench adversarial ' + mode + ' PASS: ' + str(evidence))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--mode', choices=('failure', 'freshness'))
    args = parser.parse_args()
    require(os.name == 'nt', 'Windows SDK/composed callback qualification')
    args.artifacts.mkdir(parents=True, exist_ok=True)
    mining = module('workbench_adversarial_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    selected = mining.module('workbench_adversarial_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('workbench_adversarial_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    for mode in (args.mode,) if args.mode else ('failure', 'freshness'):
        run(args, mode, mining, selected, world)


if __name__ == '__main__':
    main()
