#!/usr/bin/env python3
"""Capture the finite Properties byte-stream corpus on the retained Windows JDK."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PINS = {
    'helper': ('tests/reference/java/PropertiesObservation.java',
               '84fb0334a8e4e04f11ca667698cc0fe0b0a71a6cd9d173aa945af6322ce0ac9a'),
    'protocol': ('scripts/reference/java/properties_protocol.py',
                 '8939b0fd57f03ecb2a0ac97728c20d9e5f08ca3ab6f1d38c83fa6cd6057e0e77'),
    'runner': ('scripts/reference/java/capture_nanotime_conversion.py',
               'f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c'),
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def imported(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ('java', 'javac', 'output-dir'):
        parser.add_argument('--' + argument, type=Path, required=True)
    args = parser.parse_args()
    sources = {name: ROOT / path for name, (path, _) in PINS.items()}
    for name, (_, expected) in PINS.items():
        require(hashlib.sha256(sources[name].read_bytes()).hexdigest() == expected,
                'reviewed source binding: ' + name)
    runner = imported('properties_owned_runner', sources['runner'])
    protocol = imported('properties_protocol', sources['protocol'])
    java, javac = args.java.resolve(strict=True), args.javac.resolve(strict=True)
    require(java.parent == javac.parent, 'one JDK required')
    jdk = java.parent.parent
    inputs = {**sources, 'driver': Path(__file__).resolve(), 'java': java, 'javac': javac,
              'modules': jdk / 'lib/modules', 'jvm': jdk / 'bin/server/jvm.dll',
              'java_native': jdk / 'bin/java.dll', 'release': jdk / 'release',
              'source_archive': jdk / 'lib/src.zip'}
    for path in sorted((jdk / 'bin').rglob('*.dll')):
        inputs['runtime/' + path.relative_to(jdk).as_posix()] = path
    fp = runner.fingerprint
    before = {name: {'path': str(path), **fp(path)} for name, path in inputs.items()}
    require(all(before[name]['sha256'] == expected for name, expected in runner.PROFILE.items()),
            'pinned Windows JDK profile')
    out = args.output_dir.resolve()
    out.mkdir(exist_ok=False)
    classes = out / 'classes'
    classes.mkdir()
    for name in ('helper', 'protocol', 'runner', 'driver'):
        shutil.copy2(inputs[name], out / inputs[name].name)
    receipts = []
    report = {'schema': 'PROPERTIES_JDK_CAPTURE_V1', 'status': 'failed',
              'scope': 'Actual JDK byte-stream loads and String identities; no game initialization',
              'inputs_before': before, 'children': receipts}
    try:
        require(not runner.run([javac, '-J-Xmx256m', '-encoding', 'UTF-8', '-d', classes,
                                sources['helper']], out, 'compile', receipts), 'compiler stdout')
        traces = []
        modes = [('interpreted', ['-Xint']), ('tier1', ['-XX:TieredStopAtLevel=1']), ('default', [])]
        for name, flags in modes:
            traces.append(runner.run([java, *flags, '-Xmx128m', '-cp', classes,
                                      'PropertiesObservation'], out, name, receipts))
        require(all(trace == traces[0] for trace in traces), 'mode traces differ')
        projected = protocol.parse(traces[0])
        for trace in traces[1:]:
            protocol.parse(trace)
        fixture = out / 'properties_observation.tsv'
        fixture.write_bytes(traces[0])
        source_hashes = {path: fp(ROOT / path) for path, _ in PINS.values()}
        source_hashes[Path(__file__).resolve().relative_to(ROOT).as_posix()] = fp(Path(__file__).resolve())
        provenance = {
            'schema': 'PROPERTIES_OBSERVATION_PROVENANCE_V1',
            'scope': report['scope'], 'counts': projected['counts'],
            'fixture': fp(fixture), 'sources': source_hashes,
            'jdk_profile': {name: before[name] | {'path': name} for name in runner.PROFILE},
            'classes': {path.name: fp(path) for path in sorted(classes.glob('*.class'))},
            'modes': [{'name': name, 'flags': flags, 'trace': fp(out / (name + '.stdout')),
                       'exit_code': receipts[index + 1]['exit_code'],
                       'reaped': receipts[index + 1]['reaped']} for index, (name, flags) in enumerate(modes)],
            'limitation': 'Equal mode output does not prove JIT compilation or whole-game parity',
        }
        report.update(status='captured', counts=projected['counts'], fixture=fp(fixture))
    except BaseException as error:
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        report['inputs_after'] = {name: {'path': str(path), **fp(path)} for name, path in inputs.items()}
        report['inputs_unchanged'] = before == report['inputs_after']
        if not report['inputs_unchanged']:
            report['status'] = 'failed'
        if report['status'] == 'captured':
            provenance['inputs_unchanged'] = True
            with (out / 'properties_provenance.json').open('x', encoding='utf-8', newline='\n') as stream:
                json.dump(provenance, stream, indent=2)
                stream.write('\n')
        report['artifacts'] = {path.relative_to(out).as_posix(): fp(path)
                               for path in sorted(out.rglob('*')) if path.is_file()}
        with (out / 'capture.json').open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
    require(report['status'] == 'captured', 'capture failed; inspect retained diagnostics')
    print(json.dumps({'status': report['status'], 'counts': report['counts'], 'fixture': report['fixture']}))


if __name__ == '__main__':
    main()
