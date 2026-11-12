"""Capture finite real original Leaves/Snow-layer/Ice harvest invocations."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SUPPORT = ROOT / 'tests/reference/client/controller'
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
CLASSES = ('bk jr nk nr jp uu os ob fd ok lm hl sn gs dc iz ix gm bl wc dq ln jl vr xi '
           'wy wt xa yy pm gr kq net/minecraft/client/Minecraft').split()
TAGS = ('PROFILE\t', 'RESULT\t', 'ITEM\t', 'EVENT\t')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile-limit', type=int, help='Private diagnosis subset, never a full packet')
    parser.add_argument('--repeats', type=int, default=2)
    args = parser.parse_args()
    ensure(1 <= args.repeats <= 3, 'bounded independent fresh VMs')
    jar = args.jar.resolve(strict=True)
    ensure(digest(jar) == JAR, 'verified unchanged client archive')
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
           'pinned original-compatible libraries')
    java, javac = (Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac'))
    ensure(java.parent == javac.parent, 'one observation JDK')
    vm = java.parent / 'server/jvm.dll'
    if not vm.is_file():
        vm = java.parent.parent / 'lib/server/libjvm.so'
    inputs = [jar, *libraries, java, javac, java.parent.parent / 'lib/modules', vm.resolve(strict=True),
              HERE / 'BlockHarvestObservation.java', HERE / 'profiles.tsv', Path(__file__).resolve(),
              SUPPORT / 'capture.py', SUPPORT / 'classfile.py']
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    products = output / 'classes'
    products.mkdir()
    before = {str(path): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before, 'runs': []}
    (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n')
    try:
        profiles = (HERE / 'profiles.tsv').read_text().splitlines()
        ensure(len(profiles) == 92, 'complete original invocation inventory')
        if args.profile_limit is not None:
            ensure(1 <= args.profile_limit <= 92, 'bounded diagnosis prefix')
            profiles = profiles[:args.profile_limit]
        profile_file = output / 'profiles.tsv'
        profile_file.write_text('\n'.join(profiles) + '\n', encoding='utf-8', newline='\n')
        pins = {'client_jar_sha256': JAR, 'classes': {}}
        with zipfile.ZipFile(jar) as archive:
            for name in CLASSES:
                pins['classes'][name] = inspect_class(archive.read(name + '.class'))
        (output / 'source-pins.json').write_text(json.dumps(pins, indent=2) + '\n',
                                                encoding='utf-8', newline='\n')
        classpath = os.pathsep.join(str(path) for path in [products, jar, *libraries])
        compiled = capture([str(javac), '-cp', classpath, '-d', str(products),
                            str(HERE / 'BlockHarvestObservation.java')], output, 'compile', 60)
        require_success(compiled, empty_stderr=True)
        ensure(all(path.name.startswith('BlockHarvestObservation') for path in products.iterdir()),
               'only project observer classes, no original replacements')
        selections = []
        guard_selections = []
        for repeat in range(args.repeats):
            command = [str(java), '-Xint', '-Xverify:all', '-Xmx512m', '-Djava.awt.headless=true',
                       '-Djava.security.manager=allow', '--add-opens=java.base/java.lang=ALL-UNNAMED',
                       '--add-opens=java.base/java.util=ALL-UNNAMED', '-cp', classpath,
                       'BlockHarvestObservation', str(output / ('game-' + str(repeat))),
                       str(profile_file)]
            observed = capture(command, output, 'original-' + str(repeat), 600)
            report['runs'].append(observed)
            require_success(observed)
            lines = (output / ('original-' + str(repeat) + '.stdout')).read_text().splitlines()
            selected = [line for line in lines if line.startswith(TAGS)]
            guards = [line for line in lines if line.startswith('GUARD\t')]
            ensure(sum(line.startswith('RESULT\t') for line in selected) == len(profiles),
                   'each actual original invocation reached observation')
            ensure(guards == ['GUARD\tsource_network_attempts\t0',
                              'GUARD\tsource_native_attempts\t0', 'GUARD\tconnect_denied\t1',
                              'GUARD\tlisten_denied\t1', 'GUARD\tnative_link_denied\t1'],
                   'zero source attempts and actual denial controls')
            selections.append(selected)
            guard_selections.append(guards)
        ensure(all(rows == selections[0] for rows in selections), 'independent exact original repeats')
        ensure(all(rows == guard_selections[0] for rows in guard_selections), 'independent guard repeats')
        (output / 'observations.tsv').write_text('\n'.join(selections[0]) + '\n',
                                                encoding='utf-8', newline='\n')
        (output / 'guards.tsv').write_text('\n'.join(guard_selections[0]) + '\n',
                                          encoding='utf-8', newline='\n')
        report.update(status='PASS', complete_packet=args.profile_limit is None,
                      fresh_vm_repeats=args.repeats, profiles=len(profiles),
                      observations_sha256=digest(output / 'observations.tsv'),
                      guards_sha256=digest(output / 'guards.tsv'),
                      source_pins_sha256=digest(output / 'source-pins.json'),
                      helpers_sha256={path.name: digest(path) for path in inputs[-5:]})
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['inputs_unchanged'] = before == report['inputs_after']
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Original Block harvest capture:', output)


if __name__ == '__main__':
    main()
