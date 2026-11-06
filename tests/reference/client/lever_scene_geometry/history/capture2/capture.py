"""Execute the unchanged original scene Lever/Dust renderer twice without a graphics context."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--world', type=Path, required=True)
    args = parser.parse_args()
    support = module('lever_original_support', HERE.parent / 'picking/observe.py')
    inputs = module('lever_authored_inputs', ROOT / 'tests/tools/client/test_picking.py')
    world_input = args.world.resolve(strict=True)
    planes = HERE / 'scene-input.bin'
    jar = ROOT / 'b1.7.3.jar'
    assert digest(jar) == support.JAR_SHA
    libraries = [Path(os.environ['APPDATA']) / '.minecraft/libraries' / name for name in support.LIBRARIES]
    assert all(digest(path) == pin for path, pin in zip(libraries, support.LIBRARIES.values()))
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    assert java.parent == javac.parent
    paths = [jar, *libraries, java, javac, java.parent.parent / 'lib/modules', HERE / 'LeverSceneObservation.java', Path(__file__),
             ROOT / 'tests/tools/client/test_picking.py', HERE.parent / 'picking/observe.py', planes, HERE / 'scene-input-origin.json', *sorted(path for path in world_input.rglob('*') if path.is_file())]
    before = {str(path): digest(path) for path in paths}
    args.output.mkdir(parents=True, exist_ok=False)
    classes = args.output / 'classes'
    classes.mkdir()
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    cp = os.pathsep.join(map(str, [jar, *libraries]))
    subprocess.run([str(javac), '-classpath', cp, '-d', str(classes), str(HERE / 'LeverSceneObservation.java')],
                   check=True, env=environment, capture_output=True)
    outputs = []
    compiled = {str(path): digest(path) for path in classes.rglob('*.class')}
    for index in (1, 2):
        world = args.output / ('world-' + str(index))
        shutil.copytree(world_input, world)
        result = subprocess.run([str(java), '-cp', str(classes) + os.pathsep + cp, 'LeverSceneObservation', str(jar), str(world), str(planes)],
                                check=False, env=environment, capture_output=True, timeout=90)
        (args.output / ('ordinary-' + str(index) + '.stdout')).write_bytes(result.stdout)
        (args.output / ('ordinary-' + str(index) + '.stderr')).write_bytes(result.stderr)
        assert result.returncode == 0, result.stderr.decode(errors='replace')
        lines = result.stdout.decode('ascii').splitlines()
        assert lines[-1] == 'SCENE_COMPLETE calls=160 network=0 native=0 exec=0'
        rows = [line for line in lines if line.startswith('SCENE\t')]
        assert len(rows) == 160
        outputs.append(rows)
    assert outputs[0] == outputs[1]
    assert before == {str(path): digest(path) for path in paths}
    assert compiled == {str(path): digest(path) for path in classes.rglob('*.class')}
    (args.output / 'selected.txt').write_text('\n'.join(outputs[0]) + '\n', encoding='ascii', newline='\n')
    (args.output / 'capture.json').write_text(json.dumps({'status': 'PASS', 'inputs_sha256': before,
        'compiled_sha256': compiled, 'selected_sha256': digest(args.output / 'selected.txt'), 'calls_per_vm': 160,
        'scope': 'Unchanged cv.b/e/g, xr bounds, ordinary culling, flat and AO bases, exact scene coordinates and NW translation(0,-64,0). Explicit raw Minecraft/settings prestate; no constructor, loaded scene or GL claim.'}, indent=2) + '\n')
    print('PASS: two original VMs, 160 scene render calls each')


if __name__ == '__main__':
    main()
