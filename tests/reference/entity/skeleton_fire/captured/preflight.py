"""PRIVATE supplier/profile seal checks; never starts a JVM or native program."""
import argparse
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def need(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def live_codes(records, pins):
    codes = [r for r in records if r.get('event') == 'code']
    seen = set()
    for code in codes:
        need(set(code) == {'event', 'class', 'method', 'descriptor', 'bytes', 'sha256'}, 'closed actual original Code witness')
        key = (code['class'], code['method'], code['descriptor']); need(key not in seen, 'unique original Code witness'); seen.add(key)
        need(code['class'] in pins['classes'], 'live Code owner is bound original class')
        m = next((m for m in pins['classes'][code['class']]['methods'] if (m['name'], m['descriptor']) == key[1:]), None)
        need(m is not None and (code['bytes'], code['sha256']) == (m['code_length'], m['code_sha256']), 'executing complete original Code identity')
    need({('fr', 'a', '(Lsn;F)V'), ('sl', '<init>', '(Lfd;Lls;)V'), ('sl', 'a', '(DDDFF)V'),
          ('fd', 'a', '(Lsn;Ljava/lang/String;FF)V'), ('fd', 'b', '(Lsn;)Z'), ('fd', 'c', '(Lsn;)V'), ('lm', 'a', '(Lsn;)V')} <= seen,
         'all complete source parent/constructor/heading/sound/publication entries witnessed')
    helpers = [r for r in records if r.get('event') == 'helper-code']
    need(all(r['class'] == 'SkeletonFireObservation$Probe' for r in helpers), 'authored callback Code is separate from original authority')
    return {'original_Code_records': len(codes), 'authored_helper_Code_records': len(helpers)}


def require_seal(expected):
    need(type(expected) is str and len(expected) == 64 and digest(HERE / 'seal.json') == expected, 'exact reviewed private supplier seal')
    seal = json.loads((HERE / 'seal.json').read_text(encoding='utf-8'))
    need(seal['schema'] == 'PRIVATE_SKELETON_FIRE_SUPPLIER_SEAL_V1' and seal['original_execution'] is False, 'unexecuted preparation seal')
    for key, base in (('private_inputs', HERE), ('support_inputs', ROOT)):
        for name, pin in seal[key].items():
            path = base / name
            need(path.is_file() and digest(path) == pin, 'exact frozen supplier ' + name)
    for name in seal['private_inputs']:
        path = HERE / name
        if path.suffix == '.py':
            ast.parse(path.read_text(encoding='utf-8'), filename=str(path))
    jar = ROOT / seal['archive']['path']; need(jar.stat().st_size == seal['archive']['bytes'] and digest(jar) == seal['archive']['sha256'], 'unchanged original archive')
    spec = importlib.util.spec_from_file_location('private_fr_classfile', ROOT / 'tests/reference/entity/player/classfile.py')
    parser = importlib.util.module_from_spec(spec); spec.loader.exec_module(parser)
    pins = json.loads((HERE / 'source-pins.json').read_text(encoding='utf-8'))
    with zipfile.ZipFile(jar) as archive:
        need(len(archive.namelist()) == len(set(archive.namelist())), 'unique original archive members')
        for name, pin in pins['classes'].items():
            need(parser.parse_class(archive.read(name + '.class')) == pin, 'whole original class and Code ' + name)
    primary = next(m for m in pins['classes']['fr']['methods'] if m['name'] == 'a' and m['descriptor'] == '(Lsn;F)V')
    need(primary['code_length'] == 189 and primary['code_sha256'] == '5b793ac77ab703d4184cd034ab01f10055003bf0fccb7fbdf09b379386ccf0bc' and
         primary['code_attribute_sha256'] == '8abbc27c936f14fbdb9ff090482fbf03a8c9515c0e75994ad15e011618322a4a' and primary['exception_table'] == [], 'complete FR189 source authority')
    ctor = next(m for m in pins['classes']['fr']['methods'] if m['name'] == '<init>' and m['descriptor'] == '(Lfd;)V')
    need(ctor['access'] == 1 and ctor['code_length'] == 12, 'genuine public reflected one-World actor constructor')
    import observation
    rows = observation.profiles()
    return {'status': 'PASS_STATIC_SUPPLIER_PREFLIGHT_ONLY', 'seal_sha256': expected, 'profiles': len(rows),
            'classes': len(pins['classes']), 'JVM_started': False, 'native_started': False, 'original_execution_credit': False}


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--expected-seal-sha256', required=True)
    args = p.parse_args(); print(json.dumps(require_seal(args.expected_seal_sha256), indent=2))


if __name__ == '__main__':
    main()
