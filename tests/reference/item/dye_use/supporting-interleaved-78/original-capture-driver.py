"""Freeze and repeat declared Dye calls against unchanged original classes."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
AUTHORED = ('DyeObservation.java', 'profiles.tsv', 'declared-inputs.json', 'source-pins.json', 'capture.py')
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def tree(path):
    return {p.relative_to(path).as_posix(): digest(p) for p in sorted(path.rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--attempt', type=int, required=True)
    args = parser.parse_args()
    out = HERE / ('attempt-' + str(args.attempt))
    out.mkdir(exist_ok=False)
    jar = ROOT / 'b1.7.3.jar'
    assert digest(jar) == JAR
    P = load('dye_world_inputs', ROOT / 'tests/reference/client/picking/observe.py')
    C = load('dye_class_pins', ROOT / 'tests/reference/client/controller/classfile.py')
    libraries = [Path(os.environ['APPDATA']) / '.minecraft/libraries' / name for name in P.LIBRARIES]
    assert all(digest(p) == h for p, h in zip(libraries, P.LIBRARIES.values()))
    java, javac = Path(shutil.which('java')), Path(shutil.which('javac'))
    inputs = [jar, *libraries, java, javac, java.parent.parent / 'lib/modules',
              java.parent / 'server/jvm.dll', *(HERE / name for name in AUTHORED),
              ROOT / 'tests/reference/client/picking/observe.py',
              ROOT / 'tests/reference/client/controller/classfile.py']
    before = {str(p): digest(p) for p in inputs}
    pins = json.loads((HERE / 'source-pins.json').read_text())
    with zipfile.ZipFile(jar) as archive:
        assert pins['client_jar_sha256'] == JAR
        for name, wanted in pins['classes'].items():
            actual = C.inspect_class(archive.read(name + '.class'))
            assert wanted == actual, ('unchanged original whole class/Code', name)
    declared = json.loads((HERE / 'declared-inputs.json').read_text())
    profiles = [{'name': row.split('\t')[0], 'inputs': list(map(int, row.split('\t')[1:]))}
                for row in (HERE / 'profiles.tsv').read_text().splitlines()]
    assert profiles == declared['rows']
    frozen, classes = out / 'frozen', out / 'classes'
    frozen.mkdir()
    classes.mkdir()
    for name in AUTHORED:
        shutil.copy2(HERE / name, frozen / name)
    write(out / 'freeze.json', {'inputs_before': before,
                               'frozen_authored_files': tree(frozen), 'profiles': profiles})
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    report = {'status': 'FROZEN_NO_EXECUTION', 'original_game_class_replacements': 0,
              'inputs_before': before, 'runs': [], 'profiles': len(profiles)}

    def run(name, command, timeout):
        child = subprocess.run(command, capture_output=True, timeout=timeout, env=environment)
        (out / (name + '.stdout')).write_bytes(child.stdout)
        (out / (name + '.stderr')).write_bytes(child.stderr)
        value = {'command': command, 'returncode': child.returncode,
                 'stdout_sha256': hashlib.sha256(child.stdout).hexdigest(),
                 'stderr_sha256': hashlib.sha256(child.stderr).hexdigest()}
        write(out / (name + '.json'), value)
        assert child.returncode == 0, (name, child.stderr.decode(errors='replace'))
        return child.stdout, value

    try:
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        run('compile', [str(javac), '-encoding', 'UTF-8', '-cp', cp, '-d', str(classes),
                        str(frozen / 'DyeObservation.java')], 30)
        helpers = tree(classes)
        assert set(helpers) == {'DyeObservation.class', 'DyeObservation$Observer.class', 'DyeObservation$1.class'}
        report['helpers_before'] = helpers
        raw = []
        for repeat in range(2):
            world = out / ('ordinary-' + str(repeat)) / 'world'
            P.world(world)
            prior = tree(world)
            stdout, capture = run('ordinary-' + str(repeat), [str(java), '-Xint', '-Xverify:all',
                '-Xmx384m', '-Djava.awt.headless=true', '-Djava.security.manager=allow',
                '--add-opens=java.base/java.util=ALL-UNNAMED', '-cp', cp, 'DyeObservation',
                str(jar), str(world), str(frozen / 'profiles.tsv')], 180)
            after = tree(world)
            assert all(name == 'session.lock' or after.get(name) == h for name, h in prior.items())
            report['runs'].append({'capture': capture, 'world_before': prior, 'world_after': after})
            write(out / 'capture.json', report)
            raw.append(stdout)
        assert raw[0] == raw[1], 'two completed ordinary fresh VMs must agree byte for byte'
        lines = [line for line in raw[0].decode('ascii').splitlines() if line.startswith('DYE')]
        rows = [json.loads(line.split('\t', 1)[1]) for line in lines if line.startswith('DYE\t')]
        assert len(rows) == len(profiles)
        assert [(r['name'], r['inputs']) for r in rows] == [(r['name'], r['inputs']) for r in profiles]
        assert lines[-1].startswith('DYE_COMPLETE rows=' + str(len(rows)) + ' network=0 native=0 execs=0 denied-writes=0 ')
        (out / 'selected.jsonl').write_text('\n'.join(lines) + '\n', encoding='ascii', newline='\n')
        assert tree(classes) == helpers
        assert {str(p): digest(p) for p in inputs} == before
        report.update(status='PASS', inputs_unchanged=True, helpers_unchanged=True,
                      ordinary_fresh_vms=2, source_writes_from_jdi=0, target_invocations_from_jdi=0,
                      selected_sha256=digest(out / 'selected.jsonl'), raw_stdout_sha256=hashlib.sha256(raw[0]).hexdigest(),
                      scope='Declared direct KM and separately prepared IZ wrappers on unchanged original classes '
                            'and genuine saved World/Chunk/App/Player/statistic owners. Actual Item and World '
                            'Random seeds are declared inputs. Full finite map/array changes and ordered callbacks. '
                            'No native comparison, JDI ordering observation, graphical, server, save cycle or FPS qualification.')
    except BaseException as error:
        report['status'] = 'FAIL'
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(p): digest(p) for p in inputs}
        write(out / 'capture.json', report)
    print(json.dumps({'status': report['status'], 'profiles': len(profiles),
                       'ordinary_fresh_vms': 2, 'output': str(out), 'selected_sha256': report['selected_sha256']}))


if __name__ == '__main__':
    main()
