"""Capture original unarmed Player attack over genuine App/World/Player owners."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parents[1] / 'client/controller'
from compare import rows
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
REQUIRED = {
    'ob': {('b', '(Lgs;Lsn;)V')},
    'gs': {('d', '(Lsn;)V'), ('c', '(Lsn;I)V'), ('a', '(Lls;)V'), ('a', '(Lls;Z)V')},
    'ix': {('b', '()Liz;'), ('f_', '(I)Liz;'), ('a', '(Lsn;)I')},
    'dc': {('a', '(Lvr;I)V')},
    'ls': {('a', '(Lsn;I)Z'), ('b', '(Lsn;)V'), ('W', '()Z'), ('h_', '()Z')},
    'sn': {('m_', '()F')},
    'uu': {('b', '(I)I'), ('g', '()V')}, 'ee': set(),
    'gm': {('f', '(I)I'), ('b', '()Z'), ('c', '()Z')},
    'bi': {('a', '(I)I')}, 'ck': set(),
    'bx': {('j', '()I')}, 'dl': {('a', '(Lsn;I)Z'), ('q', '()V')},
    'gi': {('D', '()Z'), ('A', '()Ljava/lang/String;'), ('B', '()Z'), ('b', '(Z)V')},
    'ii': {('G', '()Lsn;'), ('c', '(Lsn;)V')},
    'fd': {('a', '(Ljava/lang/Class;Leq;)Ljava/util/List;'), ('b', '(Lsn;)Z')},
}
MOBS = ('cn', 'uz', 'fr', 'gb', 'uw', 'wh', 'dl', 'bx', 'ww', 'gi', 'xt')
for name in (*MOBS, 'bg', 'ar', 'gz'):
    REQUIRED.setdefault(name, set())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    ensure(digest(jar) == JAR, 'verified client archive')
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
           'verified CPU reference libraries')
    java, javac = Path(shutil.which('java')).resolve(), Path(shutil.which('javac')).resolve()
    ensure(java.parent == javac.parent, 'one JDK tool installation')
    tools = [java, javac, java.parent.parent / 'lib/modules',
             java.parent / 'server/jvm.dll' if os.name == 'nt' else
             java.parent.parent / 'lib/server/libjvm.so']
    inputs = [jar, *libraries, *tools, HERE / 'PlayerAttackObservation.java',
              Path(__file__).resolve(), HERE / 'compare.py', SUPPORT / 'capture.py',
              SUPPORT / 'classfile.py']
    before = {str(path): digest(path) for path in inputs}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    report = {'status': 'FAIL', 'inputs_before': before, 'scope': __doc__,
              'entropy': 'Original constructor and shared Math entropy remain independent; rows compare selected deterministic integer/boolean state only.'}
    try:
        pins = {'client_jar_sha256': JAR, 'classes': {}}
        with zipfile.ZipFile(jar) as archive:
            for name, signatures in REQUIRED.items():
                actual = inspect_class(archive.read(name + '.class'))
                if name in (*MOBS, 'bg', 'ar', 'gz', 'ii'):
                    ensure(not any(m['name'] in ('h_', 'm_') for m in actual['methods']),
                           'actual inherited Mob pick methods: ' + name)
                if name == 'ee':
                    ensure(not any((m['name'], m['descriptor']) in {('b', '(I)I'), ('g', '()V')}
                                   for m in actual['methods']), 'Wool inherits actual inventory methods')
                actual['methods'] = [m for m in actual['methods'] if (m['name'], m['descriptor']) in signatures]
                ensure({(m['name'], m['descriptor']) for m in actual['methods']} == signatures,
                       'actual original method declarations: ' + name)
                pins['classes'][name] = actual
        report['pins'] = pins
        classpath = os.pathsep.join(map(str, [classes, jar, *libraries]))
        report['compile'] = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath,
                                     '-d', str(classes), str(HERE / 'PlayerAttackObservation.java')],
                                    output, 'compile', 60, os.environ.copy())
        require_success(report['compile'], empty_stderr=True)
        helpers = {p.name: digest(p) for p in classes.iterdir()}
        ensure(set(helpers) == {'PlayerAttackObservation.class', 'PlayerAttackObservation$Observer.class',
                                'PlayerAttackObservation$1.class'}, 'only authored observation classes')
        report['helpers_before'] = helpers
        report['original'] = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m',
                                      '-Djava.awt.headless=true', '-Djava.security.manager=allow',
                                      '-classpath', classpath, 'PlayerAttackObservation', str(jar),
                                      str(output / 'game')], output, 'original', 120, os.environ.copy(), output)
        require_success(report['original'])
        text = (output / report['original']['stdout_file']).read_text()
        ensure(text.splitlines()[-1] == 'Original Player attack: 5 rows; network=0; native=0; genuine_owners=1',
               'ordinary original completion and external-effect guards')
        report['rows'] = rows(text)
        icons = [line.removeprefix('WOOL_ITEM_ICONS ') for line in text.splitlines()
                 if line.startswith('WOOL_ITEM_ICONS ')]
        ensure(len(icons) == 1 and len(icons[0].split(',')) == 16,
               'sixteen actual original Wool Item icon calls')
        report['wool_item_icons'] = [int(word) for word in icons[0].split(',')]
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['helpers_after'] = {p.name: digest(p) for p in classes.iterdir()}
        if before != report['inputs_after'] or report.get('helpers_before', {}) != report['helpers_after']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n')
    ensure(report['status'] == 'PASS', 'unchanged capture inputs')
    print('Original Player attack: 5 selected deterministic CPU rows; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
