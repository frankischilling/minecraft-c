"""Repeat ordinary unchanged-original Food Stack/controller observations locally."""
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
ROOT = next(p for p in HERE.parents if (p / 'CMakeLists.txt').is_file())
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def inventory(path):
    return {str(p.relative_to(path)): digest(p) for p in sorted(path.rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path,
                        default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    packet = load('food_packet_capture', HERE / 'packet.py')
    packet.validate(HERE)
    world_support = load('food_saved_world', ROOT / 'tests/reference/client/picking/observe.py')
    class_support = load('food_class_pins', ROOT / 'tests/reference/client/controller/classfile.py')
    jar = args.jar.resolve(strict=True)
    assert digest(jar) == JAR
    libraries = [(args.libraries / name).resolve(strict=True) for name in world_support.LIBRARIES]
    assert all(digest(p) == h for p, h in zip(libraries, world_support.LIBRARIES.values()))
    java, javac = [Path(shutil.which(p) or p).resolve(strict=True) for p in (args.java, args.javac)]
    assert java.parent == javac.parent
    expected_pins = json.loads((HERE / 'source-pins.json').read_text())
    with zipfile.ZipFile(jar) as archive:
        actual_pins = {'client_jar_sha256': JAR, 'classes': {
            name: class_support.inspect_class(archive.read(name + '.class'))
            for name in expected_pins['classes']}}
    assert actual_pins == expected_pins
    inputs = [jar, *libraries, java, javac, java.parent.parent / 'lib/modules',
              *(p for p in HERE.iterdir() if p.is_file()),
              ROOT / 'tests/reference/client/picking/observe.py',
              ROOT / 'tests/reference/client/controller/classfile.py']
    before = {str(p): digest(p) for p in inputs}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    frozen, classes = output / 'frozen', output / 'classes'
    frozen.mkdir()
    classes.mkdir()
    for path in HERE.iterdir():
        if path.is_file():
            shutil.copy2(path, frozen / path.name)
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    report = {'status': 'PREPARED', 'inputs_before': before, 'runs': []}
    write(output / 'capture.json', report)

    def run(name, command, timeout):
        child = subprocess.run(command, capture_output=True, timeout=timeout, env=environment)
        (output / (name + '.stdout')).write_bytes(child.stdout)
        (output / (name + '.stderr')).write_bytes(child.stderr)
        write(output / (name + '.json'), {'command': command, 'returncode': child.returncode})
        if child.returncode != 0:
            raise RuntimeError(name + ' failed: ' + child.stderr.decode(errors='replace'))
        return child.stdout

    try:
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        run('compile', [str(javac), '-encoding', 'UTF-8', '-cp', cp, '-d', str(classes),
                        str(frozen / 'FoodObservation.java'),
                        str(frozen / 'ControllerFoodObservation.java')], 30)
        helpers = inventory(classes)
        for role, classname, prefix in [('wrapper', 'FoodObservation', 'FOOD'),
                                         ('controller', 'ControllerFoodObservation', 'CONTROLLERFOOD')]:
            values = []
            for repeat in range(2):
                name = role + '-' + str(repeat)
                world = output / name / 'world'
                world_support.world(world)
                prior = inventory(world)
                raw = run(name, [str(java), '-Xint', '-Xverify:all', '-Xmx256m',
                                 '-Djava.awt.headless=true', '-Djava.security.manager=allow',
                                 '-cp', cp, classname, str(jar), str(world),
                                 str(frozen / 'profiles.tsv')], 90)
                after = inventory(world)
                assert all(key == 'session.lock' or after.get(key) == value
                           for key, value in prior.items())
                report['runs'].append({'role': role, 'repeat': repeat, 'world_before': prior,
                                       'world_after': after,
                                       'stdout_sha256': hashlib.sha256(raw).hexdigest()})
                selected = '\n'.join(line for line in raw.decode('ascii').splitlines()
                                     if line.startswith(prefix)) + '\n'
                values.append(selected)
            assert values[0] == values[1] == (HERE / (role + '.tsv')).read_text(encoding='ascii')
        assert helpers == inventory(classes)
        report['helpers'] = helpers
        report['inputs_after'] = {str(p): digest(p) for p in inputs}
        assert report['inputs_after'] == before
        report['status'] = 'PASS'
        report['rows'] = 28
        report['ordinary_fresh_vms'] = 4
    except Exception as error:
        report['status'] = 'FAIL'
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        write(output / 'capture.json', report)
    print('PASS: 28 original Food Stack/controller rows; four ordinary VMs; inputs unchanged')


if __name__ == '__main__':
    main()
