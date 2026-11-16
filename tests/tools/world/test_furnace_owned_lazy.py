"""Retain actual owned lazy Furnace controls with declared native fixture inputs."""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
MODES = ('direct-success', 'direct-failure', 'ray-success', 'ray-failure')
TIMEOUT, LIMIT = 210, 1048576


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def completed(child, mode, command):
    require(mode in MODES and type(command) is list and len(command) == 3 and
            all(type(value) is str for value in command) and command[1] == mode,
            'Exact selected native command')
    require(type(child.returncode) is int and child.returncode == 0 and
            type(child.stdout) is bytes and type(child.stderr) is bytes and
            child.stderr == b'' and len(child.stdout) <= LIMIT, 'Successful bounded native streams')
    capture = child.capture_report
    require(type(capture) is dict and child.args == command and
            capture.get('command') == command, 'Actual native command receipt')
    for key, value in (('exit', 0), ('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                       ('stdout_bytes', len(child.stdout)), ('stderr_bytes', 0)):
        require(type(capture.get(key)) is int and capture[key] == value,
                'Exact native capture value: ' + key)
    require(capture.get('reaped') is True and capture.get('pipe_capture_complete') is True and
            capture.get('timed_out') is False and capture.get('output_overflow') is False and
            type(capture.get('reader_errors')) is dict and capture['reader_errors'] == {},
            'Complete joined native capture')
    lines = child.stdout.replace(b'\r\n', b'\n').split(b'\n')
    expected = [('FURNACE_OWNED_LAZY_BEGIN schema=1 mode=' + mode).encode()]
    expected += [b'16 achievements', b'151 recipes'] * (2 if mode.startswith('ray-') else 1)
    expected += [('FURNACE_OWNED_LAZY_CASE ' + mode + ' BEGIN').encode(),
                ('FURNACE_OWNED_LAZY_CASE ' + mode + ' END').encode(),
                b'FURNACE_OWNED_LAZY_OPEN original-race/full-Arrow/full-SL']
    require(len(lines) == len(expected) + 2 and lines[:-2] == expected and lines[-1] == b'' and
            child.stdout in (b'\n'.join(lines), b'\r\n'.join(lines)),
            'Exact ordered native case and coherent final newline')
    footer = re.fullmatch(rb'PASS: ([1-9][0-9]{0,9}) actual owned lazy Furnace checks;mode=' +
                          mode.encode(), lines[-2])
    require(footer is not None and int(footer[1]) <= 4294967295,
            'Unique final positive uint32 completion')
    return int(footer[1])


def controls():
    class Controls(unittest.TestCase):
        def setUp(self):
            self.mode = MODES[0]
            self.command = ['native', self.mode, 'owned']
            stream = ('FURNACE_OWNED_LAZY_BEGIN schema=1 mode=direct-success\n'
                      '16 achievements\n151 recipes\n'
                      'FURNACE_OWNED_LAZY_CASE direct-success BEGIN\n'
                      'FURNACE_OWNED_LAZY_CASE direct-success END\n'
                      'FURNACE_OWNED_LAZY_OPEN original-race/full-Arrow/full-SL\n'
                      'PASS: 52 actual owned lazy Furnace checks;mode=direct-success\n').encode()
            capture = dict(command=self.command, exit=0, timeout_seconds=TIMEOUT, limit=LIMIT,
                           stdout_bytes=len(stream), stderr_bytes=0, reaped=True,
                           pipe_capture_complete=True, timed_out=False, output_overflow=False,
                           reader_errors={})
            self.child = SimpleNamespace(args=self.command, returncode=0, stdout=stream,
                                         stderr=b'', capture_report=capture)

        def rejected(self, child):
            with self.assertRaises(ValueError):
                completed(child, self.mode, self.command)

        def test_complete_transport(self):
            self.assertEqual(completed(self.child, self.mode, self.command), 52)
            self.child.stdout = self.child.stdout.replace(b'\n', b'\r\n')
            self.child.capture_report['stdout_bytes'] = len(self.child.stdout)
            self.assertEqual(completed(self.child, self.mode, self.command), 52)

        def test_exit_and_capture_failures(self):
            for key, value in (('reaped', False), ('pipe_capture_complete', False),
                               ('timed_out', True), ('output_overflow', True),
                               ('reader_errors', {'stdout': ['read']}), ('exit', False),
                               ('stdout_bytes', 0), ('timeout_seconds', 1), ('limit', True)):
                child = copy.deepcopy(self.child)
                child.capture_report[key] = value
                self.rejected(child)
            for value in (False, 1, -1):
                child = copy.deepcopy(self.child)
                child.returncode = value
                self.rejected(child)

        def test_footer_and_order(self):
            for old, new in ((b'52 actual', b'0 actual'), (b'52 actual', b'4294967296 actual'),
                             (b'52 actual', b'-1 actual'), (b' BEGIN\n', b' END\n'),
                             (b';mode=direct-success', b';mode=ray-success'),
                             (b'\nPASS:', b'\nextra\nPASS:')):
                child = copy.deepcopy(self.child)
                child.stdout = child.stdout.replace(old, new)
                child.capture_report['stdout_bytes'] = len(child.stdout)
                self.rejected(child)

        def test_command_and_stream_join(self):
            for field in ('args',):
                child = copy.deepcopy(self.child)
                setattr(child, field, self.command[:-1])
                self.rejected(child)
            child = copy.deepcopy(self.child)
            child.capture_report['command'] = ['other', self.mode, 'owned']
            self.rejected(child)
            for tail in (b'PASS: 52 actual owned lazy Furnace checks;mode=direct-success\n',
                         b'junk', b'\r\n'):
                child = copy.deepcopy(self.child)
                child.stdout += tail
                child.capture_report['stdout_bytes'] = len(child.stdout)
                self.rejected(child)
            child = copy.deepcopy(self.child)
            child.stderr = b'error'
            self.rejected(child)

        def test_exact_registry_setup_counts(self):
            child = copy.deepcopy(self.child)
            child.stdout = child.stdout.replace(b'16 achievements\n151 recipes\n', b'')
            child.capture_report['stdout_bytes'] = len(child.stdout)
            self.rejected(child)
            child = copy.deepcopy(self.child)
            child.stdout = child.stdout.replace(b'16 achievements\n151 recipes\n',
                                                b'16 achievements\n151 recipes\n' * 2)
            child.capture_report['stdout_bytes'] = len(child.stdout)
            self.rejected(child)
            self.mode = 'ray-success'
            self.command = ['native', self.mode, 'owned']
            child.stdout = child.stdout.replace(b'direct-success', b'ray-success')
            child.args = self.command
            child.capture_report['command'] = self.command
            child.capture_report['stdout_bytes'] = len(child.stdout)
            self.assertEqual(completed(child, self.mode, self.command), 52)
            child.stdout = child.stdout.replace(b'16 achievements\n151 recipes\n', b'', 1)
            child.capture_report['stdout_bytes'] = len(child.stdout)
            self.rejected(child)

    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    require(result.wasSuccessful(), 'Owned lazy Furnace decoder controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--mode', choices=MODES)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.controls:
        controls()
        return
    require(args.mode in MODES and args.executable is not None and args.work_dir is not None,
            'Mode, native executable and retained work directory required')
    guards = load('furnace_lazy_guards', 'tests/tools/client/test_chest_loop.py')
    saving = load('furnace_lazy_saving', 'tests/tools/world/test_source_saving.py')
    construction = load('furnace_lazy_construction', 'tests/tools/world/test_source_world_construction.py')
    recipe = load('furnace_lazy_inputs', 'tests/tools/world/arrow_owned_leaves_inputs.py')
    support = load('furnace_lazy_capture', 'tests/reference/client/statistics/support.py')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='lazy-' + args.mode + '-', dir=args.work_dir.resolve()))
    owned = evidence / 'owned'
    require(str(owned).isascii(), 'Declared native fixture ASCII directory')
    (owned / 'snapshots').mkdir(parents=True)
    inputs = recipe.prepared('collision-piston', saving, construction)
    require(set(inputs) == {'world/level.dat', 'world/data/idcounts.dat',
                            'world/region/r.0.0.mcr', 'world/preserve.bin'}, 'Exact authored seed')
    for relative, raw in inputs.items():
        require(type(raw) is bytes and len(raw) <= LIMIT, 'Bounded literal input')
        path = owned / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    selected = guards.source_inputs(executable) | {Path(__file__).resolve(),
        ROOT / 'tests/tools/world/arrow_owned_leaves_inputs.py',
        ROOT / 'tests/tools/world/test_source_world_construction.py',
        ROOT / 'tests/reference/client/statistics/support.py',
        ROOT / 'tests/world/runtime/world_access/test_furnace_owned_lazy.c',
        ROOT / 'tests/world/runtime/world_access/furnace_lazy_controls.inc',
        ROOT / 'tests/world/runtime/world_access/test_arrow_owned_leaves.c',
        ROOT / 'tests/world/runtime/world_access/test_collision_query_execution.c',
        ROOT / 'tests/world/runtime/tile_entities/test_tile_entities.c'}
    def current():
        return guards.fingerprints(selected | guards.source_inputs(executable))
    before = current()
    require(all(type(value) is str for value in before.values()), 'Readable selected source/product inputs')
    report = {'status': 'FAIL', 'mode': args.mode, 'inputs_before': before,
              'scope': 'Declared native captured Block36/Chunk61 or62 and independent defining '
                       'namespaces; real factory/orientation and checked native ownership; '
                       'no observed original race or full outer Arrow/SL claim.'}
    try:
        command = [str(executable), args.mode, str(owned)]
        child = support.run_bounded(command, evidence, 'native', timeout=TIMEOUT,
                                    limit=LIMIT, env=os.environ.copy())
        report['capture'] = child.capture_report
        print(child.stdout.decode('utf-8', errors='backslashreplace'), end='')
        print(child.stderr.decode('utf-8', errors='backslashreplace'), end='', file=sys.stderr)
        report['checks'] = completed(child, args.mode, command)
        after = guards.snapshot_files(owned)
        expected = {relative: guards.digest(owned / relative) for relative in inputs}
        require(all((owned / relative).read_bytes() == raw for relative, raw in inputs.items()),
                'Every actual authored save byte preserved')
        require(set(after) == set(expected) | {'world/session.lock'} and
                (owned / 'world/session.lock').stat().st_size == 8,
                'Only actual eight-byte Source session token added')
        report['literal_inputs_unchanged'] = True
        report['status'] = 'PASS'
    except BaseException as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        if getattr(error, 'capture_report', None) is not None:
            report['capture'] = error.capture_report
    finally:
        report['inputs_after'] = current()
        if report['inputs_after'] != before:
            report['status'] = 'FAIL'
            report['continuity_failure'] = 'Selected source/helper/product inputs changed'
        (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Owned lazy Furnace artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Actual owned lazy Furnace control: ' + str(evidence))


if __name__ == '__main__':
    main()
