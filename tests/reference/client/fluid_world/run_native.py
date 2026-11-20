"""Replay every guarded original fluid cohort with genuine native owners."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile

from packet import CAPTURED, FROZEN, HERE, digest, read_packet, require

ROOT = HERE.parents[3]
ROLES = ('FLUID_INPUT', 'FLUID_CELLS', 'FLUID', 'FLUID_POOL')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in sorted(root.rglob('*')) if p.is_file()}


def observed_rows(text):
    rows = {}
    for line in text.splitlines():
        role = line.split('\t', 1)[0]
        if role not in ROLES:
            continue
        columns = line.split('\t')
        require(len(columns) > 2 and columns[1].isdigit(), 'typed native fluid row index')
        key = (role, int(columns[1]))
        require(key not in rows, 'one native fluid row per role and index')
        rows[key] = line
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    packet = read_packet()
    executable = args.executable.resolve(strict=True)
    capture = module('fluid_native_capture', HERE.parent / 'controller/capture.py')
    picking = module('fluid_native_inputs', ROOT / 'tests/tools/client/test_picking.py')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='fluid-world-', dir=args.work_dir.resolve()))
    for name, data in picking.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    resources = inventory(output / 'resources')
    inputs = [executable, *(HERE / name for name in (*FROZEN, *CAPTURED)),
              HERE / 'provenance.json', HERE / 'packet.py', HERE / 'test_packet.py',
              Path(__file__).resolve(), HERE.parent / 'controller/capture.py',
              ROOT / 'tests/tools/client/test_picking.py',
              ROOT / 'tests/client/render/test_fluid_world.c',
              ROOT / 'tests/client/world/test_picking.c',
              ROOT / 'tests/entity/player/test_construction.c', ROOT / 'CMakeLists.txt',
              *sorted((ROOT / 'cmake').rglob('*.cmake')),
              *sorted((ROOT / 'include').rglob('*.h')),
              *(p for p in sorted((ROOT / 'src').rglob('*'))
                if p.is_file() and p.suffix in ('.c', '.h', '.inc', '.glsl'))]
    before = {str(path): digest(path) for path in sorted(set(inputs))}
    report = {
        'status': 'FAIL', 'inputs_before': before, 'resource_before': resources, 'cohorts': [],
        'scope': (
            'Three fresh native processes replay 104 original CV.k calls at matching NW '
            'translation and uninterrupted cohort batches. Exact rows cover registered operands, '
            'entered cells and Java byte-array hashes, all CV/NW primitive fields, bounds, '
            'return/null prefixes and active words. Native Vector pool identity progression and '
            'raw components match the source pool rows; source class names identify mapped '
            'concepts, not native JVM objects. Own-prior inactive NW words, buffer identities, '
            'World bindings and cleanup have separate native checks. The retained original '
            'EW/JDI trace is source evidence; these outer rows do not establish native per-query '
            'trace equality, original pixels, GL frames, extended play or FPS.')}
    try:
        original_words, native_checks, unchanged_words = 0, 0, 0
        for cohort in packet['cohorts']:
            index = cohort['index']
            work = output / ('cohort-' + str(index))
            work.mkdir()
            picking.authored_world(work / 'world')
            entry = {'index': index, 'status': 'FAIL', 'world_before': inventory(work / 'world')}
            report['cohorts'].append(entry)
            receipt = capture.capture(
                [str(executable), str(output / 'resources'), str(work / 'world'),
                 str(HERE / ('profiles-' + str(index) + '.txt')), str(index)],
                work, 'native', 100, os.environ.copy(), work)
            entry['capture'] = receipt
            entry['world_after'] = inventory(work / 'world')
            capture.require_success(receipt, empty_stderr=True)
            stdout = (work / receipt['stdout_file']).read_text(encoding='ascii')
            lines = stdout.splitlines()
            marker = re.fullmatch(
                r'Source Fluid world: ([1-9][0-9]*) checks; original calls, fields, bounds, '
                r'NW words, pool and cleanup', lines[-1] if lines else '')
            require(marker is not None, 'complete native fluid cohort and cleanup')
            expected = observed_rows('\n'.join(
                line for line in (HERE / ('selected-' + str(index) + '.txt')).read_text(
                    encoding='ascii').splitlines()
                if line.split('\t', 1)[0] in ROLES))
            actual = observed_rows(stdout)
            entry['actual_rows'] = list(actual.values())
            entry['mismatches'] = [
                {'role': key[0], 'index': key[1], 'native': actual.get(key),
                 'original': expected.get(key)}
                for key in sorted(set(actual) | set(expected)) if actual.get(key) != expected.get(key)]
            if entry['mismatches']:
                raise AssertionError('native/original fluid row differs: ' +
                                     str(entry['mismatches'][0]))
            rows = cohort['observed']
            words = sum(row['delta'] for row in rows)
            own = sum(2097152 - row['delta'] for row in rows)
            controls = [line for line in lines if line.startswith('FLUID_NATIVE ')]
            require(controls == [f'FLUID_NATIVE cohort={index} calls={len(rows)} '
                                 f'own-unchanged-words={own} cleanup=1'],
                    'actual native own-prior inactive-word and ownership controls')
            original_words += words
            native_checks += int(marker[1])
            unchanged_words += own
            entry.update(status='PASS', original_calls=len(rows), exact_rows=len(expected),
                         appended_words=words, native_checks=int(marker[1]),
                         own_unchanged_words=own, controls=controls[0])
        report.update(status='PASS', original_calls=104, exact_rows=416,
                      original_appended_words=original_words, native_checks=native_checks,
                      native_own_unchanged_words=unchanged_words)
    finally:
        report['inputs_after'] = {path: digest(Path(path)) for path in before}
        report['resource_after'] = inventory(output / 'resources')
        same = before == report['inputs_after'] and resources == report['resource_after']
        if not same:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8',
                                       newline='\n')
        require(same, 'native replay source/executable/packet/resources changed')
    print('PASS: 104 exact original fluid calls and 416 rows; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
