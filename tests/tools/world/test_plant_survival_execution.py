"""Retain actual plant light/material ownership controls and bounded native receipts."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
TIMEOUT = 180
MODES = ('descendant', 'source-descendant', 'source', 'sky-visible', 'sky-hidden',
         'negative-y', 'defining-class', 'sky-descendant', 'sky-source-descendant', 'sky-source',
         'mushroom-descendant', 'mushroom-source-descendant', 'mushroom-source',
         'mushroom-negative-y', 'mushroom-high-y', 'mushroom-bright',
         'mushroom-opaque-true', 'mushroom-opaque-false', 'mushroom-null-o',
         'cactus-descendant', 'cactus-source-descendant', 'cactus-source', 'cactus-completion',
         'reed-descendant', 'reed-source-descendant', 'reed-source', 'reed-completion',
         'cactus-solid-first', 'cactus-null-material', 'cactus-supported',
         'reed-stacked', 'reed-no-substrate', 'reed-null-material', 'reed-canonical-refusal',
         'reed-west-water', 'reed-east-water', 'reed-north-water', 'reed-south-water', 'reed-dry')
FAILURE_MODES = ('descendant', 'source-descendant', 'source', 'sky-descendant',
                 'sky-source-descendant', 'sky-source', 'mushroom-descendant',
                 'mushroom-source-descendant', 'mushroom-source')
RECEIPT_KEYS = {'command', 'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete',
                'reader_errors', 'output_overflow', 'stdout_file', 'stderr_file', 'stdout_bytes',
                'stderr_bytes', 'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def pin(path):
    require(path.is_file() and not path.is_symlink(), 'Regular literal input required')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepared():
    # The existing independently checked builder supplies one real source
    # Chunk at (1,0), with its actual Item262/5/7 at (24.25,64,8.25).
    builder = module('plant_survival_source_inputs',
                     ROOT / 'tests/tools/world/test_mutation_support_execution.py')
    return builder.prepared()


def result_line(mode):
    require(type(mode) is str and mode in MODES, 'Declared finite plant mode')
    if mode.startswith(('cactus-', 'reed-')):
        cactus = mode.startswith('cactus-')
        branch = mode.removeprefix('cactus-' if cactus else 'reed-')
        if branch in ('descendant', 'source-descendant', 'source', 'completion'):
            raw = 'OK' if branch == 'completion' else 'BUSY' if branch == 'descendant' else 'IO_ERROR'
            native = 'UNSUPPORTED_STATE' if branch == 'completion' else 'OK' if branch == 'source' else 'BUSY'
            return (f'PLANT_SURVIVAL_RESULT mode={mode} raw={raw} native={native} callbacks=1 '
                    f'site={"or.g25" if cactus else "ri.a76"} query=fd.f4 output-preserved=1 '
                    'Chunk.a=0 retained=1 drained=1 replay=0')
        if branch == 'canonical-refusal':
            return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=UNCHANGED native=BUSY callbacks=0 '
                    'query=none output-preserved=1 canonical-math=1 entered=0 replay=0')
        if mode == 'reed-null-material':
            return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=OK native=OK callbacks=0 '
                    'query=fd.f4 nullable=1 reached-get=1 replay=0')
        if mode == 'cactus-null-material':
            return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=UNSUPPORTED_STATE native=OK callbacks=0 '
                    'site=or.g10 query=fd.f4 registry=NULL_REFERENCE output-preserved=1 '
                    'material-completed=1 later-query=0 replay=0')
        entered = mode not in ('reed-stacked', 'reed-no-substrate')
        pc = (0 if not entered else 7 if mode == 'cactus-solid-first' else 61 if cactus else
              {'reed-west-water': 56, 'reed-east-water': 76, 'reed-north-water': 96,
               'reed-south-water': 116, 'reed-dry': 116}[mode])
        answer = mode in ('cactus-supported', 'reed-stacked') or mode.endswith('-water')
        return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=OK native=OK callbacks=0 value={int(answer)} '
                f'material-pc={pc} material-entered={int(entered)} '
                f'below-entered={int(mode != "cactus-solid-first")} cold-acquisitions=0 replay=0')
    if mode in FAILURE_MODES:
        sky = mode.startswith('sky-')
        mushroom = mode.startswith('mushroom-')
        branch = mode[4:] if sky else mode[9:] if mushroom else mode
        raw = 'BUSY' if branch == 'descendant' else 'IO_ERROR'
        native = 'OK' if branch == 'source' else 'BUSY'
        site, query = (('wb.g18', 'fd.l7') if sky else ('tl.g18', 'fd.m23') if mushroom
                       else ('wb.g5', 'fd.m23'))
        return (f'PLANT_SURVIVAL_RESULT mode={mode} raw={raw} native={native} callbacks=1 '
                f'site={site} query={query} output-preserved=1 Chunk.a=0 retained=1 drained=1 replay=0')
    if mode.startswith('mushroom-'):
        if mode == 'mushroom-null-o':
            return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=UNSUPPORTED_STATE native=OK callbacks=0 '
                    'site=tl.d4 caller=tl.g37 query=fd.m18 registry=NULL_REFERENCE '
                    'output-preserved=1 substrate=1 Chunk.a=0 replay=0')
        outside = mode in ('mushroom-negative-y', 'mushroom-high-y')
        if outside or mode == 'mushroom-bright':
            return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=OK native=OK callbacks=0 '
                    f'site={"tl.g0" if outside else "tl.g18"} '
                    f'query={"none" if outside else "fd.m"} value=0 '
                    f'reached-get={0 if outside else 1} substrate=0 null-o=1 Chunk.a=0 replay=0')
        opaque = int(mode == 'mushroom-opaque-true')
        return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=OK native=OK callbacks=0 site=tl.g37 '
                f'query=fd.m value={opaque} reached-get=1 substrate=1 opaque={opaque} Chunk.a=0 replay=0')
    if mode in ('sky-visible', 'sky-hidden'):
        value = int(mode == 'sky-visible')
        return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=OK native=OK callbacks=0 site=wb.g18 '
                f'query=fd.l value={value} substrate={value} asymmetric-height=1 Chunk.a=0 replay=0')
    negative = mode == 'negative-y'
    return (f'PLANT_SURVIVAL_RESULT mode={mode} raw=OK native=OK callbacks=0 query=fd.m '
            f'value={0 if negative else 12} reached-get={0 if negative else 1} '
            f'defining-Chunk.a={0 if negative else 1} World-Chunk.a=0 replay=0')


def validate_receipt(receipt, command, output, mode):
    require(type(mode) is str and mode in MODES, 'Declared finite plant mode')
    require(type(receipt) is dict and set(receipt) == RECEIPT_KEYS, 'Closed process receipt')
    require(type(command) is list and len(command) == 3 and command[1] == mode and
            all(type(word) is str for word in command) and type(receipt['command']) is list and
            receipt['command'] == command, 'Actual process command')
    require(type(receipt['exit']) is int and receipt['exit'] == 0 and
            type(receipt['timeout_seconds']) is int and receipt['timeout_seconds'] == TIMEOUT and
            receipt['timed_out'] is False and receipt['pipe_capture_complete'] is True and
            receipt['output_overflow'] is False and type(receipt['reader_errors']) is dict and
            receipt['reader_errors'] == {}, 'Complete successful bounded process')
    streams = {}
    for name, limit in (('stdout', 4 * 1024 * 1024), ('stderr', 1024 * 1024)):
        filename = mode + '.' + name
        path = output / filename
        require(receipt[name + '_file'] == filename and path.is_file() and not path.is_symlink(),
                'Literal captured stream file')
        require(type(receipt[name + '_limit']) is int and receipt[name + '_limit'] == limit and
                type(receipt[name + '_bytes']) is int and 0 <= receipt[name + '_bytes'] <= limit and
                receipt[name + '_bytes'] == path.stat().st_size and
                receipt[name + '_sha256'] == pin(path), 'Actual captured extent/hash')
        streams[name] = path.read_bytes()
    require(streams['stderr'] == b'' and streams['stdout'].endswith(b'\n'), 'Native output completion')
    stdout = streams['stdout'].replace(b'\r\n', b'\n')
    require(b'\r' not in stdout, 'Native output newlines')
    lines = stdout.decode('ascii').splitlines()
    require(len(lines) == 6 and lines[:4] == [f'PLANT_SURVIVAL_BEGIN schema=1 mode={mode}',
            '16 achievements', '151 recipes', result_line(mode)] and
            lines[4] == 'PLANT_SURVIVAL_OPEN finite-owned-leaf/full-wake/Arrow/original',
            'Exact finite mode and native control sequence')
    footer = re.fullmatch(r'PASS: ([1-9][0-9]{0,5}) actual owned plant survival checks;mode=' +
                          re.escape(mode), lines[5])
    require(footer is not None, 'Native terminal check count')
    return int(footer[1])


def self_test():
    inputs = prepared()
    require(set(inputs) == {'world/level.dat', 'world/region/r.0.0.mcr',
                            'world/data/idcounts.dat', 'world/preserve.bin'} and
            all(type(value) is bytes and value for value in inputs.values()), 'Closed authored inputs')
    with tempfile.TemporaryDirectory(prefix='plant-survival-schema-') as work:
        output = Path(work)
        cases = {}
        for mode in MODES:
            stdout = ('\n'.join([f'PLANT_SURVIVAL_BEGIN schema=1 mode={mode}', '16 achievements',
                '151 recipes', result_line(mode),
                'PLANT_SURVIVAL_OPEN finite-owned-leaf/full-wake/Arrow/original',
                f'PASS: 1 actual owned plant survival checks;mode={mode}']) + '\n').encode('ascii')
            (output / (mode + '.stdout')).write_bytes(stdout)
            (output / (mode + '.stderr')).write_bytes(b'')
            command = ['synthetic-native', mode, 'synthetic-input']
            receipt = {'command': command, 'exit': 0, 'timeout_seconds': TIMEOUT, 'timed_out': False,
                'pipe_capture_complete': True, 'reader_errors': {}, 'output_overflow': False,
                'stdout_file': mode + '.stdout', 'stderr_file': mode + '.stderr',
                'stdout_bytes': len(stdout), 'stderr_bytes': 0, 'stdout_sha256': pin(output / (mode + '.stdout')),
                'stderr_sha256': pin(output / (mode + '.stderr')), 'stdout_limit': 4 * 1024 * 1024,
                'stderr_limit': 1024 * 1024}
            require(validate_receipt(receipt, command, output, mode) == 1,
                    'Synthetic receipt positive: ' + mode)
            cases[mode] = command, receipt
        command, receipt = cases['descendant']
        refused = 0
        for key, value in (('extra', 1), ('exit', False), ('exit', 1), ('timed_out', True),
                ('pipe_capture_complete', False), ('output_overflow', True),
                ('reader_errors', {'stdout': ['lost']}), ('stdout_bytes', False),
                ('stdout_sha256', '0' * 64), ('stdout_file', '../outside'), ('command', ['other'])):
            bad = copy.deepcopy(receipt)
            bad[key] = value
            try:
                validate_receipt(bad, command, output, 'descendant')
            except ValueError:
                refused += 1
            else:
                raise ValueError('Malformed synthetic receipt accepted: ' + key)
        for bad_mode in ('flower', False, 'sky-source-extra'):
            try:
                validate_receipt(receipt, command, output, bad_mode)
            except ValueError:
                refused += 1
            else:
                raise ValueError('Undeclared or non-string mode accepted')
        for mode, before, after in (
                ('descendant', 'raw=BUSY native=BUSY', 'raw=OK native=OK'),
                ('source', 'raw=IO_ERROR native=OK', 'raw=IO_ERROR native=BUSY'),
                ('source-descendant', 'output-preserved=1', 'output-preserved=0'),
                ('sky-visible', 'query=fd.l', 'query=fd.m'),
                ('sky-hidden', 'asymmetric-height=1', 'asymmetric-height=0'),
                ('negative-y', 'reached-get=0', 'reached-get=1'),
                ('defining-class', 'defining-Chunk.a=1 World-Chunk.a=0',
                 'defining-Chunk.a=0 World-Chunk.a=1'),
                ('sky-descendant', 'site=wb.g18', 'site=wb.g5'),
                ('sky-source-descendant', 'query=fd.l7', 'query=fd.m23'),
                ('sky-source', 'raw=IO_ERROR native=OK', 'raw=IO_ERROR native=BUSY'),
                ('mushroom-descendant', 'site=tl.g18', 'site=wb.g5'),
                ('mushroom-source-descendant', 'query=fd.m23', 'query=fd.l7'),
                ('mushroom-source', 'raw=IO_ERROR native=OK', 'raw=IO_ERROR native=BUSY'),
                ('mushroom-negative-y', 'reached-get=0', 'reached-get=1'),
                ('mushroom-high-y', 'value=0', 'value=1'),
                ('mushroom-bright', 'substrate=0', 'substrate=1'),
                ('mushroom-opaque-true', 'value=1', 'value=0'),
                ('mushroom-opaque-false', 'opaque=0', 'opaque=1'),
                ('mushroom-null-o', 'registry=NULL_REFERENCE', 'registry=OK'),
                ('cactus-descendant', 'site=or.g25', 'site=or.g7'),
                ('cactus-source-descendant', 'query=fd.f4', 'query=fd.f18'),
                ('cactus-source', 'raw=IO_ERROR native=OK', 'raw=IO_ERROR native=BUSY'),
                ('cactus-completion', 'raw=OK native=UNSUPPORTED_STATE', 'raw=BUSY native=UNSUPPORTED_STATE'),
                ('reed-descendant', 'site=ri.a76', 'site=ri.a56'),
                ('reed-source-descendant', 'query=fd.f4', 'query=fd.f18'),
                ('reed-source', 'raw=IO_ERROR native=OK', 'raw=IO_ERROR native=BUSY'),
                ('reed-completion', 'raw=OK native=UNSUPPORTED_STATE', 'raw=BUSY native=UNSUPPORTED_STATE'),
                ('cactus-solid-first', 'below-entered=0', 'below-entered=1'),
                ('cactus-null-material', 'material-completed=1', 'material-completed=0'),
                ('cactus-supported', 'material-pc=61', 'material-pc=7'),
                ('reed-stacked', 'material-entered=0', 'material-entered=1'),
                ('reed-no-substrate', 'value=0', 'value=1'),
                ('reed-null-material', 'nullable=1', 'nullable=0'),
                ('reed-canonical-refusal', 'canonical-math=1', 'canonical-math=0'),
                ('reed-west-water', 'material-pc=56', 'material-pc=116'),
                ('reed-east-water', 'material-pc=76', 'material-pc=116'),
                ('reed-north-water', 'material-pc=96', 'material-pc=116'),
                ('reed-south-water', 'material-entered=1', 'material-entered=0'),
                ('reed-dry', 'value=0', 'value=1')):
            command, receipt = cases[mode]
            path = output / (mode + '.stdout')
            original = path.read_bytes()
            require(original.count(before.encode('ascii')) == 1, 'One declared receipt field')
            path.write_bytes(original.replace(before.encode('ascii'), after.encode('ascii'), 1))
            bad = copy.deepcopy(receipt)
            bad['stdout_bytes'] = path.stat().st_size
            bad['stdout_sha256'] = pin(path)
            try:
                try:
                    validate_receipt(bad, command, output, mode)
                except ValueError:
                    refused += 1
                else:
                    raise ValueError('Corrupt mode-specific receipt accepted')
            finally:
                path.write_bytes(original)
        require(refused == 53, 'All malformed receipts and finite semantic controls refused')
    print('PASS: authored slot1 Item input; 39 synthetic modes; 53 malformed receipts refused; no native run')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    require(args.executable is not None and args.work_dir is not None, 'Executable and work directory required')
    executable = args.executable.resolve(strict=True)
    require(executable.is_file() and not executable.is_symlink(), 'Literal native executable')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='plant-survival-', dir=args.work_dir.resolve()))
    capture_path = ROOT / 'tests/reference/client/controller/capture.py'
    capture = module('plant_survival_bounded_capture', capture_path)
    report = {'schema': 'PLANT_SURVIVAL_EXECUTION_RUN_V1', 'status': 'FAIL', 'modes': {},
        'scope': 'Thirty-nine finite owned plant light/material controls with actual SourceSaving Item activation, '
                 'declared light/height/material inputs and checked native drainage. '
                 'No original comparison, full wake, Arrow or terrain-generation qualification.'}
    before = None
    paths = []
    try:
        for mode in MODES:
            for relative, data in prepared().items():
                path = output / mode / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
        paths = [executable, Path(__file__).resolve(), capture_path,
            ROOT / 'cmake/tests/world_plant_survival_execution.cmake',
            ROOT / 'tests/world/runtime/world_access/test_plant_survival_execution.c',
            ROOT / 'tests/world/runtime/world_access/test_block_scalar_execution.c',
            ROOT / 'tests/world/runtime/world_access/test_collision_query_execution.c',
            ROOT / 'tests/world/runtime/tile_entities/test_tile_entities.c',
            ROOT / 'tests/tools/world/test_mutation_support_execution.py',
            ROOT / 'tests/tools/world/test_source_saving.py',
            ROOT / 'tests/tools/world/test_source_world_construction.py',
            ROOT / 'tests/tools/world/collision_query_inputs.py']
        paths.extend(path for mode in MODES for path in (output / mode).rglob('*') if path.is_file())
        for folder in ('src', 'include'):
            paths.extend(path for path in (ROOT / folder).rglob('*') if path.suffix in ('.c', '.h', '.inc'))
        paths = sorted(set(paths))
        before = {str(path): pin(path) for path in paths}
        report['input_before_sha256'] = before
        for mode in MODES:
            require(before == {str(path): pin(path) for path in paths}, 'Inputs unchanged before native entry')
            command = [str(executable), mode, str(output / mode)]
            receipt = capture.capture(command, output, mode, TIMEOUT)
            report['modes'][mode] = {'process': receipt}
            report['modes'][mode]['checks'] = validate_receipt(receipt, command, output, mode)
            report['modes'][mode]['receipt_sha256'] = pin(output / (mode + '.capture.json'))
        report['status'] = 'PASS_FINITE_NATIVE_CONTROLS_ONLY'
    except Exception as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['input_after_sha256'] = {str(path): pin(path) for path in paths}
        report['inputs_unchanged'] = before is not None and before == report['input_after_sha256']
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if before is not None:
            require(report['inputs_unchanged'], 'Executed input preservation')
    print('PASS: 39 actual owned plant survival modes; capture=' + str(output))


if __name__ == '__main__':
    main()
