#!/usr/bin/env python3
"""Capture untouched nested Item construction at PC2664 using pinned Windows JDK17."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
RUNNER = ROOT / 'scripts/reference/java/capture_nanotime_conversion.py'
SOURCES = {
    'ItemRegistryPrefixObservation.java': '0b38543e91fbf81f74c44b9290c8a9b8e047a64a881041f4eb8035667adcff1b',
    'ItemRegistryPrefixController.java': '8a3c6f8386ec06fd271123e0911a5925a2035ea103bfcef88b2f7edbcfdaca89',
}
JARS = {
    'client': (1465375, 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'),
    'server': (503100, '033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d'),
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('java', 'javac', 'client', 'server', 'output-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    # Import only the already reviewed bounded owned-child runner.
    import hashlib
    require(hashlib.sha256(RUNNER.read_bytes()).hexdigest() ==
            'f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c', 'bounded runner binding')
    spec = importlib.util.spec_from_file_location('registry_owned_runner', RUNNER)
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    fingerprint = runner.fingerprint
    java, javac = args.java.resolve(strict=True), args.javac.resolve(strict=True)
    require(java.parent == javac.parent, 'one JDK required')
    jdk = java.parent.parent
    inputs = {'java': java, 'javac': javac, 'modules': jdk / 'lib/modules',
              'jvm': jdk / 'bin/server/jvm.dll', 'java_native': jdk / 'bin/java.dll',
              'release': jdk / 'release', 'source_archive': jdk / 'lib/src.zip',
              'bounded_runner': RUNNER, 'capture_driver': Path(__file__).resolve(),
              'client_jar': args.client.resolve(strict=True), 'server_jar': args.server.resolve(strict=True)}
    for name in SOURCES:
        inputs[name] = ROOT / 'tests/reference/item' / name
    for path in sorted((jdk / 'bin').rglob('*.dll')):
        inputs['runtime/' + path.relative_to(jdk).as_posix()] = path
    before = {name: {'path': str(path), **fingerprint(path)} for name, path in inputs.items()}
    for name, expected in runner.PROFILE.items():
        require(before[name]['sha256'] == expected, 'pinned Windows JDK profile: ' + name)
    for name, expected in SOURCES.items():
        require(before[name]['sha256'] == expected, 'reviewed source: ' + name)
    for side, (size, checksum) in JARS.items():
        require(before[side + '_jar']['bytes'] == size and before[side + '_jar']['sha256'] == checksum,
                'pinned original ' + side)
    output = args.output_dir.resolve()
    output.mkdir(exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    receipts = []
    report = {'status': 'failed', 'scope': 'Actual original Block-first entry stopped at nested Item2664 before statistics active use; requested debugger termination, not natural bootstrap completion',
              'inputs_before': before, 'children': receipts, 'sides': {}}
    try:
        runner.run([javac, '-J-Xmx256m', '--add-modules', 'jdk.jdi', '-encoding', 'UTF-8', '-d', classes,
                    inputs['ItemRegistryPrefixObservation.java'], inputs['ItemRegistryPrefixController.java']],
                   output, 'compile', receipts)
        report['compiled_classes'] = {p.name: fingerprint(p) for p in sorted(classes.glob('*.class'))}
        require(bool(report['compiled_classes']), 'compiled helper classes required')
        for side in JARS:
            result = {'status': 'unavailable'}
            report['sides'][side] = result
            try:
                runner.run([java, '-Xint', '-Xmx256m', '--add-modules', 'jdk.jdi', '-cp', classes,
                            'ItemRegistryPrefixController', side, inputs[side + '_jar'], output / side],
                           output, side + '-controller', receipts)
                receipt_raw = (output / side / 'receipt.tsv').read_bytes()
                require(receipt_raw.endswith(b'\n') and b'\r' not in receipt_raw, 'literal receipt framing')
                rows = [line.split('\t') for line in receipt_raw.decode('ascii').splitlines()]
                require(rows[0] == ['ITEM_PREFIX_PROCESS_V1'] and ['STATUS', 'PASS'] in rows, 'accepted process receipt')
                child = [r for r in rows if r[0] == 'CHILD']
                require(len(child) == 1 and child[0][2:] == ['true', '0', 'true', 'true'], 'owned debugger child exit/reap')
                require(['WATCHDOG', 'false', 'false'] in rows, 'deadline watchdog must not fire')
                raw = (output / side / 'snapshot.raw.tsv').read_bytes()
                require(raw.endswith(b'\n') and b'\r' not in raw and len(raw) <= 8 * 1024 * 1024, 'raw snapshot framing')
                snapshot = [line.split('\t') for line in raw.decode('ascii').splitlines()]
                require(snapshot[0] == ['ITEM_REGISTRY_PREFIX_V1', side] and snapshot[-1][0:4] == ['END', '106', '96', '5'], 'complete prefix snapshot')
                require(int(snapshot[-1][-1]) == len(snapshot) - 1, 'full snapshot footer')
                require(sum(r[0] == 'STATIC' and r[1] == 'BLOCK' for r in snapshot) == 113, 'all static cells')
                require(sum(r[0] == 'BLOCK_ALIAS' for r in snapshot) == 96, 'all named Blocks')
                require(sum(r[0] == 'PRIVATE_RANDOM' for r in snapshot) == 4, 'four actual private Randoms')
                require(sum(r[0] == 'STATIC' and r[1] == 'ITEM' for r in snapshot) == 108, 'all Item static cells')
                require(sum(r[0] == 'ITEM_ALIAS' for r in snapshot) == 106, 'all named Items')
                require(sum(r[0] == 'ITEM_STATIC_RANDOM' for r in snapshot) == 1, 'actual Item Random')
                result.update(status='captured', child_pid=int(child[0][1]), rows=len(snapshot),
                              snapshot=fingerprint(output / side / 'snapshot.raw.tsv'),
                              receipt=fingerprint(output / side / 'receipt.tsv'))
            except Exception as error:
                result['failure'] = str(error)
        report['status'] = 'captured' if all(v['status'] == 'captured' for v in report['sides'].values()) else 'unavailable'
    except Exception as error:
        report['failure'] = str(error)
        raise
    finally:
        report['inputs_after'] = {name: {'path': str(path), **fingerprint(path)} for name, path in inputs.items()}
        report['inputs_unchanged'] = before == report['inputs_after']
        if not report['inputs_unchanged']:
            report['status'] = 'failed'
        report['artifacts'] = {p.relative_to(output).as_posix(): fingerprint(p)
                               for p in sorted(output.rglob('*')) if p.is_file()}
        with (output / 'capture.json').open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
        print(json.dumps({'output': str(output), 'status': report['status'], 'sides': report['sides']}, indent=2))
    require(report['status'] == 'captured', 'original observation unavailable; retained diagnostics are authoritative')


if __name__ == '__main__':
    main()
